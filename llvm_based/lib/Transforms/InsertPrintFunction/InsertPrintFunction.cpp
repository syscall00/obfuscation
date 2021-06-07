//===- InsertPrintFunction.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                   The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
  
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
  
#include <vector>
  
using namespace llvm;
  
namespace {
class InsertPrintFunction : public FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  InsertPrintFunction() : FunctionPass(ID) {}
  
  bool runOnFunction(Function &F) override {
    /*
    if (F.empty()) {
      return false;
    }
    
    Module *parentModule = F.getParent();
    Constant *availablePrint = parentModule->getOrInsertFunction("printf",
       FunctionType::get(IntegerType::getInt32Ty(parentModule->getContext()),
       PointerType::get(Type::getInt8Ty(parentModule->getContext()), 0),
                                                /* Var Args */ //true));
   /* if (auto printFunction = dyn_cast<Function>(availablePrint)) {
      BasicBlock *entryBasicBlock = &F.getEntryBlock();
      BasicBlock *splitBasicBlock = entryBasicBlock->splitBasicBlock(entryBasicBlock->getFirstNonPHIOrDbgOrLifetime());
      BasicBlock *generatedBasicBlock = BasicBlock::Create(F.getContext(), "PrintFunctionName", &F, splitBasicBlock);
      
      BranchInst *newBr = BranchInst::Create(generatedBasicBlock);
      ReplaceInstWithInst(entryBasicBlock->getTerminator(), newBr);
      
      IRBuilder<> instructionBuilder(generatedBasicBlock);
      auto functionName = F.getName().str();
      auto printedName = ConstantDataArray::getString(parentModule->getContext(), functionName + "\n");
      auto allocInstruction = instructionBuilder.CreateAlloca(printedName->getType());
      instructionBuilder.CreateStore(printedName, allocInstruction);
      Value *zero = ConstantInt::get(Type::getInt32Ty(parentModule->getContext()), 0);
      auto ptr = instructionBuilder.CreateInBoundsGEP(allocInstruction, {zero, zero});
      std::vector<Value *> args {
      ptr
      };
      instructionBuilder.CreateCall(printFunction, args, "printfCall");
      BranchInst::Create(splitBasicBlock, generatedBasicBlock);
      return true;
    }
    return false;
  */
    printf("InsertPrintFunction compiled\n");
    return false;
  }
};
}
  
char InsertPrintFunction::ID = 0;
static RegisterPass<InsertPrintFunction> X("InsertPrintFunction", "Insert Print Function Pass");
