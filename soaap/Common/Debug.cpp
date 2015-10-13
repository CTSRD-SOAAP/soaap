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

#include "Debug.h"

#include <Common/CmdLineOpts.h>

#include <llvm/Support/raw_ostream.h>

#include <regex>
#include <stdlib.h>
#include <unistd.h>

using namespace llvm;
using namespace soaap;

#ifndef NDEBUG
bool soaap::debugging(string ModuleName, int Verbosity, string FunctionName) {
  if (CmdLineOpts::DebugModule.empty() || Verbosity > CmdLineOpts::DebugVerbosity) {
    return false;
  }
  return matches(ModuleName, CmdLineOpts::DebugModule)
      && (CmdLineOpts::DebugFunction.empty() || matches(FunctionName, CmdLineOpts::DebugFunction));
}

void soaap::showPreamble(string ModuleName, string FunctionName) {
  static string lastModule = "";
  static string lastFunc = "";

  if (ModuleName != lastModule || FunctionName != lastFunc) {
    lastModule = ModuleName;
    lastFunc = FunctionName;
    errs().changeColor(raw_ostream::Colors::GREEN);
    errs() << "[" << FunctionName << " (" << ModuleName << ")]\n";
    errs().resetColor(); 
  }
}

bool soaap::matches(string name, string pattern) {
  regex reg(pattern);
  return regex_match(name, reg);
}
#endif
