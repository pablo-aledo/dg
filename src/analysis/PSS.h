#ifndef _DG_PSS_H_
#define _DG_PSS_H_

#include "AnalysisGeneric.h"

namespace dg {
namespace analysis {
namespace pss {

class PSSNode
{
};

class StoreNode : public PSSNode
{
    PSSNode *what, *where;

public:
    StoreNode(PSSNode *what, PSSNode *where)
        :what(what), where(where) {}
};

class LoadNode : public PSSNode
{
    PSSNode *from;

public:
    LoadNode(PSSNode *from): from(from) {}
};

class GEP : public PSSNode
{
    PSSNode *from;
    Offset offset;

public:
    GEP(PSSNode *from, const Offset& o)
        :from(from), offset(o) {}
};

class CastNode : public PSSNode
{
    PSSNode *from;

public:
    CastNode(PSSNode *from) : from(from) {}
};

class CallNode : public PSSNode
{
};

// namespace pss
// namespace analysis
// namespace dg

#endif
