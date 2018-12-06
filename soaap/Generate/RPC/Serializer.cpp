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

#include "Generate/RPC/Serializer.h"

#include "llvm/Support/raw_ostream.h"

#define IN_SOAAP_GENERATOR
#include "soaap_gen.h"

using namespace soaap;

void Serializer::serializeArgument(IRBuilder<>& B, Function* F, unsigned idx, Value* V,
    Value* nvlAddr, InputAnnotation* annotationData,
    std::map<Value*, Value*>& allocatedArgs) {
  std::string name = std::to_string(idx) + SBOX_ARG;
  serialize(B, F, name, V, nvlAddr, annotationData, allocatedArgs);
}

void Serializer::serializeGlobal(IRBuilder<>& B, Function* F, unsigned idx, Value* V,
    Value* nvlAddr, std::map<Value*, Value*>& allocatedArgs) {
  Type* valTy = V->getType();
  if (valTy->isPointerTy() && valTy->getPointerElementType()->isIntegerTy()) {
    std::string name = std::to_string(idx) + SBOX_GLOBAL;
    serialize(B, F, name, V, nvlAddr, NULL, allocatedArgs);
  }
}

void Serializer::serialize(IRBuilder<>& B, Function* F, std::string name, Value* V,
    Value* nvlAddr, InputAnnotation* annotationData,
    std::map<Value*, Value*>& allocatedArgs) {
  LLVMContext& C = M.getContext();
  Type* valTy = V->getType();
  Value* nvl = B.CreateLoad(nvlAddr);
  if (valTy->isIntegerTy()) {
    if (!annotationData) {
      nvlistAddNumber(B, nvl, name, V);
    } else {
      if (annotationData->type != InputType::FileDescriptor) {
        errs() << "\n";
        errs() << " [ERROR] Unexpected annotation for integer value " << V->getName() <<
          "\n";
        exit(1);
      }
      nvlistAddDescriptor(B, nvl, name, V);
    }
  } else if (valTy->isPointerTy()) {
    std::string funcName = F->getName().str();
    BasicBlock* notNullPtrBr = BasicBlock::Create(C, "soaap_gen_" + funcName + 
        "_opt_ptr_" + name + "_not_null", F);
    BasicBlock* nullPtrDoneBr = BasicBlock::Create(C, "soaap_gen_" + funcName +
        "_opt_ptr_" + name + "_done", F);

    if (!annotationData || annotationData->optional) {
      BasicBlock* nullPtrBr = BasicBlock::Create(C, "soaap_gen_" + funcName +
          "_opt_ptr_" + name + "_null", F);
      B.CreateCondBr(B.CreateIsNotNull(V), notNullPtrBr, nullPtrBr);

      B.SetInsertPoint(nullPtrBr);
      nvl = B.CreateLoad(nvlAddr);
      nvlistAddNull(B, nvl, name);
      B.CreateBr(nullPtrDoneBr);
    } else {
      B.CreateBr(notNullPtrBr);
    }

    B.SetInsertPoint(notNullPtrBr);
    nvl = B.CreateLoad(nvlAddr);
    if (!annotationData || annotationData->type == InputType::Pointer) {
      if (valTy->getPointerElementType()->isStructTy()) {
        StructType* ST = dyn_cast<StructType>(valTy->getPointerElementType());
        Value* structNvl = nvlistCreate(B);
        Value* structNvlAddr = B.CreateAlloca(structNvl->getType());
        B.CreateStore(structNvl, structNvlAddr);
        int elemIdx = 0;
        for (StructType::element_iterator I = ST->element_begin(), E = ST->element_end();
            I != E; ++I, ++elemIdx) {
          Type* T = *I;
          if (T->isPointerTy() && T->getPointerElementType()->isFunctionTy()) {
            continue;
          }
          std::string elemName = name + "_" + std::to_string(elemIdx);
          serialize(B, F, elemName, B.CreateLoad(B.CreateStructGEP(ST, V, elemIdx)),
              structNvlAddr, NULL, allocatedArgs);
        }
        nvlistAddNvlist(B, nvl, name, structNvl);
      } else {
        serialize(B, F, name, B.CreateLoad(V), nvlAddr, NULL, allocatedArgs);
      }
    } else if (annotationData->type == InputType::FileDescriptor) {
      Value* fd = filenoFunc(B, V);
      nvlistAddDescriptor(B, nvl, name, fd);
    } else if (annotationData->type == InputType::Buffer) {
      if (!valTy->getPointerElementType()->isIntegerTy() ||
          valTy->getPointerElementType()->getIntegerBitWidth() > 64) {
        errs() << "\n";
        errs() << " [ERROR] The buffer value " << V->getName() <<
          " is not an integer buffer.\n";
        exit(1);
      }
      /* TODO needs a pointer to allocated args struct. */
      Value* len = B.CreateLoad(allocatedArgs[annotationData->linkedArg]);
      if (valTy->getPointerElementType()->isIntegerTy(64)) {
        nvlistAddNumberArray(B, nvl, name, V, len);
      } else {
        unsigned width = valTy->getPointerElementType()->getIntegerBitWidth();
        if (width % 8 != 0) {
          errs() << "\n";
          errs() << " [ERROR] Buffer elements have to be aligned to 8 bits in buffer " <<
            V->getName() << "\n";
          exit(1);
        }
        Value* bufferSize = B.CreateIntCast(B.CreateMul(len,
              ConstantInt::get(len->getType(), width / 8, false)),
            Type::getInt64Ty(C), false);
        nvlistAddBinary(B, nvl, name, V, bufferSize);
      }
    } else if (annotationData->type == InputType::NullTerminatedBuffer) {
      if (!valTy->getPointerElementType()->isIntegerTy(8)) {
        errs() << "\n";
        errs() << " [ERROR] Null terminated buffer " << V->getName() <<
          " is not a string.\n";
        exit(1);
      }
      nvlistAddString(B, nvl, name, V);
    } else {
      errs() << "\n";
      errs() << " [ERROR] Found an unsupported value " << V->getName() << "\n";
      valTy->dump();
      exit(1);
    }

    B.CreateBr(nullPtrDoneBr);
    B.SetInsertPoint(nullPtrDoneBr);
  } else if (valTy->isStructTy()) {
    StructType* ST = dyn_cast<StructType>(valTy);
    Value* structNvl = nvlistCreate(B);
    Value* structNvlAddr = B.CreateAlloca(structNvl->getType());
    B.CreateStore(structNvl, structNvlAddr);
    Value* structPtr = B.CreateAlloca(valTy);
    B.CreateStore(V, structPtr);
    int elemIdx = 0;
    for (StructType::element_iterator I = ST->element_begin(), E = ST->element_end();
        I != E; ++I, ++elemIdx) {
      Type* T = *I;
      if (T->isPointerTy() && T->getPointerElementType()->isFunctionTy()) {
        continue;
      }
      std::string elemName = name + "_" + std::to_string(elemIdx);
      serialize(B, F, elemName, B.CreateLoad(B.CreateStructGEP(ST, structPtr, elemIdx)),
          structNvlAddr, NULL, allocatedArgs);
    }
    nvlistAddNvlist(B, nvl, name, structNvl);
  } else if (valTy->isArrayTy()) {
    ArrayType* AT = dyn_cast<ArrayType>(valTy);
    Type* elemTy = valTy->getArrayElementType();
    if (!elemTy->isIntegerTy()) {
      errs() << "\n";
      errs() << " [ERROR] Found an unsupported value " << V->getName() << "\n";
      valTy->dump();
      exit(1);
    }
    unsigned width = elemTy->getIntegerBitWidth();
    unsigned numElems = AT->getNumElements();
    width = numElems * width / 8;
    Value* len = ConstantInt::get(Type::getInt64Ty(C), width, false);
    Value* arrPtr = B.CreateAlloca(valTy);
    B.CreateStore(V, arrPtr);
    nvlistAddBinary(B, nvl, name, B.CreatePointerCast(arrPtr, Type::getInt8PtrTy(C)), len);
  } else {
    errs() << "\n";
    errs() << " [ERROR] Found an unsupported value " << V->getName() << "\n";
    valTy->dump();
    exit(1);
  }
}

