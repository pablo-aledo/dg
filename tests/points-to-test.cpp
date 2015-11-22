#include <assert.h>
#include <cstdarg>
#include <cstdio>

#include "test-runner.h"
#include "test-dg.h"

#include "analysis/PSS.h"

namespace dg {
namespace tests {

using analysis::Pointer;
using namespace analysis::pss;

class GenericPointsToTest : public Test
{
public:
    GenericPointsToTest() : Test("test process functions from PSS") {}

    void store_load()
    {

        AllocationNode *A = new AllocationNode(8);
        AllocationNode *B = new AllocationNode(8);

        PSSNode *SI = new StoreNode(A, B);
        PSSNode *L = new LoadNode(B);

        // process the nodes
        bool ret;
        ret = (*SI)();
        check(ret, "StoreNode process() bug");
        ret = (*L)();
        check(ret, "LoadNode process() bug");

        analysis::PointsToSetT& PS = L->getPointsTo();
        check(PS.size() == 1, "size should have been 1, but is %lu", PS.size());
        check(*PS.begin() == Pointer(A->getMemoryObject(), 0),
              "line: %lu, Store->Load bug", __LINE__);
    }

    void test()
    {
        store_load();
    }
};

}; // namespace tests
}; // namespace dg

int main(int argc, char *argv[])
{
    using namespace dg::tests;
    TestRunner Runner;

    Runner.add(new GenericPointsToTest());

    return Runner();
}
