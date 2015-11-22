#ifndef _LLVM_ANALYSIS_GENERIC_H_
#define _LLVM_ANALYSIS_GENERIC_H_

#include <map>
#include <set>
#include <assert.h>

#include "analysis/Pointer.h"

namespace llvm {
    class Value;
    class ConstantExpr;
    class DataLayout;
};

namespace dg {

class LLVMDependenceGraph;
class LLVMNode;

namespace analysis {

Pointer getConstantExprPointer(const llvm::ConstantExpr *CE,
                               LLVMDependenceGraph *dg,
                               const llvm::DataLayout *DL);

LLVMNode *getOperand(LLVMNode *node, const llvm::Value *val,
                     unsigned int idx, const llvm::DataLayout *DL);

} // namespace analysis
} // namespace dg

#endif // _LLVM_ANALYSIS_GENERIC_H_
