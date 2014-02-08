#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/DebugUtils.h"
#include "llvm/DebugInfo.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cxxabi.h>
#include <stdlib.h>
#include "soaap.h"

using namespace soaap;
using namespace llvm;
using namespace std;

GlobalVariableVector ClassHierarchyUtils::classes;
ClassHierarchy ClassHierarchyUtils::classToSubclasses;
//ClassHierarchy ClassHierarchyUtils::classToDescendents;
map<CallInst*,FunctionVector> ClassHierarchyUtils::callToCalleesCache;
map<GlobalVariable*,GlobalVariable*> ClassHierarchyUtils::typeInfoToVTable;
map<GlobalVariable*,GlobalVariable*> ClassHierarchyUtils::vTableToTypeInfo;
map<GlobalVariable*,map<int,int> > ClassHierarchyUtils::vTableToSecondaryVTableMaps;
map<GlobalVariable*,map<GlobalVariable*,int> > ClassHierarchyUtils::classToBaseOffset;
bool ClassHierarchyUtils::cachingDone = false;

void ClassHierarchyUtils::findClassHierarchy(Module& M) {
  // extract call hierarchy using std::type_info structures rather
  // than debug info. The former works even for when there are anonymous
  // namespaces. type_info structs can be obtained from vtable globals.
  // (for more info, see http://mentorembedded.github.io/cxx-abi/abi.html)
  for (Module::global_iterator I=M.global_begin(), E=M.global_end(); I != E; I++) {
    GlobalVariable* G = &*I;
    if (G->getName().startswith("_ZTV")) {
      if (G->hasInitializer()) {
        DEBUG(G->dump());
        ConstantArray* Ginit = cast<ConstantArray>(G->getInitializer());
        // Ginit[1] is the type_info global for this vtable's type
        bool typeInfoFound = false;
        bool primaryVTable = true;
        for (int i=0; i<Ginit->getNumOperands(); i++) {
          // typeinfo will be the first global variable in the array.
          // It's not always at a fixed index so we have to search for it...
          if (GlobalVariable* TI = dyn_cast<GlobalVariable>(Ginit->getOperand(i)->stripPointerCasts())) {
            //TI->dump();
            typeInfoToVTable[TI] = G;
            vTableToTypeInfo[G] = TI;
            processTypeInfo(TI); // process TI recursively (it contains refs to super-class TIs)
            typeInfoFound = true;
            if (primaryVTable) {
              primaryVTable = false;
              vTableToSecondaryVTableMaps[G][0] = i+1;  // i+1 is also the size of the vtable header            
            }
            else {
              // offset_to_top is at the previous index 
              ConstantExpr* offsetValCast = cast<ConstantExpr>(Ginit->getOperand(i-1)->stripPointerCasts());
              ConstantInt* offsetVal = cast<ConstantInt>(offsetValCast->getOperand(0)); 
              int offsetToTop = offsetVal->getSExtValue();
              if (offsetToTop > 0) {
                dbgs() << "ERROR: offsetToTop is positive!\n";
                G->dump();
              }
              else {
                offsetToTop *= -1;
              }
              
              DEBUG(dbgs() << "offsetToTop: " << offsetToTop << "\n");
              vTableToSecondaryVTableMaps[G][offsetToTop] = i+1;
            }
          }
        }

        if (!typeInfoFound) {
          dbgs() << "ERROR: vtable initializer is not a global variable...\n";
          dbgs() << *G << " = " << *Ginit << "\n";
        }
      }
    }
  }
  
  DEBUG(ppClassHierarchy(classToSubclasses));
}

