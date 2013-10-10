#ifndef SOAAP_UTILS_CLASSHIERARCHYUTILS_H
#define SOAAP_UTILS_CLASSHIERARCHYUTILS_H

#include "llvm/IR/Module.h"
#include "Common/Typedefs.h"

using namespace llvm;

namespace soaap {
  class ClassHierarchyUtils {
    public:
      static void findClassHierarchy(Module& M);
      static void findAllCalleesForVirtualCalls(Module& M);
      static FunctionVector findAllCalleesForVirtualCall(CallInst* C, Module& M);
    
    private:
      static StringVector classes;
      static map<string,StringVector> classToSubclasses;
      static map<string,StringVector> classToDescendents;
      static void calculateTransitiveClosure();
      static string convertTypeIdToVTableId(string typeId);
      static void ppClassHierarchy(map<string,StringVector>& classHierarchy);
      static void ppClassHierarchyHelper(string c, map<string,StringVector>& classHierarchy, int nesting);
  };
}

#endif
