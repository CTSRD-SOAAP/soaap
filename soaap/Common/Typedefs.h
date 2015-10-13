/*
 * Copyright (c) 2013-2015 Khilan Gudka
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

#ifndef SOAAP_COMMON_TYPEDEFS_H
#define SOAAP_COMMON_TYPEDEFS_H

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"

#include <list>
#include <map>
#include <vector>
#include <set>
#include <unordered_map>
#include <utility>

#include "Analysis/InfoFlow/Context.h"

using namespace std;
using namespace llvm;

namespace soaap {
  typedef SmallVector<Function*,16> FunctionVector;
  typedef SmallSet<Function*,16> FunctionSet;
  typedef map<Function*,int> FunctionIntMap;
  typedef SmallVector<CallInst*,16> CallInstVector;
  typedef SmallSet<CallInst*,16> CallInstSet;
  typedef DenseMap<const Value*,int> ValueIntMap;
  typedef map<const Value*,FunctionSet> ValueFunctionSetMap;
  typedef SmallVector<string,16> StringVector;
  typedef set<string> StringSet;
  typedef SmallVector<Context*,8> ContextVector;
  typedef vector<BasicBlock*> BasicBlockVector;  // use <vector> as can be large
  typedef list<Instruction*> InstTrace;
  typedef list<Instruction*> InstVector;
  typedef SmallVector<GlobalVariable*,8> GlobalVariableVector;
  typedef SmallSet<Value*,16> ValueSet;
  typedef SmallSet<Argument*,16> ArgumentSet;
  typedef pair<CallInst*,Function*> CallGraphEdge;
}

#endif
