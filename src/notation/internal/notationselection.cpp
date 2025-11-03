/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
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
#include "notationselection.h"

#include <QMimeData>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/measure.h"

#include "notationselectionrange.h"
#include "notationerrors.h"

#include "log.h"

// NEW: accessibility plumbing
#include "accessibility/api/iaccessibilitycontroller.h"
#include "accessibility/internal/accessible_score_element.h"
#include "modularity/ioc.h"

using namespace muse;
using namespace mu::notation;

NotationSelection::NotationSelection(IGetScore* getScore)
    : m_getScore(getScore)
{
    m_range = std::make_shared<NotationSelectionRange>(getScore);
}

bool NotationSelection::isNone() const
{
    return score()->selection().isNone();
}

bool NotationSelection::isRange() const
{
    return score()->selection().isRange();
}

SelectionState NotationSelection::state() const
{
    return score()->selection().state();
}

Ret NotationSelection::canCopy() const
{
    if (isNone()) {
        return make_ret(Err::EmptySelection);
    }

    if (!score()->selection().canCopy()) {
        return make_ret(Err::SelectCompleteTupletOrTremolo);
    }

    return muse::make_ok();
}

muse::ByteArray NotationSelection::mimeData() const
{
    return score()->selection().mimeData();
}

QMimeData* NotationSelection::qMimeData() const
{
    QString mimeType = score()->selection().mimeType();
    if (mimeType.isEmpty()) {
        return nullptr;
    }

    QMimeData* mimeData = new QMimeData();
    mimeData->setData(mimeType, score()->selection().mimeData().toQByteArray());

    return mimeData;
}

EngravingItem* NotationSelection::element() const
{
    return score()->selection().element();
}

const std::vector<EngravingItem*>& NotationSelection::elements() const
{
    return score()->selection().elements();
}

std::vector<Note*> NotationSelection::notes(NoteFilter filter) const
{
    switch (filter) {
    case NoteFilter::All: return score()->selection().noteList();
    case NoteFilter::WithTie: return score()->cmdTieNoteList(score()->selection(), false);
    case NoteFilter::WithSlur: {
        NOT_IMPLEMENTED;
        return {};
    }
    }

    return {};
}

muse::RectF NotationSelection::canvasBoundingRect() const
{
    if (isNone()) {
        return RectF();
    }

    if (const mu::engraving::EngravingItem* element = score()->selection().element()) {
        return element->canvasBoundingRect();
    }

    RectF result;

    for (const RectF& rect : range()->boundingArea()) {
        result = result.united(rect);
    }

    return result;
}

INotationSelectionRangePtr NotationSelection::range() const
{
    return m_range;
}

mu::engraving::Score* NotationSelection::score() const
{
    return m_getScore->score();
}

void NotationSelection::onElementHit(EngravingItem* el)
{
    m_lastElementHit = el;

    //
    // Accessibility bridge:
    // Tell the AccessibilityController that the score focus moved to `el`
    // so that screen readers (and our repeatCurrentElementInfo hotkey)
    // will read musical info (pitch, duration, bar, staff, etc.)
    //

    // 1. Get the global accessibility controller from IoC
    auto accCtrl = muse::modularity::globalIoc()
        ->resolve<muse::accessibility::IAccessibilityController>("accessibility");

    if (!accCtrl) {
        return;
    }

    // 2. Figure out which QWindow this selection belongs to.
    // We don't have direct access to the notation view's QWindow here,
    // so we pass nullptr for now. Speech will still work without a window.
    QWindow* win = nullptr;

    // 3. Keep a single persistent AccessibleScoreElement and just retarget it
    static muse::accessibility::AccessibleScoreElement* s_accScoreElem = nullptr;

    if (!s_accScoreElem) {
        // First time: create and register with the accessibility controller.
        //
        // We pass:
        //   - m_getScore->iocContext() as the ContextPtr (so AccessibleScoreElement
        //     can participate in the same dependency graph as notation code),
        //   - the EngravingItem* we just focused,
        //   - the QWindow* (currently nullptr).
        //
        // NOTE: if m_getScore doesn't expose iocContext(), you may need to adjust
        // this argument to whatever object in this layer has a ContextPtr.
        s_accScoreElem = new muse::accessibility::AccessibleScoreElement(
            /* ctx   */ m_getScore->iocContext(),
            /* elem  */ el,
            /* win   */ win
        );

        accCtrl->reg(s_accScoreElem);
    } else {
        // Subsequent focus changes: update in place
        s_accScoreElem->updateFromSelection(el, win);
    }

    // 4. Mark it as focused/selected so AccessibilityController will treat it
    //    as lastFocused() and emit proper focus events for assistive tech
    s_accScoreElem->setState(muse::accessibility::IAccessible::State::Focused, true);
    s_accScoreElem->setState(muse::accessibility::IAccessible::State::Selected, true);

    // 5. Clear any pending announcement override, so repeatCurrentElementInfo()
    //    and focus events speak the live element info instead of stale text
    accCtrl->announce(QString());
}

mu::engraving::MeasureBase* NotationSelection::startMeasureBase() const
{
    return score()->selection().startMeasureBase();
}

mu::engraving::MeasureBase* NotationSelection::endMeasureBase() const
{
    return score()->selection().endMeasureBase();
}

std::vector<mu::engraving::System*> NotationSelection::selectedSystems() const
{
    return score()->selection().selectedSystems();
}

EngravingItem* NotationSelection::lastElementHit() const
{
    return m_lastElementHit;
}

bool NotationSelection::elementsSelected(const mu::engraving::ElementTypeSet& types) const
{
    return score()->selection().elementsSelected(types);
}
