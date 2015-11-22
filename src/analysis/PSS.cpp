#include "PSS.h"

namespace dg {
namespace analysis {
namespace pss {

// we don't need to do anything else with allocation,
// we already added the pointsTo set in constructor
// Since we assume SSA form, this pointsTo will never change
bool AllocationNode::process()
{
    return false;
}

bool StoreNode::process()
{
    bool changed = false;

    // copy points-to set of 'what' to memory location
    // of 'where'
    for (const Pointer& p : what->getPointsTo()) {
        for (const Pointer& to : where->getPointsTo()) {
            changed |= to.object->addPointsTo(to.offset, p);
        }
    }

    return changed;
}

bool LoadNode::process()
{
    bool changed = false;

    // load pointers from memory locations pointed by 'from'
    for (const Pointer& p : from->getPointsTo())
        for (const Pointer& memptr : p.object->getPointsTo(p.offset))
            changed |= addPointsTo(memptr);

    return changed;
};

} // namespace pss
} // namespace analysis
} // namespace dg
