#include "Common/Debug.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
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
map<CallInst*,FunctionSet> ClassHierarchyUtils::callToCalleesCache;
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
  
  // 1. Process type_info structures
  for (Module::global_iterator I=M.global_begin(), E=M.global_end(); I != E; I++) {
    GlobalVariable* G = &*I;
    if (G->getName().startswith("_ZTI")) {
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found type_info: " << G->getName() << "\n");
      processTypeInfo(G);
    }
  }

  // 2. Process vtables
  for (Module::global_iterator I=M.global_begin(), E=M.global_end(); I != E; I++) {
    GlobalVariable* G = &*I;
    if (G->getName().startswith("_ZTV")) {
      if (G->hasInitializer()) {
        SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found vtable: " << *G << "\n");
        ConstantArray* Ginit = cast<ConstantArray>(G->getInitializer());
        // Ginit[1] is the type_info global for this vtable's type
        bool typeInfoFound = false;
        bool primaryVTable = true;
        for (int i=0; i<Ginit->getNumOperands(); i++) {
          // typeinfo will be the first global variable in the array.
          // It's not always at a fixed index so we have to search for it...
          if (GlobalVariable* TI = dyn_cast<GlobalVariable>(Ginit->getOperand(i)->stripPointerCasts())) {

            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found TI " << TI->getName() << " at index " << i << "\n");
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Storing mapping " << G->getName() << " <-> " << TI->getName() << "\n");
            //TI->dump();
            typeInfoToVTable[TI] = G;
            vTableToTypeInfo[G] = TI;
            //processTypeInfo(TI); // process TI recursively (it contains refs to super-class TIs)
            typeInfoFound = true;
            if (primaryVTable) {
              SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Primary VTable\n");
              primaryVTable = false;
              vTableToSecondaryVTableMaps[G][0] = i+1;  // i+1 is also the size of the vtable header            
            }
            else {
              SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Secondary VTable\n");
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
              
              SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "offsetToTop: " << offsetToTop << "\n");
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
  
  SDEBUG("soaap.util.classhierarchy", 3, ppClassHierarchy(classToSubclasses));
}

void ClassHierarchyUtils::cacheAllCalleesForVirtualCalls(Module& M) {
  if (!cachingDone) {
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      if (F->isDeclaration()) continue;
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Processing " << F->getName() << "\n");
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          GlobalVariable* definingTypeVTableVar = NULL;
          GlobalVariable* definingTypeTIVar = NULL;
          GlobalVariable* staticTypeVTableVar = NULL;
          GlobalVariable* staticTypeTIVar = NULL;
          bool hasMetadata = false;
          if (MDNode* N = I->getMetadata("soaap_defining_vtable_var")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_defining_vtable_var\n");
            definingTypeVTableVar = cast<GlobalVariable>(N->getOperand(0));
            definingTypeTIVar = vTableToTypeInfo[definingTypeVTableVar];
            hasMetadata = true;
          }
          else if (MDNode* N = I->getMetadata("soaap_defining_vtable_name")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_defining_vtable_name\n");
            ConstantDataArray* definingTypeVTableConstant = cast<ConstantDataArray>(N->getOperand(0));
            string definingTypeVTableConstantStr = definingTypeVTableConstant->getAsString().str();
            string definingTypeTIStr = "_ZTI" + definingTypeVTableConstantStr.substr(4);
            definingTypeTIVar = M.getGlobalVariable(definingTypeTIStr);
            /*definingTypeVTableVar = M.getGlobalVariable(definingTypeVTableConstantStr, true);
            if (definingTypeVTableVar == NULL) {
              definingTypeTIVar = M.getGlobalVariable(definingTypeTIStr);
              dbgs() << "definingTypeVTableVar is null: " << definingTypeVTableConstantStr << "\n";
              dbgs() << "definingTypeTIStr: " << definingTypeTIStr << "\n";

              if (MDNode* N2 = I->getMetadata("dbg")) {
                DILocation loc(N2);
                dbgs() << "location - " << loc.getFilename() << ":" << loc.getLineNumber() << "\n";
              }
            }*/
            hasMetadata = true;
          }
          if (MDNode* N = I->getMetadata("soaap_static_vtable_var")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_static_vtable_var\n");
            staticTypeVTableVar = cast<GlobalVariable>(N->getOperand(0));
            staticTypeTIVar = vTableToTypeInfo[staticTypeVTableVar];
            hasMetadata = true;
          }
          else if (MDNode* N = I->getMetadata("soaap_static_vtable_name")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_static_vtable_name\n");
            ConstantDataArray* staticTypeVTableConstant = cast<ConstantDataArray>(N->getOperand(0));
            string staticTypeVTableConstantStr = staticTypeVTableConstant->getAsString().str();
            string staticTypeTIStr = "_ZTI" + staticTypeVTableConstantStr.substr(4);
            staticTypeTIVar = M.getGlobalVariable(staticTypeTIStr);
            //dbgs() << "staticTypeVTableConstantStr: " << staticTypeVTableConstantStr << "\n";
            /*staticTypeVTableVar = M.getGlobalVariable(staticTypeVTableConstantStr, true);
            if (staticTypeVTableVar == NULL) {
              string staticTypeTIStr = "_ZTI" + staticTypeVTableConstantStr.substr(4);
              staticTypeTIVar = M.getGlobalVariable(staticTypeTIStr);
              dbgs() << "staticTypeVTableVar is null: " << staticTypeVTableConstantStr << "\n";
              dbgs() << "staticTypeTIStr: " << staticTypeTIStr << "\n";
              if (MDNode* N2 = I->getMetadata("dbg")) {
                DILocation loc(N2);
                dbgs() << "location - " << loc.getFilename() << ":" << loc.getLineNumber() << "\n";
              }
            }*/
            hasMetadata = true;
          }
          if (hasMetadata) {
            if (definingTypeTIVar == NULL || staticTypeTIVar == NULL) {
              SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "definingTypeTIVar or staticTypeTIVar is NULL\n");
            }
            else {
              callToCalleesCache[C] = findAllCalleesForVirtualCall(C, definingTypeTIVar, staticTypeTIVar, M);
              SDEBUG("soaap.util.classhierarchy", 4, dbgs() << "Num of callees: " << callToCalleesCache[C].size() << "\n");
            }
          }
        }
      }
    }
    cachingDone = true;
  }
}