void Serializer::deserializeArgument(IRBuilder<>& B, Function* F, unsigned idx,
    Value* nvlAddr, Value* alloc, InputAnnotation* annotationData, Value* linkedArg) {
  std::string name = std::to_string(idx) + SBOX_ARG;
  deserialize(B, F, name, nvlAddr, alloc, annotationData, linkedArg);
}

void Serializer::deserializeGlobal(IRBuilder<>& B, Function* F, unsigned idx,
    Value* nvlAddr, Value* alloc) {
  Type* valTy = alloc->getType();
  if (valTy->isPointerTy() && valTy->getPointerElementType()->isIntegerTy()) {
    std::string name = std::to_string(idx) + SBOX_GLOBAL;
    deserialize(B, F, name, nvlAddr, alloc, NULL, NULL);
  }
}

void Serializer::deserialize(IRBuilder<>& B, Function* F, std::string name,
    Value* nvlAddr, Value* alloc, InputAnnotation* annotationData, Value* linkedArg) {
  LLVMContext& C = M.getContext();
  Type* ptrTy = alloc->getType()->getPointerElementType();
  // outs() << "Deserializing a value with index " << idx << "\n";
  // ptrTy->dump();
  Value* nvl = B.CreateLoad(nvlAddr);

  if (ptrTy->isIntegerTy()) {
    if (!annotationData || annotationData->type == InputType::Pointer) {
      if (ptrTy->getIntegerBitWidth() > 64) {
        errs() << "\n";
        errs() << " [ERROR] Integer value has to be 64 bits or fewer in width.\n";
        exit(1);
      }
      B.CreateStore(B.CreateIntCast(nvlistGetNumber(B, nvl, name), ptrTy, false), alloc);
    } else {
      if (annotationData->type != InputType::FileDescriptor) {
        errs() << "\n";
        errs() << " [ERROR] Unexpected annotation for integer value.\n";
        exit(1);
      }
      /* TODO don't forget to limit descriptor rights in module generator */
      B.CreateStore(nvlistGetDescriptor(B, nvl, name), alloc);
    }
  } else if (ptrTy->isPointerTy()) {
    std::string funcName = F->getName().str();
    Type* T = ptrTy->getPointerElementType();

    BasicBlock* notNullPtrBr = BasicBlock::Create(C, "soaap_gen_" + funcName +
        "_opt_ptr_" + name + "_not_null", F);
    BasicBlock* nullPtrDoneBr = BasicBlock::Create(C, "soaap_gen_" + funcName +
        "_opt_ptr_" + name + "_done", F);

    if (!annotationData || annotationData->optional) {
      BasicBlock* nullPtrBr = BasicBlock::Create(C, "soaap_gen_" + funcName +
          "_opt_ptr_" + name + "_null", F);
      Value* ptrIsNull = nvlistExistsNull(B, nvl, name);
      B.CreateCondBr(B.CreateIntCast(ptrIsNull, Type::getInt1Ty(C), false),
          nullPtrBr, notNullPtrBr);

      B.SetInsertPoint(nullPtrBr);
      B.CreateStore(ConstantPointerNull::get(dyn_cast<PointerType>(ptrTy)), alloc);
      B.CreateBr(nullPtrDoneBr);
    } else {
      B.CreateBr(notNullPtrBr);
    }

    B.SetInsertPoint(notNullPtrBr);
    Value* ptr = B.CreateLoad(alloc);
    nvl = B.CreateLoad(nvlAddr);
    if (annotationData && annotationData->type == InputType::FileDescriptor) {
      B.CreateStore(nvlistGetDescriptor(B, nvl, name), ptr);
    } else if (T->isStructTy()) {
      StructType* ST = dyn_cast<StructType>(T);
      Value* structNvl = nvlistGetNvlist(B, nvl, name);
      Value* structNvlAddr = B.CreateAlloca(structNvl->getType());
      B.CreateStore(structNvl, structNvlAddr);
      int elemIdx = 0;
      for (StructType::element_iterator I = ST->element_begin(), E = ST->element_end();
          I != E; ++I, ++elemIdx) {
        Type* ET = *I;
        if (ET->isPointerTy() && ET->getPointerElementType()->isFunctionTy()) {
          continue;
        }
        std::string elemName = name + "_" + std::to_string(elemIdx);
        deserialize(B, F, elemName, structNvlAddr, B.CreateStructGEP(ST, ptr, elemIdx),
            NULL, NULL);
      }
    } else if (!annotationData || annotationData->type == InputType::Pointer) {
      // Value* elemPtr = B.CreateAlloca(T);
      deserialize(B, F, name, nvlAddr, B.CreateLoad(alloc), NULL, NULL);
      // B.CreateStore(elemPtr, alloc);
    } else if (annotationData->type == InputType::Buffer) {
      if (linkedArg == NULL) {
        errs() << "A buffer needs a linked length argument\n";
        exit(1);
      }
      Value* out;
      if (T->isIntegerTy(64)) {
        B.CreateStore(nvlistGetNumberArray(B, nvl, name, linkedArg), alloc);
      } else {
        /* TODO handle non-char buffer case */
        B.CreateStore(nvlistGetBinary(B, nvl, name, linkedArg), alloc);
      }
    } else if (annotationData->type == InputType::NullTerminatedBuffer) {
      B.CreateStore(nvlistGetString(B, nvl, name), alloc);
    } else {
      errs() << "\n";
      errs() << " [ERROR] Found an unsupported value.\n";
      ptrTy->dump();
      exit(1);
    }

    B.CreateBr(nullPtrDoneBr);
    B.SetInsertPoint(nullPtrDoneBr);
  } else if (ptrTy->isStructTy()) {
    StructType* ST = dyn_cast<StructType>(ptrTy);
    Value* structNvl = nvlistGetNvlist(B, nvl, name);
    Value* structNvlAddr = B.CreateAlloca(structNvl->getType());
    B.CreateStore(structNvl, structNvlAddr);
    int elemIdx = 0;
    for (StructType::element_iterator I = ST->element_begin(), E = ST->element_end();
        I != E; ++I, ++elemIdx) {
      Type* ET = *I;
      if (ET->isPointerTy() && ET->getPointerElementType()->isFunctionTy()) {
        continue;
      }
      std::string elemName = name + "_" + std::to_string(elemIdx);
      deserialize(B, F, elemName, structNvlAddr, B.CreateStructGEP(ST, alloc, elemIdx),
          NULL, NULL);
    }
  } else if (ptrTy->isArrayTy()) {
    /* TODO be able to deserialize [8 x i32] and [48 x i8] */
  } else {
    errs() << "\n";
    errs() << " [ERROR] Found an unsupported value in " << F->getName() << " of type: \n";
    ptrTy->dump();
    exit(1);
  }
}

