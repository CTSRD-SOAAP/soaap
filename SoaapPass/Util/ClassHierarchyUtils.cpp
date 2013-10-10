#include "Util/ClassHierarchyUtils.h"
#include "llvm/DebugInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/ProfileInfo.h"
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

StringVector ClassHierarchyUtils::classes;
map<string,StringVector> ClassHierarchyUtils::classToSubclasses;
map<string,StringVector> ClassHierarchyUtils::classToDescendents;

void ClassHierarchyUtils::findClassHierarchy(Module& M) {
  NamedMDNode* NMD = M.getNamedMetadata("llvm.dbg.cu");
  //ostringstream ss;
  for (int i=0; i<NMD->getNumOperands(); i++) {
    DICompileUnit CU(NMD->getOperand(i));
    DIArray types = CU.getRetainedTypes();
    for (int j=0; j<types.getNumElements(); j++) {
      DICompositeType clazz(types.getElement(j));
      if (clazz.getTag() == dwarf::DW_TAG_class_type) {
        string clazzName = clazz.getIdentifier()->getString().str();
        classes.push_back(clazzName);
        DEBUG(dbgs() << "Class: " << clazzName << "\n");
        DIArray members = clazz.getTypeArray();
        for (int k=0; k<members.getNumElements(); k++) {
          DIDescriptor member(members.getElement(k));
          if (member.getTag() == dwarf::DW_TAG_inheritance) {
            DIDerivedType inheritance(member);
            DICompositeType base(inheritance.getTypeDerivedFrom());
            string baseName = base.getIdentifier()->getString().str();
            DEBUG(dbgs() << "    Base: " << baseName << "\n");
            classToSubclasses[baseName].push_back(clazzName);
          }
        }
      }
    }
  }

  DEBUG(ppClassHierarchy(classToSubclasses));

  // calculate transitive closure of hierarchy
  calculateTransitiveClosure();

  DEBUG(ppClassHierarchy(classToDescendents));
}

void ClassHierarchyUtils::calculateTransitiveClosure() {
  // Easiest approach is probably a fixed-point computation.
  // Initial approximation is the subclass relation.
  classToDescendents = classToSubclasses;

  bool change = false;
  do {
    change = false;
    for (map<string,StringVector>::iterator I=classToDescendents.begin(); I != classToDescendents.end(); I++) {
      string base = I->first;
      StringVector descendents = I->second;
      for (string c : descendents) {
        for (string cSub : classToSubclasses[c]) {
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

void ClassHierarchyUtils::ppClassHierarchy(map<string,StringVector>& classHierarchy) {
  // first find all classes that do not have subclasses
  StringVector baseClasses = classes;
  for (map<string,StringVector>::iterator I=classHierarchy.begin(); I != classHierarchy.end(); I++) {
    StringVector subclasses = I->second;
    for (string sc : subclasses) {
      baseClasses.erase(remove(baseClasses.begin(), baseClasses.end(), sc), baseClasses.end());
    }
  }

  for (string bc : baseClasses) {
    ppClassHierarchyHelper(bc, classHierarchy, 0);
  }
}

void ClassHierarchyUtils::ppClassHierarchyHelper(string c, map<string,StringVector>& classHierarchy, int nesting) {
  for (int i=0; i<nesting-1; i++) {
    dbgs() << "    ";
  }
  if (nesting > 0){
    dbgs() << " -> ";
  }
  int status = -4;
  string cCopy = c;

  char* demangled = abi::__cxa_demangle(cCopy.replace(0, 4, "_Z").c_str(), 0, 0, &status);
  dbgs() << (status == 0 ? demangled : c) << "\n";
  for (string sc : classHierarchy[c]) {
    ppClassHierarchyHelper(sc, classHierarchy, nesting+1);
  }
}

void ClassHierarchyUtils::findAllCalleesForVirtualCalls(Module& M) {
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    if (F->isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (!isa<IntrinsicInst>(&*I)) {
        if (CallInst* C = dyn_cast<CallInst>(&*I)) {
          if (C->getCalledFunction() == NULL) {
            DEBUG(dbgs() << "Potential candidate: " << *C << "\n");
            findAllCalleesForVirtualCall(C, M);
          }
        }
      }
    }
  }
}

FunctionVector ClassHierarchyUtils::findAllCalleesForVirtualCall(CallInst* C, Module& M) {
  
  FunctionVector callees;

  //TODO: determine whether this is a virtual function call or a normal c-style
  // function pointer call

  // if this is a virtual call, then we need to find the callee
  // Firstly, find the static type and the vtable index for the function being called
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
  //
  
  // We check to see if this is a c++ virtual call by looking for the above pattern.
  // We first check for the vtable lookup and then whether the receiver type is a 
  // c++ class. Finally, we check to see if the vtable global exists. If these 3
  // steps can be carried out then we conclude that this is a virtual call.

  // vtable check:
  LoadInst* callee = cast<LoadInst>(C->getCalledValue());
  if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(callee->getPointerOperand())) {
    int vtableIdx = cast<ConstantInt>(gep->getOperand(1))->getSExtValue();
    DEBUG(dbgs() << "receiverVar vtable idx: " << vtableIdx << "\n");
  
    // now check receiver type:
    // First arg will be the receiver 
    if (LoadInst* receiver = cast<LoadInst>(C->getArgOperand(0)->stripPointerCasts())) {
      Value* receiverVar = receiver->getPointerOperand();
      DEBUG(dbgs() << "receiverVar: " << *receiverVar << "\n");

      // To get the static type of the receiver var, look for the call to llvm.dbg.declare()
      // (While not difficult to do, llvm already provides a helper function to do this):
      DbgDeclareInst* dbgDecl = FindAllocaDbgDeclare(receiverVar);
      DEBUG(dbgs() << "receiverVar dbgDecl: " << *dbgDecl << "\n");

      DIVariable varDbg(dbgDecl->getVariable());
      DIDerivedType varPtrTypeDbg(varDbg.getType());
      DICompositeType varClassTypeDbg(varPtrTypeDbg.getTypeDerivedFrom());

      if (MDString* varClazzId = varClassTypeDbg.getIdentifier()) {
        string varClazzName = varClazzId->getString().str();
        DEBUG(dbgs() << "receiverVar class name: " << varClazzName << "\n");

        // Obtain the callees, starting from the receiver's static class type
        if (find(classes.begin(), classes.end(), varClazzName) != classes.end()) {
          StringVector descendents = classToDescendents[varClazzName];
          descendents.push_back(varClazzName); // add varClazzName itself
          for (string clazz : descendents) {
            string vtableName = convertTypeIdToVTableId(clazz);
            if (GlobalVariable* vtableVar = M.getGlobalVariable(vtableName)) {
              ConstantArray* vtable = cast<ConstantArray>(vtableVar->getInitializer());
              Function* vfunc = cast<Function>(vtable->getOperand(vtableIdx+2)->stripPointerCasts());
              DEBUG(dbgs() << "receiverVar class: " << clazz << "\n");
              DEBUG(dbgs() << "receiverVar vtable func: " << vfunc->getName() << "\n");
              callees.push_back(vfunc);
            }
          }
        }
      }
    }
  }

  return callees;
}

string ClassHierarchyUtils::convertTypeIdToVTableId(string typeId) {
  // replace _ZTS with _ZTV
  return typeId.replace(0, 4, "_ZTV");
}
