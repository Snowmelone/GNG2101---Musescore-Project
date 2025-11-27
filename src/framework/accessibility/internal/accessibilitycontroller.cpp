/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 */

#include "accessibilitycontroller.h"

#include <QGuiApplication>
#include <QAccessible>
#include <QWindow>
#include <QTimer>
#include <QTextToSpeech>
#include <QKeyEvent>

// Only include private header where it is used
#ifdef Q_OS_LINUX
  #include <private/qcoreapplication_p.h>
#endif

#include "iqaccessibleinterfaceregister.h"
#include "log.h"

#ifdef MUSE_MODULE_ACCESSIBILITY_TRACE
  #define MYLOG() LOGI()
#else
  #define MYLOG() LOGN()
#endif

using namespace muse;
using namespace muse::modularity;
using namespace muse::accessibility;

namespace {

// Owned by this translation unit, not in header
AccessibleObject* s_rootObject = nullptr;
std::shared_ptr<IQAccessibleInterfaceRegister> g_accessibleInterfaceRegister;

static void updateHandlerNoop(QAccessibleEvent*) {}

// Small QObject based event filter that forwards to AccessibilityController
class AccessibilityKeyFilter : public QObject
{
public:
    explicit AccessibilityKeyFilter(AccessibilityController* controller)
        : QObject(nullptr)
        , m_controller(controller)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!m_controller) {
            return QObject::eventFilter(watched, event);
        }

        bool handled = m_controller->eventFilter(watched, event);
        if (handled) {
            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    AccessibilityController* m_controller = nullptr;
};

static QAccessibleInterface* muAccessibleFactory(const QString& classname, QObject* object)
{
    if (!g_accessibleInterfaceRegister) {
        g_accessibleInterfaceRegister = globalIoc()->resolve<IQAccessibleInterfaceRegister>("accessibility");
    }

    auto interfaceGetter = g_accessibleInterfaceRegister->interfaceGetter(classname);
    if (interfaceGetter) {
        return interfaceGetter(object);
    }

    return AccessibleStub::accessibleInterface(object);
}

} // anonymous namespace

AccessibilityController::AccessibilityController(const muse::modularity::ContextPtr& iocCtx)
    : m_iocContext(iocCtx)
{

    m_pretendFocusTimer.setInterval(80); // Value found experimentally
    m_pretendFocusTimer.setSingleShot(true);
    m_pretendFocusTimer.callOnTimeout([this]() {
        restoreFocus();
    });

    // Do not pass this as parent, AccessibilityController is not a QObject
    m_textToSpeech = new QTextToSpeech();
}


AccessibilityController::~AccessibilityController()
{
    m_pretendFocusTimer.stop();

    if (qApp && m_keyFilter) {
        qApp->removeEventFilter(m_keyFilter);
    }

    delete m_keyFilter;
    m_keyFilter = nullptr;

    delete m_textToSpeech;
    m_textToSpeech = nullptr;

    unreg(this);
}

QAccessibleInterface* AccessibilityController::accessibleInterface(QObject*)
{
    return static_cast<QAccessibleInterface*>(new AccessibleItemInterface(s_rootObject));
}

void AccessibilityController::setAccessibilityEnabled(bool enabled)
{
    m_enabled = enabled;
}

// iocContext() helper removed – IAccessible already provides it

void AccessibilityController::init()
{
    QAccessible::installFactory(muAccessibleFactory);

    reg(this);
    const Item& self = findItem(this);
    s_rootObject = self.object;

    QAccessible::installRootObjectHandler(nullptr);
    QAccessible::setRootObject(s_rootObject);

    // Install a global key filter that forwards key events into this controller.
    // We use an internal QObject subclass because AccessibilityController
    // itself does not inherit QObject.
    if (qApp && !m_keyFilter) {
        m_keyFilter = new AccessibilityKeyFilter(this);
        qApp->installEventFilter(m_keyFilter);
    }

    // Actions dispatcher integration is disabled in this version.
}


void AccessibilityController::reg(IAccessible* item)
{
    if (!m_enabled) {
        return;
    }

    if (!m_inited) {
        m_inited = true;
        init();
    }

    if (findItem(item).isValid()) {
        LOGW() << "Already registered";
        return;
    }

    MYLOG() << "item: " << item->accessibleName();

    Item it;
    it.item = item;
    it.object = new AccessibleObject(item);

    // The original version wired the controller into AccessibleObject via a
    // weak pointer. That required std::enable_shared_from_this on the
    // controller, which is not used here, so we skip it.
    // it.object->setController(weak_from_this());

    it.iface = QAccessible::queryAccessibleInterface(it.object);

    m_allItems.insert(item, it);

    if (item->accessibleParent() == this) {
        m_children.append(item);
    }

    QAccessibleEvent ev(it.object, QAccessible::ObjectCreated);
    sendEvent(&ev);
}

