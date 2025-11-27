/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "accessibleiteminterface.h"

#include <QWindow>

#include "global/translation.h"
#include "accessibilitycontroller.h"
#include "log.h"

//#define MUE_ENABLE_ACCESSIBILITY_TRACE

#undef MYLOG
#ifdef MUE_ENABLE_ACCESSIBILITY_TRACE
    #define MYLOG() LOGI()
#else
    #define MYLOG() LOGN()
#endif

using namespace muse;
using namespace muse::accessibility;

AccessibleItemInterface::AccessibleItemInterface(AccessibleObject* object)
    : muse::Injectable(object->item()->iocContext())
{
    m_object = object;
}

bool AccessibleItemInterface::isValid() const
{
    return m_object != nullptr;
}

QObject* AccessibleItemInterface::object() const
{
    return m_object;
}

QWindow* AccessibleItemInterface::window() const
{
    QWindow* window = m_object->item()->accessibleWindow();
    if (window) {
        return window;
    }

    return interactiveProvider()->topWindow();
}

QRect AccessibleItemInterface::rect() const
{
    return m_object->item()->accessibleRect();
}

// Tree navigation helpers
QAccessibleInterface* AccessibleItemInterface::parent() const
{
    // In the original MuseScore code this delegated to AccessibilityController
    // tree helpers. Those helpers are private in this version, so we
    // conservatively return nullptr here.
    return nullptr;
}

int AccessibleItemInterface::childCount() const
{
    // Without controller tree helpers, report no children from here.
    return 0;
}

QAccessibleInterface* AccessibleItemInterface::child(int) const
{
    return nullptr;
}

int AccessibleItemInterface::indexOfChild(const QAccessibleInterface*) const
{
    return -1;
}

QAccessibleInterface* AccessibleItemInterface::childAt(int, int) const
{
    NOT_IMPLEMENTED;
    return nullptr;
}

QAccessibleInterface* AccessibleItemInterface::focusChild() const
{
    // Without tree helpers we cannot reliably compute focused child
    return nullptr;
}

// State / role / text
QAccessible::State AccessibleItemInterface::state() const
{
    IAccessible* item = m_object->item();
    QAccessible::State state;
    state.invalid = false;
    state.disabled = !item->accessibleState(IAccessible::State::Enabled);
    state.invisible = state.disabled;

    if (state.disabled) {
        return state;
    }

    IAccessible::Role r = item->accessibleRole();
    switch (r) {
    case IAccessible::Role::NoRole: break;
    case IAccessible::Role::Application: {
        state.active = true;
    } break;
    case IAccessible::Role::Dialog: {
        state.active = item->accessibleState(IAccessible::State::Active);
    } break;
    case IAccessible::Role::Panel: {
        state.active = item->accessibleState(IAccessible::State::Active);
    } break;
    case IAccessible::Role::Button: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::RadioButton: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);

        state.checkable = true;
        state.checked = item->accessibleState(IAccessible::State::Checked);
    } break;
    case IAccessible::Role::EditableText: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::StaticText: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::SilentRole: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::List: {
        state.active = item->accessibleState(IAccessible::State::Active);
    } break;
    case IAccessible::Role::ListItem: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);

        state.selectable = true;
        state.selected = item->accessibleState(IAccessible::State::Selected);
    } break;
    case IAccessible::Role::Information: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::ElementOnScore: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::CheckBox: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);

        state.checkable = true;
        state.checked = item->accessibleState(IAccessible::State::Checked);
    } break;
    case IAccessible::Role::ComboBox: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::MenuItem: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::SpinBox: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::Range: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    case IAccessible::Role::Group: {
        state.focusable = true;
        state.focused = item->accessibleState(IAccessible::State::Focused);
    } break;
    default: {
        LOGW() << "not handled role: " << static_cast<int>(r);
    } break;
    }

    return state;
}

