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

#ifndef SOAAP_UTILS_SANDBOXUTILS_H
#define SOAAP_UTILS_SANDBOXUTILS_H

#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/CallGraph.h"
#include <string>
#include <map>

using namespace std;
using namespace llvm;

namespace soaap {
  class SandboxUtils {
    public:
      static SandboxVector findSandboxes(Module& M);
      static void reinitSandboxes(SandboxVector& sandboxes);
      static string stringifySandboxNames(int sandboxNames);
      static string stringifySandboxVector(SandboxVector& sandboxes);
      static bool isSandboxEntryPoint(Module& M, Function* F);
      static bool isWithinSandboxedRegion(Instruction* I, SandboxVector& sandboxes);
      static Sandbox* getSandboxForEntryPoint(Function* F, SandboxVector& sandboxes);
      static SandboxVector getSandboxesContainingMethod(Function* F, SandboxVector& sandboxes);
      static SandboxVector getSandboxesContainingInstruction(Instruction* I, SandboxVector& sandboxes);
      static Sandbox* getSandboxWithName(string name, SandboxVector& sandboxes);
      static void outputSandboxedFunctions(SandboxVector& sandboxes);
      static void outputPrivilegedFunctions();
      static bool isSandboxedFunction(Function* F, SandboxVector& sandboxes);
      static SandboxVector convertNamesToVector(int sandboxNames, SandboxVector& sandboxes);
      static void validateSandboxCreations(SandboxVector& sandboxes);
      
      static FunctionSet getPrivilegedMethods(Module& M);
      static void recalculatePrivilegedMethods(Module& M);
      static bool isPrivilegedMethod(Function* F, Module& M);
      static bool isPrivilegedInstruction(Instruction* I, SandboxVector& sandboxes, Module& M);
    
    private:
      static FunctionSet privilegedMethods;
      static map<string,int> sandboxNameToBitIdx;
      static map<int,string> bitIdxToSandboxName;
      static int nextSandboxNameBitIdx;
      static SmallSet<Function*,16> sandboxEntryPoints;
      static void createEmptySandboxIfNew(string name, SandboxVector& sandboxes, Module& M);
      static int assignBitIdxToSandboxName(string sandboxName);
      static void calculateSandboxedMethods(Function* F, Sandbox* S, FunctionVector& sandboxedMethods);
      static void calculatePrivilegedMethods(Module& M);
      static void calculatePrivilegedMethodsHelper(Module& M, Function* F);
      static void findAllSandboxedInstructions(Instruction* I, string sboxName, InstVector& insts);
  };
}

#endif