void ClassHierarchyUtils::cacheAllCalleesForVirtualCalls(Module& M) {
  if (!cachingDone) {
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->isDeclaration()) continue;
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          GlobalVariable* definingTypeVTableVar = NULL;
          GlobalVariable* staticTypeVTableVar = NULL;
          bool hasMetadata = false;
          if (MDNode* N = I->getMetadata("soaap_defining_vtable_var")) {
            definingTypeVTableVar = cast<GlobalVariable>(N->getOperand(0));
            hasMetadata = true;
          }
          else if (MDNode* N = I->getMetadata("soaap_defining_vtable_name")) {
            ConstantDataArray* definingTypeVTableConstant = cast<ConstantDataArray>(N->getOperand(0));
            string definingTypeVTableConstantStr = definingTypeVTableConstant->getAsString().str();
            //dbgs() << "definingTypeVTableConstantStr: " << definingTypeVTableConstantStr << "\n";
            definingTypeVTableVar = M.getGlobalVariable(definingTypeVTableConstantStr, true);
            hasMetadata = true;
          }
          if (MDNode* N = I->getMetadata("soaap_static_vtable_var")) {
            staticTypeVTableVar = cast<GlobalVariable>(N->getOperand(0));
            hasMetadata = true;
          }
          else if (MDNode* N = I->getMetadata("soaap_static_vtable_name")) {
            ConstantDataArray* staticTypeVTableConstant = cast<ConstantDataArray>(N->getOperand(0));
            string staticTypeVTableConstantStr = staticTypeVTableConstant->getAsString().str();
            //dbgs() << "staticTypeVTableConstantStr: " << staticTypeVTableConstantStr << "\n";
            staticTypeVTableVar = M.getGlobalVariable(staticTypeVTableConstantStr, true);
            hasMetadata = true;
          }
          if (definingTypeVTableVar != NULL) {
            DEBUG(dbgs() << "Found definingVTableVar: " << *definingTypeVTableVar << "\n");
            if (staticTypeVTableVar == NULL) {
              dbgs() << "definingVTableVar is not null, but staticTypeVTableVar is NULL\n";
              // This could be the case if no instance of the static type is ever created
              staticTypeVTableVar = definingTypeVTableVar;
            }
            callToCalleesCache[C] = findAllCalleesForVirtualCall(C, definingTypeVTableVar, staticTypeVTableVar, M);
          }
          else if (hasMetadata) {
            dbgs() << "Defining VTable is NULL!\n";
            I->dump();
          }
        }
      }
    }
    cachingDone = true;
  }
}

FunctionVector ClassHierarchyUtils::getCalleesForVirtualCall(CallInst* C, Module& M) {
  if (!cachingDone) {
    cacheAllCalleesForVirtualCalls(M);
  }
  return callToCalleesCache[C];
}

void ClassHierarchyUtils::processTypeInfo(GlobalVariable* TI) {
  if (find(classes.begin(), classes.end(), TI) == classes.end()) {
    classes.push_back(TI);

    // extract direct base classes from type_info
    if (TI->hasInitializer()) {
      ConstantStruct* TIinit = cast<ConstantStruct>(TI->getInitializer());

      int TInumOperands = TIinit->getNumOperands();
      if (TInumOperands > 2) {
        // we have >= 1 base class(es).
        if (TInumOperands == 3) {
          // abi::__si_class_type_info
          GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(2)->stripPointerCasts());
          classToSubclasses[baseTI].push_back(TI);
          //dbgs() << "  " << *baseTI << "\n";
          classToBaseOffset[TI][baseTI] = 0;
          processTypeInfo(baseTI);
          
        }
        else {
          // abi::__vmi_class_type_info
          // (skip over first two additional fields, which are "flags" and "base_count")
          for (int i=4; i<TInumOperands; i+=2) {
            GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(i)->stripPointerCasts());
            classToSubclasses[baseTI].push_back(TI);
            int offset_flags = cast<ConstantInt>(TIinit->getOperand(i+1))->getSExtValue();
            DEBUG(dbgs() << TI->getName() << " -> " << baseTI->getName() << "\n");
            DEBUG(dbgs() << "  offset_flags = " << offset_flags << "\n");
            int offset = offset_flags >> 8; // TODO: is this value defined as a constant somewhere?
            DEBUG(dbgs() << "  offset = " << offset << "\n");
            classToBaseOffset[TI][baseTI] = offset;
            //dbgs() << "  " << *baseTI << "\n";
            processTypeInfo(baseTI);
          }
        }
      }
    }
    else {
      DEBUG(dbgs() << "ERROR: TI " << TI->getName() << " does not have initializer\n");
    }
  }
}

void ClassHierarchyUtils::ppClassHierarchy(ClassHierarchy& classHierarchy) {
  // first find all classes that do not have subclasses
  GlobalVariableVector baseClasses = classes;
  for (ClassHierarchy::iterator I=classHierarchy.begin(); I != classHierarchy.end(); I++) {
    GlobalVariableVector subclasses = I->second;
    for (GlobalVariable* sc : subclasses) {
      baseClasses.erase(remove(baseClasses.begin(), baseClasses.end(), sc), baseClasses.end());
    }
  }

  for (GlobalVariable* bc : baseClasses) {
    ppClassHierarchyHelper(bc, classHierarchy, 0);
  }
}

