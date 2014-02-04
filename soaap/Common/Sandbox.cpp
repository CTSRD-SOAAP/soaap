#include "Common/Sandbox.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
#include "Util/LLVMAnalyses.h"
#include "Util/PrettyPrinters.h"
#include "Util/SandboxUtils.h"
#include "soaap.h"

#include "llvm/DebugInfo.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace soaap;

Sandbox::Sandbox(string n, int i, Function* entry, bool p, Module& m, int o, int c) 
  : Context(CK_SANDBOX), name(n), nameIdx(i), entryPoint(entry), persistent(p), module(m), overhead(o), clearances(c) {
	DEBUG(dbgs() << INDENT_2 << "Finding sandboxed functions\n");
  findSandboxedFunctions();
	DEBUG(dbgs() << INDENT_2 << "Finding shared global variables\n");
  findSharedGlobalVariables();
	DEBUG(dbgs() << INDENT_2 << "Finding callgates\n");
  findCallgates();
	DEBUG(dbgs() << INDENT_2 << "Finding capabilities\n");
  findCapabilities();
  DEBUG(dbgs() << INDENT_2 << "Finding creation points\n");
  findCreationPoints();
}

Sandbox::Sandbox(string n, int i, InstVector& r, bool p, Module& m) 
  : Context(CK_SANDBOX), name(n), nameIdx(i), region(r), entryPoint(NULL), persistent(p), module(m), overhead(0), clearances(0) {
	DEBUG(dbgs() << INDENT_2 << "Finding sandboxed functions\n");
  findSandboxedFunctions();
	DEBUG(dbgs() << INDENT_2 << "Finding shared global variables\n");
  findSharedGlobalVariables();
	DEBUG(dbgs() << INDENT_2 << "Finding callgates\n");
  findCallgates();
	//DEBUG(dbgs() << INDENT_2 << "Finding capabilities\n");
  //findCapabilities();
  DEBUG(dbgs() << INDENT_2 << "Finding creation points\n");
  findCreationPoints();
}

Function* Sandbox::getEntryPoint() {
  return entryPoint;
}

string Sandbox::getName() {
  return name;
}

int Sandbox::getNameIdx() {
  return nameIdx;
}

FunctionVector Sandbox::getFunctions() {
  return functions;
}

GlobalVariableIntMap Sandbox::getGlobalVarPerms() {
  return sharedVarToPerms;
}

ValueIntMap Sandbox::getCapabilities() {
  return caps;
}

bool Sandbox::isAllowedToReadGlobalVar(GlobalVariable* gv) {
  return sharedVarToPerms.find(gv) != sharedVarToPerms.end() && sharedVarToPerms[gv] & VAR_READ_MASK;
}

FunctionVector Sandbox::getCallgates() {
  return callgates;
}

bool Sandbox::isCallgate(Function* F) {
  return find(callgates.begin(), callgates.end(), F) != callgates.end();
}

int Sandbox::getClearances() {
  return clearances;
}

int Sandbox::getOverhead() {
  return overhead;
}

bool Sandbox::isPersistent() {
  return persistent;
}

CallInstVector Sandbox::getCreationPoints() {
  return creationPoints;
}

void Sandbox::findSandboxedFunctions() {
  FunctionVector initialFuncs;
  if (entryPoint == NULL) {
    // scan the code region for the set of top-level functions being called
    for (Instruction* I : region) {
      if (CallInst* C  = dyn_cast<CallInst>(I)) {
        for (Function* F : CallGraphUtils::getCallees(C, module)) {
          if (find(initialFuncs.begin(), initialFuncs.end(), F) == initialFuncs.end()) {
            initialFuncs.push_back(F);
          }
        }
      }
    }
  }
  else {
    initialFuncs.push_back(entryPoint);
  }
  CallGraph* CG = LLVMAnalyses::getCallGraphAnalysis();
  for (Function* F : initialFuncs) {
    CallGraphNode* node = CG->getOrInsertFunction(F);
    findSandboxedFunctionsHelper(node);
  }
}

