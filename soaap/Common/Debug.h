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

#ifndef SOAAP_COMMON_DEBUG_H
#define SOAAP_COMMON_DEBUG_H

#include "Common/CmdLineOpts.h"
#include "Util/DebugUtils.h"

#include "llvm/Support/Debug.h"

#ifdef NDEBUG
#define SDEBUG(NAME,VERBOSITY,X)
#else
#define SDEBUG(NAME,VERBOSITY,X)  \
  if (!CmdLineOpts::DebugModule.empty() && VERBOSITY <= CmdLineOpts::DebugVerbosity) {\
    if (soaap::matches(NAME, CmdLineOpts::DebugModule) \
        && (CmdLineOpts::DebugFunction.empty() || soaap::matches(__FUNCTION__, CmdLineOpts::DebugFunction))) {\
      do { \
        showPreamble(NAME, __FUNCTION__); \
        X; \
      } while (0); \
    }\
  }
#endif

using namespace llvm;
using namespace std;

namespace soaap {
#ifndef NDEBUG  
  bool debugging(string ModuleName, int Verbosity, string FunctionName);
  void showPreamble(string ModuleName, string FunctionName);
  bool matches(string name, string pattern);
#endif
}

#endif