void Serializer::copyBuffer(IRBuilder<>& B, Function* F, Value* src, Value* dst,
    Value* len) {
  LLVMContext& C = M.getContext();
  std::string funcName = F->getName().str();
  BasicBlock* outBufLoop = BasicBlock::Create(C, "soaap_gen_" + funcName +
      "_out_buf_loop", F);
  BasicBlock* outBufDone = BasicBlock::Create(C, "soaap_gen_" + funcName +
      "_out_buf_done", F);

  Type* lenTy = len->getType();
  Value* Idx = B.CreateAlloca(lenTy);
  Value* i = ConstantInt::get(lenTy, 0, false);
  Value* One = ConstantInt::get(lenTy, 1, false);
  B.CreateStore(i, Idx);
  B.CreateCondBr(B.CreateICmpULT(i, len), outBufLoop, outBufDone);

  B.SetInsertPoint(outBufLoop);
  i = B.CreateLoad(Idx);
  Value* srcVal = B.CreateLoad(B.CreateGEP(src, ArrayRef<Value*>(i)));
  Value* dstPtr = B.CreateGEP(dst, ArrayRef<Value*>(i));
  B.CreateStore(srcVal, dstPtr);
  i = B.CreateAdd(i, One);
  B.CreateStore(i, Idx);
  B.CreateCondBr(B.CreateICmpULT(i, len), outBufLoop, outBufDone);

  B.SetInsertPoint(outBufDone);
}