void AccessibilityController::unreg(IAccessible* aitem)
{
    if (!aitem) {
        return;
    }

    MYLOG() << aitem->accessibleName();

    Item item = m_allItems.take(aitem);
    if (!item.isValid()) {
        return;
    }

    if (item.item == m_lastFocused) {
        m_lastFocused = nullptr;
    }

    if (item.item == m_pretendFocus) {
        m_pretendFocus = nullptr;
    }

    if (m_children.contains(aitem)) {
        m_children.removeOne(aitem);
    }

    QAccessibleEvent ev(item.object, QAccessible::ObjectDestroyed);
    sendEvent(&ev);

    delete item.object;
}

void AccessibilityController::announce(const QString& announcement)
{
    m_announcement = announcement;

    if (!m_lastFocused || announcement.isEmpty()) {
        return;
    }

    const Item& focused = findItem(m_lastFocused);
    if (!focused.isValid()) {
        return;
    }

#if 0
    QAccessibleAnnouncementEvent event(focused.object, announcement);
    event.setPoliteness(QAccessible::AnnouncementPoliteness::Assertive);
    sendEvent(&event);
    return;
#endif

    static constexpr QAccessible::Event eventType = QAccessible::NameChanged;

    if (focused.iface && needsRevoicing(*focused.iface, eventType)) {
        triggerRevoicing(focused);
        return;
    }

    QAccessibleEvent event(focused.object, eventType);
    sendEvent(&event);
}

QString AccessibilityController::announcement() const
{
    return m_announcement;
}

QString AccessibilityController::buildSpokenDescriptionFor(const IAccessible* acc) const
{
    if (!acc) {
        return QStringLiteral("No element focused");
    }

    // Score elements
    if (acc->accessibleRole() == IAccessible::Role::ElementOnScore) {
        QString mainInfo = acc->accessibleScreenReaderInfo();
        QString extra    = acc->accessibleExtraInfo();

        QStringList parts;
        if (!mainInfo.isEmpty()) {
            parts << mainInfo;
        }
        if (!extra.isEmpty()) {
            parts << extra;
        }

        if (!parts.isEmpty()) {
            return parts.join(QStringLiteral("; "));
        }
    }

    // Generic UI element
    QStringList parts;
    const QString name = acc->accessibleName();
    const QString desc = acc->accessibleDescription();
    const QString val  = acc->accessibleValue().toString();

    if (!name.isEmpty()) {
        parts << name;
    }
    if (!desc.isEmpty()) {
        parts << desc;
    }
    if (!val.isEmpty()) {
        parts << QStringLiteral("value: ") + val;
    }

    if (parts.isEmpty()) {
        return QStringLiteral("Unknown element");
    }

    return parts.join(QStringLiteral(", "));
}

const IAccessible* AccessibilityController::accessibleRoot() const
{
    return this;
}

const IAccessible* AccessibilityController::lastFocused() const
{
    return m_lastFocused;
}

bool AccessibilityController::needToVoicePanelInfo() const
{
    return m_needToVoicePanelInfo;
}

QString AccessibilityController::currentPanelAccessibleName() const
{
    const IAccessible* focusedItemPanel = panel(m_lastFocused);
    return focusedItemPanel ? focusedItemPanel->accessibleName() : QString();
}

void AccessibilityController::setIgnoreQtAccessibilityEvents(bool ignore)
{
    if (ignore) {
        QAccessible::installUpdateHandler(updateHandlerNoop);
    } else {
        QAccessible::installUpdateHandler(nullptr);
    }
}

bool AccessibilityController::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);

    if (!m_repeatHotkeyEnabled || !event) {
        return false;
    }

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (!ke->isAutoRepeat()
            && ke->key() == m_repeatHotkey
            && (ke->modifiers() == Qt::NoModifier)) {

            repeatCurrentElementInfo();
            // We handled this key, do not let MuseScore treat F12 as a normal shortcut
            return true;
        }
    }

    return false;
}