void Sandbox::findSandboxedFunctionsHelper(CallGraphNode* node) {
  Function* F = node->getFunction();
  DEBUG(dbgs() << INDENT_3 << "Visiting " << F->getName() << "\n");
   
  // check for cycle
  if (find(functions.begin(), functions.end(), F) != functions.end()) {
    return;
  }

  // check if entry point to another sandbox
  if (SandboxUtils::isSandboxEntryPoint(module, F) && F != entryPoint) {
    return;
  }

  functions.push_back(F);

  //  outs() << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
  for (CallGraphNode::iterator I=node->begin(), E=node->end(); I != E; I++) {
    Value* V = I->first;
    CallGraphNode* calleeNode = I->second;
    if (Function* calleeFunc = calleeNode->getFunction()) {
      findSandboxedFunctionsHelper(calleeNode);
    }
  }
}

/*
 * Find global variables that are annotated as being shared with this 
 * sandbox and record how they are allowed to be accessed
 */
void Sandbox::findSharedGlobalVariables() {

  /*
   * Global variable annotations are added to the global intrinsic
   * array called llvm.global.annotations:
   *
   * int fd __attribute__((annotate("var_read")));
   *
   * @fd = common global i32 0, align 4
   * @.str = private unnamed_addr constant [9 x i8] c"var_read\00", section "llvm.metadata"
   * @.str1 = private unnamed_addr constant [7 x i8] c"test-var.c\00", section "llvm.metadata"
   * @llvm.global.annotations = appending global [1 x { i8*, i8*, i8*, i32 }]
   *    [{ i8*, i8*, i8*, i32 }
   *     { i8* bitcast (i32* @fd to i8*),   // annotated global variable
   *       i8* getelementptr inbounds ([9 x i8]* @.str, i32 0, i32 0),  // annotation
   *       i8* getelementptr inbounds ([7 x i8]* @.str1, i32 0, i32 0), // file name
   *       i32 1 }] // line number
   */
  GlobalVariable* lga = module.getNamedGlobal("llvm.global.annotations");
  if (lga != NULL) {
    ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
    for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
      ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

      // get the annotation value first
      GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
      ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
      StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

      GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
      if (isa<GlobalVariable>(annotatedVal)) {
        GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
        if (annotationStrArrayCString.startswith(VAR_READ)) {
          StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_READ)+1);
          if (sandboxName == name) {
            sharedVarToPerms[annotatedVar] |= VAR_READ_MASK;
          }
          dbgs() << INDENT_3 << "Found annotated global var " << annotatedVar->getName() << "\n";
        }
        else if (annotationStrArrayCString.startswith(VAR_WRITE)) {
          StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_WRITE)+1);
          if (sandboxName == name) {
            sharedVarToPerms[annotatedVar] |= VAR_WRITE_MASK;
          }
          dbgs() << INDENT_3 << "Found annotated global var " << annotatedVar->getName() << "\n";
        }
      }
    }
  }
}

/*
 * Find those functions that have been declared as being callgates
 * using the __callgates(...) macro. A callgate is a method whose
 * execution requires privileges different to those possessed by
 * the current sandbox.
 */
