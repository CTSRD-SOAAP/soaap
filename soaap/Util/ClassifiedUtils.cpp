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

#include "Util/ClassifiedUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;
using namespace llvm;

int ClassifiedUtils::nextClassNameBitIdx = 0;
map<string,int> ClassifiedUtils::classNameToBitIdx;
map<int,string> ClassifiedUtils::bitIdxToClassName;

string ClassifiedUtils::stringifyClassNames(int classNames) {
  string classNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (classNames & (1 << currIdx)) {
      string className = bitIdxToClassName[currIdx];
      if (!first) 
        classNamesStr += ",";
      classNamesStr += className;
      first = false;
    }
  }
  classNamesStr += "]";
  return classNamesStr;
}

StringVector ClassifiedUtils::convertNamesToVector(int classNames) {
  StringVector vec;
  int currIdx = 0;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (classNames & (1 << currIdx)) {
      string className = bitIdxToClassName[currIdx];
      vec.push_back(className);
    }
  }
  return vec;
}

void ClassifiedUtils::assignBitIdxToClassName(string className) {
  if (classNameToBitIdx.find(className) == classNameToBitIdx.end()) {
    dbgs() << "    Assigning index " << nextClassNameBitIdx << " to class name \"" << className << "\"\n";
    classNameToBitIdx[className] = nextClassNameBitIdx;
    bitIdxToClassName[nextClassNameBitIdx] = className;
    nextClassNameBitIdx++;
  }
}

int ClassifiedUtils::getBitIdxFromClassName(string className) {
  return classNameToBitIdx[className];
}