void AccessibilityController::repeatCurrentElementInfo()
{
    if (!m_textToSpeech) {
        return;
    }

    QString textToSpeak;

    // First try our internal accessibility tree (score elements and panels that
    // registered through IAccessible and reg()).
    if (m_lastFocused) {
        textToSpeak = buildSpokenDescriptionFor(m_lastFocused);
    }

    // If that did not give us anything meaningful, fall back to Qt's
    // accessibility information for the currently focused object in the UI.
    if (textToSpeak.isEmpty()) {
        QObject* focusObj = QGuiApplication::focusObject();
        if (focusObj) {
            if (QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(focusObj)) {
                QString name  = iface->text(QAccessible::Name);
                QString descr = iface->text(QAccessible::Description);
                QString value = iface->text(QAccessible::Value);

                QStringList parts;
                if (!name.isEmpty()) {
                    parts << name;
                }
                if (!descr.isEmpty()) {
                    parts << descr;
                }
                if (!value.isEmpty()) {
                    parts << value;
                }

                if (!parts.isEmpty()) {
                    textToSpeak = parts.join(QStringLiteral(", "));
                }
            }
        }
    }

    if (textToSpeak.isEmpty()) {
        textToSpeak = QStringLiteral("No element focused");
    }

    if (m_textToSpeech->state() == QTextToSpeech::Speaking) {
        m_textToSpeech->stop();
    }

    m_textToSpeech->say(textToSpeak);
    MYLOG() << "Repeating: " << textToSpeak;
}

// IAccessible implementation for the root controller
const IAccessible* AccessibilityController::accessibleParent() const
{
    return nullptr;
}

size_t AccessibilityController::accessibleChildCount() const
{
    return static_cast<size_t>(m_children.size());
}

const IAccessible* AccessibilityController::accessibleChild(size_t i) const
{
    if (i >= static_cast<size_t>(m_children.size())) {
        return nullptr;
    }
    return m_children.at(static_cast<int>(i));
}

QWindow* AccessibilityController::accessibleWindow() const
{
    // Root controller is not itself a window
    return nullptr;
}

IAccessible::Role AccessibilityController::accessibleRole() const
{
    return IAccessible::Role::Application;
}

QString AccessibilityController::accessibleName() const
{
    // This string is what screen readers will speak for the application root
    return QStringLiteral("MuseScore");
}

QString AccessibilityController::accessibleDescription() const
{
    return QString();
}

bool AccessibilityController::accessibleState(State st) const
{
    switch (st) {
    case State::Enabled:
        return m_enabled;
    case State::Active:
        return true;
    default:
        return false;
    }
}

QRect AccessibilityController::accessibleRect() const
{
    return QRect();
}

bool AccessibilityController::accessibleIgnored() const
{
    return false;
}

QVariant AccessibilityController::accessibleValue() const
{
    return {};
}

QVariant AccessibilityController::accessibleMaximumValue() const
{
    return {};
}

QVariant AccessibilityController::accessibleMinimumValue() const
{
    return {};
}

QVariant AccessibilityController::accessibleValueStepSize() const
{
    return {};
}

void AccessibilityController::accessibleSelection(int, int* start, int* end) const
{
    if (start) {
        *start = 0;
    }
    if (end) {
        *end = 0;
    }
}

int AccessibilityController::accessibleSelectionCount() const
{
    return 0;
}

int AccessibilityController::accessibleCursorPosition() const
{
    return 0;
}

QString AccessibilityController::accessibleText(int, int) const
{
    return QString();
}

QString AccessibilityController::accessibleTextBeforeOffset(int, TextBoundaryType,
                                                            int* start, int* end) const
{
    if (start) {
        *start = 0;
    }
    if (end) {
        *end = 0;
    }
    return QString();
}

QString AccessibilityController::accessibleTextAfterOffset(int, TextBoundaryType,
                                                           int* start, int* end) const
{
    if (start) {
        *start = 0;
    }
    if (end) {
        *end = 0;
    }
    return QString();
}

QString AccessibilityController::accessibleTextAtOffset(int, TextBoundaryType,
                                                        int* start, int* end) const
{
    if (start) {
        *start = 0;
    }
    if (end) {
        *end = 0;
    }
    return QString();
}

int AccessibilityController::accessibleCharacterCount() const
{
    return 0;
}

int AccessibilityController::accessibleRowIndex() const
{
    return 0;
}

async::Channel<IAccessible::Property, Val> AccessibilityController::accessiblePropertyChanged() const
{
    // Root does not currently broadcast property changes
    return async::Channel<IAccessible::Property, Val>();
}

async::Channel<IAccessible::State, bool> AccessibilityController::accessibleStateChanged() const
{
    // Root does not currently broadcast state changes
    return async::Channel<IAccessible::State, bool>();
}