QAccessible::Role AccessibleItemInterface::role() const
{
    IAccessible::Role r = m_object->item()->accessibleRole();
    switch (r) {
    case IAccessible::Role::NoRole: return QAccessible::NoRole;
    case IAccessible::Role::Application: return QAccessible::Application;
    case IAccessible::Role::Dialog: return QAccessible::Dialog;
    case IAccessible::Role::Panel:
#if defined(Q_OS_WIN)
        return QAccessible::StaticText;
#else
        return QAccessible::Pane;
#endif
    case IAccessible::Role::StaticText: return QAccessible::StaticText;
    case IAccessible::Role::SilentRole: {
#if defined(Q_OS_MACOS)
        return QAccessible::StaticText;
#else
        return QAccessible::ListItem;
#endif
    }
    case IAccessible::Role::EditableText: return QAccessible::EditableText;
    case IAccessible::Role::Button: return QAccessible::Button;
    case IAccessible::Role::CheckBox: return QAccessible::CheckBox;
    case IAccessible::Role::RadioButton: return QAccessible::RadioButton;
    case IAccessible::Role::ComboBox: return QAccessible::ComboBox;
    case IAccessible::Role::List: return QAccessible::List;
    case IAccessible::Role::ListItem: return QAccessible::ListItem;
    case IAccessible::Role::MenuItem: return QAccessible::MenuItem;
    case IAccessible::Role::SpinBox: return QAccessible::SpinBox;
    case IAccessible::Role::Range: return QAccessible::Slider;
    case IAccessible::Role::Group:
    case IAccessible::Role::Information:
    case IAccessible::Role::ElementOnScore: {
#ifdef Q_OS_WIN
        return QAccessible::StaticText;
#else
        return QAccessible::UserRole;
#endif
    }
    }

    LOGE() << "not handled role: " << static_cast<int>(r);
    return QAccessible::NoRole;
}

QString AccessibleItemInterface::text(QAccessible::Text textType) const
{
    // Must match AccessibilityController::propertyChanged handling
    switch (textType) {
    case QAccessible::Name: {
        QString ann = announcement();
        if (!ann.isEmpty()) {
            return ann;
        }

        QString name = m_object->item()->accessibleName();
#if defined(Q_OS_MACOS)
        QString desc = description();
        if (!desc.isEmpty()) {
            name += ", " + desc;
        }
#endif
        if (auto controller = m_object->controller().lock()) {
            if (m_object->item() == controller->lastFocused() && controller->needToVoicePanelInfo()) {
                QString panelName = controller->currentPanelAccessibleName();
                if (!panelName.isEmpty()) {
                    name.prepend(muse::qtrc("accessibility", "%1 panel").arg(panelName) + ", ");
                }
            }
        }
        return name;
    }
#if !defined(Q_OS_MACOS)
#if defined(Q_OS_WINDOWS)
    case QAccessible::Accelerator:
#else
    case QAccessible::Description:
#endif
    {
        if (!announcement().isEmpty()) {
            break;
        }
        return description();
    }
#endif
    default:
        break;
    }

    return QString();
}

void AccessibleItemInterface::setText(QAccessible::Text, const QString&)
{
    NOT_IMPLEMENTED;
}

// Value and text interfaces
QVariant AccessibleItemInterface::currentValue() const
{
    return m_object->item()->accessibleValue();
}

void AccessibleItemInterface::setCurrentValue(const QVariant&)
{
    NOT_IMPLEMENTED;
}

QVariant AccessibleItemInterface::maximumValue() const
{
    return m_object->item()->accessibleMaximumValue();
}

QVariant AccessibleItemInterface::minimumValue() const
{
    return m_object->item()->accessibleMinimumValue();
}

QVariant AccessibleItemInterface::minimumStepSize() const
{
    return m_object->item()->accessibleValueStepSize();
}

void AccessibleItemInterface::selection(int selectionIndex,
                                        int* startOffset,
                                        int* endOffset) const
{
    m_object->item()->accessibleSelection(selectionIndex, startOffset, endOffset);
}

int AccessibleItemInterface::selectionCount() const
{
    return m_object->item()->accessibleSelectionCount();
}

void AccessibleItemInterface::addSelection(int, int)
{
    NOT_IMPLEMENTED;
}

void AccessibleItemInterface::removeSelection(int)
{
    NOT_IMPLEMENTED;
}

void AccessibleItemInterface::setSelection(int, int, int)
{
    NOT_IMPLEMENTED;
}

int AccessibleItemInterface::cursorPosition() const
{
    return m_object->item()->accessibleCursorPosition();
}

void AccessibleItemInterface::setCursorPosition(int)
{
    NOT_IMPLEMENTED;
}

QString AccessibleItemInterface::text(int startOffset, int endOffset) const
{
    return m_object->item()->accessibleText(startOffset, endOffset);
}

QString AccessibleItemInterface::textBeforeOffset(int offset,
                                                  QAccessible::TextBoundaryType boundaryType,
                                                  int* startOffset,
                                                  int* endOffset) const
{
    return m_object->item()->accessibleTextBeforeOffset(
        offset, muBoundaryType(boundaryType), startOffset, endOffset);
}

