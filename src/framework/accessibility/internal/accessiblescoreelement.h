#pragma once

#include "accessibility/iaccessible.h"

#include "modularity/ioc.h"
#include "global/async/channel.h"
#include "global/types/val.h"

#include <QRect>
#include <QVariant>
#include <QWindow>

namespace mu {
namespace engraving {
class EngravingItem;
}
}

namespace muse::accessibility {

class AccessibleScoreElement : public IAccessible
{
public:
    // Two argument constructor used by notationselection
    AccessibleScoreElement(mu::engraving::EngravingItem* elem,
                           QWindow* win)
        : m_elem(elem)
        , m_window(win)
    {
    }

    // Optional three argument constructor used by accessibility controller
    AccessibleScoreElement(const muse::modularity::ContextPtr& ctx,
                           mu::engraving::EngravingItem* elem,
                           QWindow* win)
        : m_ctx(ctx)
        , m_elem(elem)
        , m_window(win)
    {
    }

    void updateFromSelection(mu::engraving::EngravingItem* elem,
                             QWindow* win)
    {
        m_elem   = elem;
        m_window = win;
    }

    // IAccessible implementation
    const IAccessible* accessibleParent() const override { return nullptr; }
    size_t accessibleChildCount() const override { return 0; }
    const IAccessible* accessibleChild(size_t) const override { return nullptr; }

    QWindow* accessibleWindow() const override { return m_window; }

    muse::modularity::ContextPtr iocContext() const override { return m_ctx; }

    Role accessibleRole() const override { return Role::ElementOnScore; }

    QString accessibleName() const override { return buildScreenReaderMain(); }
    QString accessibleDescription() const override { return buildScreenReaderExtra(); }

    bool accessibleState(State st) const override
    {
        switch (st) {
        case State::Enabled:  return m_elem != nullptr;
        case State::Active:   return true;
        case State::Focused:  return m_focused;
        case State::Selected: return m_selected;
        default:              return false;
        }
    }

    QRect accessibleRect() const override { return elementScreenRect(); }
    bool accessibleIgnored() const override { return m_elem == nullptr; }

    QVariant accessibleValue() const override { return {}; }
    QVariant accessibleMaximumValue() const override { return {}; }
    QVariant accessibleMinimumValue() const override { return {}; }
    QVariant accessibleValueStepSize() const override { return {}; }

    void accessibleSelection(int, int*, int*) const override {}
    int accessibleSelectionCount() const override { return 0; }

    int accessibleCursorPosition() const override { return 0; }

    QString accessibleText(int, int) const override { return {}; }
    QString accessibleTextBeforeOffset(int, TextBoundaryType, int*, int*) const override { return {}; }
    QString accessibleTextAfterOffset(int, TextBoundaryType, int*, int*) const override { return {}; }
    QString accessibleTextAtOffset(int, TextBoundaryType, int*, int*) const override { return {}; }
    int accessibleCharacterCount() const override { return 0; }

    int accessibleRowIndex() const override { return 0; }

    async::Channel<IAccessible::Property, Val> accessiblePropertyChanged() const override
    {
        return m_propertyChanged;
    }

    async::Channel<IAccessible::State, bool> accessibleStateChanged() const override
    {
        return m_stateChanged;
    }

    void setState(State st, bool arg) override
    {
        bool changed = false;

        switch (st) {
        case State::Focused:
            if (m_focused != arg) {
                m_focused = arg;
                changed   = true;
            }
            break;

        case State::Selected:
            if (m_selected != arg) {
                m_selected = arg;
                changed    = true;
            }
            break;

        default:
            break;
        }

        if (changed) {
            m_stateChanged.send(st, arg);
        }
    }

    QString accessibleScreenReaderInfo() const override { return buildScreenReaderMain(); }
    QString accessibleExtraInfo() const override { return buildScreenReaderExtra(); }

private:
    QString buildScreenReaderMain() const
    {
        if (!m_elem) {
            return QStringLiteral("Score element");
        }
        return QStringLiteral("Score element");
    }

    QString buildScreenReaderExtra() const
    {
        return {};
    }

    QRect elementScreenRect() const
    {
        Q_UNUSED(m_elem);
        Q_UNUSED(m_window);
        return {};
    }

private:
    muse::modularity::ContextPtr m_ctx;
    mu::engraving::EngravingItem* m_elem = nullptr;
    QWindow* m_window = nullptr;

    bool m_focused  = false;
    bool m_selected = false;

    mutable async::Channel<IAccessible::Property, Val> m_propertyChanged;
    mutable async::Channel<IAccessible::State, bool>   m_stateChanged;
};

} // namespace muse::accessibility
