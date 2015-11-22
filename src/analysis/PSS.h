#ifndef _DG_PSS_H_
#define _DG_PSS_H_

#include "Pointer.h"

namespace dg {
namespace analysis {
namespace pss {

// PSSNode
//
// Pointer state subgraph generic node.
class PSSNode
{
    PointsToSetT pointsTo;

public:
    PointsToSetT& getPointsTo() { return pointsTo; }
    const PointsToSetT& getPointsTo() const { return pointsTo; }

    bool addPointsTo(const Pointer& ptr)
    {
        return pointsTo.insert(ptr).second;
    }

    virtual bool process() = 0;
    bool operator()() { return process(); }
};

class AllocationNode : public PSSNode
{
    MemoryObject *memobject;

public:
    AllocationNode(uint64_t size, bool is_heap = false)
        : memobject (new MemoryObject(size, is_heap, this))
    {
        addPointsTo(Pointer(memobject, 0));
    }

    MemoryObject *getMemoryObject() { return memobject; }
    const MemoryObject *getMemoryObject() const { return memobject; }

    virtual bool process();
};

class StoreNode : public PSSNode
{
    PSSNode *what, *where;

public:
    StoreNode(PSSNode *what, PSSNode *where)
        :what(what), where(where) {}

    virtual bool process();
};

class LoadNode : public PSSNode
{
    PSSNode *from;

public:
    LoadNode(PSSNode *from): from(from) {}

    virtual bool process();
};

class GEP : public PSSNode
{
    PSSNode *from;
    Offset offset;

public:
    GEP(PSSNode *from, const Offset& o)
        :from(from), offset(o) {}

    virtual bool process();
};

class CastNode : public PSSNode
{
    PSSNode *from;

public:
    CastNode(PSSNode *from) : from(from) {}

    virtual bool process();
};

class PHINode : public PSSNode
{
    //std::vector<PSSNode *> from;

public:
    //PHINode(PSSNode *from) : from(from) {}

    virtual bool process();
};

class CallNode : public PSSNode
{

public:
    virtual bool process();
};

} // namespace pss
} // namespace analysis
} // namespace dg

#endif
