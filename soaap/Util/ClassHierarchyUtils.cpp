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
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cxxabi.h>
#include <stdlib.h>
#include "soaap.h"

// from C++ ABI Spec
#define virtual_mask 0x1
#define offset_shift 8

using namespace soaap;
using namespace llvm;
using namespace std;

GlobalVariableVector ClassHierarchyUtils::classes;
ClassHierarchy ClassHierarchyUtils::classToSubclasses;
map<CallInst*,FunctionSet> ClassHierarchyUtils::callToCalleesCache;
map<GlobalVariable*,GlobalVariable*> ClassHierarchyUtils::typeInfoToVTable;
map<GlobalVariable*,GlobalVariable*> ClassHierarchyUtils::vTableToTypeInfo;
map<GlobalVariable*,map<int,int> > ClassHierarchyUtils::vTableToSecondaryVTableMaps;
map<GlobalVariable*,map<GlobalVariable*,int> > ClassHierarchyUtils::classToBaseOffset;
map<GlobalVariable*,map<GlobalVariable*,int> > ClassHierarchyUtils::classToVBaseOffsetOffset;
bool ClassHierarchyUtils::cachingDone = false;

void ClassHierarchyUtils::findClassHierarchy(Module& M) {
  // Extract class hierarchy using std::type_info structures rather than debug
  // info. The former works even for when there are anonymous namespaces.
  // (for more info, see http://mentorembedded.github.io/cxx-abi/abi.html)
  // 
  // Class names are usually global, except in the case of anonymous namespaces, in which case
  // they may be renamed during linking. This renaming is not consistent across type_info and
  // vtables. For example, the following are for the same class:
  // _ZTIN7content12_GLOBAL__N_115HeaderFlattenerE60908 and
  // _ZTVN7content12_GLOBAL__N_115HeaderFlattenerE60906.
  // 
  // (renamed from
  // _ZTVN7content12_GLOBAL__N_115HeaderFlattenerE and
  // _ZTIN7content12_GLOBAL__N_115HeaderFlattenerE).
  //
  // VTables give us the corresponding type_info, so we process them first and store
  // the VT <-> TI mappings, as this will be useful later.
  for (Module::global_iterator I=M.global_begin(), E=M.global_end(); I != E; I++) {
    GlobalVariable* VT = &*I;
    if (VT->getName().startswith("_ZTV")) {
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found vtable: " << VT->getName() << "\n");
      if (VT->hasInitializer()) {
        ConstantArray* VTinit = cast<ConstantArray>(VT->getInitializer());
        for (int i=0; i<VTinit->getNumOperands(); i++) {
          // type_info will be the first global variable in the array.
          // It's not always at a fixed index so we have to search for it...
          // The first one corresponds to the primary vtable, all others
          // correspond to secondary vtables. All type_info occurrences will
          // be the same.
          if (GlobalVariable* TI = dyn_cast<GlobalVariable>(VTinit->getOperand(i)->stripPointerCasts())) {
            if (typeInfoToVTable.find(TI) != typeInfoToVTable.end()) {
              report_fatal_error(TI->getName() + " <-> " + VT->getName() + " mapping already exists!");
            }
            // Record TI <-> VT mappings, as they will be useful later
            typeInfoToVTable[TI] = VT;
            vTableToTypeInfo[VT] = TI;
            break;  // we only need to process the first one
          }
        }
      }
      else {
        SDEBUG("soaap.util.classhierarchy", 1, dbgs() << "WARNING: VTable " << VT->getName() << " does not have initializer\n");
      }
    }
  }

  // Process type_info structures, recording class-hierarchy information and base offsets
  for (Module::global_iterator I=M.global_begin(), E=M.global_end(); I != E; I++) {
    GlobalVariable* G = &*I;
    if (G->getName().startswith("_ZTI")) {
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found type_info: " << G->getName() << "\n");
      processTypeInfo(G, M);
    }
  }

  SDEBUG("soaap.util.classhierarchy", 3, ppClassHierarchy(classToSubclasses));
}