void ClassHierarchyUtils::ppClassHierarchyHelper(GlobalVariable* c, ClassHierarchy& classHierarchy, int nesting) {
  for (int i=0; i<nesting-1; i++) {
    dbgs() << "    ";
  }
  if (nesting > 0){
    dbgs() << " -> ";
  }
  int status = -4;
  string cName = c->getName();

  char* demangled = abi::__cxa_demangle(cName.replace(0, 4, "_Z").c_str(), 0, 0, &status);
  dbgs() << (status == 0 ? demangled : cName) << "\n";
  for (GlobalVariable* sc : classHierarchy[c]) {
    ppClassHierarchyHelper(sc, classHierarchy, nesting+1);
  }
}


FunctionVector ClassHierarchyUtils::findAllCalleesForVirtualCall(CallInst* C, GlobalVariable* definingTypeVTableVar, GlobalVariable* staticTypeVTableVar, Module& M) {
  
  FunctionVector callees;

  // We know this is a virtual call, as it has already been annotated with debugging metadata
  // in a previous pass (mdnode N). 
  //
  // Metadata node N contains the vtable global variable for C's static class type.
  // We still need to obtain the vtable idx and use the class hierarchy to also obtain
  // descendent vtables and thus descendent implementations of C's callees.
  //
  // Relevant bit of code for a vtable lookup, will look like this:
  //
  // call void @llvm.dbg.declare(metadata !{%"class.box::D"** %d}, metadata !46), !dbg !48
  // store %"class.box::D"* %4, %"class.box::D"** %d, align 8, !dbg !48
  // %5 = load %"class.box::D"** %d, align 8, !dbg !49
  // %6 = bitcast %"class.box::D"* %5 to %"class.box::A"*, !dbg !49
  // %7 = bitcast %"class.box::A"* %6 to void (%"class.box::A"*)***, !dbg !49
  // %8 = load void (%"class.box::A"*)*** %7, !dbg !49
  // %9 = getelementptr inbounds void (%"class.box::A"*)** %8, i64 0, !dbg !49
  // %10 = load void (%"class.box::A"*)** %9, !dbg !49
  // call void %10(%"class.box::A"* %6), !dbg !49

  DEBUG(dbgs() << "Call: " << *C << "\n");
  if (LoadInst* calledVal = dyn_cast<LoadInst>(C->getCalledValue())) {
    if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(calledVal->getPointerOperand())) {
      if (ConstantInt* cVTableIdxVal = dyn_cast<ConstantInt>(gep->getOperand(1))) {
        int cVTableIdx = cVTableIdxVal->getSExtValue();
        DEBUG(dbgs() << "relative cVTableIdx: " << cVTableIdx << "\n");
 
        // find all implementations in reciever's class (corresponding to the
        // static type) as well as all descendent classes. Note: callees will
        // be at same idx in all vtables
        GlobalVariable* definingClazzTI = vTableToTypeInfo[definingTypeVTableVar];
        GlobalVariable* staticClazzTI = vTableToTypeInfo[staticTypeVTableVar];

        /*
        if (definingClazzTI == NULL) {
          dbgs() << "definingClazzTI is null, vtable: " << definingTypeVTableVar->getName() << "\n";
          C->dump();
        }
        if (staticClazzTI == NULL) {
          dbgs() << "staticClazzTI is null, vtable: " << staticTypeVTableVar->getName() << "\n";
          C->dump();
        }
        */

        if (definingClazzTI != NULL && staticClazzTI != NULL) {
          int subObjOffset = findSubObjOffset(definingClazzTI, staticClazzTI);
          DEBUG(dbgs() << *definingClazzTI << " is at offset " << subObjOffset << " in " << *staticClazzTI << "\n");
          findAllCalleesInSubClasses(C, staticClazzTI, cVTableIdx, subObjOffset, callees);
        }
      }
      else {
        dbgs() << "vtable idx is NOT a ConstantInt\n";
        DEBUG(C->dump());
        DEBUG(gep->getOperand(1)->dump());
      }
    }
  }

  // debugging
  bool dbg = false;
  DEBUG(dbg = true);
  if (dbg) {
    dbgs() << "Callees: [";
    int i = 0;
    for (Function* F : callees) {
      dbgs() << F->getName();
      if (i < callees.size()-1)
        dbgs() << ", ";
      i++;
    }
    dbgs() << "]\n";
  }

  return callees;
}

int ClassHierarchyUtils::findSubObjOffset(GlobalVariable* definingClazzTI, GlobalVariable* staticClazzTI) {
  if (definingClazzTI == staticClazzTI) {
    return 0;
  }
  else {
    for (GlobalVariable* subTI : classToSubclasses[definingClazzTI]) {
      // When moving to a subclass, adjust subObjOffset by adding TI's offset in subTI.
      // (relative ordering of subobjects are always maintained).
      int subObjOffset = findSubObjOffset(subTI, staticClazzTI);
      if (subObjOffset != -1) {
        return subObjOffset + classToBaseOffset[subTI][definingClazzTI];
      }
    }
    return -1;
  }
}

