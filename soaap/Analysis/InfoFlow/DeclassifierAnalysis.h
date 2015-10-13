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

#ifndef SOAAP_ANALYSIS_INFOFLOW_DECLASSIFIERANALYSIS_H
#define SOAAP_ANALYSIS_INFOFLOW_DECLASSIFIERANALYSIS_H

#include "Analysis/InfoFlow/InfoFlowAnalysis.h"
#include "Common/Typedefs.h"

#include <map>

using namespace llvm;
using namespace std;

namespace soaap {

  class DeclassifierAnalysis: public InfoFlowAnalysis<bool> {
    public:
      DeclassifierAnalysis() : InfoFlowAnalysis<bool>(true, true) { }
      virtual bool isDeclassified(const Value* V);

    protected:
      map<Value*, InstVector> valueToDeclassifiedRegion;
      virtual void initialise(ValueContextPairList& worklist, Module& M, SandboxVector& sandboxes);
      virtual void postDataFlowAnalysis(Module& M, SandboxVector& sandboxes);
      virtual bool performMeet(bool from, bool& to);
      virtual bool performUnion(bool from, bool& to);
      virtual bool bottomValue() { return false; }
      virtual bool checkEqual(bool f1, bool f2) { return f1 == f2; }
      virtual string stringifyFact(bool fact);
      virtual void findAllFollowingInstructions(Instruction* I, Value* V);
  };
}

#endif 
