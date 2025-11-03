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

#include "accessibleobject.h"
#include "accessibleiteminterface.h"
#include "accessiblestub.h"
#include "iqaccessibleinterfaceregister.h"

#include "log.h"

namespace muse {
namespace accessibility {

class AccessibilityController : public muse::Injectable, public IAccessible
{
public:
    explicit AccessibilityController(const muse::modularity::ContextPtr& iocCtx);
    ~AccessibilityController() override;

    // Configuration
    void setAccessibilityEnabled(bool enabled);
    // Backward-compatible shim
    void setAccesibilityEnabled(bool enabled) { setAccessibilityEnabled(enabled); }
    void setIgnoreQtAccessibilityEvents(bool ignore);

    // Announcements / TTS
    void announce(const QString& announcement);
    QString announcement() const;
    void repeatCurrentElementInfo();

    // IAccessible interface (root)
    const IAccessible* accessibleRoot() const override;
    const IAccessible* lastFocused() const;
    bool needToVoicePanelInfo() const;
    QString currentPanelAccessibleName() const;

    // IAccessible tree navigation
    IAccessible* accessibleParent() const override;
    size_t accessibleChildCount() const override;
    IAccessible* accessibleChild(size_t i) const override;
    QWindow* accessibleWindow() const override;
    IAccessible::Role accessibleRole() const override;
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
    async::Channel<IAccessible::Property, Val> accessiblePropertyChanged() const override;
    async::Channel<IAccessible::State, bool> accessibleStateChanged() const override;
    void setState(State, bool) override;

    // Extended speech helpers (public so other helpers like AccessibilityRepeater can reuse)
    QString buildSpokenDescriptionFor(const IAccessible* acc) const;

    // Exposed signals/channels
    async::Channel<QAccessibleEvent*> eventSent() const;

    // Factory hook for Qt
    QAccessibleInterface* accessibleInterface(QObject*);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
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

    // Focus tracking
    IAccessible* m_lastFocused = nullptr;
    IAccessible* m_preDispatchFocus = nullptr;
    QString m_preDispatchName;
    QString m_announcement;

    // Items
    QList<IAccessible*> m_children;
    QHash<const IAccessible*, Item> m_allItems;
    IAccessible* m_pretendFocus = nullptr;

    // Hotkey config
    bool m_repeatHotkeyEnabled = true;
    int  m_repeatHotkey = Qt::Key_R;

    // Timers / TTS
    QTimer m_pretendFocusTimer;
    QTextToSpeech* m_textToSpeech = nullptr;

    // Channel
    async::Channel<QAccessibleEvent*> m_eventSent;
};

}} // namespace muse::accessibility
