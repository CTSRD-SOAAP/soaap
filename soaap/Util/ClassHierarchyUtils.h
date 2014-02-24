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
      static FunctionSet getCalleesForVirtualCall(CallInst* C, Module& M);
    
    private:
      static GlobalVariableVector classes;
      static ClassHierarchy classToSubclasses;
      static map<GlobalVariable*,GlobalVariable*> typeInfoToVTable;
      static map<GlobalVariable*,GlobalVariable*> vTableToTypeInfo;
      static map<CallInst*,FunctionSet> callToCalleesCache;
      static map<GlobalVariable*,map<int,int> > vTableToSecondaryVTableMaps;
      static map<GlobalVariable*,map<GlobalVariable*,int> > classToBaseOffset;
      static bool cachingDone;
      static int findSubObjOffset(GlobalVariable* definingClazzTI, GlobalVariable* staticClazzTI);
      static void processTypeInfo(GlobalVariable* TI);
      static FunctionSet findAllCalleesForVirtualCall(CallInst* C, GlobalVariable* definingVTableVar, GlobalVariable* staticVTableVar, Module& M);
      static void findAllCalleesInSubClasses(CallInst* C, GlobalVariable* TI, int vtableIdx, int subObjOffset, FunctionSet& callees);
      static Function* extractFunctionFromThunk(Function* F);
      static void ppClassHierarchy(ClassHierarchy& classHierarchy);
      static void ppClassHierarchyHelper(GlobalVariable* c, ClassHierarchy& classHierarchy, int nesting);
  };
}

#endif
