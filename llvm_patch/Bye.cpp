#include <iostream>

#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//===-- HelloWorld.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/HelloWorld.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/PassManager.h"                                           
#include "llvm/Passes/PassBuilder.h"                                       
#include "llvm/Passes/PassPlugin.h"



using namespace llvm;

// PreservedAnalyses HelloWorldPass::run(Function &F,
//                                       FunctionAnalysisManager &AM) {                                
//   // errs() << F.getName() << "\n";
//   // return PreservedAnalyses::all();
// }

static void insert_asm_instruction(BasicBlock &BB, Instruction *InsertPtr, Module *M, const std::string& assembly_str)
{
  FunctionType *Ty = FunctionType::get(Type::getVoidTy(M->getContext()), false);
  IRBuilder<> IRB_temp_1(InsertPtr);
  llvm::InlineAsm *IA = llvm::InlineAsm::get(Ty,assembly_str,"~{dirflag},~{fpsr},~{flags}",true,false,InlineAsm::AD_ATT); 
  ArrayRef<Value *> Args = {};
  llvm::CallInst *Ptr = IRB_temp_1.CreateCall(IA, Args);
  Ptr->addAttributeAtIndex(AttributeList::FunctionIndex, Attribute::NoUnwind);
}

// PreservedAnalyses HelloWorldPass::run(Function &F, FunctionAnalysisManager &AM)
// {
 
// }


// PassPluginLibraryInfo getPassPluginInfo() {                                
//      const auto callback = [](PassBuilder &PB) {                            
//          PB.registerPipelineEarlySimplificationEPCallback([&](ModulePassManager &MPM, auto) {
//              MPM.addPass(createModuleToFunctionPassAdaptor(HelloWorldPass()));      
//              return true;                                                   
//          });                                                                
//      };                                                                     
                                                                            
//      return {LLVM_PLUGIN_API_VERSION, "helloworld", "0.0.1", callback};           
// };                                                                         
                                                                          

// extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
//      return getPassPluginInfo();
// }

          

// static cl::opt<bool> Wave("wave-goodbye", cl::init(false),
//                           cl::desc("wave good bye"));

// namespace {

// bool runBye(Function &F) {
//   if (Wave) {
//     errs() << "Bye: ";
//     errs().write_escaped(F.getName()) << '\n';
//   }
//   return false;
// }

// struct LegacyBye : public FunctionPass {
//   static char ID;
//   LegacyBye() : FunctionPass(ID) {}
//   bool runOnFunction(Function &F) override { return runBye(F); }
// };

struct Bye : PassInfoMixin<Bye> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    outs() << "Running the pass...\n";
    // if (!runBye(F))
    //   return PreservedAnalyses::all();
    

    Module *M = F.getParent();
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (auto *Call = dyn_cast<CallInst>(&I)) {
          if (Function *Callee = Call->getCalledFunction()) {
            if (Callee->getName() == "llvm.x86.sse2.mfence" || Callee->getName() == "llvm.x86.sse2.sfence") {
              errs() << "Found mfence or sfence CallInst!" << "\n";
              BasicBlock::iterator IP = BB.getFirstInsertionPt();

              insert_asm_instruction(BB, &(*IP), M, "push %rax");
              insert_asm_instruction(BB, &(*IP), M, "movl $$1, %eax");

              if (Callee->getName() == "llvm.x86.sse2.mfence") {
                insert_asm_instruction(BB, &(*IP), M, "movl $$0, %ebx"); // FIXME: set number to 0 for mfence, 1 for sfence.
              } else {
                insert_asm_instruction(BB, &(*IP), M, "movl $$1, %ebx");
              }
              
              insert_asm_instruction(BB, &(*IP), M, "int $$0x80"); // Return value is stored in 
              insert_asm_instruction(BB, &(*IP), M, "pop %rax");
              
          } else if (isa<FenceInst>(&I)) {
            errs() << "Found FenceInst!" << "\n";

            BasicBlock::iterator IP = BB.getFirstInsertionPt();

              insert_asm_instruction(BB, &(*IP), M, "push %rax");
              insert_asm_instruction(BB, &(*IP), M, "movl $$1, %eax");
              insert_asm_instruction(BB, &(*IP), M, "xorl %ebx, %ebx");
              insert_asm_instruction(BB, &(*IP), M, "int $$0x80");
              insert_asm_instruction(BB, &(*IP), M, "pop %rax");
              insert_asm_instruction(BB, &(*IP), M, "pop %rax");
          }
        }
      }
    }
    }

    return PreservedAnalyses::all();
  }
};

// } // namespace

// static RegisterPass<LegacyBye> X("goodbye", "Good Bye World Pass",
//                                  false /* Only looks at CFG */,
//                                  false /* Analysis Pass */);

/* New PM Registration */

llvm::PassPluginLibraryInfo getPassPluginInfo() {                                
     const auto callback = [](PassBuilder &PB) {                            
         PB.registerPipelineEarlySimplificationEPCallback([&](ModulePassManager &MPM, auto) {
          std::cout << "Registered!" << std::endl; 
             MPM.addPass(createModuleToFunctionPassAdaptor(Bye()));      
             return true;                                                   
         });                                                                
     };                                                                     
                                                                            
     return {LLVM_PLUGIN_API_VERSION, "name", "0.0.1", callback};           
};                                                                         
                                                                            
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
     return getPassPluginInfo();
}



// llvm::PassPluginLibraryInfo getByePluginInfo() {
//   return {LLVM_PLUGIN_API_VERSION, "Bye", LLVM_VERSION_STRING,
//           [](PassBuilder &PB) {

//              PB.registerPipelineEarlySimplificationEPCallback([&](ModulePassManager &MPM, auto) {
//              MPM.addPass(createModuleToFunctionPassAdaptor(Bye()));      
//              return true;                                                   
//             }); 
              
//             // PB.registerPipelineParsingCallback([](StringRef Name, llvm::ModulePassManager &MPM, ArrayRef<llvm::PassBuilder::PipelineElement>) {
             
//             //  if (Name == "goodbye") {
//             //   std::cout << "Adding pass..." << std::endl;
//             //   MPM.addPass(createModuleToFunctionPassAdaptor(Bye()));      
//             //   return true;
//             //  }
//             //  return false;                                                   
//            }); 
//             // PB.registerVectorizerStartEPCallback(
//             //     [](llvm::FunctionPassManager &PM, OptimizationLevel Level) {
//             //       PM.addPass(Bye());
//             //     });
//             // PB.registerPipelineParsingCallback(
//             //     [](StringRef Name, llvm::FunctionPassManager &PM,
//             //        ArrayRef<llvm::PassBuilder::PipelineElement>) {
//             //       if (Name == "goodbye") {
//             //         PM.addPass(Bye());
//             //         return true;
//             //       }
//             //       return false;
//             //     });
//   }};
//}

// #ifndef LLVM_BYE_LINK_INTO_TOOLS
// extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
// llvmGetPassPluginInfo() {
//   return getByePluginInfo();
// }
// #endif