FunctionSet ClassHierarchyUtils::getCalleesForVirtualCall(CallInst* C, Module& M) {
  if (!cachingDone) {
    cacheAllCalleesForVirtualCalls(M);
  }
  return callToCalleesCache[C];
}

void ClassHierarchyUtils::processTypeInfo(GlobalVariable* TI) {
  if (find(classes.begin(), classes.end(), TI) == classes.end()) {
    SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Adding class " << TI->getName() << "\n");
    classes.push_back(TI);

    // extract direct base classes from type_info
    if (TI->hasInitializer()) {
      ConstantStruct* TIinit = cast<ConstantStruct>(TI->getInitializer());

      int TInumOperands = TIinit->getNumOperands();
      if (TInumOperands > 2) { // first two operands are vptr and type name
        // we have >= 1 base class(es).
        if (TInumOperands == 3) {
          // abi::__si_class_type_info
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "abi::__si_class_type_info\n");
          GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(2)->stripPointerCasts());
          classToSubclasses[baseTI].push_back(TI);
          //dbgs() << "  " << *baseTI << "\n";
          classToBaseOffset[TI][baseTI] = 0;
          processTypeInfo(baseTI);
        }
        else {
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "abi::__vmi_class_type_info\n");
          // abi::__vmi_class_type_info
          // (skip over first two additional fields, which are "flags" and "base_count")
          for (int i=4; i<TInumOperands; i+=2) {
            GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(i)->stripPointerCasts());
            classToSubclasses[baseTI].push_back(TI);
            int offset_flags = cast<ConstantInt>(TIinit->getOperand(i+1))->getSExtValue();
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << TI->getName() << " -> " << baseTI->getName() << "\n");
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  offset_flags = " << offset_flags << "\n");
            int offset = offset_flags >> 8; // TODO: is this value defined as a constant somewhere?
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  offset = " << offset << "\n");
            classToBaseOffset[TI][baseTI] = offset;
            //dbgs() << "  " << *baseTI << "\n";
            processTypeInfo(baseTI);
          }
        }
      }
    }
    else {
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "ERROR: TI " << TI->getName() << " does not have initializer\n");
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


FunctionSet ClassHierarchyUtils::findAllCalleesForVirtualCall(CallInst* C, GlobalVariable* definingTypeTIVar, GlobalVariable* staticTypeTIVar, Module& M) {
  
  FunctionSet callees;

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

  SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Call: " << *C << "\n");
  if (LoadInst* calledVal = dyn_cast<LoadInst>(C->getCalledValue())) {
    if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(calledVal->getPointerOperand())) {
      if (ConstantInt* cVTableIdxVal = dyn_cast<ConstantInt>(gep->getOperand(1))) {
        int cVTableIdx = cVTableIdxVal->getSExtValue();
        SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "relative cVTableIdx: " << cVTableIdx << "\n");
 
        // find all implementations in reciever's class (corresponding to the
        // static type) as well as all descendent classes. Note: callees will
        // be at same idx in all vtables
        //GlobalVariable* definingTypeTIVar = vTableToTypeInfo[definingTypeVTableVar];
        //GlobalVariable* staticTypeTIVar = vTableToTypeInfo[staticTypeVTableVar];

        /*
        if (definingTypeTIVar == NULL) {
          dbgs() << "definingTypeTIVar is null, vtable: " << definingTypeVTableVar->getName() << "\n";
          C->dump();
        }
        if (staticTypeTIVar == NULL) {
          dbgs() << "staticTypeTIVar is null, vtable: " << staticTypeVTableVar->getName() << "\n";
          C->dump();
        }
        */

        //if (definingTypeTIVar != NULL && staticTypeTIVar != NULL) {
          int subObjOffset = findSubObjOffset(definingTypeTIVar, staticTypeTIVar);
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << *definingTypeTIVar << " is at offset " << subObjOffset << " in " << *staticTypeTIVar << "\n");
          findAllCalleesInSubClasses(C, staticTypeTIVar, cVTableIdx, subObjOffset, callees);
        //}
      }
      else {
        dbgs() << "vtable idx is NOT a ConstantInt\n";
        SDEBUG("soaap.util.classhierarchy", 3, C->dump());
        SDEBUG("soaap.util.classhierarchy", 3, gep->getOperand(1)->dump());
      }
    }
  }

  // debugging
  bool dbg = false;
  SDEBUG("soaap.util.classhierarchy", 3, dbg = true);
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

