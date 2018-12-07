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

#include "Generate/SandboxModuleGenerator.h"

#include <cassert>
#include "errno.h"

#include "OS/Sandbox/Capsicum.h"
#include "Util/CallGraphUtils.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"

#define IN_SOAAP_GENERATOR
#include "soaap.h"
#include "soaap_gen.h"

using namespace soaap;

SandboxModuleGenerator::SandboxModuleGenerator(Module& O, Sandbox& S) : O(O), S(S) {
  M = new Module(S.getName() + "_sbox_module", O.getContext());
  LLVMContext& C = M->getContext();
  mainFd = new GlobalVariable(*M, Type::getInt32Ty(C), false, GlobalValue::CommonLinkage,
      ConstantInt::get(Type::getInt32Ty(C), 0));

  generate();
  generateDispatchFunction();
  generateMainFunction();

  /* Remove all llvm.var.annotation calls */
  if (Function* F = M->getFunction("llvm.var.annotation")) {
    for (User* U : F->users()) {
      if (IntrinsicInst* I = dyn_cast<IntrinsicInst>(U)) {
        I->eraseFromParent();
      }
    }
  }

  if (GlobalVariable* lga = M->getNamedGlobal("llvm.global.annotations")) {
    lga->eraseFromParent();
  }
}

Module* SandboxModuleGenerator::get() {
  return M;
}