void Sandbox::findCallgates() {
  /*
   * Callgates are declared using the variadic macro
   * __callgates(fns...), that passes the functions as arguments
   * to the function __soaap_declare_callgates_helper:
   *
   * #define __soaap_callgates(fns...) \
   *    void __soaap_declare_callgates() { \
   *      __soaap_declare_callgates_helper(0, fns); \
   *    }
   *
   * Hence, we must find the "call @__soaap_declare_callgates_helper"
   * instruction and obtain the list of functions from its arguments
   */
  for (Function& F : module.getFunctionList()) {
    if (F.getName().startswith("__soaap_declare_callgates_helper_")) {
      DEBUG(dbgs() << INDENT_3 << "Found __soaap_declare_callgates_helper_\n");
      StringRef sandboxName = F.getName().substr(strlen("__soaap_declare_callgates_helper")+1);
      dbgs() << INDENT_3 << "Sandbox name: " << sandboxName << "\n";
      if (sandboxName == name) {
        for (User::use_iterator u = F.use_begin(), e = F.use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<CallInst>(user)) {
            CallInst* annotateCallgatesCall = dyn_cast<CallInst>(user);
            /*
             * Start at 1 because we skip the first unused argument
             * (The C language requires that there be at least one
             * non-variable argument).
             */
            for (unsigned int i=1; i<annotateCallgatesCall->getNumArgOperands(); i++) {
              Function* callgate = dyn_cast<Function>(annotateCallgatesCall->getArgOperand(i)->stripPointerCasts());
              outs() << INDENT_3 << "Callgate " << i << " is " << callgate->getName() << "\n";
              callgates.push_back(callgate);
            }
          }
        }
      }
    }
  }
}