QString AccessibleItemInterface::textAfterOffset(int offset,
                                                 QAccessible::TextBoundaryType boundaryType,
                                                 int* startOffset,
                                                 int* endOffset) const
{
    return m_object->item()->accessibleTextAfterOffset(
        offset, muBoundaryType(boundaryType), startOffset, endOffset);
}

QString AccessibleItemInterface::textAtOffset(int offset,
                                              QAccessible::TextBoundaryType boundaryType,
                                              int* startOffset,
                                              int* endOffset) const
{
    return m_object->item()->accessibleTextAtOffset(
        offset, muBoundaryType(boundaryType), startOffset, endOffset);
}

int AccessibleItemInterface::characterCount() const
{
    return m_object->item()->accessibleCharacterCount();
}

QRect AccessibleItemInterface::characterRect(int) const
{
    NOT_IMPLEMENTED;
    return QRect();
}

int AccessibleItemInterface::offsetAtPoint(const QPoint&) const
{
    NOT_IMPLEMENTED;
    return -1;
}

void AccessibleItemInterface::scrollToSubstring(int, int)
{
    NOT_IMPLEMENTED;
}

QString AccessibleItemInterface::attributes(int,
                                            int* startOffset,
                                            int* endOffset) const
{
    NOT_IMPLEMENTED;
    *startOffset = -1;
    *endOffset = -1;
    return QString();
}

// Table and selection helpers
bool AccessibleItemInterface::isSelected() const
{
    return m_object->item()->accessibleState(IAccessible::State::Selected);
}

QList<QAccessibleInterface*> AccessibleItemInterface::columnHeaderCells() const
{
    NOT_IMPLEMENTED;
    return {};
}

QList<QAccessibleInterface*> AccessibleItemInterface::rowHeaderCells() const
{
    NOT_IMPLEMENTED;
    return {};
}

int AccessibleItemInterface::columnIndex() const
{
    NOT_IMPLEMENTED;
    return 0;
}

int AccessibleItemInterface::rowIndex() const
{
    return m_object->item()->accessibleRowIndex();
}

int AccessibleItemInterface::columnExtent() const
{
    NOT_IMPLEMENTED;
    return 1;
}

int AccessibleItemInterface::rowExtent() const
{
    NOT_IMPLEMENTED;
    return 1;
}

QAccessibleInterface* AccessibleItemInterface::table() const
{
    return parent();
}

// Interface casting
void* AccessibleItemInterface::interface_cast(QAccessible::InterfaceType type)
{
    QAccessible::Role itemRole = role();
    if (type == QAccessible::InterfaceType::ValueInterface && itemRole == QAccessible::Slider) {
        return static_cast<QAccessibleValueInterface*>(this);
    } else if (type == QAccessible::InterfaceType::TextInterface) {
        return static_cast<QAccessibleTextInterface*>(this);
    }

    bool isListType = type == QAccessible::InterfaceType::TableCellInterface;
#ifdef Q_OS_WIN
    isListType |= type == QAccessible::InterfaceType::ActionInterface;
#endif

    if (isListType && itemRole == QAccessible::ListItem) {
        return static_cast<QAccessibleTableCellInterface*>(this);
    }

    return nullptr;
}

IAccessible::TextBoundaryType AccessibleItemInterface::muBoundaryType(
        QAccessible::TextBoundaryType qtBoundaryType) const
{
    switch (qtBoundaryType) {
    case QAccessible::TextBoundaryType::CharBoundary: return IAccessible::CharBoundary;
    case QAccessible::TextBoundaryType::WordBoundary: return IAccessible::WordBoundary;
    case QAccessible::TextBoundaryType::SentenceBoundary: return IAccessible::SentenceBoundary;
    case QAccessible::TextBoundaryType::ParagraphBoundary: return IAccessible::ParagraphBoundary;
    case QAccessible::TextBoundaryType::LineBoundary: return IAccessible::LineBoundary;
    case QAccessible::TextBoundaryType::NoBoundary: return IAccessible::NoBoundary;
    }

    return IAccessible::NoBoundary;
}

// Announcement / description
QString AccessibleItemInterface::announcement() const
{
    if (auto controller = m_object->controller().lock()) {
        if (m_object->item() == controller->lastFocused()) {
            return controller->announcement();
        }
    }

    return QString();
}

QString AccessibleItemInterface::description() const
{
    IAccessible* item = m_object->item();
    QString desc = item->accessibleDescription();

    if (desc.isEmpty() || item->accessibleName().contains(desc, Qt::CaseInsensitive)) {
        return QString();
    }

    return desc;
}