void SandboxModuleGenerator::generate() {
  M->setDataLayout(O.getDataLayout());
  M->setTargetTriple(O.getTargetTriple());
  M->setModuleInlineAsm(O.getModuleInlineAsm());

  SmallVector<GlobalVariable*, 10> varsToCopy;
  set<GlobalVariable*> globs;
  SmallVector<Function*, 10> funcsToCopy;
  SmallVector<GlobalAlias*, 10> aliasesToCopy;
  SmallSet<GlobalVariable*, 10> copiedVars;
  SmallSet<Function*, 10> copiedFuncs;
  SmallSet<GlobalAlias*, 10> copiedAliases;

  for (Value* I : S.getInternalGlobals()) {
    GlobalVariable* V = dyn_cast<GlobalVariable>(&*I);
    varsToCopy.push_back(V);
    globs.insert(V);
  }

  for (Module::global_iterator I = O.global_begin(), E = O.global_end();
      I != E; ++I) {
    GlobalVariable* GV = &*I;
    if (GV->hasSection()) {
      continue;
    }
    if (S.isAllowedToReadGlobalVar(GV)) {
      varsToCopy.push_back(GV);
      globs.insert(GV);
    }
    if (GV->isConstant()) {
      varsToCopy.push_back(GV);
      globs.insert(GV);
    }
    /* Copy over constants used by any of the sandboxed functions. */
    /* TODO only copy over constants. */
    for (User* U : GV->users()) {
      if (Instruction* Inst = dyn_cast<Instruction>(U)) {
        if (S.containsInstruction(Inst)) {
          varsToCopy.push_back(GV);
          globs.insert(GV);
        }
      } else if (Constant* C = dyn_cast<Constant>(U)) {
        for (User* CU : C->users()) {
          if (Instruction* CInst = dyn_cast<Instruction>(CU)) {
            if (S.containsInstruction(CInst)) {
              varsToCopy.push_back(GV);
              globs.insert(GV);
            }
          }
        }
      }
    }
  }

  for (Function* I : S.getFunctions()) {
    Function* F = &*I;
    funcsToCopy.push_back(F);
  }

  for (Function* I : S.getInternalFunctions()) {
    Function* F = &*I;
    funcsToCopy.push_back(F);
  }

  /* Calls to library functions are not copied, so get all functions which may be called
   * from the sandbox. */
  for (CallInst* I : S.getCalls()) {
    if (Function* F = CallGraphUtils::getDirectCallee(I)) {
      if (F->isDeclaration()) {
        funcsToCopy.push_back(F);
      }
    }
  }

  /* Copy over the constant variables. */
  while (!varsToCopy.empty()) {
    GlobalVariable* I = varsToCopy.back();
    varsToCopy.pop_back();
    if (!copiedVars.count(I)) {
      GlobalVariable* GV = new GlobalVariable(*M, I->getType()->getElementType(),
          I->isConstant(), I->getLinkage(), (Constant*) nullptr, I->getName(),
          (GlobalVariable*) nullptr, I->getThreadLocalMode(),
          I->getType()->getAddressSpace());
      GV->copyAttributesFrom(I);
      copiedVars.insert(I);
      VMap[I] = GV;
      
      if (I->hasInitializer()) {
        Constant* Init = I->getInitializer();
        if (GlobalVariable* V = dyn_cast<GlobalVariable>(Init)) {
          if (!copiedVars.count(V))
            varsToCopy.push_back(V);
        } else if (Function* F = dyn_cast<Function>(Init)) {
          if (!copiedFuncs.count(F))
            funcsToCopy.push_back(F);
        } else if (GlobalAlias* A = dyn_cast<GlobalAlias>(Init)) {
          if (!copiedAliases.count(A))
            aliasesToCopy.push_back(A);
        }
      }
    }
  }

  /* Copy over the sandboxed functions. */
  while (!funcsToCopy.empty()) {
    Function* I = funcsToCopy.back();
    funcsToCopy.pop_back();
    if (!copiedFuncs.count(I)) {
      Function* F = Function::Create(cast<FunctionType>(I->getType()->getElementType()),
          I->getLinkage(), I->getName(), M);
      F->copyAttributesFrom(I);
      copiedFuncs.insert(I);
      VMap[I] = F;
    }
  }

  /* Copy over the aliases. */
  while (!aliasesToCopy.empty()) {
    GlobalAlias* I = aliasesToCopy.back();
    aliasesToCopy.pop_back();
    if (!copiedAliases.count(I)) {
      PointerType* PTy = cast<PointerType>(I->getType());
      GlobalAlias* GA = GlobalAlias::create(PTy, I->getLinkage(), I->getName(), M);
      GA->copyAttributesFrom(I);
      copiedAliases.insert(I);
      VMap[I] = GA;
    }
  }

  /* Copy over global variable initializers. */
  for (GlobalVariable* I : copiedVars) {
    GlobalVariable* GV = cast<GlobalVariable>(VMap[I]);
    if (I->hasInitializer()) {
      GV->setInitializer(MapValue(I->getInitializer(), VMap));
    }
  }

  /* Copy over function bodies. */
  for (Function* I : copiedFuncs) {
    Function* F = cast<Function>(VMap[I]);
    if (!I->isDeclaration()) {
      Function::arg_iterator DestI = F->arg_begin();
      for (Function::const_arg_iterator J = I->arg_begin(); J != I->arg_end(); ++J) {
        DestI->setName(J->getName());
        VMap[J] = DestI++;
      }

      SmallVector<ReturnInst*, 8> Returns;
      CloneFunctionInto(F, I, VMap, /*ModuleLevelChanges=*/true, Returns);
    }

  }

  /* Copy over the aliasees. */
  for (GlobalAlias* I : copiedAliases) {
    GlobalAlias* GA = cast<GlobalAlias>(VMap[I]);
    if (const Constant* C = I->getAliasee())
      GA->setAliasee(MapValue(C, VMap));
  }

  SboxGenUtils::insertSandboxFunctions(*M);
}