void Sandbox::findCapabilities() {
  /*
   * Find those file descriptor parameters that are shared with the
   * sandboxed method.
   *
   * These will be annotated parameters that are turned by
   * Clang/LLVM into calls to the intrinsic function
   * llvm.var.annotation, with the param as the arg. This is how
   * local variable annotations are represented in general in LLVM
   *
   * A parameter annotation looks like this:
   * void m(int ifd __fd_read) { ... }
   *
   * It is turned into an intrinsic call as follows:
   *
   * call void @llvm.var.annotation(
   *   i8* %ifd.addr1,      // param (llvm creates a local var for the param by appending .addrN to the end of the param name)
   *   i8* getelementptr inbounds ([8 x i8]* @.str2, i32 0, i32 0),  // annotation
   *   i8* getelementptr inbounds ([30 x i8]* @.str3, i32 0, i32 0),  // file name
   *   i32 5)              // line number
   *
   * @.str2 = private unnamed_addr constant [8 x i8] c"fd_read\00", section "llvm.metadata"
   * @.str3 = private unnamed_addr constant [30 x i8] c"../../tests/test-param-decl.c\00", section "llvm.metadata"
   */
   
  for (BasicBlock& BB : entryPoint->getBasicBlockList()) {
    for (Instruction& I : BB.getInstList()) {
      if (CallInst* call = dyn_cast<CallInst>(&I)) {
        if (Function* callee = call->getCalledFunction()) {
          if (callee->getName() == "llvm.var.annotation") {
            Value* annotatedVar = dyn_cast<Value>(call->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(call->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();

            DEBUG(dbgs() << INDENT_3 << "Annotation: " << annotationStrValCString << "\n");

            /*
             * Find out the enclosing function and record which
             * param was annotated. We have to do this because
             * llvm creates a local var for the param by appending
             * and associates the annotation with the newly created
             * local var i.e. see ifd and ifd.addr1 above
             */
            if (DbgDeclareInst* dbgDecl = FindAllocaDbgDeclare(annotatedVar)) {
              DIVariable varDbg(dbgDecl->getVariable());
              string annotatedVarName = varDbg.getName().str();

              // find the annotated parameter
              Argument* annotatedArg = NULL;
              for (Argument& arg : entryPoint->getArgumentList()) {
                if (arg.getName().str() == annotatedVarName) {
                  annotatedArg = &arg;
                }
              }

              if (annotatedArg != NULL) {
                if (annotationStrValCString == FD_READ) {
                  caps[annotatedArg] |= FD_READ_MASK;
                }
                else if (annotationStrValCString == FD_WRITE) {
                  caps[annotatedArg] |= FD_WRITE_MASK;
                }
                DEBUG(dbgs() << INDENT_3 << "Found annotated file descriptor " << annotatedArg->getName() << "\n");
              }
            }
          }
        }
      }
    }
  }
}

void Sandbox::findCreationPoints() {
  // look for calls to __builtin_annotation(SOAAP_PERSISTENT_SANDBOX_CREATE_<NAME_OF_SANDBOX>)
  if (Function* AnnotFunc = module.getFunction("llvm.annotation.i32")) {
    for (Value::use_iterator UI = AnnotFunc->use_begin(), UE = AnnotFunc->use_end(); UI != UE; ++UI) {
      User* U = UI.getUse().getUser();
      if (isa<IntrinsicInst>(U)) {
        IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(U);
        GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
        ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
        StringRef annotationStrValCString = annotationStrValArray->getAsCString();
        
        if (annotationStrValCString.startswith(SOAAP_PERSISTENT_SANDBOX_CREATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SOAAP_PERSISTENT_SANDBOX_CREATE)+1); //+1 because of _
          if (sandboxName == name) {
            DEBUG(dbgs() << INDENT_3 << "Found persistent creation point: "; annotateCall->dump(););
            creationPoints.push_back(annotateCall);
            persistent = true;
          }
        }
        else if (annotationStrValCString.startswith(SOAAP_EPHEMERAL_SANDBOX_CREATE)) {
          StringRef sandboxName = annotationStrValCString.substr(strlen(SOAAP_EPHEMERAL_SANDBOX_CREATE)+1); //+1 because of _
          if (sandboxName == name) {
            dbgs() << INDENT_3 << "Found ephemeral creation point: "; annotateCall->dump();;
            creationPoints.push_back(annotateCall);
            persistent = false;
          }
        }
      }
    }
  }

  validateEntryPointCalls();
}

// check that all entrypoint calls are dominated by a creation call
void Sandbox::validateEntryPointCalls() {
  // We traverse the whole-program CFG starting from main, treating sandbox 
  // creation-point calls as leaves (i.e. we don't traverse past them). If
  // after this, an entrypoint call is still reached, then a path has been
  // found without going through a creation-point call.
  if (Function* MainFunc = module.getFunction("main")) {
    BasicBlock& EntryBB = MainFunc->getEntryBlock();
    BasicBlockVector visited;
    InstTrace trace;
    validateEntryPointCallsHelper(&EntryBB, visited, trace);
  }
}

bool Sandbox::validateEntryPointCallsHelper(BasicBlock* BB, BasicBlockVector& visited, InstTrace& trace) {
  if (find(visited.begin(), visited.end(), BB) != visited.end()) {
    return false;
  }
  else {
    // Check instructions in BB (in order):
    //  - if a creation-point call, then stop.
    //  - if an entrypoint call, then flag an error.
    //  - if a function call, recurse on callee's entry bb
    // Otherwise, recurse on BB's successors
    visited.push_back(BB);
    for (Instruction& I : *BB) {
      if (find(creationPoints.begin(), creationPoints.end(), &I) != creationPoints.end()) {
        return true;
      }
      else if (CallInst* CI = dyn_cast<CallInst>(&I)) {
        trace.push_front(CI);
        FunctionVector callees = CallGraphUtils::getCallees(CI, module);
        for (Function* callee : callees) {
          if (callee == entryPoint) {
            // error
            outs() << "\n";
            outs() << " *** Found call to sandbox entrypoint \"" << entryPoint->getName() << "\" that is not preceded by sandbox creation\n";
            outs() << " Possible trace:\n";
            PrettyPrinters::ppTrace(trace);
            outs() << "\n";
            trace.pop_front();
            return false;
          }
          else if (!callee->isDeclaration()) {
            // recurse on callee's entry bb
            if (validateEntryPointCallsHelper(&callee->getEntryBlock(), visited, trace)) {
              trace.pop_front();
              return true; // all paths through callee have a creation-point
            }
          }
        }
        trace.pop_front();
      }
    }

    // Recurse on BB's successors
    bool creationOnAllPaths = (succ_begin(BB) != succ_end(BB)); // true <-> at least one successor bb
    for (succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; ++SI) {
      BasicBlock* SuccBB = *SI;
      creationOnAllPaths = creationOnAllPaths && validateEntryPointCallsHelper(SuccBB, visited, trace);
    }

    return creationOnAllPaths;
  }
}