void ClassHierarchyUtils::processTypeInfo(GlobalVariable* TI, Module& M) {
  if (find(classes.begin(), classes.end(), TI) == classes.end()) {
    SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Adding class " << TI->getName() << "\n");
    classes.push_back(TI);

    // First process the correspoding virtual table. We store the indexes of the primary
    // and secondary vtables, indexed by the corresponding subobject offset. This will allow
    // us to quickly jump to a particular sub-vtable.
    if (typeInfoToVTable.find(TI) != typeInfoToVTable.end()) {
      GlobalVariable* VT = typeInfoToVTable[TI]; 
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found vtable: " << VT->getName() << "\n");
      ConstantArray* VTinit = cast<ConstantArray>(VT->getInitializer());
      for (int i=0; i<VTinit->getNumOperands(); i++) {
        // Each sub-vtable is preceded by a reference to the class's type_info instance.
        // (See https://mentorembedded.github.io/cxx-abi/abi.html).
        if (TI == VTinit->getOperand(i)->stripPointerCasts()) {
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Found TI at index " << i << "\n");
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Storing mapping " << VT->getName() << " <-> " << TI->getName() << "\n");
          
          // the offset to top is the offset from the vptr pointing to this
          // sub-vtable, back to the top of the object. This will be negative,
          // but the absolute value gives us the offset from the start of the
          // object (i.e. first vptr), to the subobject.
          int offsetToTop = 0;
          if (ConstantExpr* offsetValCast = dyn_cast<ConstantExpr>(VTinit->getOperand(i-1)->stripPointerCasts())) {
            ConstantInt* offsetVal = cast<ConstantInt>(offsetValCast->getOperand(0));
            offsetToTop = offsetVal->getSExtValue(); // will be 0 for the primary
          }
          if (offsetToTop > 0) {
            report_fatal_error("ERROR: offsetToTop is positive!");
          }
          else {
            offsetToTop *= -1;
          }
          
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "offsetToTop: " << offsetToTop << "\n")
          vTableToSecondaryVTableMaps[VT][offsetToTop] = i+1;
        }
      }
    }
    else {
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "No vtable found for " << TI->getName() << "\n");
    }

    // Now process the type_info struct, extract direct base class info (what
    // their subobject offsets are, or if they are virtual then their
    // virtual-base-offset-offset)
    if (TI->hasInitializer()) {
      ConstantStruct* TIinit = cast<ConstantStruct>(TI->getInitializer());

      int TInumOperands = TIinit->getNumOperands();
      if (TInumOperands > 2) { // first two operands are vptr and type name
        // we have >= 1 base class(es).
        if (TInumOperands == 3) {
          // abi::__si_class_type_info: single, public, non-virtual base at
          // offset 0
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "abi::__si_class_type_info\n");
          GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(2)->stripPointerCasts());
          classToSubclasses[baseTI].push_back(TI);
          //dbgs() << "  " << *baseTI << "\n";
          classToBaseOffset[TI][baseTI] = 0;
          processTypeInfo(baseTI, M);
        }
        else {
          // abi::__vmi_class_type_info: all other cases
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "abi::__vmi_class_type_info\n");
          // (skip over first two additional fields, which are "flags" and "base_count")
          for (int i=4; i<TInumOperands; i+=2) {
            GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(i)->stripPointerCasts());
            classToSubclasses[baseTI].push_back(TI);
            long offset_flags = cast<ConstantInt>(TIinit->getOperand(i+1))->getSExtValue();
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << TI->getName() << " -> " << baseTI->getName() << "\n");
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  offset_flags = " << offset_flags << "\n");
            ptrdiff_t offset = offset_flags >> offset_shift;
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  offset = " << offset << "\n");
            // If this a virtual base class, then we obtain the
            // vbase-offset-offset.  This is the offset from the start of the
            // derived class's (primary) vtable to the location containing the
            // vbase-offset, which is the offset from the start of the object
            // to the virtual base's subobject. Note that virtual base
            // subobjects only exist once and are laid out after all other
            // non-virtual objects. We keep track of the vbase-offset-offset,
            // so that we can find the vbase offset when going down the class
            // hierarchy. (The vbase offset may differ every time but the
            // vbase-offset-offset will remain the same relative to the current
            // class's vtable in subclasses).
            if (offset_flags & virtual_mask) { // is this a virtual base class?
              SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "virtual base class: " << baseTI->getName() << "\n");
              ptrdiff_t vbaseOffsetOffset = offset/sizeof(long); // this should be negative
              SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vbase-offset-offset: " << vbaseOffsetOffset << "\n");
              classToVBaseOffsetOffset[TI][baseTI] = vbaseOffsetOffset;
              if (typeInfoToVTable.find(TI) != typeInfoToVTable.end()) {
                GlobalVariable* VT = typeInfoToVTable[TI];
                int vbaseOffsetIdx = vTableToSecondaryVTableMaps[VT][0]+vbaseOffsetOffset;
                SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vbase-offset-idx: " << vbaseOffsetIdx << "\n");
              }
              else {
                SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "No VTable for " << TI->getName() << "\n");
              }
            }
            else {
              // In the non-virtual base class case, we just record the subobj
              // offset. This is the offset from the start of the derived
              // object to the base class's subobject. We use this information
              // together with the offset-to-top info we obtained from the
              // vtable to find the base class's subvtable. Note that the
              // relative position of the base class subobj within the current
              // derived object will always remain the same, even in
              // subclasses.
              classToBaseOffset[TI][baseTI] = offset;
            }
            //dbgs() << "  " << *baseTI << "\n";
            processTypeInfo(baseTI, M);
          }
        }
      }
    }
    else {
      SDEBUG("soaap.util.classhierarhcy", 1, dbgs() << "WARNING: TI " + TI->getName() + " does not have initializer\n");
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
          // All relevant virtual calls will have metadata that we have
          // inserted during compilation to IR in clang. The defining type
          // gives us the vtable index and the relevant subobject that contains
          // the function at this index. The static type gives us the class
          // that we start finding callees from, as the possible callee could
          // be from this class or any subclass.
          if (MDNode* N = I->getMetadata("soaap_defining_vtable_var")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_defining_vtable_var\n");
            definingTypeVTableVar = cast<GlobalVariable>(getMDNodeOperandValue(N, 0));
            definingTypeTIVar = vTableToTypeInfo[definingTypeVTableVar];
            hasMetadata = true;
          }
          else if (MDNode* N = I->getMetadata("soaap_defining_vtable_name")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_defining_vtable_name\n");

            ConstantDataArray* definingTypeVTableConstant = cast<ConstantDataArray>(getMDNodeOperandValue(N, 0));
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
            staticTypeVTableVar = cast<GlobalVariable>(getMDNodeOperandValue(N, 0));
            staticTypeTIVar = vTableToTypeInfo[staticTypeVTableVar];
            hasMetadata = true;
          }
          else if (MDNode* N = I->getMetadata("soaap_static_vtable_name")) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "soaap_static_vtable_name\n");
            ConstantDataArray* staticTypeVTableConstant = cast<ConstantDataArray>(getMDNodeOperandValue(N, 0));
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
              bool debug = false;
              SDEBUG("soaap.util.classhierarchy", 3, debug = true);
              if (debug) {
                if (definingTypeTIVar != NULL) {
                  dbgs() << "   definingTypeTIVar is: " << definingTypeTIVar->getName() << "\n";
                }
                if (staticTypeTIVar != NULL) {
                  dbgs() << "   staticTypeTIVar is: " << staticTypeTIVar->getName() << "\n";
                }
                if (MDLocation* loc = dyn_cast_or_null<MDLocation>(I->getMetadata("dbg"))) {
                  dbgs() << "   location: " << loc->getFilename() << ":" << loc->getLine() << "\n";
                }
              }
            }
            else {
              callToCalleesCache[C] = findAllCalleesForVirtualCall(C, definingTypeTIVar, staticTypeTIVar, M);
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


FunctionSet ClassHierarchyUtils::findAllCalleesForVirtualCall(CallInst* C, GlobalVariable* definingTypeTIVar, GlobalVariable* staticTypeTIVar, Module& M) {
  
  FunctionSet callees;

  // We know this is a virtual call, as it has already been annotated with
  // debugging metadata by clang.
  //
  // We still need to obtain the vtable idx and use the class hierarchy to also
  // obtain descendent vtables and thus descendent implementations of C's
  // callees.
  //
  // Relevant bit of code for a vtable lookup will look like this:
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
  //
  // We work our way backwards from the call to the vtable idx:

  SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Call: " << *C << "\n");
  if (LoadInst* calledVal = dyn_cast<LoadInst>(C->getCalledValue())) {
    if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(calledVal->getPointerOperand())) {
      if (ConstantInt* cVTableIdxVal = dyn_cast<ConstantInt>(gep->getOperand(1))) {
        int cVTableIdx = cVTableIdxVal->getSExtValue();
        SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "relative cVTableIdx: " << cVTableIdx << "\n");
        // Find all implementations in reciever's class (corresponding to the
        // static type) as well as all descendent classes. VTables are built 
        // inductively from the vtables of base classes. This allows an object
        // of a type D to be treated as an object of type B, just by passing a
        // pointer to B's subojbect within D. Thus, 


        // special case for virtual base classes, which will come after all
        // non-virtual classes in the vtable and will only appear once
        findAllCalleesInSubClasses(C, definingTypeTIVar, staticTypeTIVar, cVTableIdx, callees);
        SDEBUG("soaap.util.classhierarchy", 4, dbgs() << "Num of callees: " << callToCalleesCache[C].size() << "\n");
        /*
        dbgs() << "Num of callees: " << callees.size() << "\n";
        if (callees.empty()) {
          dbgs() << "  definingTypeTI: " << definingTypeTIVar->getName() << "\n";
          dbgs() << "  staticTypeTI: " << staticTypeTIVar->getName() << "\n";
          dbgs() << "  vtableIdx: " << cVTableIdx << "\n";
          if (MDNode* N = C->getMetadata("dbg")) {
            DILocation loc(N);
            dbgs() << "  location: " << loc.getFilename() << ":" << loc.getLineNumber() << "\n";
          }
          dbgs() << "\n";
        }
        */
      }
      else {
        SDEBUG("soaap.util.classhierarchy", 3, C->dump());
        SDEBUG("soaap.util.classhierarchy", 3, gep->getOperand(1)->dump());
        report_fatal_error("vtable idx is NOT a ConstantInt");
      }
    }
    else {
      if (MDLocation* loc = dyn_cast_or_null<MDLocation>(C->getMetadata("dbg"))) {
        dbgs() << "Call location: " << loc->getFilename() << ":" << loc->getLine() << "\n";
      }
      report_fatal_error("V-call sequence does not have a gep where expected");
    }
  }
  else {
    if (MDLocation* loc = dyn_cast_or_null<MDLocation>(C->getMetadata("dbg"))) {
      dbgs() << "Call location: " << loc->getFilename() << ":" << loc->getLine() << "\n";
    }
    report_fatal_error("V-call sequence does not have a loadinst where expected");
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
        dbgs() << ",";
      i++;
    }
    dbgs() << "]\n";
  }

  return callees;
}