int ClassHierarchyUtils::findSubObjOffset(GlobalVariable* definingTypeTIVar, GlobalVariable* staticTypeTIVar) {
  if (definingTypeTIVar == staticTypeTIVar) {
    return 0;
  }
  else {
    for (GlobalVariable* subTI : classToSubclasses[definingTypeTIVar]) {
      // When moving to a subclass, adjust subObjOffset by adding TI's offset
      // in subTI.
      // (relative ordering of subobjects are always maintained).
      int subObjOffset = findSubObjOffset(subTI, staticTypeTIVar);
      if (subObjOffset != -1) {
        return subObjOffset + classToBaseOffset[subTI][definingTypeTIVar];
      }
    }
    return -1;
  }
}

void ClassHierarchyUtils::findAllCalleesInSubClasses(CallInst* C, GlobalVariable* TI, int vtableIdx, int subObjOffset, FunctionSet& callees) {
  SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Looking for func at vtable idx " << vtableIdx << " in " << TI->getName() << " (subObjOffset=" << subObjOffset << ")\n");
  bool skip = false;
  if (GlobalVariable* vTableVar = typeInfoToVTable[TI]) { // Maybe NULL if TI doesn't have a vtable def
    ConstantArray* clazzVTable = cast<ConstantArray>(vTableVar->getInitializer());

    // It's possible that TI is a superclass that doesn't contain the function.
    // This will be due to a downcast... the receiver object was initially of
    // the superclass type and then it was downcasted before the vtable func is
    // called. So we skip this class and move to the subclass. There are two
    // ways to discover if a downcast did occur:
    //   1) the subObjOffset doesn't appear in clazzVTable
    //   2) the vtable doesn't contain a function at the vtable idx relative to
    //   the start of the subobject's vtable (because it is added by a
    //   descendent class)
    // We cannot reliably detect downcasts however as the function may have
    // been introduced in-between TI and the casted-to type and so we may infer
    // more callees.
    if (vTableToSecondaryVTableMaps[vTableVar].find(subObjOffset) != vTableToSecondaryVTableMaps[vTableVar].end()) {
      int subObjVTableOffset = vTableToSecondaryVTableMaps[vTableVar][subObjOffset];
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "    Absolute vtable index: " << (subObjVTableOffset+vtableIdx) << "\n");
      if ((subObjVTableOffset+vtableIdx) < clazzVTable->getNumOperands()) {
        Value* clazzVTableElem = clazzVTable->getOperand(subObjVTableOffset+vtableIdx)->stripPointerCasts();
        if (Function* callee = dyn_cast<Function>(clazzVTableElem)) {
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  vtable entry is func: " << callee->getName() << "\n");
          if (callee->getName().str() != "__cxa_pure_virtual") { // skip pure virtual functions
            // if this is a thunk then we extract the actual function from within
            callee = extractFunctionFromThunk(callee);
            callees.insert(callee);
          }
        }
        else {
          // The vtable entry may not be a function if we're looking for a function
          // in the vtable of a class that was statically casted to (using static_cast).
          // In such a case we recursively search subclasses (as we don't know which
          // subclass it is) and thus in one such subclass, the vtable entry might be 
          // a TI pointer and not a function pointer.
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  vtable entry " << vtableIdx << " is not a Function\n");
          SDEBUG("soaap.util.classhierarchy", 3, C->dump());
          SDEBUG("soaap.util.classhierarchy", 3, clazzVTableElem->dump());
        }
      }
      else {
        SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "ERROR: index exceeds size of vtable " << vTableVar->getName() << "\n");
        skip = true;
      }
    }
    else {
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "ERROR: subObjectOffset " << subObjOffset << " does not exist in vtable " << vTableVar->getName() << "\n");
      skip = true;
    }
  }
  for (GlobalVariable* subTI : classToSubclasses[TI]) {
    // When moving to a subclass, adjust subobjoffset by adding TI's offset in subTI.
    // (relative ordering of subsubobjects within a subobject are always maintained).
    int subSubObjOffset = skip ? subObjOffset : (classToBaseOffset[subTI][TI] + subObjOffset);

    SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "adjusting subObjOffset from " << subObjOffset << " to " << (subSubObjOffset) << "\n");
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
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Replacing thunk " << F->getName() << " with " << callee->getName() << "\n");
          return callee;
        }
      }
    }
  }
  return F;
}
