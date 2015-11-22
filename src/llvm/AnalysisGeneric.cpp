#include <map>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/raw_ostream.h>

#include "AnalysisGeneric.h"
#include "LLVMDependenceGraph.h"
#include "PointsTo.h"

using namespace llvm;

namespace dg {
namespace analysis {

static LLVMNode *createNodeWithMemAlloc(const Value *val)
{
    LLVMNode *n = new LLVMNode(val);
    MemoryObj *&mo = n->getMemoryObj();
    mo = new MemoryObj(n);
    n->addPointsTo(Pointer(mo));

    return n;
}


static LLVMNode *getConstantExprNode(const llvm::ConstantExpr *CE,
                                     LLVMDependenceGraph *dg,
                                     const llvm::DataLayout *DL);

static LLVMNode *getOrCreateNode(LLVMDependenceGraph *dg, const Value *val,
                                 const llvm::DataLayout *DL)
{
    LLVMNode *n = dg->getNode(val);
    if (n)
        return n;

    if (llvm::isa<llvm::Function>(val)) {
        n = createNodeWithMemAlloc(val);
    } else if (llvm::isa<llvm::ConstantPointerNull>(val)) {
        n = new LLVMNode(val);
        n->addPointsTo(NullPointer);
    } else if (const llvm::ConstantExpr *CE
                = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
        return getConstantExprNode(CE, dg, DL);
    } else {
        errs() << "ERR get-or-create-node: unhandled value " << *val << "\n";
        return nullptr;
    }

    n->setDG(dg);
    return n;
}

static Pointer handleConstantBitCast(LLVMDependenceGraph *dg,
                                     const BitCastInst *BC, const llvm::DataLayout *DL)
{
    if (!BC->isLosslessCast()) {
        errs() << "WARN: Not a loss less cast unhandled ConstExpr"
               << *BC << "\n";
        return UnknownMemoryLocation;
    }

    const Value *llvmOp = BC->stripPointerCasts();
    LLVMNode *op = getOrCreateNode(dg, llvmOp, DL);
    if (!op) {
        errs() << "ERR: unsupported BitCast constant operand" << *BC << "\n";
    } else {
        PointsToSetT& ptset = op->getPointsTo();
        if (ptset.size() != 1) {
            errs() << "ERR: constant BitCast with not only one pointer "
                   << *BC << "\n";
        } else
            return *ptset.begin();
    }

    return UnknownMemoryLocation;
}

static inline unsigned getPointerBitwidth(const DataLayout *DL, const Value *ptr)
{
    const Type *Ty = ptr->getType();
    return DL->getPointerSizeInBits(Ty->getPointerAddressSpace());
}

static Pointer handleConstantGep(LLVMDependenceGraph *dg,
                                 const GetElementPtrInst *GEP,
                                 const llvm::DataLayout *DL)
{
    const Value *op = GEP->getPointerOperand();
    LLVMNode *opNode;
    if (isa<GlobalVariable>(op))
        // while initializing globals, we may not have the points-to propagated,
        // so we need to use the original global instead of parameter global
        // FIXME: this is hack, get rid of me
        opNode = dg->getGlobalNode(op);
    else
        opNode = dg->getNode(op);

    // FIXME this is sound, but may be unprecise
    //  - we should use getOperand for getting opNode,
    //  becaues we can have ConstantExpr inserted in ConstantExpr
    //  (getelementptr (inttoptr ..) ...), so we can get null here
    //  as opNode
    if (!opNode) {
        // is this recursively created expression?
        if (isa<ConstantExpr>(op)) {
            opNode = getConstantExprNode(cast<ConstantExpr>(op), dg, DL);
        }

        if (!opNode) {
            errs() << "No node for Constant GEP operand: " << *GEP << "\n";
            return UnknownMemoryLocation;
        }
    }

    PointsToSetT& S = opNode->getPointsTo();
    // since this is constant expr, there's no way how we could
    // get extra points-to binding in runtime
    assert(S.size() == 1);
    MemoryObj *mo = (*S.begin()).obj;
    if (!mo) {
        errs() << "ERR: no memory object in " << *opNode->getKey() << "\n";
        return UnknownMemoryLocation;
    }

    Pointer pointer(mo, UNKNOWN_OFFSET);
    unsigned bitwidth = getPointerBitwidth(DL, op);
    APInt offset(bitwidth, 0);

    if (GEP->accumulateConstantOffset(*DL, offset)) {
        if (offset.isIntN(bitwidth))
            pointer.offset = offset.getZExtValue();
        else
            errs() << "WARN: Offset greater than "
                   << bitwidth << "-bit" << *GEP << "\n";
    }
    // else offset is set to UNKNOWN (in constructor)

    return pointer;
}

Pointer getConstantExprPointer(const ConstantExpr *CE,
                               LLVMDependenceGraph *dg,
                               const llvm::DataLayout *DL)
{
    Pointer pointer = UnknownMemoryLocation;

    const Instruction *Inst = const_cast<ConstantExpr*>(CE)->getAsInstruction();
    if (const GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Inst)) {
        pointer = handleConstantGep(dg, GEP, DL);
    } else if (const BitCastInst *BC = dyn_cast<BitCastInst>(Inst)) {
        pointer = handleConstantBitCast(dg, BC, DL);
    } else {
            errs() << "ERR: Unsupported ConstantExpr " << *CE << "\n";
            errs() << "      ^^ returning unknown pointer\n";
    }