void SandboxModuleGenerator::generateMainFunction() {
  LLVMContext& C = M->getContext();
  IRBuilder<> B(C);
  Serializer R(*M);

  /* Useful types */
  FunctionType* mainType = TypeBuilder<int(int, char**), false>::get(C);
  FunctionType* printfType = TypeBuilder<int(char*, ...), false>::get(C);
  IntegerType* i64 = Type::getInt64Ty(C);
  IntegerType* i32 = Type::getInt32Ty(C);

  /* Useful functions */
  Function* F = cast<Function>(M->getOrInsertFunction("main", mainType));

  Function* capEnter = M->getFunction("cap_enter");
  Function* dispatch = M->getFunction("soaap_dispatch");
  Function* exitFunc = M->getFunction("exit");
  Function* nvlistRecv = M->getFunction("nvlist_recv");
  Function* nvlistSend = M->getFunction("nvlist_send");
  Function* errFunc = M->getFunction("err");
  Function* printfFunc = cast<Function>(M->getOrInsertFunction("printf", printfType));
  Function* errnoFunc = M->getFunction("__error");

  /* Useful constants. */
  Value* Zero = ConstantInt::get(i32, 0);
  Value* One = ConstantInt::get(i32, 1);
  Value* Two = ConstantInt::get(i32, 2);

  /* Basic blocks */
  std::string prefix = "soaap_gen_main";
  BasicBlock* mainBlock = BasicBlock::Create(C, prefix, F);
  BasicBlock* mainArgsErrBlock = BasicBlock::Create(C, prefix + "_args_err", F);
  BasicBlock* mainArgsOkBlock = BasicBlock::Create(C, "soaap_gen_main_args_ok", F);
  BasicBlock* capEnterErrBlock = BasicBlock::Create(C, "soaap_gen_cap_enter_err", F);
  BasicBlock* mainRecvLoopBlock = BasicBlock::Create(C, "soaap_gen_main_recv_loop", F);
  BasicBlock* mainRecvErrBlock = BasicBlock::Create(C, "soaap_gen_main_recv_err", F);
  BasicBlock* mainRecvOkBlock = BasicBlock::Create(C, "soaap_gen_main_recv_ok", F);
  BasicBlock* reqTypeTermBlock = BasicBlock::Create(C, "soaap_gen_main_req_term", F);
  BasicBlock* reqTypeFuncBlock = BasicBlock::Create(C, "soaap_gen_main_req_func", F);
  BasicBlock* reqTypeErrBlock = BasicBlock::Create(C, "soaap_gen_main_req_err", F);
  BasicBlock* mainSendErrBlock = BasicBlock::Create(C, "soaap_gen_main_send_err", F);
  BasicBlock* mainSendOkBlock = BasicBlock::Create(C, "soaap_gen_main_send_ok", F);

  B.SetInsertPoint(mainBlock);

  /* Create a global variable for storing the main process socket fd. */

  Function::arg_iterator mainArgs = F->arg_begin();
  Value* argc = mainArgs++;
  argc->setName("argc");
  Value* argcIsTwo = B.CreateICmpEQ(argc, Two);
  B.CreateCondBr(argcIsTwo, mainArgsOkBlock, mainArgsErrBlock);

  B.SetInsertPoint(mainArgsOkBlock);
  Value* argv = mainArgs++;
  argv->setName("argv");

  /*
   * Get the first character of the first string passed to the main.
   * This is the file descriptor for the socket.
   */
  Value* fd = B.CreateGEP(argv, ArrayRef<Value*>(One));
  fd = B.CreateLoad(B.CreateLoad(fd));
  fd = B.CreateIntCast(fd, Type::getInt32Ty(C), false);
  B.CreateStore(fd, mainFd);

  /* Enter capability mode. */
  Value* capEnterErr = B.CreateCall(capEnter);
  Value* errnoVal = B.CreateCall(errnoFunc);
  errnoVal = B.CreateLoad(errnoVal);
  capEnterErr = B.CreateICmpSLT(capEnterErr, Zero);
  errnoVal = B.CreateICmpNE(errnoVal, ConstantInt::get(i32, ENOSYS));
  capEnterErr = B.CreateAnd(capEnterErr, errnoVal);
  B.CreateCondBr(capEnterErr, capEnterErrBlock, mainRecvLoopBlock);

  B.SetInsertPoint(mainRecvLoopBlock);

  /* Receive a command over the socket. */
  Value* request = B.CreateCall(nvlistRecv, ArrayRef<Value*>({fd, Zero}));

  B.CreateCondBr(B.CreateIsNotNull(request), mainRecvOkBlock, mainRecvErrBlock);

  B.SetInsertPoint(mainRecvOkBlock);

  /* Distinguish between request types */
  Value* reqType = R.nvlistGetNumber(B, request, MESSAGE_TYPE);
  SwitchInst* requestSwitch = B.CreateSwitch(reqType, reqTypeErrBlock, 2);
  requestSwitch->addCase(ConstantInt::get(i64, TERMINATE_SANDBOX, false),
      reqTypeTermBlock);
  requestSwitch->addCase(ConstantInt::get(i64, FUNCTION_CALL, false), reqTypeFuncBlock);

  /* Case: terminate sandbox. */
  B.SetInsertPoint(reqTypeTermBlock);
  B.CreateCall(exitFunc, Zero);
  B.CreateUnreachable();

  /* Case: function call. */
  B.SetInsertPoint(reqTypeFuncBlock);
  assert(dispatch && "Creating main for a sandbox without a dispatch function.");
  Value* nvl = B.CreateCall(dispatch, ArrayRef<Value*>({request}));

  /* Send a reply. */
  Value* retTypeField = B.CreateGlobalStringPtr(StringRef(MESSAGE_TYPE));
  B.CreateCall(M->getFunction("nvlist_add_number"), ArrayRef<Value*>({nvl, retTypeField,
        ConstantInt::get(Type::getInt64Ty(C), RETURN)}));
  Value* response = B.CreateCall(nvlistSend, ArrayRef<Value*>({fd, nvl}));
  Value* sendIsOk = B.CreateICmpSGE(response, Zero);
  B.CreateCondBr(sendIsOk, mainSendOkBlock, mainSendErrBlock);

  B.SetInsertPoint(mainSendOkBlock);
  B.CreateBr(mainRecvLoopBlock);

  /* Handle errors. */
  B.SetInsertPoint(mainArgsErrBlock);
  Value* errStr = B.CreateGlobalStringPtr(StringRef("incorrect args"));
  B.CreateCall(errFunc, ArrayRef<Value*>({One, errStr}));
  B.CreateUnreachable();

  B.SetInsertPoint(capEnterErrBlock);
  errStr = B.CreateGlobalStringPtr(StringRef("unable to enter capability mode."));
  B.CreateCall(errFunc, ArrayRef<Value*>({One, errStr}));
  B.CreateUnreachable();

  B.SetInsertPoint(mainRecvErrBlock);
  errStr = B.CreateGlobalStringPtr(StringRef("nvlist_recv"));
  B.CreateCall(errFunc, ArrayRef<Value*>({One, errStr}));
  B.CreateUnreachable();

  B.SetInsertPoint(mainSendErrBlock);
  errStr = B.CreateGlobalStringPtr(StringRef("nvlist_send"));
  B.CreateCall(errFunc, ArrayRef<Value*>({One, errStr}));
  B.CreateUnreachable();

  B.SetInsertPoint(reqTypeErrBlock);
  errStr = B.CreateGlobalStringPtr(StringRef("Invalid request type"));
  B.CreateCall(errFunc, ArrayRef<Value*>({One, errStr}));
  B.CreateUnreachable();
}