void ClassHierarchyUtils::findAllCalleesInSubClasses(CallInst* C, GlobalVariable* definingTypeTI, GlobalVariable* staticTypeTI, int vtableIdx, FunctionSet& callees) {
  /* 
   * Two important types:
   *   1. defining type - this is the class that contains the reaching definition
   *                      of the called function and in whose vtable it exists
   *                      at idx vtableIdx.
   *
   *   2. static type   - this is the receiver type of the instance object and
   *                      so possible callees could be either in this type or
   *                      any of its descendent types.
   *                    
   * VTables are built inductively from the vtables of base classes. This
   * allows an object of type D to be treated as a base object of type B, just
   * by passing a pointer to B's subojbect within D. We already have subobject
   * offset information from the type_info struct and we know where the
   * corresponding sub-vtables are from processing the vtable struct. As
   * relative offsets don't change as you move down the class hierarchy, e.g.
   * the offset of B-in-D will remain the same in a subclass E of D, we can
   * easily find B-in-D-in-E, from the offset of D-in-E and B-in-D. Moreover,
   * overridden virtual functions will appear in the same vtable location
   * within the defining class.
   *
   * We first have to find the subobject offset of the defining type within the
   * static type and then recursively move down adjusting offsets.
   *
   * Virtual base classes complicate things, as they will only appear once in
   * an object and vtable and so break the inductiveness. In this case, we use
   * vbase-offset-offset information (already obtained from the type_info
   * struct) to find the vbase offset and then the corresponding vtable.  There
   * are some further complications, as the defining object may be a suboject
   * of the virtual base. So we have to keep track of both the subobject offset
   * up to the virtual base V and also the subobject offset of the
   * direct-subclass W of V in W's descendents because the vbase-offset-offset
   * will be relative to W's vtable.
   *
   * NOTE: A LIMITATION OF THIS IMPLEMENTATION IS THAT IT CAN ONLY HANDLE AT
   *       MOST ONE VIRTUAL BASE CLASS PER PATH FROM THE DEFINING TYPE TO ANY
   *       SUBCLASS
   */
  findAllCalleesInSubClassesHelper(C, definingTypeTI, staticTypeTI, vtableIdx, INT_MAX, 0, 0, false, callees);
}

