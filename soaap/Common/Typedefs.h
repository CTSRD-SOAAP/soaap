#ifndef SOAAP_COMMON_TYPEDEFS_H
#define SOAAP_COMMON_TYPEDEFS_H

#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include <list>
#include <map>
#include <vector>

#include "Analysis/InfoFlow/Context.h"

using namespace std;
using namespace llvm;

namespace soaap {
  typedef SmallVector<Function*,16> FunctionVector;
  typedef SmallSet<Function*,16> FunctionSet;
  typedef map<Function*,int> FunctionIntMap;
  typedef SmallVector<CallInst*,16> CallInstVector;
  typedef SmallSet<CallInst*,16> CallInstSet;
  typedef map<const Value*,int> ValueIntMap;
  typedef SmallVector<string,16> StringVector;
  typedef SmallVector<Context*,8> ContextVector;
  typedef vector<BasicBlock*> BasicBlockVector;  // use <vector> as can be large
  typedef list<Instruction*> InstTrace;
  typedef list<Instruction*> InstVector;
  typedef SmallVector<GlobalVariable*,8> GlobalVariableVector;
  typedef SmallSet<Value*,16> ValueSet;
}

#endif
