#include "ClassDebugInfoPass.h"
#include "Util/CallGraphUtils.h"
#include "Util/ClassHierarchyUtils.h"
#include "Util/DebugUtils.h"
#include "llvm/DebugInfo.h"
#include "llvm/Analysis/ProfileInfo.h"
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
        GlobalVariable* cVTableVar;
        bool instHasMetadata = false;
        if (MDNode* N = I->getMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND)) {
          cVTableVar = cast<GlobalVariable>(N->getOperand(0));
          instHasMetadata = true;
        }
        else if (MDNode* N = I->getMetadata(SOAAP_VTABLE_NAME_MDNODE_KIND)) {
          ConstantDataArray* classTypeIdConstant = cast<ConstantDataArray>(N->getOperand(0));
          string classTypeIdStr = classTypeIdConstant->getAsString().str();
          DEBUG(dbgs() << "classTypeIdStr: " << classTypeIdStr << "\n");
          cVTableVar = M.getGlobalVariable(classTypeIdStr.replace(0, 4, "_ZTV"));
          instHasMetadata = true;
        }
        if (instHasMetadata) {
          if (cVTableVar == NULL) {
            // So far the reason for this has been because the mangled name we infer 
            // during the ClassDebugInfoPass is incorrect. This is probably because 
            // the code performs a static_cast to a type that is not a subtype of 
            // the variable's static type (such as in a template class). For chromium, this 
            // tends to occur in WebKit's  WFT::RefCounted class that performs the following
            // call that leads to T's destructor being invoked:
            //
            //  delete static_cast<T*>(this)
            // 
            DEBUG(dbgs() << "ERROR: cVTableVar is NULL\n");
            DEBUG(I->dump());
            I->setMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND, NULL);
            I->setMetadata(SOAAP_VTABLE_NAME_MDNODE_KIND, NULL);
            continue;
          }
          CallInst* C = cast<CallInst>(&*I);
          callToCalleesCache[C] = findAllCalleesForVirtualCall(C, cVTableVar, M);
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

  DEBUG(dbgs() << "Call: " << *C << "\n");
  if (LoadInst* calledVal = dyn_cast<LoadInst>(C->getCalledValue())) {
    if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(calledVal->getPointerOperand())) {
      if (ConstantInt* cVTableIdxVal = dyn_cast<ConstantInt>(gep->getOperand(1))) {
        int cVTableIdx = cVTableIdxVal->getSExtValue();
        DEBUG(dbgs() << "relative cVTableIdx: " << cVTableIdx << "\n");
        
        // If the function being called is defined in a base class, then this 
        // might be a vtable call into a secondary vtable (i.e. the vtable of
        // a base class/sub-object, unless it is a primary base) then the 
        // compiler will have adjusted the address of "this" to point to the 
        // subobject's vtable pointer in the object and the vtable index will
        // be relative to that. We check to see if that adjustment did occur 
        // and if so then use that as the static type. This is what the 
        // code looks like for when calling a virtual function via a subobject:

        // %9 = load %"class.box::D"** %d, align 8, !dbg !76
        // %10 = bitcast %"class.box::D"* %9 to i8*, !dbg !76
        // %add.ptr = getelementptr inbounds i8* %10, i64 16, !dbg !76
        // %11 = bitcast i8* %add.ptr to %"class.box::B"*, !dbg !76
        // %12 = bitcast %"class.box::B"* %11 to void (%"class.box::B"*)***, !dbg !76
        // %vtable4 = load void (%"class.box::B"*)*** %12, !dbg !76
        // %vfn5 = getelementptr inbounds void (%"class.box::B"*)** %vtable4, i64 0, !dbg !76
        // %13 = load void (%"class.box::B"*)** %vfn5, !dbg !76
        // call void %13(%"class.box::B"* %11), !dbg !76
        int subObjOffset = 0;
        if (LoadInst* cVTableLoad = dyn_cast<LoadInst>(gep->getPointerOperand()->stripPointerCasts())) {
          if (GetElementPtrInst* gep2 = dyn_cast<GetElementPtrInst>(cVTableLoad->getPointerOperand()->stripPointerCasts())) {
            if (ConstantInt* subObjOffsetVal = dyn_cast<ConstantInt>(gep2->getOperand(1))) {
              subObjOffset = subObjOffsetVal->getSExtValue();
              DEBUG(dbgs() << "subObjOffset: " << subObjOffset << "\n");
              //vtableOffset = vTableToSecondaryVTableMaps[cVTableVar][subObjOffset]; // starting idx of secondary vtable
            }
          }
        }

        // find all implementations in reciever's class (corresponding to the
        // static type) as well as all descendent classes. Note: callees will
        // be at same idx in all vtables
        GlobalVariable* cClazzTI = vTableToTypeInfo[cVTableVar];
        findAllCalleesInSubClasses(C, cClazzTI, cVTableIdx, subObjOffset, callees);
      }
      else {
        DEBUG(dbgs() << "vtable idx is NOT a ConstantInt\n");
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

void ClassHierarchyUtils::findAllCalleesInSubClasses(CallInst* C, GlobalVariable* TI, int vtableIdx, int subObjOffset, FunctionVector& callees) {
  DEBUG(dbgs() << "Looking for func at vtable idx " << vtableIdx << " in " << TI->getName() << " (subObjOffset=" << subObjOffset << ")\n");
  GlobalVariable* vTableVar = typeInfoToVTable[TI];
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
  bool skip = false;
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
        // The vtable entry may no be a function if we're looking for a function
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
  for (GlobalVariable* subTI : classToSubclasses[TI]) {
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

//
// Format of file is as follows (for each function):
//
// <name of function>
// <number of virtual calls>
// <call index_i>
// <number of callees>
// <callee_1>
// ...
// <callee_n>
//

void ClassHierarchyUtils::dumpVirtualCalleeInformation(Module& M, string filename) {
  ofstream file(filename.c_str(), ios::out);
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    int numVCalls = 0; // number of soaap-annotated function calls
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (I->getMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND) || I->getMetadata(SOAAP_VTABLE_NAME_MDNODE_KIND)) {
        numVCalls++;
      }
    }
    if (numVCalls > 0) {
      file << F->getName().str() << "\n";
      file << numVCalls << "\n";
      int callIdx = 0;
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (I->getMetadata(SOAAP_VTABLE_VAR_MDNODE_KIND) || I->getMetadata(SOAAP_VTABLE_NAME_MDNODE_KIND)) {
            file << callIdx << "\n"; // callee index
            FunctionVector callees = callToCalleesCache[C];
            file << callees.size() << "\n"; // number of callees
            for (Function* callee : callees) {
              file << callee->getName().str() << "\n";
            }
          }
          callIdx++;
        }
      }
    }
  }
}

