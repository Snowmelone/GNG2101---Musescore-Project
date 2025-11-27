#pragma once
/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 */

#include <QObject>
#include <QRect>
#include <QWindow>
#include <QTimer>
#include <QString>
#include <QEvent>
#include <QVariant>
#include <QAccessible>

class QTextToSpeech;
class QAccessibleEvent;
class QAccessibleInterface;

#include "modularity/ioc.h"

// Go one level up from /internal to find these
#include "../iaccessible.h"
#include "../iaccessibilitycontroller.h"

#include "accessibleobject.h"
#include "accessibleiteminterface.h"
#include "accessiblestub.h"

#include "log.h"

namespace muse {
namespace accessibility {

class AccessibilityController : public IAccessibilityController, public IAccessible
{
public:
    // Bring IAccessible nested types into scope so we can use short names
    using State            = IAccessible::State;
    using TextBoundaryType = IAccessible::TextBoundaryType;
    using Role             = IAccessible::Role;
    using Property         = IAccessible::Property;
    using Val              = muse::Val;

    explicit AccessibilityController(const muse::modularity::ContextPtr& iocCtx);
    ~AccessibilityController() override;

    // Satisfy IAccessible pure virtual
    muse::modularity::ContextPtr iocContext() const override { return m_iocContext; }

    // Configuration
    void setAccessibilityEnabled(bool enabled);
    // Backward compatible shim
    void setAccesibilityEnabled(bool enabled) { setAccessibilityEnabled(enabled); }
    void setIgnoreQtAccessibilityEvents(bool ignore);

    // Announcements / TTS
    void announce(const QString& announcement);
    QString announcement() const;
    void repeatCurrentElementInfo();

    // Controller extras
    const IAccessible* accessibleRoot() const;
    const IAccessible* lastFocused() const;
    bool needToVoicePanelInfo() const;
    QString currentPanelAccessibleName() const;

    // IAccessible tree navigation
    const IAccessible* accessibleParent() const override;
    size_t accessibleChildCount() const override;
    const IAccessible* accessibleChild(size_t i) const override;
    QWindow* accessibleWindow() const override;
    Role accessibleRole() const override;
    QString accessibleName() const override;
    QString accessibleDescription() const override;
    bool accessibleState(State st) const override;
    QRect accessibleRect() const override;
    bool accessibleIgnored() const override;
    QVariant accessibleValue() const override;
    QVariant accessibleMaximumValue() const override;
    QVariant accessibleMinimumValue() const override;
    QVariant accessibleValueStepSize() const override;
    void accessibleSelection(int, int*, int*) const override;
    int accessibleSelectionCount() const override;
    int accessibleCursorPosition() const override;
    QString accessibleText(int, int) const override;
    QString accessibleTextBeforeOffset(int, TextBoundaryType, int*, int*) const override;
    QString accessibleTextAfterOffset(int, TextBoundaryType, int*, int*) const override;
    QString accessibleTextAtOffset(int, TextBoundaryType, int*, int*) const override;
    int accessibleCharacterCount() const override;
    int accessibleRowIndex() const override;
    async::Channel<Property, Val> accessiblePropertyChanged() const override;
    async::Channel<State, bool> accessibleStateChanged() const override;
    void setState(State, bool) override;

    // Extended speech helper (public so helpers like AccessibilityRepeater can reuse)
    QString buildSpokenDescriptionFor(const IAccessible* acc) const;

    // Exposed signals or channels
    async::Channel<QAccessibleEvent*> eventSent() const;

    // Factory hook for Qt
    static QAccessibleInterface* accessibleInterface(QObject*);

public:
    // This is used from an internal QObject based event filter
    bool eventFilter(QObject* watched, QEvent* event);

public:
    // Internal item wrapper used by controller
    struct Item {
        IAccessible* item = nullptr;
        AccessibleObject* object = nullptr;
        QAccessibleInterface* iface = nullptr;
        bool isValid() const { return item && object; }
        bool isUsable() const { return isValid() && !item->accessibleIgnored(); }
    };

    // Lifecycle
    void init();
    void reg(IAccessible* item);
    void unreg(IAccessible* item);

    // Event helpers
    void sendEvent(QAccessibleEvent* ev);
    void cancelPreviousReading();
    void savePanelAccessibleName(const IAccessible* oldItem, const IAccessible* newItem);
    bool needsRevoicing(const QAccessibleInterface& iface, QAccessible::Event eventType) const;
    void triggerRevoicing(const Item& current);
    void restoreFocus();
    void setExternalFocus(const Item& other);

    // Tree utilities
    const IAccessible* panel(const IAccessible* item) const;
    const Item& findSiblingItem(const Item& parent, const Item& current) const;
    const Item& findItem(const IAccessible* aitem) const;
    QAccessibleInterface* parentIface(const IAccessible* item) const;
    int childCount(const IAccessible* item) const;
    QAccessibleInterface* child(const IAccessible* item, int i) const;
    int indexOfChild(const IAccessible* item, const QAccessibleInterface* iface) const;
    QAccessibleInterface* focusedChild(const IAccessible* item) const;
    IAccessible* pretendFocus() const;

private:
    // State
    bool m_enabled = true;
    bool m_inited = false;
    bool m_needToVoicePanelInfo = false;
    bool m_ignorePanelChangingVoice = false;

    // IOC context
    muse::modularity::ContextPtr m_iocContext;

    // Focus tracking
    IAccessible* m_lastFocused = nullptr;
    IAccessible* m_preDispatchFocus = nullptr;
    QString m_preDispatchName;
    QString m_announcement;

    // Items
    QList<IAccessible*> m_children;
    QHash<const IAccessible*, Item> m_allItems;
    IAccessible* m_pretendFocus = nullptr;

    // Repeat spoken description hotkey
    bool m_repeatHotkeyEnabled = true;
    int  m_repeatHotkey = Qt::Key_F12;


    // Timers or TTS
    QTimer m_pretendFocusTimer;
    QTextToSpeech* m_textToSpeech = nullptr;

    // Global key filter object (QObject based)
    QObject* m_keyFilter = nullptr;

    // Channel
    async::Channel<QAccessibleEvent*> m_eventSent;
};

}} // namespace muse::accessibility
