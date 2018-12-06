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

#include "Util/StructUtils.h"

#include "Util/DebugUtils.h"
#define IN_SOAAP_GENERATOR
#include "soaap.h"
#include "soaap_gen.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace soaap;

StructVector findStructs(Module& M) {
  StructVector structs;
  GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations");
  if (!lga) return structs;
  // ConstantArray* lgaArray = dyn_cast<ConstantArray>(
  //     lga->getInitializer()->stripPointerCasts());
  // for (User::op_iterator I = lgaArray->op_begin(), E = lgaArray->op_end(); E != I; ++I) {
  //   ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(I->get());

  //   GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(
  //       lgaArrayElement->getOperand(1)->stripPointerCasts());
  //   ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(
  //       annotationStrVar->getInitializer());
  //   StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
  //   GlobalValue* annotatedVal = dyn_cast<GlobalValue>(
  //       lgaArrayElement->getOperand(0)->stripPointerCasts());
  //   if (!isa<StructType>(annotatedVal)) continue;
  //   StructType* st = dyn_cast<StructType>(annotatedVal);
  //   if (annotationStrArrayCString.startswith(CLASS)) {
  //     outs() << INDENT_1 << "Found a class " << st->getName() << "\n";
  //     outs() << INDENT_2 << "Annotation string: " << annotationStrArrayCString << "\n";
  //     structs.push_back(st);
  //   } else {
  //     errs() << INDENT_1 << "Invalid annotation for struct " << st->getName() << "\n";
  //     errs() << INDENT_2 << "Annotation string: " << annotationStrArrayCString << "\n";
  //     exit(1);
  //   }
  // }
  return structs;
}
