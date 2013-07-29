#ifndef SOAAP_TYPEDEFS_H
#define SOAAP_TYPEDEFS_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include <map>

#include "Analysis/InfoFlow/Context.h"

using namespace std;
using namespace llvm;

namespace soaap {
  typedef SmallVector<Function*,16> FunctionVector;
  typedef SmallSet<Function*,16> FunctionSet;
  typedef map<Function*,int> FunctionIntMap;
  typedef SmallVector<CallInst*,16> CallInstVector;
  typedef map<const Value*,int> ValueIntMap;
  typedef SmallVector<string,16> StringVector;
  typedef SmallVector<Context*,8> ContextVector;
}

#endif
