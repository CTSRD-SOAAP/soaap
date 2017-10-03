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

#ifndef SOAAP_UTILS_CLASSHIERARCHYUTILS_H
#define SOAAP_UTILS_CLASSHIERARCHYUTILS_H

#include "llvm/IR/Module.h"
#include "Common/Typedefs.h"

#include <utility>

using namespace llvm;
using namespace std;

namespace soaap {
  typedef map<GlobalVariable*,GlobalVariableVector> ClassHierarchy;
  class ClassHierarchyUtils {
    public:
      static void findClassHierarchy(Module& M);
      static void cacheAllCalleesForVirtualCalls(Module& M);
      static FunctionSet getCalleesForVirtualCall(CallInst* C, Module& M);
    
    private:
      static GlobalVariableVector classes;
      static ClassHierarchy classToSubclasses;
      static map<GlobalVariable*,GlobalVariable*> typeInfoToVTable;
      static map<GlobalVariable*,GlobalVariable*> vTableToTypeInfo;
      static map<CallInst*,FunctionSet> callToCalleesCache;
      static map<GlobalVariable*,map<int,pair<int,int> > > vTableToSecondaryVTableMaps;
      static map<GlobalVariable*,map<GlobalVariable*,int> > classToBaseOffset;
      static map<GlobalVariable*,map<GlobalVariable*,int> > classToVBaseOffsetOffset;
      static bool cachingDone;

      static void processTypeInfo(GlobalVariable* TI, Module& M);
      static FunctionSet findAllCalleesForVirtualCall(CallInst* C, GlobalVariable* definingTypeTIVar, GlobalVariable* staticTypeTIVar, Module& M);
      static void findAllCalleesInSubClasses(CallInst* C, GlobalVariable* definingTypeTI, GlobalVariable* staticTypeTI, int vtableIdx, FunctionSet& callees);
      static void findAllCalleesInSubClassesHelper(CallInst* C, GlobalVariable* TI, GlobalVariable* staticTypeTI, int vtableIdx, int vbaseOffsetOffset, int subObjOffset, int vbaseSubObjOffset, bool collectingCallees, FunctionSet& callees);
      static Function* extractFunctionFromThunk(Function* F);
      static void ppClassHierarchy(ClassHierarchy& classHierarchy);
      static void ppClassHierarchyHelper(GlobalVariable* c, ClassHierarchy& classHierarchy, int nesting);
      static Value* getMDNodeOperandValue(MDNode* N, unsigned I);
  };
}

#endif