void AccessibilityController::setState(State, bool)
{
    // No state tracked on the root controller for now
}

async::Channel<QAccessibleEvent*> AccessibilityController::eventSent() const
{
    return m_eventSent;
}

// Internal helpers
void AccessibilityController::sendEvent(QAccessibleEvent* ev)
{
    if (!ev) {
        return;
    }

    m_eventSent.send(ev);
    QAccessible::updateAccessibility(ev);
}

void AccessibilityController::cancelPreviousReading()
{
    // No queued speech in this simplified version
}

void AccessibilityController::savePanelAccessibleName(const IAccessible* oldItem,
                                                      const IAccessible* newItem)
{
    Q_UNUSED(oldItem);
    Q_UNUSED(newItem);
    // The original implementation cached panel names for revoicing,
    // which is not essential for basic functionality.
}

bool AccessibilityController::needsRevoicing(const QAccessibleInterface&,
                                             QAccessible::Event) const
{
    // For now we simply use the direct NameChanged event path
    return false;
}

void AccessibilityController::triggerRevoicing(const Item& current)
{
    if (!current.object) {
        return;
    }

    QAccessibleEvent ev(current.object, QAccessible::NameChanged);
    sendEvent(&ev);
}

void AccessibilityController::restoreFocus()
{
    if (!m_pretendFocus) {
        return;
    }

    const Item& item = findItem(m_pretendFocus);
    if (!item.isUsable()) {
        m_pretendFocus = nullptr;
        return;
    }

    setExternalFocus(item);
    m_pretendFocus = nullptr;
}

void AccessibilityController::setExternalFocus(const Item& other)
{
    if (!other.isUsable()) {
        return;
    }

    m_lastFocused = other.item;
}

// Find nearest panel owning the given item
const IAccessible* AccessibilityController::panel(const IAccessible* item) const
{
    const IAccessible* current = item;
    while (current) {
        if (current->accessibleRole() == IAccessible::Role::Panel) {
            return current;
        }
        current = current->accessibleParent();
    }
    return nullptr;
}

const AccessibilityController::Item& AccessibilityController::findSiblingItem(const Item& parent,
                                                                              const Item& current) const
{
    Q_UNUSED(parent);
    Q_UNUSED(current);
    static Item invalid;
    return invalid;
}

const AccessibilityController::Item& AccessibilityController::findItem(const IAccessible* aitem) const
{
    static Item invalid;
    auto it = m_allItems.constFind(aitem);
    if (it == m_allItems.constEnd()) {
        return invalid;
    }
    return it.value();
}

QAccessibleInterface* AccessibilityController::parentIface(const IAccessible* item) const
{
    if (!item) {
        return nullptr;
    }

    const IAccessible* parent = item->accessibleParent();
    if (!parent) {
        return nullptr;
    }

    const Item& it = findItem(parent);
    return it.iface;
}

int AccessibilityController::childCount(const IAccessible* item) const
{
    return item ? static_cast<int>(item->accessibleChildCount()) : 0;
}

QAccessibleInterface* AccessibilityController::child(const IAccessible* item, int i) const
{
    if (!item) {
        return nullptr;
    }

    const IAccessible* ch = item->accessibleChild(static_cast<size_t>(i));
    if (!ch) {
        return nullptr;
    }

    const Item& it = findItem(ch);
    return it.iface;
}

int AccessibilityController::indexOfChild(const IAccessible* item,
                                          const QAccessibleInterface* iface) const
{
    if (!item || !iface) {
        return -1;
    }

    int count = static_cast<int>(item->accessibleChildCount());
    for (int i = 0; i < count; ++i) {
        const IAccessible* ch = item->accessibleChild(static_cast<size_t>(i));
        if (!ch) {
            continue;
        }
        const Item& it = findItem(ch);
        if (it.iface == iface) {
            return i;
        }
    }

    return -1;
}

QAccessibleInterface* AccessibilityController::focusedChild(const IAccessible* item) const
{
    if (!item) {
        return nullptr;
    }

    int count = static_cast<int>(item->accessibleChildCount());
    for (int i = 0; i < count; ++i) {
        const IAccessible* ch = item->accessibleChild(static_cast<size_t>(i));
        if (!ch) {
            continue;
        }
        if (ch->accessibleState(IAccessible::State::Focused)) {
            const Item& it = findItem(ch);
            return it.iface;
        }
    }

    return nullptr;
}

IAccessible* AccessibilityController::pretendFocus() const
{
    return m_pretendFocus;
}

namespace muse::accessibility {


} // namespace muse::accessibility