void SandboxModuleGenerator::generateDispatchFunction() {
  LLVMContext& C = M->getContext();
  IRBuilder<> B(C);
  Serializer R(*M);
  Type* i64 = Type::getInt64Ty(C);

  FunctionType* printfType = TypeBuilder<int(char*, ...), false>::get(C);
  Function* printfFunc = cast<Function>(M->getOrInsertFunction("printf", printfType));

  InputAnnotationMap inputMap = S.getAnnotatedInputs();

  /* Useful functions. */
  Function* capRightsInit = M->getFunction("__cap_rights_init");
  Function* capRightsLimit = M->getFunction("cap_rights_limit");
  Function* errFunc = M->getFunction("err");
  Function* errnoFunc = M->getFunction("__error");
  Function* exitFunc = M->getFunction("exit");
  Function* fdopenFunc = M->getFunction("fdopen");

  Type* nvlistPtrTy = M->getFunction("nvlist_create")->getReturnType();
  Type* capRightsPtrTy = capRightsInit->getReturnType();

  std::string DName = "soaap_dispatch";
  FunctionType* dispatchType = FunctionType::get(nvlistPtrTy, ArrayRef<Type*>(
        {nvlistPtrTy}), false);
  Function* D = cast<Function>(M->getOrInsertFunction(StringRef(DName), dispatchType));

  Function::arg_iterator args = D->arg_begin();
  Value* request = args++;
  request->setName("request");

  /* Useful types. */
  Type* i1 = Type::getInt1Ty(C);
  Type* i32 = Type::getInt32Ty(C);
  Type* i64ptr = PointerType::get(i64, 0);

  Value* Zero = ConstantInt::get(Type::getInt32Ty(C), 0);
  Value* One = ConstantInt::get(Type::getInt32Ty(C), 1);
  Value* Zeroi8 = ConstantInt::get(Type::getInt8Ty(C), 0);
  Value* Onei8 = ConstantInt::get(Type::getInt8Ty(C), 1);

  BasicBlock* dispatchEntry = BasicBlock::Create(C, DName + "_entry", D);
  BasicBlock* errorBlock = BasicBlock::Create(C, DName + "_error", D);

  B.SetInsertPoint(dispatchEntry);
  Value* reqAddr = B.CreateAlloca(nvlistPtrTy);
  Value* resAddr = B.CreateAlloca(nvlistPtrTy);
  Value* funcAddr = B.CreateAlloca(i64);

  /* Allocate space for all arguments except for buffers. */
  std::map<Value*, Value*> allocatedArgs;
  for (Function* F : S.getEntryPoints()) {
      for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
      Value* argVal = dyn_cast<Value>(A);
      Type* argTy = A->getType();
      if (!inputMap.count(argVal) && argTy->isIntegerTy() &&
          argTy->getIntegerBitWidth() < 64) {
        allocatedArgs[argVal] = B.CreateAlloca(i64);
      } else {
        allocatedArgs[argVal] = B.CreateAlloca(argTy);
      }
    }
  }

  B.CreateStore(request, reqAddr);
  request = B.CreateLoad(reqAddr);

  /* Create the response. */
  Value* response = R.nvlistCreate(B);
  B.CreateStore(response, resAddr);
  response = B.CreateLoad(resAddr);

  /* Get the function code. */
  Value* func = R.nvlistGetNumber(B, request, FUNCTION_CODE);
  SwitchInst* dispatchTable = B.CreateSwitch(func, errorBlock,
      S.getEntryPoints().size());

  for (Function* F : S.getEntryPoints()) {
    /* Actual dispatch option. */
    BasicBlock* caseBlock = BasicBlock::Create(C,
        "soaap_dispatch_case_" + F->getName().str(), D);
    dispatchTable->addCase(
        ConstantInt::get(TypeBuilder<uint64_t, false>::get(C),
          S.getIDForEntryPoint(F)),
        caseBlock);
    B.SetInsertPoint(caseBlock);

    /* Build a map of pointers from linked args to ints */
    std::set<Value*> linkedArgs;
    for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
      Value* V = dyn_cast<Value>(A);
      if (inputMap.count(V) && inputMap[V].type == InputType::Buffer) {
        linkedArgs.insert(inputMap[V].linkedArg);
      }
    }

    std::map<Value*, unsigned> linkedArgIdxs;
    unsigned argIdx = 0;
    for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
      Value* V = dyn_cast<Value>(A);
      if (linkedArgs.count(V)) {
        linkedArgIdxs[V] = argIdx;
      }
      ++argIdx;
    }

    /* TODO skip linked arguments (unless the linked idx < current idx), in
     * which case skip loading a linked arg. */
    argIdx = 0;
    for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
      Value* V = dyn_cast<Value>(A);
      if (inputMap.count(V) && inputMap[V].access == InputAccess::Out) {
        ++argIdx;
        continue;
      }

      InputAnnotation* annotation = NULL;
      Value* linkedArgPtr = NULL;
      if (inputMap.count(V)) {
        annotation = &inputMap[V];
        if (annotation->type == InputType::Buffer) {
          unsigned linkedArgIdx = linkedArgIdxs[annotation->linkedArg];
          linkedArgPtr = allocatedArgs[annotation->linkedArg];
          R.deserializeArgument(B, D, linkedArgIdx, reqAddr, linkedArgPtr, NULL, NULL);
        }
      }

      Type* valTy = V->getType();
      Value* ptr = allocatedArgs[V];
      if (inputMap.count(V) && inputMap[V].type == InputType::FileDescriptor &&
          valTy->isPointerTy()) {
        ptr = B.CreateAlloca(Type::getInt32Ty(C));
      } else if (inputMap.count(V) && inputMap[V].type == InputType::Pointer) {
        Value* alloc = B.CreateAlloca(valTy->getPointerElementType());
        B.CreateStore(alloc, ptr);
      }
      
      R.deserializeArgument(B, D, argIdx, reqAddr, ptr, annotation, linkedArgPtr);

      if (inputMap.count(V) && inputMap[V].type == InputType::FileDescriptor) {
        Value* fd = B.CreateLoad(ptr);
        if (valTy->isPointerTy()) {
          bool reading = S.getCapStrings()[V].count("read") > 0;
          bool writing = S.getCapStrings()[V].count("write") > 0;
          std::string modeStr;
          if (reading && writing) {
            modeStr = "r+";
          } else if (writing) {
            modeStr = "wb";
          } else if (reading) {
            modeStr = "rb";
          } else {
            errs() << "Incorrect mode for file descriptor " << V->getName().str() <<
              "\n";
            exit(1);
          }
          B.CreateStore(R.fdopenFunc(B, fd, modeStr), allocatedArgs[V]);
          limitDescriptorRights(B, V, fd, F->getName().str(), D);
        }
      } else if (valTy->isPointerTy() && valTy->getPointerElementType()->isStructTy()) {
        StructType* ST = dyn_cast<StructType>(valTy->getPointerElementType());
        unsigned elemIdx = 0;
        for (StructType::element_iterator I = ST->element_begin(), E = ST->element_end();
            I != E; ++I, ++elemIdx) {
          Type* ET = *I;
          if (!ET->isPointerTy() || !ET->getPointerElementType()->isFunctionTy()) {
            continue;
          }
          std::string methodName = "soaap_sbox_method_" + ST->getName().str() + "_" +
            std::to_string(elemIdx);
          Function* method = generateStructMethod(ST, elemIdx, argIdx);
          B.CreateStore(method, B.CreateStructGEP(ST, B.CreateLoad(allocatedArgs[V]),
                elemIdx));
        }
      }

      ++argIdx;
    }

    for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
      Value* argVal = dyn_cast<Value>(A);
      if (inputMap.count(argVal) && inputMap[argVal].access == InputAccess::Out) {
        Value* lenVal = inputMap[argVal].linkedArg;
        Value* len = B.CreateLoad(allocatedArgs[lenVal]);
        Value* arr = B.CreateAlloca(A->getType()->getArrayElementType(), len);
        B.CreateStore(arr, allocatedArgs[argVal]);
      }
    }

    std::vector<Value*> args = {};
    for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
      Value* argVal = dyn_cast<Value>(A);
      Value* arg = B.CreateLoad(allocatedArgs[argVal]);
      Type* argTy = A->getType();
      if (!inputMap.count(argVal) && argTy->getIntegerBitWidth() < 64) {
        arg = B.CreateIntCast(arg, argTy, false);
      }
      args.push_back(arg);
    }

    /* Get the global variables. */
    unsigned globIdx = 0;
    for (Value* V : S.getLoadedGlobalVars()) {
      R.deserializeGlobal(B, F, globIdx, reqAddr, VMap[V]);
      globIdx++;
    }

    Value* retVal = B.CreateCall(dyn_cast<Function>(VMap[F]), ArrayRef<Value*>(args));

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

      Value* arg = B.CreateLoad(allocatedArgs[V]);
      InputAnnotation* annotation = NULL;
      if (inputMap.count(V)) {
        annotation = &inputMap[V];
      }
      R.serializeArgument(B, D, argIdx, arg, resAddr, annotation, allocatedArgs);
      ++argIdx;
    }

    /* Marshall the modified globals */
    globIdx = 0;
    for (Value* V : S.getStoredGlobalVars()) {
      R.serializeGlobal(B, F, globIdx, VMap[V], resAddr, allocatedArgs);
      globIdx++;
    }

    Value* debugOut = B.CreateGlobalStringPtr("Serialising the retunr value \n");
    B.CreateCall(M->getFunction("printf"), ArrayRef<Value*>({debugOut}));

    Type* retTy = F->getReturnType();
    if (retTy->isIntegerTy() && retTy->getIntegerBitWidth() <= 64) {
      R.nvlistAddNumber(B, response, RETURN_VALUE, retVal);
    } else if (!retTy->isVoidTy()) {
      /* TODO handle errors properly */
      outs() << "Unsupported return type.\n";
      retTy->dump();
    }

    debugOut = B.CreateGlobalStringPtr("Serialised the retunr value\n");
    B.CreateCall(M->getFunction("printf"), ArrayRef<Value*>({debugOut}));

    R.nvlistAddBool(B, response, SBOX_ERROR, Zeroi8);
    B.CreateRet(response);
  }

  B.SetInsertPoint(errorBlock);
  response = B.CreateLoad(resAddr);
  R.nvlistAddBool(B, response, SBOX_ERROR, Onei8);
  B.CreateRet(response);

}