void ClassHierarchyUtils::findAllCalleesInSubClassesHelper(CallInst* C, GlobalVariable* TI, GlobalVariable* staticTypeTI, int vtableIdx, int vbaseOffsetOffset, int subObjOffset, int vbaseSubObjOffset, bool collectingCallees, FunctionSet& callees) {
  
  // We walk down the hierarchy until we reach staticTypeTI. While we are doing
  // this we update the subobject offset of the defining type.
  SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "--> visiting " << TI->getName() << "\n");
  bool skip = false;

  if (TI == staticTypeTI || collectingCallees) { // We have (already) reached the static type
    SDEBUG("soaap.util.classhierarchy", 3, dbgs() << TI->getName() << " == " << staticTypeTI->getName() << ", collectingCallees: " << collectingCallees << "\n");
    if (TI == staticTypeTI) {
      collectingCallees = true;
    }

    if (GlobalVariable* VT = typeInfoToVTable[TI]) {
      // Obtain start-of-vtable index for subobject within VT
      ConstantArray* VTinit = cast<ConstantArray>(VT->getInitializer());
      // It's possible that TI is a superclass that doesn't contain the
      // function.  This will be due to a downcast... the receiver object was
      // initially of the superclass type and then it was downcasted before the
      // vtable func is called. So we skip this class and move to the subclass.
      // There are two ways to discover if a downcast did occur:
      //   1) the subObjOffset doesn't appear in the vtable
      //   2) the vtable doesn't contain a function at the vtable idx relative
      //      to the start of the subobject's vtable (because it is added by a
      //      descendent class)
      // We cannot reliably detect downcasts however as the function may have
      // been introduced in-between TI and the casted-to type and so we may
      // infer more callees.
      if (vTableToSecondaryVTableMaps[VT].find(subObjOffset) != vTableToSecondaryVTableMaps[VT].end()) {
        int vtableOffset = vTableToSecondaryVTableMaps[VT][subObjOffset];
        SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "absolute vtable index: " << (vtableOffset+vtableIdx) << "\n");
        if (vbaseOffsetOffset != INT_MAX) {
          // function is in a virtual base vtable, so we need to obtain the
          // vbase-offset using the vbase-offset-offset
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "looking for vbaseOffset in " << TI->getName() << "[" << vtableOffset+vbaseOffsetOffset << "]\n");
          int vbaseOffset = 0;
          if (ConstantExpr* CE = dyn_cast<ConstantExpr>(VTinit->getOperand(vtableOffset+vbaseOffsetOffset))) {
            vbaseOffset = cast<ConstantInt>(CE->getOperand(0))->getSExtValue();
          }
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vbaseOffset in " << TI->getName() << ": " << vbaseOffset << "\n");
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "offsetToTop at index " << (vtableOffset-2) << ": ");
          int offsetToTop = 0;
          if (ConstantExpr* offsetToTopCE = dyn_cast<ConstantExpr>(VTinit->getOperand(vtableOffset-2))) {
            offsetToTop = cast<ConstantInt>(offsetToTopCE->getOperand(0))->getSExtValue();
          }
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << offsetToTop << "\n");
          // The vbase-offset value will be relative to this suboject. Thus, to
          // get the offset from the start of the derived object, we add the
          // offset-to-top value.
          if (offsetToTop < 0) {
            vbaseOffset += (-1 * offsetToTop);
          }
          // The defining type may have been a sub object of the virtual base
          // object, so get the actual offset by adding vbaseSubObjOffset
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "adjusting vbaseOffset to " << (vbaseOffset+vbaseSubObjOffset) << "\n");
          int objOffset = vbaseOffset+vbaseSubObjOffset;
          if (vTableToSecondaryVTableMaps[VT].find(objOffset) == vTableToSecondaryVTableMaps[VT].end()) {
            report_fatal_error("Secondary VTable at offset " + Twine(objOffset) + " does not exist in " + VT->getName());
          }
          vtableOffset = vTableToSecondaryVTableMaps[VT][vbaseOffset+vbaseSubObjOffset];
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vtableOffset for vbase in " << TI->getName() << " is " << vtableOffset << "\n");
        }
        if ((vtableOffset+vtableIdx) < VTinit->getNumOperands()) {
          Value* clazzVTableElem = VTinit->getOperand(vtableOffset+vtableIdx)->stripPointerCasts();
          if (Function* callee = dyn_cast<Function>(clazzVTableElem)) {
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  vtable entry is func: " << callee->getName() << "\n");
            if (callee->getName().str() != "__cxa_pure_virtual") { // skip pure virtual functions
              // if this is a thunk then we extract the actual function from
              // within
              callee = extractFunctionFromThunk(callee);
              callees.insert(callee);
            }
          }
          else {
            // The vtable entry may not be a function if we're looking for a
            // function in the vtable of a class that was statically casted to
            // (using static_cast).  In such a case we recursively search
            // subclasses (as we don't know which subclass it is) and thus in one
            // such subclass, the vtable entry might be a TI pointer and not a
            // function pointer.
            SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "  vtable entry " << vtableIdx << " is not a Function\n");
            SDEBUG("soaap.util.classhierarchy", 3, C->dump());
            SDEBUG("soaap.util.classhierarchy", 3, clazzVTableElem->dump());
            skip = true;
          }
        }
        else {
          // vtable entry might not exist because the function was added by a
          // subclass, we keep going down
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vtable entry " << vtableIdx << " does not exist");
          skip = true;
        }
      }
      else {
        SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "WARNING: subObjOffset " << subObjOffset << " does not exist in vtable " << VT->getName() << "\n");
        skip = true;
      }
    }
  }
  else {
    SDEBUG("soaap.util.classhierarchy", 3, dbgs() << TI->getName() << " != " << staticTypeTI->getName() << "\n");
  }
  // recurse on subclasses
  for (GlobalVariable* subTI : classToSubclasses[TI]) {
    SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "------> recursing on " << subTI->getName() << "\n");
    if (classToVBaseOffsetOffset[subTI].find(TI) != classToVBaseOffsetOffset[subTI].end()) {
      if (vbaseOffsetOffset != INT_MAX) {
        report_fatal_error("discovered another virtual base class on this path!");
      }
      // TI is a virtual base of subTI
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << TI->getName() << " is a virtual base of " << subTI->getName() << "\n");
      int vbaseOffOff = classToVBaseOffsetOffset[subTI][TI];
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vbaseOffsetOffset: " << vbaseOffOff << "\n");
      // subObjOffset now becomes the vbaseSubObjOffset and subObjOffset is 0
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "vbaseSubObjOffset: " << subObjOffset << ", subObjOffset: 0\n");
      findAllCalleesInSubClassesHelper(C, subTI, staticTypeTI, vtableIdx, vbaseOffOff, 0, subObjOffset, collectingCallees, callees);
    }
    else {
      // update subObjOffset by adding the offset for TI in subTI
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << TI->getName() << " is a non-virtual base of " << subTI->getName() << "\n");
      int subObjOff = subObjOffset + (skip ? 0 : classToBaseOffset[subTI][TI]);
      SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "subObjOffset: " << subObjOff << "\n");
      findAllCalleesInSubClassesHelper(C, subTI, staticTypeTI, vtableIdx, vbaseOffsetOffset, subObjOff, vbaseSubObjOffset, collectingCallees, callees);
    }
  }
}

Function* ClassHierarchyUtils::extractFunctionFromThunk(Function* F) {
  if (F->getName().startswith("_ZTh") || F->getName().startswith("_ZTv")) {
    // F is a thunk
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      // the first non-intrinsic call will be to the actual function
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (!isa<IntrinsicInst>(C)) {
          Function* callee = CallGraphUtils::getDirectCallee(C);
          SDEBUG("soaap.util.classhierarchy", 3, dbgs() << "Replacing thunk " << F->getName() << " with " << callee->getName() << "\n");
          if (callee == NULL) {
            report_fatal_error("Function extracted from thunk is NULL");
          }
          return callee;
        }
      }
    }
  }
  return F;
}

Value* ClassHierarchyUtils::getMDNodeOperandValue(MDNode* N, unsigned I) {
  Metadata* MD = N->getOperand(I);
  ValueAsMetadata* VMD = cast<ValueAsMetadata>(MD);
  return VMD->getValue();
}
