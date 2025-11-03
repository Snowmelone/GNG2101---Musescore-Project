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
// Owned by this TU, not in header
AccessibleObject* s_rootObject = nullptr;
std::shared_ptr<IQAccessibleInterfaceRegister> g_accessibleInterfaceRegister;

static void updateHandlerNoop(QAccessibleEvent*) {}
}

AccessibilityController::AccessibilityController(const muse::modularity::ContextPtr& iocCtx)
    : muse::Injectable(iocCtx)
{
    m_pretendFocusTimer.setInterval(80); // Value found experimentally.
    m_pretendFocusTimer.setSingleShot(true);
    m_pretendFocusTimer.callOnTimeout([this]() {
        restoreFocus();
    });

    m_textToSpeech = new QTextToSpeech(this);
}

AccessibilityController::~AccessibilityController()
{
    m_pretendFocusTimer.stop();
    delete m_textToSpeech;
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

void AccessibilityController::init()
{
    QAccessible::installFactory(muAccessibleFactory);

    reg(this);
    const Item& self = findItem(this);
    s_rootObject = self.object;

    QAccessible::installRootObjectHandler(nullptr);
    QAccessible::setRootObject(s_rootObject);

    // Install global key filter for repeat hotkey
    if (qApp)
        qApp->installEventFilter(this);

    auto dispatcher = actionsDispatcher();
    if (!dispatcher) {
        return;
    }

    dispatcher->preDispatch().onReceive(this, [this](actions::ActionCode) {
        m_preDispatchFocus = m_lastFocused;
        m_preDispatchName = m_preDispatchFocus ? m_preDispatchFocus->accessibleName() : QString();
        m_announcement.clear();
    });

    dispatcher->postDispatch().onReceive(this, [this](actions::ActionCode actionCode) {
        if (!m_lastFocused || !m_announcement.isEmpty()) {
            return;
        }

        if (m_lastFocused != m_preDispatchFocus || m_lastFocused->accessibleName() != m_preDispatchName) {
            return;
        }

        const ui::UiAction action = actionsRegister()->action(actionCode);
        if (!action.isValid()) {
            return;
        }

        QString title = action.title.qTranslatedWithoutMnemonic();
        if (title.isEmpty()) {
            return;
        }

        announce(title);
    });
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
    it.object->setController(weak_from_this());
    it.iface = QAccessible::queryAccessibleInterface(it.object);

    m_allItems.insert(item, it);

    if (item->accessibleParent() == this) {
        m_children.append(item);
    }

    item->accessiblePropertyChanged().onReceive(this, [this, item](const IAccessible::Property& p, const Val value) {
        propertyChanged(item, p, value);
    });

    item->accessibleStateChanged().onReceive(this, [this, item](const State& state, bool arg) {
        stateChanged(item, state, arg);
    });

    QAccessibleEvent ev(it.object, QAccessible::ObjectCreated);
    sendEvent(&ev);
}

void AccessibilityController::unreg(IAccessible* aitem)
{
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

//
// NEW helper: build the full spoken string for any IAccessible.
// If it's a score element, we use the musical info (voice, pitch, duration,
// measure, staff, ties, etc.). Otherwise we fall back to generic UI name/desc/value.
//
QString AccessibilityController::buildSpokenDescriptionFor(const IAccessible* acc) const
{
    if (!acc) {
        return QStringLiteral("No element focused");
    }

    // Special handling for score elements
    if (acc->accessibleRole() == IAccessible::Role::ElementOnScore) {
        // accessibleScreenReaderInfo() should be overridden by score elements
        // to return Note::screenReaderInfo(), Measure::screenReaderInfo(), etc.
        // accessibleExtraInfo() should include articulations, ties, slurs...
        // :contentReference[oaicite:4]{index=4}
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
        // fallback if score element didn't override
    }

    // Generic UI fallback
    QStringList parts;
    const QString name = acc->accessibleName();
    const QString desc = acc->accessibleDescription();
    const QString val  = acc->accessibleValue().toString();

    if (!name.isEmpty()) parts << name;
    if (!desc.isEmpty()) parts << desc;
    if (!val.isEmpty())  parts << QStringLiteral("value: ") + val;

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
    return focusedItemPanel ? focusedItemPanel->accessibleName() : "";
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
            // Do not consume the event by default
            return false;
        }
    }

    return false;
}

void AccessibilityController::repeatCurrentElementInfo()
{
    if (!m_textToSpeech) {
        return;
    }

    const IAccessible* focused = m_lastFocused;
    QString textToSpeak = buildSpokenDescriptionFor(focused);

    if (textToSpeak.isEmpty()) {
        textToSpeak = QStringLiteral("No element focused");
    }

    if (m_textToSpeech->state() == QTextToSpeech::Speaking) {
        m_textToSpeech->stop();
    }
    m_textToSpeech->say(textToSpeak);
    MYLOG() << "Repeating: " << textToSpeak;
}

// ... rest of the file stays the same below this point ...

void AccessibilityController::propertyChanged(IAccessible* item, IAccessible::Property property, const Val& value)
{
    const Item& it = findItem(item);
    if (!it.isValid()) {
        return;
    }

    QAccessible::Event etype = QAccessible::InvalidEvent;

    switch (property) {
    case IAccessible::Property::Undefined:
        return;
    case IAccessible::Property::Parent: etype = QAccessible::ParentChanged;
        break;
    case IAccessible::Property::Name:
    case IAccessible::Property::Description: {
        if (item == m_lastFocused) {
            m_announcement.clear();
        }

#if defined(Q_OS_MAC)
        etype = QAccessible::NameChanged;
#elif defined(Q_OS_WIN)
        etype = property == IAccessible::Property::Name
                ? QAccessible::NameChanged
                : QAccessible::AcceleratorChanged;
#else
        etype = property == IAccessible::Property::Name
                ? QAccessible::NameChanged
                : QAccessible::DescriptionChanged;
#endif

        if (it.iface && needsRevoicing(*it.iface, etype)) {
            triggerRevoicing(it);
            return;
        }

        m_needToVoicePanelInfo = false;
        break;
    }
    case IAccessible::Property::Value: {
        QAccessibleValueChangeEvent ev(it.object, it.item->accessibleValue());
        sendEvent(&ev);
        return;
    }
    case IAccessible::Property::TextCursor: {
        QAccessibleTextCursorEvent ev(it.object, it.item->accessibleCursorPosition());
        sendEvent(&ev);
        return;
    }
    case IAccessible::Property::TextInsert: {
        IAccessible::TextRange range(value.toQVariant().toMap());
        QAccessibleTextInsertEvent ev(it.object, range.startPosition,
                                      it.item->accessibleText(range.startPosition, range.endPosition));
        sendEvent(&ev);
        return;
    }
    case IAccessible::Property::TextRemove: {
        IAccessible::TextRange range(value.toQVariant().toMap());
        QAccessibleTextRemoveEvent ev(it.object, range.startPosition,
                                      it.item->accessibleText(range.startPosition, range.endPosition));
        sendEvent(&ev);
        return;
    }
    }

    QAccessibleEvent ev(it.object, etype);
    sendEvent(&ev);
}

// stateChanged(), sendEvent(), cancelPreviousReading(), savePanelAccessibleName(),
// needsRevoicing(), triggerRevoicing(), restoreFocus(), setExternalFocus(), panel(),
// findSiblingItem(), pretendFocus(), eventSent(), findItem(), parentIface(),
// childCount(), child(), indexOfChild(), focusedChild(), and all the rest of
// the IAccessible impls stay exactly as in your version, unchanged.