void SandboxModuleGenerator::limitDescriptorRights(IRBuilder<>& B,
    Value* arg, Value* fd, std::string funcName, Function* dispatch) {
  LLVMContext& C = M->getContext();

  Function* capRightsInit = M->getFunction("__cap_rights_init");
  Function* capRightsLimit = M->getFunction("cap_rights_limit");
  Function* errnoFunc = M->getFunction("__error");
  Function* errFunc = M->getFunction("err");

  Type* i32 = Type::getInt32Ty(C);
  Type* i64 = Type::getInt64Ty(C);
  Type* capRightsPtrTy = capRightsInit->getReturnType();

  Value* i32Zero = ConstantInt::get(i32, 0);
  Value* i64Zero = ConstantInt::get(i64, 0);
  Value* i32One = ConstantInt::get(i32, 1);

  std::string name = arg->getName().str();
  BasicBlock* capLimitErrBlock = BasicBlock::Create(C,
      "soaap_gen_func_" + funcName + "_fd_limit_" + name + "_err", dispatch);
  BasicBlock* capLimitOkBlock = BasicBlock::Create(C,
      "soaap_gen_func_" + funcName + "_fd_limit_" + name + "_ok", dispatch);

  /* Initialise the cap_rights_t struct. */
  Value* rightsPtr = B.CreateAlloca(capRightsPtrTy->getPointerElementType());
  std::vector<Value*> limitArgs = {i32Zero, rightsPtr};
  Capsicum cap;
  std::vector<uint64_t> rights = cap.rightsForStrings(S.getCapStrings()[arg]);
  for (uint64_t R : rights) {
    limitArgs.push_back(ConstantInt::get(i64, R));
  }
  limitArgs.push_back(i64Zero);
  B.CreateCall(capRightsInit, ArrayRef<Value*>(limitArgs));

  /* Limit the rights on the file descriptor. */
  Value* capLimitErr = B.CreateCall(capRightsLimit, ArrayRef<Value*>({fd, rightsPtr}));
  Value* errnoVal = B.CreateCall(errnoFunc);
  errnoVal = B.CreateLoad(errnoVal);
  errnoVal = B.CreateICmpNE(errnoVal, ConstantInt::get(i32, ENOSYS));
  capLimitErr = B.CreateICmpSLT(capLimitErr, i32Zero);
  capLimitErr = B.CreateAnd(capLimitErr, errnoVal);
  B.CreateCondBr(capLimitErr, capLimitErrBlock, capLimitOkBlock);

  B.SetInsertPoint(capLimitErrBlock);
  Value* errStr = B.CreateGlobalStringPtr(StringRef("unable to limit rights for %d"));
  B.CreateCall(errFunc, ArrayRef<Value*>({i32One, errStr, fd}));
  B.CreateUnreachable();

  B.SetInsertPoint(capLimitOkBlock);
}