Function* Serializer::getFunction(std::string name) {
  Function* F = M.getFunction(name);
  assert(F && "Function does not exist.");
  return F;
}

Value* Serializer::filenoFunc(IRBuilder<>& B, Value* V) {
  return B.CreateCall(getFunction("fileno"), ArrayRef<Value*>({V}));
}

Value* Serializer::fdopenFunc(IRBuilder<>& B, Value* V, std::string mode) {
  Value* M = B.CreateGlobalStringPtr(StringRef(mode));
  return B.CreateCall(getFunction("fdopen"), ArrayRef<Value*>({V, M}));
}

void Serializer::nvlistAddBinary(IRBuilder<>& B, Value* nvl, std::string name,
    Value* V, Value* L) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_binary"), ArrayRef<Value*>({nvl, N, V, L}));
}

void Serializer::nvlistAddBool(IRBuilder<>& B, Value* nvl, std::string name, Value* V) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_bool"), ArrayRef<Value*>({nvl, N, V}));
}

void Serializer::nvlistAddDescriptor(IRBuilder<>& B, Value* nvl, std::string name,
    Value* V) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_descriptor"), ArrayRef<Value*>({nvl, N, V}));
}

void Serializer::nvlistAddNull(IRBuilder<>& B, Value* nvl, std::string name) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_null"), ArrayRef<Value*>({nvl, N}));
}