void ClassHierarchyUtils::readVirtualCalleeInformation(Module& M, string filename) {
  string line;
  ifstream ifile(filename.c_str(), ios::in);
  if (ifile.is_open()) {
    while (!ifile.eof()) {
      getline(ifile, line); // name of func
      string funcName = line;
      dbgs() << INDENT_1 << "Func name: " << funcName << "\n";
      if (Function* F = M.getFunction(funcName)) {
        getline(ifile, line); // num of calls
        int numVCalls = atoi(line.c_str());
        if (numVCalls > 0) {
          // first cache all callinst's in F
          map<int,CallInst*> callsMap;
          int callIdx = 0;
          for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
            if (CallInst* C = dyn_cast<CallInst>(&*I)) {
              callsMap[callIdx++] = C;
            }
          }
          // now read in callee information for specific callinst's 
          // as specified by indexes
          for (int j=0; j<numVCalls; j++) {
            getline(ifile, line); // call idx
            int vcallIdx = atoi(line.c_str());
            dbgs() << INDENT_2 << "CallIdx: " << vcallIdx << "\n";
            getline(ifile, line); // num of callees
            int numCallees = atoi(line.c_str());
            dbgs() << INDENT_2 << "Number of callees: " << numCallees << "\n";
            if (numCallees > 0) {
              // read in callees for this call
              FunctionVector callees;
              for (int k=0; k<numCallees; k++) {
                getline(ifile, line); // name of callee
                string calleeName = line;
                dbgs() << INDENT_3 << "Callee: " << calleeName << "\n";
                if (Function* callee = M.getFunction(calleeName)) {
                  callees.push_back(callee);
                }
                else {
                  dbgs() << "Callee " << calleeName << " does not exist\n";
                }
              }
              CallInst* C = callsMap[vcallIdx];
              callToCalleesCache[C] = callees;
            }
          }
        }
      }
      else {
        dbgs() << "Could not find function: " << funcName << "\n";
      }
    }
    cachingDone = true;
  }
}
