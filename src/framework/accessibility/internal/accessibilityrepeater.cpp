#include "accessibilityrepeater.h"
#include "log.h"

using namespace muse::accessibility;

AccessibilityRepeater::AccessibilityRepeater(QObject* parent)
    : QObject(parent)
{
    m_speech = new QTextToSpeech(this);
}

AccessibilityRepeater::~AccessibilityRepeater()
{
    // QTextToSpeech is deleted automatically by Qt parent-child system
}

void AccessibilityRepeater::repeatCurrentElement()
{
    const QString info = getCurrentElementInfo();
    if (!info.isEmpty()) {
        speak(info);
    }
}

QString AccessibilityRepeater::getCurrentElementInfo() const
{
    // Get the currently focused accessible item
    const IAccessible* focused = accessibilityController()->lastFocused();
    if (!focused) {
        return QStringLiteral("No element focused");
    }

    // If it's a score element (note, rest, measure, barline, articulation, etc.)
    // we prefer accessibleScreenReaderInfo() + accessibleExtraInfo()
    // because that's where MuseScore encodes rich musical info:
    // voice, pitch, duration, cross-staff, range warnings, measure number,
    // staff name, ties, articulations, etc. :contentReference[oaicite:5]{index=5}
    if (focused->accessibleRole() == IAccessible::Role::ElementOnScore) {
        QString mainInfo = focused->accessibleScreenReaderInfo();
        QString extra    = focused->accessibleExtraInfo();

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
        // fall through if empty
    }

    // Generic UI fallback (buttons, panels, etc.)
    QStringList parts;
    const QString name = focused->accessibleName();
    const QString desc = focused->accessibleDescription();
    const QString val  = focused->accessibleValue().toString();

    if (!name.isEmpty()) parts << name;
    if (!desc.isEmpty()) parts << desc;
    if (!val.isEmpty())  parts << QStringLiteral("value: ") + val;

    if (parts.isEmpty()) {
        return QStringLiteral("Unknown element");
    }

    return parts.join(QStringLiteral(", "));
}

void AccessibilityRepeater::speak(const QString& text)
{
    if (!m_speech) {
        return;
    }

    if (m_speech->state() == QTextToSpeech::Error) {
        LOGE() << "Text-to-speech error";
        return;
    }

    // Stop any current speech and speak the new text
    if (m_speech->state() == QTextToSpeech::Speaking) {
        m_speech->stop();
    }

    m_speech->say(text);
    LOGI() << "Speaking: " << text;
}