void Serializer::nvlistAddNumber(IRBuilder<>& B, Value* nvl, std::string name, Value* V) {
  if (V->getType()->isPointerTy()) {
    outs() << "Calling nvlist add number with pointer type " << *V << "\n";
    exit(1);
  }
  unsigned width = V->getType()->getIntegerBitWidth();
  if (width > 64) {
    errs() << "\n";
    errs() << " [ERROR] Integer value " << V->getName() <<
      " must be 64 bits or fewer in width.\n";
    exit(1);
  }

  if (width < 64) {
    V = B.CreateIntCast(V, Type::getInt64Ty(M.getContext()), false);
  }

  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_number"), ArrayRef<Value*>({nvl, N, V}));
}

void Serializer::nvlistAddNumberArray(IRBuilder<>& B, Value* nvl, std::string name,
    Value* V, Value* L) {
  V = B.CreatePointerCast(V, Type::getInt64PtrTy(M.getContext()));
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_number_array"), ArrayRef<Value*>({nvl, N, V, L}));
}

void Serializer::nvlistAddNvlist(IRBuilder<>& B, Value* nvl, std::string name, Value* V) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_nvlist"), ArrayRef<Value*>({nvl, N, V}));
}

void Serializer::nvlistAddString(IRBuilder<>& B, Value* nvl, std::string name,
    Value* V) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  B.CreateCall(getFunction("nvlist_add_string"), ArrayRef<Value*>({nvl, N, V}));
}

Value* Serializer::nvlistCreate(IRBuilder<>& B) {
  return B.CreateCall(M.getFunction("nvlist_create"), ArrayRef<Value*>(
        {ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false)}));
}

Value* Serializer::nvlistExistsNull(IRBuilder<>& B, Value* nvl, std::string name) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_exists_null"), ArrayRef<Value*>({nvl, N}));
}

Value* Serializer::nvlistGetBinary(IRBuilder<>& B, Value* nvl, std::string name,
    Value* lenPtr) {
  if (!lenPtr->getType()->getPointerElementType()->isIntegerTy(64)) {
    Type* i64 = Type::getInt64Ty(M.getContext());
    Value* len = B.CreateLoad(lenPtr);
    lenPtr = B.CreateAlloca(i64);
    B.CreateStore(B.CreateIntCast(len, i64, false), lenPtr);
  }
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_get_binary"),
      ArrayRef<Value*>({nvl, N, lenPtr}));
}

Value* Serializer::nvlistGetDescriptor(IRBuilder<>& B, Value* nvl, std::string name) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_get_descriptor"), ArrayRef<Value*>({nvl, N}));
}

Value* Serializer::nvlistGetNumber(IRBuilder<>& B, Value* nvl, std::string name) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_get_number"), ArrayRef<Value*>({nvl, N}));
}

Value* Serializer::nvlistGetNumberArray(IRBuilder<>& B, Value* nvl, std::string name,
    Value* lenPtr) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_get_number_array"),
      ArrayRef<Value*>({nvl, N, lenPtr}));
}

Value* Serializer::nvlistGetNvlist(IRBuilder<>& B, Value* nvl, std::string name) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_get_nvlist"), ArrayRef<Value*>({nvl, N}));
}

Value* Serializer::nvlistGetString(IRBuilder<>& B, Value* nvl, std::string name) {
  Value* N = B.CreateGlobalStringPtr(StringRef(name));
  return B.CreateCall(getFunction("nvlist_get_string"), ArrayRef<Value*>({nvl, N}));
}