void ClassHierarchyUtils::findAllCalleesInSubClasses(CallInst* C, GlobalVariable* TI, int vtableIdx, int subObjOffset, FunctionVector& callees) {
  DEBUG(dbgs() << "Looking for func at vtable idx " << vtableIdx << " in " << TI->getName() << " (subObjOffset=" << subObjOffset << ")\n");
  bool skip = false;
  if (GlobalVariable* vTableVar = typeInfoToVTable[TI]) { // Maybe NULL if TI doesn't have a vtable def
    ConstantArray* clazzVTable = cast<ConstantArray>(vTableVar->getInitializer());

    // It's possible that TI is a superclass that doesn't contain the function. This
    // will be due to a downcast... the receiver object was initially of the superclass type
    // and then it was downcasted before the vtable func is called. So we skip this class and 
    // move to the subclass. There are two ways to discover if a downcast did occur:
    //   1) the subObjOffset doesn't appear in clazzVTable
    //   2) the vtable doesn't contain a function at the vtable idx relative to the start of 
    //      the subobject's vtable (because it is added by a descendent class)
    // We cannot reliably detect downcasts however as the function may have been introduced 
    // in-between TI and the casted-to type and so we may infer more callees.
    if (vTableToSecondaryVTableMaps[vTableVar].find(subObjOffset) != vTableToSecondaryVTableMaps[vTableVar].end()) {
      int subObjVTableOffset = vTableToSecondaryVTableMaps[vTableVar][subObjOffset];
      DEBUG(dbgs() << "    Absolute vtable index: " << (subObjVTableOffset+vtableIdx) << "\n");
      if ((subObjVTableOffset+vtableIdx) < clazzVTable->getNumOperands()) {
        Value* clazzVTableElem = clazzVTable->getOperand(subObjVTableOffset+vtableIdx)->stripPointerCasts();
        if (Function* callee = dyn_cast<Function>(clazzVTableElem)) {
          DEBUG(dbgs() << "  vtable entry is func: " << callee->getName() << "\n");
          if (callee->getName().str() != "__cxa_pure_virtual") { // skip pure virtual functions
            // if this is a thunk then we extract the actual function from within
            callee = extractFunctionFromThunk(callee);
            if (find(callees.begin(), callees.end(), callee) == callees.end()) {
              callees.push_back(callee);
            }
          }
        }
        else {
          // The vtable entry may not be a function if we're looking for a function
          // in the vtable of a class that was statically casted to (using static_cast).
          // In such a case we recursively search subclasses (as we don't know which
          // subclass it is) and thus in one such subclass, the vtable entry might be 
          // a TI pointer and not a function pointer.
          DEBUG(dbgs() << "  vtable entry " << vtableIdx << " is not a Function\n");
          DEBUG(C->dump());
          DEBUG(clazzVTableElem->dump());
        }
      }
      else {
        DEBUG(dbgs() << "ERROR: index exceeds size of vtable " << vTableVar->getName() << "\n");
        skip = true;
      }
    }
    else {
      DEBUG(dbgs() << "ERROR: subObjectOffset " << subObjOffset << " does not exist in vtable " << vTableVar->getName() << "\n");
      skip = true;
    }
  }
  for (GlobalVariable* subTI : classToSubclasses[TI]) {
    // When moving to a subclass, adjust subobjoffset by adding TI's offset in subTI.
    // (relative ordering of subsubobjects within a subobject are always maintained).
    int subSubObjOffset = skip ? subObjOffset : (classToBaseOffset[subTI][TI] + subObjOffset);

    DEBUG(dbgs() << "adjusting subObjOffset from " << subObjOffset << " to " << (subSubObjOffset) << "\n");
    findAllCalleesInSubClasses(C, subTI, vtableIdx, subSubObjOffset, callees);
  }
}

Function* ClassHierarchyUtils::extractFunctionFromThunk(Function* F) {
  if (F->getName().startswith("_ZTh")) {
    // F is a thunk
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      // the first non-intrinsic call will be to the actual function
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (!isa<IntrinsicInst>(C)) {
          Function* callee = CallGraphUtils::getDirectCallee(C);
          DEBUG(dbgs() << "Replacing thunk " << F->getName() << " with " << callee->getName() << "\n");
          return callee;
        }
      }
    }
  }
  return F;
}
