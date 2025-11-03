#pragma once

#include "accessibility/api/iaccessible.h"
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
        // We could emit propertyChanged here if we want live updates,
        // but first pass doesn't strictly need it.
    }

    //
    // IAccessible implementation
    //

    const IAccessible* accessibleParent() const override
    {
        // Controller treats top-level score focus objects like direct children.
        return nullptr;
    }

    size_t accessibleChildCount() const override { return 0; }
    const IAccessible* accessibleChild(size_t) const override { return nullptr; }

    QWindow* accessibleWindow() const override
    {
        return m_window;
    }

    muse::modularity::ContextPtr iocContext() const override
    {
        return m_ctx;
    }

    Role accessibleRole() const override
    {
        return Role::ElementOnScore;
    }

    QString accessibleName() const override
    {
        // Short fallback string if the screen reader only asks for "Name".
        return buildScreenReaderMain();
    }

    QString accessibleDescription() const override
    {
        // Secondary context. Screen readers on Linux/Windows often read this too.
        return buildScreenReaderExtra();
    }

    bool accessibleState(State st) const override
    {
        switch (st) {
        case State::Enabled:  return m_elem != nullptr;
        case State::Active:   return true;
        case State::Focused:  return m_focused;
        case State::Selected: return m_selected;
        case State::Checked:  return false;
        default:              return false;
        }
    }

    QRect accessibleRect() const override
    {
        // This rect needs to be in *window coordinates*. We currently only
        // get the element's canvas rect in score coordinates. We return an
        // empty QRect for now and we’ll improve once we see how view mapping works.
        return elementScreenRect();
    }

    bool accessibleIgnored() const override
    {
        return (m_elem == nullptr);
    }

    // Value interface (N/A for notes)
    QVariant accessibleValue() const override { return QVariant(); }
    QVariant accessibleMaximumValue() const override { return QVariant(); }
    QVariant accessibleMinimumValue() const override { return QVariant(); }
    QVariant accessibleValueStepSize() const override { return QVariant(); }

    // Text interface (not used for score elements)
    void accessibleSelection(int, int*, int*) const override {}
    int accessibleSelectionCount() const override { return 0; }

    int accessibleCursorPosition() const override { return 0; }

    QString accessibleText(int, int) const override { return QString(); }
    QString accessibleTextBeforeOffset(int, TextBoundaryType, int*, int*) const override { return QString(); }
    QString accessibleTextAfterOffset(int, TextBoundaryType, int*, int*) const override { return QString(); }
    QString accessibleTextAtOffset(int, TextBoundaryType, int*, int*) const override { return QString(); }
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
                changed = true;
            }
            break;
        case State::Selected:
            if (m_selected != arg) {
                m_selected = arg;
                changed = true;
            }
            break;
        default:
            break;
        }

        if (changed) {
            m_stateChanged.send(st, arg);
        }
    }

    //
    // These are the new virtuals you added to IAccessible.
    // AccessibilityController::buildSpokenDescriptionFor() calls these.
    //
    QString accessibleScreenReaderInfo() const override
    {
        // Long/main spoken description:
        // "Voice 1, C sharp 4 quarter note, measure 12, beat 3, right hand staff"
        return buildScreenReaderMain();
    }

    QString accessibleExtraInfo() const override
    {
        // Attachments:
        // "Staccato. Accent. Start of tie."
        return buildScreenReaderExtra();
    }

private:
    //
    // Helpers – these are currently stubs and you will later fill them
    // by calling real methods on EngravingItem / Note / etc.
    //

    QString buildScreenReaderMain() const
    {
        if (!m_elem) {
            return QString();
        }

        // TODO: call the real MuseScore function that already exists.
        // e.g. m_elem->screenReaderInfo()
        //
        // For first working build we can at least name the class type.
        // (This will confirm we're actually focusing score content.)
        return QStringLiteral("Score element");
    }

    QString buildScreenReaderExtra() const
    {
        if (!m_elem) {
            return QString();
        }

        // TODO: call something like m_elem->accessibleExtraInfo() if it exists.
        return QString();
    }

    QRect elementScreenRect() const
    {
        // TODO: eventually map m_elem->canvasBoundingRect() to window coordinates.
        // For now just return empty so we don't crash.
        return QRect();
    }

private:
    muse::modularity::ContextPtr m_ctx;
    mu::engraving::EngravingItem* m_elem = nullptr;
    QWindow* m_window = nullptr;

    bool m_focused = false;
    bool m_selected = false;

    mutable async::Channel<IAccessible::Property, Val> m_propertyChanged;
    mutable async::Channel<IAccessible::State, bool> m_stateChanged;
};

} // namespace muse::accessibility
