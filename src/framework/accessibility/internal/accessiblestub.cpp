#include "accessiblestub.h"

using namespace muse::accessibility;

QAccessibleInterface* AccessibleStub::accessibleInterface(QObject* object)
{
    Q_UNUSED(object);
    // Stub provides no custom accessibility interface
    // Returning nullptr tells Qt we do not handle this object
    return nullptr;
}