    delete Inst;
    return pointer;
}

static LLVMNode *getConstantIntToPtrNode(const ConstantExpr *CE,
                                         const llvm::DataLayout *DL)
{
    using namespace llvm;

    const Value *val = CE->getOperand(0);
    if (!isa<ConstantInt>(val)) {
        errs() << "Unhandled constant inttoptr " << *CE << "\n";
        abort();
    }

    const ConstantInt *C = cast<ConstantInt>(val);
    uint64_t value = C->getLimitedValue();

    LLVMNode *&node = intToPtrMap[value];
    if (!node) {
        node = new LLVMNode(C);
        const Value *dest = CE->getOperand(1);
        Type *Ty = dest->getType()->getContainedType(0);
        uint64_t size = 0;
        if (Ty->isSized())
            size = DL->getTypeAllocSize(Ty);

        MemoryObj *&mo = node->getMemoryObj();
        mo = new MemoryObj(node, size);
        node->addPointsTo(mo);
    }

    return node;
}

static LLVMNode *getConstantExprNode(const llvm::ConstantExpr *CE,
                                     LLVMDependenceGraph *dg,
                                     const llvm::DataLayout *DL)
{
    LLVMNode *node;
    // we have these nodes stored
    if (isa<IntToPtrInst>(CE)) {
        node = getConstantIntToPtrNode(CE, DL);
    } else {
        // FIXME add these nodes somewhere,
        // so that we can delete them later
        node = new LLVMNode(CE);

        // set points-to sets
        Pointer ptr = getConstantExprPointer(CE, dg, DL);
        node->addPointsTo(ptr);
    }

    node->setDG(dg);
    return node;
}

static LLVMNode *getUnknownNode(LLVMDependenceGraph *dg, const llvm::Value *val,
                                const llvm::DataLayout *DL)
{
    LLVMNode *node = nullptr;

    using namespace llvm;
    if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(val)) {
        node = getConstantExprNode(CE, dg, DL);
    } else if (isa<Function>(val) || isa<ConstantPointerNull>(val)) {
        // if the function was created via function pointer during
        // points-to analysis, the operand may not be not be set.
        // What is worse, the function may not be created either,
        // so the node just may not exists at all, so we need to
        // create it
        node = getOrCreateNode(dg, val, DL);
    } else {
        errs() << "ERR: Unsupported operand: " << *val << "\n";
        abort();
    }

    assert(node && "Did not get a node");
    return node;
}

/*
 * we have DependenceGraph::getNode() which retrives existing node.
 * The operand nodes may not exists, though.
 * This function gets the existing node, or creates new one and sets
 * it as an operand.
 */
LLVMNode *getOperand(LLVMNode *node, const llvm::Value *val,
                     unsigned int idx, const llvm::DataLayout *DL)
{
    // ok, before calling this we call llvm::Value::getOperand() to get val
    // and in node->getOperand() we call it too. It is small overhead, but just
    // to know where to optimize when going to extrems

    LLVMNode *op = node->getOperand(idx);
    if (op)
        return op;

    LLVMDependenceGraph *dg = node->getDG();

    // set new operand
    op = getUnknownNode(dg, val, DL);
    assert(op && "Did not get op");

    node->setOperand(op, idx);
    return op;
}

} // namespace analysis
} // namespace dg


