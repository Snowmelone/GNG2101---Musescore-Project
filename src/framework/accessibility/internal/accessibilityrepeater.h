#ifndef MU_ACCESSIBILITY_ACCESSIBILITYREPEATER_H
#define MU_ACCESSIBILITY_ACCESSIBILITYREPEATER_H

#include <QObject>
#include <QTextToSpeech>

#include "modularity/ioc.h"
#include "iaccessibilitycontroller.h"
#include "iaccessible.h"

namespace muse::accessibility {

class AccessibilityRepeater : public QObject
{
    Q_OBJECT
    INJECT(IAccessibilityController, accessibilityController)

public:
    explicit AccessibilityRepeater(QObject* parent = nullptr);
    ~AccessibilityRepeater() override;

    // Call this when the user presses the repeat shortcut
    void repeatCurrentElement();

private:
    QString getCurrentElementInfo() const;
    void speak(const QString& text);

    QTextToSpeech* m_speech = nullptr;
};

} // namespace muse::accessibility

#endif // MU_ACCESSIBILITY_ACCESSIBILITYREPEATER_H
