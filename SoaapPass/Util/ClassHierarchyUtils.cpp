#include "ClassDebugInfoPass.h"
#include "Util/ClassHierarchyUtils.h"
#include "llvm/DebugInfo.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

#include <sstream>
#include <cxxabi.h>

#include "soaap.h"

using namespace soaap;
using namespace llvm;
using namespace std;

GlobalVariableVector ClassHierarchyUtils::classes;
ClassHierarchy ClassHierarchyUtils::classToSubclasses;
ClassHierarchy ClassHierarchyUtils::classToDescendents;
map<CallInst*,FunctionVector> ClassHierarchyUtils::callToCalleesCache;
map<GlobalVariable*,GlobalVariable*> ClassHierarchyUtils::typeInfoToVTable;
map<GlobalVariable*,GlobalVariable*> ClassHierarchyUtils::vTableToTypeInfo;
bool ClassHierarchyUtils::cachingDone = false;

void ClassHierarchyUtils::findClassHierarchy(Module& M) {
  // extract call hierarchy using std::type_info structures rather
  // than debug info. The former works even for when there are anonymous
  // namespaces. type_info structs can be obtained from vtable globals.
  // (for more info, see http://mentorembedded.github.io/cxx-abi/abi.html)
  for (Module::global_iterator I=M.global_begin(), E=M.global_end(); I != E; I++) {
    GlobalVariable* G = &*I;
    if (G->getName().startswith("_ZTVN")) {
      if (G->hasInitializer()) {
        ConstantArray* Ginit = cast<ConstantArray>(G->getInitializer());
        // Ginit[1] is the type_info global for this vtable's type
        GlobalVariable* TI = cast<GlobalVariable>(Ginit->getOperand(1)->stripPointerCasts());
        //TI->dump();
        typeInfoToVTable[TI] = G;
        vTableToTypeInfo[G] = TI;
        processTypeInfo(TI); // process TI recursively (it contains refs to super-class TIs)
      }
    }
  }
  
  ppClassHierarchy(classToSubclasses);

  // calculate transitive closure of hierarchy
  calculateTransitiveClosure();

  ppClassHierarchy(classToDescendents);
}

void ClassHierarchyUtils::cacheAllCalleesForVirtualCalls(Module& M) {
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      GlobalVariable* cVTableVar;
      bool instHasMetadata = false;
      if (MDNode* N = I->getMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND)) {
        cVTableVar = cast<GlobalVariable>(N->getOperand(0));
        instHasMetadata = true;
      }
      else if (MDNode* N = I->getMetadata(SOAAP_VTABLE_NAME_MDNODE_KIND)) {
        ConstantDataArray* classTypeIdConstant = cast<ConstantDataArray>(N->getOperand(0));
        string classTypeIdStr = classTypeIdConstant->getAsString().str();
        dbgs() << "classTypeIdStr: " << classTypeIdStr << "\n";
        cVTableVar = M.getGlobalVariable(classTypeIdStr.replace(0, 4, "_ZTV"));
        instHasMetadata = true;
      }
      if (instHasMetadata) {
        if (cVTableVar == NULL) {
          dbgs() << "ERROR: cVTableVar is NULL\n";
        }
        CallInst* C = cast<CallInst>(&*I);
        callToCalleesCache[C] = findAllCalleesForVirtualCall(C, cVTableVar, M);
      }
    }
  }
  cachingDone = true;
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
    ConstantStruct* TIinit = cast<ConstantStruct>(TI->getInitializer());

    int TInumOperands = TIinit->getNumOperands();
    if (TInumOperands > 2) {
      // we have >= 1 base class(es).
      if (TInumOperands == 3) {
        // abi::__si_class_type_info
        GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(2)->stripPointerCasts());
        classToSubclasses[baseTI].push_back(TI);
        //dbgs() << "  " << *baseTI << "\n";
        processTypeInfo(baseTI);
      }
      else {
        // abi::__vmi_class_type_info
        // (skip over first two additional fields, which are "flags" and "base_count")
        for (int i=4; i<TInumOperands; i+=2) {
          GlobalVariable* baseTI = cast<GlobalVariable>(TIinit->getOperand(i)->stripPointerCasts());
          classToSubclasses[baseTI].push_back(TI);
          //dbgs() << "  " << *baseTI << "\n";
          processTypeInfo(baseTI);
        }
      }
    }
  }
}

void ClassHierarchyUtils::calculateTransitiveClosure() {
  // Easiest approach is probably a fixed-point computation.
  // Initial approximation is the subclass relation.
  classToDescendents = classToSubclasses;

  bool change = false;
  do {
    change = false;
    for (ClassHierarchy::iterator I=classToDescendents.begin(); I != classToDescendents.end(); I++) {
      GlobalVariable* base = I->first;
      GlobalVariableVector descendents = I->second;
      for (GlobalVariable* c : descendents) {
        for (GlobalVariable* cSub : classToSubclasses[c]) {
          if (find(descendents.begin(), descendents.end(), cSub) == descendents.end()) {
            descendents.push_back(cSub);
            change = true;
          }
        }
      }
      classToDescendents[base] = descendents;
    }
  } while (change);
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


FunctionVector ClassHierarchyUtils::findAllCalleesForVirtualCall(CallInst* C, GlobalVariable* cVTableVar, Module& M) {
  
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

  dbgs() << "Call: " << *C << "\n";
  if (LoadInst* calledVal = dyn_cast<LoadInst>(C->getCalledValue())) {
    if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(calledVal->getPointerOperand())) {
      if (!isa<ConstantInt>(gep->getOperand(1))) {
        dbgs() << "vtable idx is NOT a ConstantInt\n";
        C->dump();
        gep->getOperand(1)->dump();
      }
      if (ConstantInt* cVTableIdxVal = dyn_cast<ConstantInt>(gep->getOperand(1))) {
        int cVTableIdx = cVTableIdxVal->getSExtValue();
        
        // find all implementations in reciever's class (corresponding to the static type)
        // as well as all descendent classes. Note: callees will be at same idx in all vtables.
        GlobalVariable* cClazzTI = vTableToTypeInfo[cVTableVar];
        GlobalVariableVector descendents = classToDescendents[cClazzTI];
        descendents.push_back(cClazzTI);
        for (GlobalVariable* clazzTI : descendents) {
          dbgs() << "Looking for function at index " << cVTableIdx << " in " << clazzTI->getName() << "\n";
          GlobalVariable* clazzVTableVar = typeInfoToVTable[clazzTI];
          ConstantArray* clazzVTable = cast<ConstantArray>(clazzVTableVar->getInitializer());
          Value* clazzVTableElem = clazzVTable->getOperand(cVTableIdx+2)->stripPointerCasts();
          if (Function* callee = dyn_cast<Function>(clazzVTableElem)) {
            dbgs() << "  vtable entry is func: " << callee->getName() << "\n";
            if (find(callees.begin(), callees.end(), callee) == callees.end()) {
              callees.push_back(callee);
            }
          }
          else {
            dbgs() << "  vtable entry " << (cVTableIdx+2) << " is not a Function\n";
            clazzVTableElem->dump();
          }
        }
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