Function* SandboxModuleGenerator::generateStructMethod(StructType* ST, unsigned elemIdx, 
    unsigned argIdx) {
  LLVMContext& C = M->getContext();
  IRBuilder<> B(C);

  Type* i32 = Type::getInt32Ty(C);
  Value* i32Zero = ConstantInt::get(i32, 0, false);

  Type* T = ST->getElementType(elemIdx);
  if (!T->isPointerTy() || !T->getPointerElementType()->isFunctionTy()) {
    errs() << "Struct element is not a function pointer.\n";
    exit(1);
  }
  FunctionType* FT = dyn_cast<FunctionType>(T->getPointerElementType());

  std::string methodName = "soaap_sbox_method_" + ST->getName().str() + "_" +
    std::to_string(elemIdx);
  Function* F = cast<Function>(M->getOrInsertFunction(methodName, FT));

  BasicBlock* methodEntry = BasicBlock::Create(C, methodName + "_entry", F);
  BasicBlock* methodSendOkBlock = BasicBlock::Create(C, methodName + "_send_ok", F);
  BasicBlock* methodSendErrBlock = BasicBlock::Create(C, methodName + "_send_err", F);
  BasicBlock* methodRecvOkBlock = BasicBlock::Create(C, methodName + "_recv_ok", F);
  BasicBlock* methodRecvErrBlock = BasicBlock::Create(C, methodName + "_recv_err", F);
  
  Serializer R(*M);

  B.SetInsertPoint(methodEntry);
  std::map<Value*, Value*> allocatedArgs = SboxGenUtils::allocateArguments(B, F);
  SboxGenUtils::storeArguments(B, F, allocatedArgs);
  Value* nvl = R.nvlistCreate(B);
  Value* nvlAddr = B.CreateAlloca(nvl->getType());
  B.CreateStore(nvl, nvlAddr);

  unsigned I = 0;
  for (Argument* A = F->arg_begin(); A != F->arg_end(); A++) {
    Value* V = dyn_cast<Value>(A);
    Value* arg = B.CreateLoad(allocatedArgs[V]);
    R.serializeArgument(B, F, I, arg, nvlAddr, NULL, allocatedArgs);
    ++I;
  }

  /* Marshall the modified globals */
  unsigned globIdx = 0;
  for (Value* V : S.getStoredGlobalVars()) {
    R.serializeGlobal(B, F, globIdx, VMap[V], nvlAddr, allocatedArgs);
    globIdx++;
  }

  /* Message type */
  Value* name = B.CreateGlobalStringPtr(StringRef(MESSAGE_TYPE));
  B.CreateCall(M->getFunction("nvlist_add_number"), ArrayRef<Value*>(
        {nvl, name, ConstantInt::get(Type::getInt64Ty(C), METHOD_INVOCATION)}));

  /* Error status */
  name = B.CreateGlobalStringPtr(StringRef(SBOX_ERROR));
  B.CreateCall(M->getFunction("nvlist_add_bool"), ArrayRef<Value*>(
        {nvl, name, ConstantInt::get(Type::getInt8Ty(C), 0)}));

  /* Argument index in the original function. */
  name = B.CreateGlobalStringPtr(StringRef("arg_idx"));
  B.CreateCall(M->getFunction("nvlist_add_number"), ArrayRef<Value*>(
        {nvl, name, ConstantInt::get(Type::getInt64Ty(C), argIdx)}));

  /* Element index of the function in the struct. */
  name = B.CreateGlobalStringPtr(StringRef("elem_idx"));
  B.CreateCall(M->getFunction("nvlist_add_number"), ArrayRef<Value*>(
        {nvl, name, ConstantInt::get(Type::getInt64Ty(C), elemIdx)}));

  /* Send the nvlist and wait for the reply. */
  Value* fd = B.CreateLoad(mainFd);
  Value* sendIsOk = B.CreateCall(M->getFunction("nvlist_send"), ArrayRef<Value*>(
      {fd, nvl}));
  sendIsOk = B.CreateICmpSGE(sendIsOk, i32Zero);
  B.CreateCondBr(sendIsOk, methodSendOkBlock, methodSendErrBlock);

  B.SetInsertPoint(methodSendOkBlock);
  Value* response = B.CreateCall(M->getFunction("nvlist_recv"), ArrayRef<Value*>(
      {fd, i32Zero}));
  B.CreateCondBr(B.CreateIsNotNull(response), methodRecvOkBlock, methodRecvErrBlock);

  B.SetInsertPoint(methodRecvOkBlock);

  /* Get the global variables. */
  globIdx = 0;
  for (Value* V : S.getLoadedGlobalVars()) {
    R.deserializeGlobal(B, F, globIdx, nvlAddr, VMap[V]);
    globIdx++;
  }

  /* TODO support non-void return types */
  B.CreateRetVoid();

  B.SetInsertPoint(methodRecvErrBlock);
  generateErrorMessage(C, B, "method nvlist_recv");

  B.SetInsertPoint(methodSendErrBlock);
  generateErrorMessage(C, B, "method nvlist_send");


  return F;
}


void SandboxModuleGenerator::generateErrorMessage(LLVMContext& C, IRBuilder<>& B,
    std::string message) {
  Value* errStr = B.CreateGlobalStringPtr(StringRef(message));
  B.CreateCall(M->getFunction("err"), ArrayRef<Value*>(
        {ConstantInt::get(Type::getInt32Ty(C), 1), errStr}));
  B.CreateUnreachable();
}
