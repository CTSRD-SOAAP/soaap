#ifndef SOAAP_UTILS_CLASSHIERARCHYUTILS_H
#define SOAAP_UTILS_CLASSHIERARCHYUTILS_H

#include "llvm/IR/Module.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  typedef map<GlobalVariable*,GlobalVariableVector> ClassHierarchy;
  class ClassHierarchyUtils {
    public:
      static void findClassHierarchy(Module& M);
      static void cacheAllCalleesForVirtualCalls(Module& M);
      static FunctionVector getCalleesForVirtualCall(CallInst* C, Module& M);
      static void dumpVirtualCallees(Module& M);
      static void readVirtualCallees(Module& M);
    
    private:
      static GlobalVariableVector classes;
      static ClassHierarchy classToSubclasses;
      static ClassHierarchy classToDescendents;
      static map<GlobalVariable*,GlobalVariable*> typeInfoToVTable;
      static map<GlobalVariable*,GlobalVariable*> vTableToTypeInfo;
      static map<CallInst*,FunctionVector> callToCalleesCache;
      static bool cachingDone;
      static void processTypeInfo(GlobalVariable* TI);
      static void calculateTransitiveClosure();
      static FunctionVector findAllCalleesForVirtualCall(CallInst* C, MDNode* N, Module& M);
      static void ppClassHierarchy(ClassHierarchy& classHierarchy);
      static void ppClassHierarchyHelper(GlobalVariable* c, ClassHierarchy& classHierarchy, int nesting);
  };
}

#endif
