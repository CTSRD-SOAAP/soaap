/*
 * Copyright (c) 2017 Gabriela Sklencarova
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Generate/SandboxGenerator.h"

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include "Common/Debug.h"
#include "Generate/RPC/Serializer.h"
#include "Util/DebugUtils.h"
#include "Util/SboxGenUtils.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#define IN_SOAAP_GENERATOR
#include "soaap.h"
#include "soaap_gen.h"

#include <cassert>
#include <sys/nv.h>

using namespace soaap;

void SandboxGenerator::generate(Module& M, SandboxVector& sandboxes,
    vector<Module*>& modules) {
  /* Get LLVM context */
  LLVMContext &C = M.getContext();
  IRBuilder<> B(C);
  Serializer R(M);

  /* Helper constants. */
  Value* Zero = ConstantInt::get(Type::getInt64Ty(C), 0, false);

  /* Useful functions. */
  Function* exitFunc = M.getFunction("exit");

  assert(M.getFunction("nvlist_create") != nullptr &&
      "The function nvlist_create does not exist; did you forgot to #include <soaap_gen>?");

  /* Useful types. */
  Type* nvlistPtrTy = M.getFunction("nvlist_create")->getReturnType();
  Type* i64 = Type::getInt64Ty(C);
  Type* i32 = Type::getInt32Ty(C);
  Type* i8ptr = PointerType::get(Type::getInt8Ty(C), 0);
  Type* i64ptr = PointerType::get(Type::getInt64Ty(C), 0);

  unsigned moduleIdx = 0;

  /* Create a global array to hold the file descriptors of the sandboxes. */
  ArrayType* sboxDescriptorsType = ArrayType::get(Type::getInt32Ty(C),
      sandboxes.size());
  /* Construct an empty initializer array. */
  Value* sboxDescriptors = new GlobalVariable(M, sboxDescriptorsType, false,
      GlobalValue::CommonLinkage, ConstantAggregateZero::get(sboxDescriptorsType));

  bool persistentSandboxExists = false; 
  for (Sandbox* S : sandboxes) {
    /* Generate a separate module for each sandbox. */
    SandboxModuleGenerator moduleGenerator(M, *S);
    Module* GM = moduleGenerator.get();

    bool persistent = S->isPersistent();
    persistentSandboxExists = persistentSandboxExists || persistent;

    SboxGenUtils::insertParentFunctions(M);

    /* Delete sandbox internal functions */
    for (Function* F : S->getInternalFunctions()) {
      F->removeFromParent();
    }

    for (Function* F : S->getEntryPoints()) {
      F->deleteBody();
      Type* retTy = F->getReturnType();

      InputAnnotationMap inputMap = S->getAnnotatedInputs();

      /* Insert a call to the child process. */
      BasicBlock* funcEntry = BasicBlock::Create(
          C, "soaap_gen_func_" + F->getName().str() + "_entry", F);
      B.SetInsertPoint(funcEntry);

      Value* nvlAddr = B.CreateAlloca(nvlistPtrTy);
      std::map<Value*, Value*> allocatedArgs = SboxGenUtils::allocateArguments(B, F);
      SboxGenUtils::storeArguments(B, F, allocatedArgs);

      /* Create a request nvlist and add the function index to it. */
      Value* nvl = R.nvlistCreate(B);
      B.CreateStore(nvl, nvlAddr);

      /* Marshal input arguments. */
      unsigned argIdx = 0;
      for (auto A = F->arg_begin(); A != F->arg_end(); A++) {
        /* Don't marshall output buffers. */
        Value* V = dyn_cast<Value>(A);
        if (inputMap.count(V) && inputMap[V].access == InputAccess::Out) {
          argIdx++;
          continue;
        }

        Value* arg = B.CreateLoad(allocatedArgs[V]);
        InputAnnotation* annotation = NULL;
        if (inputMap.count(V)) {
          annotation = &inputMap[V];
        }

        R.serializeArgument(B, F, argIdx, arg, nvlAddr, annotation, allocatedArgs);
        argIdx++;
      }

      /* Marshal globals. */
      unsigned globIdx = 0;
      for (Value* V : S->getLoadedGlobalVars()) {
        R.serializeGlobal(B, F, globIdx, V, nvlAddr, allocatedArgs);
        globIdx++;
      }

      /* Get the sandbox file descriptor. */
      Value* fdPtr = B.CreateAlloca(Type::getInt32Ty(C));
      Value* fd;
      if (persistent) {
        Value* sboxData = B.CreateGEP(sboxDescriptors, ArrayRef<Value*>(
              {Zero, ConstantInt::get(Type::getInt32Ty(C), moduleIdx, false)}));
        fd = B.CreateLoad(sboxData);
      } else {
        fd = generateCreateCall(M, B, GM->getName());
      }
      B.CreateStore(fd, fdPtr);

      Function* addFnCallData = M.getFunction("soaap_gen_add_function_call_data");
      assert(addFnCallData &&
          "The function soaap_gen_add_function_call_data does not exist.");

      nvl = B.CreateLoad(nvlAddr);
      B.CreateCall(addFnCallData, ArrayRef<Value*>(
            {nvl, ConstantInt::get(i64, S->getIDForEntryPoint(F), false)}));
      // B.CreateStore(nvl, nvlAddr);

      std::string bbName = "soaap_gen_func_" + F->getName().str();
      BasicBlock* sboxRecvLoop = BasicBlock::Create(C, bbName + "_recv_loop", F);
      BasicBlock* msgTypeError = BasicBlock::Create(C, bbName + "_msg_error", F);
      BasicBlock* msgReturn = BasicBlock::Create(C, bbName + "_msg_return", F);
      BasicBlock* msgInvoke = BasicBlock::Create(C, bbName + "_msg_invoke", F);

      B.CreateBr(sboxRecvLoop);
      B.SetInsertPoint(sboxRecvLoop);

      Function* enterSboxFn = M.getFunction("soaap_gen_enter_sbox");
      assert(enterSboxFn && "The function soaap_gen_enter_sbox does not exist.");

      fd = B.CreateLoad(fdPtr);
      nvl = B.CreateCall(enterSboxFn, ArrayRef<Value*>({fd, nvl}));
      B.CreateStore(nvl, nvlAddr);

      unsigned numStructs = 0;
      for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
        Value* V = dyn_cast<Value>(A);
        Type* argTy = SboxGenUtils::stripPointerType(A->getType());
        /* TODO check that it's not a buffer */
        if (argTy->isStructTy() &&
            (!inputMap.count(V) || inputMap[V].type != InputType::FileDescriptor)) {
          ++numStructs;
        }
      }

      Value* msgType = R.nvlistGetNumber(B, nvl, MESSAGE_TYPE);
      SwitchInst* msgTypeSwitch = B.CreateSwitch(msgType, msgTypeError, 2);

      msgTypeSwitch->addCase(ConstantInt::get(IntegerType::get(C, 64),
            METHOD_INVOCATION, false), msgInvoke);

      // SwitchInst* msgTypeSwitch = B.CreateSwitch(msgType, msgTypeError, 2);
      msgTypeSwitch->addCase(ConstantInt::get(IntegerType::get(C, 64), RETURN, false),
          msgReturn);
      // msgTypeSwitch->addCase(ConstantInt::get(
      //       IntegerType::get(C, 64), METHOD_INVOCATION, false), msgInvoke);
      
      B.SetInsertPoint(msgInvoke);
      if (numStructs == 0) {
        B.CreateBr(msgTypeError);
      } else {
        nvl = B.CreateLoad(nvlAddr);
        Function* addMethodReturnData = M.getFunction(
            "soaap_gen_add_method_return_data");
        assert(addMethodReturnData &&
            "The function soaap_gen_add_method_return_data does not exist.");

        BasicBlock* argIdxError = BasicBlock::Create(C, "soaap_arg_idx_error", F);

        /* Retrieve the argument index. */
        Value* argIdx = R.nvlistGetNumber(B, nvl, "arg_idx");
        SwitchInst* argSwitch = B.CreateSwitch(argIdx, argIdxError, numStructs);

        B.SetInsertPoint(argIdxError);
        /* TODO */
        B.CreateCall(exitFunc, ConstantInt::get(i32, 0, false));
        B.CreateUnreachable();

        unsigned idx = 0;
	for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
          Value* V = dyn_cast<Value>(A);
          Type* argTy = A->getType();
          if (!argTy->isPointerTy() || !argTy->getPointerElementType()->isStructTy() ||
              (inputMap.count(V) && inputMap[V].type == InputType::FileDescriptor)) {
            ++idx;
            continue;
          }

          BasicBlock* argCaseBlock = BasicBlock::Create(C,
              "soaap_arg_idx_case_" + std::to_string(idx), F);
          argSwitch->addCase(
              ConstantInt::get(IntegerType::get(C, 64), idx, false), argCaseBlock);
          B.SetInsertPoint(argCaseBlock);

          Value* arg = B.CreateLoad(allocatedArgs[V]);
          nvl = B.CreateLoad(nvlAddr);

          StructType* structTy = dyn_cast<StructType>(argTy->getPointerElementType());
          unsigned numFns = 0;
          for (StructType::element_iterator I = structTy->element_begin(),
              E = structTy->element_end(); I != E; ++I) {
            Type* T = *I;
            if (T->isPointerTy() && T->getPointerElementType()->isFunctionTy()) {
              ++numFns;
            }
          }

          BasicBlock* elemIdxError = BasicBlock::Create(C, "soaap_arg_" +
              std::to_string(idx) + "_elem_error", F);

          Value* elemIdx = R.nvlistGetNumber(B, nvl, "elem_idx");
          SwitchInst* elemSwitch = B.CreateSwitch(elemIdx, elemIdxError, numFns);

          B.SetInsertPoint(elemIdxError);
          /* TODO */
          B.CreateCall(exitFunc, ConstantInt::get(i32, 0, false));
          B.CreateUnreachable();

          unsigned structIdx = 0;
          for (StructType::element_iterator I = structTy->element_begin(),
              E = structTy->element_end(); I != E; ++I, ++structIdx) {
            Type* T = *I;
            if (!T->isPointerTy() || !T->getPointerElementType()->isFunctionTy()) {
              continue;
            }
            FunctionType* FT = dyn_cast<FunctionType>(T->getPointerElementType());

            BasicBlock* elemCaseBlock = BasicBlock::Create(C, "soaap_arg_" + 
                std::to_string(idx) + "_elem_idx_" + std::to_string(structIdx), F);
            elemSwitch->addCase(
                ConstantInt::get(IntegerType::get(C, 64), structIdx, false),
                elemCaseBlock);
            B.SetInsertPoint(elemCaseBlock);

            Value* fn = B.CreateLoad(B.CreateStructGEP(structTy, arg, structIdx));
            nvl = B.CreateLoad(nvlAddr);

            /* Allocate space for the arguments. */
            std::vector<Value*> allocatedArgs;
            for (Type* P : FT->params()) {
              allocatedArgs.push_back(B.CreateAlloca(P));
            }

            unsigned fnArgIdx = 0;
            std::map<Value*, unsigned> linkedArgIdxs;
            for (Type* P : FT->params()) {
              /* TODO check that this is a supported type (not a fd, FILE*, buffer...) */
              R.deserializeArgument(B, F, fnArgIdx, nvlAddr, allocatedArgs[fnArgIdx],
                  NULL, NULL);
              ++fnArgIdx;
            }

            std::vector<Value*> fnArgs;
            for (Value* ptr : allocatedArgs) {
              fnArgs.push_back(B.CreateLoad(ptr));
            }

            /* Get globals */
            nvl = B.CreateLoad(nvlAddr);
            unsigned globIdx = 0;
            for (Value* V : S->getStoredGlobalVars()) {
              R.deserializeGlobal(B, F, globIdx, nvlAddr, V);
              globIdx++;
            }

            B.CreateCall(fn, ArrayRef<Value*>({fnArgs}));

            nvl = R.nvlistCreate(B);

            /* Marshal globals. */
            globIdx = 0;
            for (Value* V : S->getLoadedGlobalVars()) {
              // TODO
              std::map<Value*, Value*> allocatedArgs;
              R.serializeGlobal(B, F, globIdx, V, nvlAddr, allocatedArgs);
              globIdx++;
            }

            B.CreateCall(addMethodReturnData, ArrayRef<Value*>({nvl}));
            B.CreateStore(nvl, nvlAddr);
            B.CreateBr(sboxRecvLoop);
          }

          ++idx;
        }
      }

      B.SetInsertPoint(msgTypeError);
      /* TODO */
      B.CreateCall(exitFunc, ConstantInt::get(i32, 0, false));
      B.CreateUnreachable();

      B.SetInsertPoint(msgReturn);

      if (!persistent) {
        fd = B.CreateLoad(fdPtr);
        generateTerminateCall(M, B, fd);
      }

      std::set<Value*> linkedArgs;
      for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
        Value* V = dyn_cast<Value>(A);
        if (inputMap.count(V) && inputMap[V].type == InputType::Buffer) {
          linkedArgs.insert(inputMap[V].linkedArg);
        }
      }

      std::map<Value*, unsigned> linkedArgIdxs;
      argIdx = 0;
      for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
        Value* V = dyn_cast<Value>(A);
        if (linkedArgs.count(V)) {
          linkedArgIdxs[V] = argIdx;
        }
        ++argIdx;
      }

      nvl = B.CreateLoad(nvlAddr);
      /* Retrieve any out buffers. */
      argIdx = 0;
      for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
        Value* V = dyn_cast<Value>(A);
        Type* valTy = V->getType();
        if (!valTy->isPointerTy()) {
          ++argIdx;
          continue;
        }

        if (inputMap.count(V) && (inputMap[V].access != InputAccess::Out &&
              inputMap[V].access != InputAccess::Inout)) {
          ++argIdx;
          continue;
        }

        InputAnnotation* annotation = NULL;
        if (inputMap.count(V)) {
          annotation = &inputMap[V];
        }

        else if (annotation && annotation->type == InputType::Buffer) {
          Value* ptr = B.CreateAlloca(valTy);
          Value* linkedArgPtr = allocatedArgs[annotation->linkedArg];
          R.deserializeArgument(B, F, argIdx, nvlAddr, ptr, annotation, linkedArgPtr);
          R.copyBuffer(B, F, B.CreateLoad(ptr), B.CreateLoad(allocatedArgs[V]),
              B.CreateLoad(linkedArgPtr));
        } else {
          R.deserializeArgument(B, F, argIdx, nvlAddr, allocatedArgs[V],
              annotation, NULL);
        }

        ++argIdx;
      }

      /* Get globals */
      globIdx = 0;
      for (Value* V : S->getStoredGlobalVars()) {
        R.deserializeGlobal(B, F, globIdx, nvlAddr, V);
        globIdx++;
      }

      retTy = F->getReturnType();
      if (retTy->isIntegerTy() && retTy->getIntegerBitWidth() <= 64) {
        Value* retValue = R.nvlistGetNumber(B, nvl, RETURN_VALUE);
        retValue = B.CreateIntCast(retValue, retTy, true);
        B.CreateRet(retValue);
      } else if (retTy->isVoidTy()) {
        B.CreateRetVoid();
      } else {
        /* TODO handle errors properly. */
        outs() << "Unsupported return type.\n";
        retTy->dump();
      }

      F->setLinkage(GlobalValue::InternalLinkage);
    }

    /* Add the module to the vector of generated modules. */
    modules.push_back(GM);
    moduleIdx++;
  }

  if (!persistentSandboxExists) return;

  Function* mainFn = M.getFunction("main");
  Instruction* mainFirstInst = mainFn->getEntryBlock().getFirstNonPHI();

  if (!mainFirstInst) {
    return;
  }

  moduleIdx = 0;
  for (Module* mod : modules) {
    if (!sandboxes[moduleIdx]->isPersistent()) {
      ++moduleIdx;
      continue;
    }
    /* Create the sandbox. */
    // IRBuilder<> B(C);
    B.SetInsertPoint(mainFirstInst);
    Value* sboxData = generateCreateCall(M, B, mod->getName());
    B.CreateStore(sboxData, B.CreateGEP(sboxDescriptors, ArrayRef<Value*>({Zero,
          ConstantInt::get(Type::getInt64Ty(C), moduleIdx, false)})));

    /* Instrument all exit return points to close the sandboxes. */
    Function* terminateSbox = M.getFunction("soaap_gen_terminate_sbox");
    for (BasicBlock& BB : mainFn->getBasicBlockList()) {
      TerminatorInst* mainLastInst = BB.getTerminator();
      if (isa<ReturnInst>(mainLastInst)) {
        CallInst* terminateCall = CallInst::Create(terminateSbox,
            ArrayRef<Value*>({sboxData}));
        terminateCall->setTailCall();
        terminateCall->insertBefore(mainLastInst);
      }
    }

    moduleIdx++;
  }


  if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
    lga->eraseFromParent();
  }

  StripDebugInfo(M);
}

Value* SandboxGenerator::generateCreateCall(Module& M, IRBuilder<>& B, StringRef name) {
  Function* createFn = M.getFunction("soaap_gen_create_sbox");
  assert(createFn && "soaap_gen_create_sbox does not exist");
  Value* namePtr = B.CreateGlobalStringPtr(name);
  return B.CreateCall(createFn, ArrayRef<Value*>({namePtr}));
}

void SandboxGenerator::generateTerminateCall(Module& M, IRBuilder<>& B, Value* fd) {
  Function* terminateFn = M.getFunction("soaap_gen_terminate_sbox");
  assert(terminateFn && "soaap_gen_terminate_sbox");
  B.CreateCall(terminateFn, ArrayRef<Value*>({fd}));
}
