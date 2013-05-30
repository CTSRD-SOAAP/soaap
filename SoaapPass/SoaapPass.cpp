#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/ilist.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"

#include "soaap.h"
#include "soaap_perf.h"

#include "Common/Typedefs.h"
#include "Common/Sandbox.h"
#include "Analysis/InfoFlow/AccessOriginAnalysis.h"
#include "Analysis/InfoFlow/SandboxPrivateAnalysis.h"
#include "Analysis/InfoFlow/ClassifiedAnalysis.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/SandboxUtils.h"
#include "Utils/ClassifiedUtils.h"
#include "Utils/PrettyPrinters.h"

#include <iostream>
#include <vector>
#include <climits>
#include <functional>

using namespace llvm;
using namespace std;

static cl::list<std::string> ClVulnerableVendors("soaap-vulnerable-vendors",
       cl::desc("Comma-separated list of vendors whose code should "
                "be treated as vulnerable"),
       cl::value_desc("list of vendors"), cl::CommaSeparated);

namespace soaap {

  struct SoaapPass : public ModulePass {

    static char ID;
    bool emPerf;

    map<GlobalVariable*,int> varToPerms;
    map<GlobalVariable*,int> globalVarToSandboxNames;

//    SmallVector<Instruction*,16> sandboxCreationPoints;
//    map<Instruction*,int> sandboxCreationPointToName;

    SandboxVector sandboxes;

    FunctionIntMap sandboxedMethodToOverhead;
    FunctionVector persistentSandboxEntryPoints;
    FunctionVector allSandboxEntryPoints;

    map<Function*,int> sandboxedMethodToNames;
    SmallVector<Function*,16> callgates;
    FunctionIntMap callgateToSandboxes;
    FunctionVector privAnnotMethods;
    FunctionVector privilegedMethods;
    FunctionVector allReachableMethods;
    FunctionVector sandboxedMethods;
    FunctionVector syscallReachableMethods;

    // classification stuff
    map<Function*,int> sandboxedMethodToClearances;

    // past-vulnerability stuff
    SmallVector<CallInst*,16> pastVulnAnnotatedPoints;
    FunctionVector pastVulnAnnotatedFuncs;
    map<Function*,string> pastVulnAnnotatedFuncToCVE;
    
    // provenance
    SmallVector<StringRef,16> vulnerableVendors;

    SoaapPass() : ModulePass(ID) {
      emPerf = false;
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      if (!emPerf) {
        AU.setPreservesCFG();
        AU.addRequired<CallGraph>();
        AU.addRequired<ProfileInfo>();
      }
    }

    virtual bool runOnModule(Module& M) {

      outs() << "* Running " << getPassName() << "\n";
    
      CallGraph& CG = getAnalysis<CallGraph>();
      ProfileInfo& PI = getAnalysis<ProfileInfo>();
      LLVMAnalyses::setCallGraphAnalysis(&CG);
      LLVMAnalyses::setProfileInfoAnalysis(&PI);
      
      //outs() << "* Adding dynamic call edges to callgraph (if available)\n";
      //loadDynamicCallEdges(M);

      outs() << "* Processing command-line options\n"; 
      processCmdLineArgs(M);

      outs() << "* Finding sandboxes\n";
      findSandboxes(M);

      outs() << "* Finding global variables\n";
      findSharedGlobalVariables(M);

      outs() << "* Finding privileged annotations\n";
      findPrivilegedAnnotations(M);

      outs() << "* Finding callgates\n";
      findCallgates(M);

      outs() << "* Finding past vulnerability annotations\n";
      findPastVulnerabilityAnnotations(M);

      outs() << "* Finding code provenanace annotations\n";
      findCodeProvenanaceAnnotations(M);

      if (emPerf) {
        instrumentPerfEmul(M);
      }
      else {
        // do the checks statically

        outs() << "* Calculating sandboxed methods\n";
        calculateSandboxedMethods(M);
        outs() << "   " << sandboxedMethods.size() << " methods found\n";

        outs() << "* Calculating privileged methods\n";
        calculatePrivilegedMethods(M);
        
        outs() << "* Checking global variable accesses\n";
        checkGlobalVariables(M);

        outs() << "* Checking file descriptor accesses\n";
        outs() << "   Calculating syscall-reachable methods\n";
        calculateSyscallReachableMethods(M);
        outs() << "   Found " << syscallReachableMethods.size() << " methods\n";
        checkFileDescriptors(M);

        outs() << "* Checking propagation of data from sandboxes to privileged components\n";
        checkOriginOfAccesses(M);
        
        outs() << "* Checking propagation of classified data\n";
        checkPropagationOfClassifiedData(M);

        outs() << "* Checking propagation of sandbox-private data\n";
        checkPropagationOfSandboxPrivateData(M);

        outs() << "* Checking rights leaked by past vulnerable code\n";
        checkLeakedRights(M);

        outs() << "* Checking for calls to privileged functions from sandboxes\n";
        checkPrivilegedCalls(M);
      }

      //WORKAROUND: remove calls to llvm.ptr.annotate.p0i8, otherwise LLVM will
      //            crash when generating object code.
      if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
        outs() << "BUG WORKAROUND: Removing calls to intrinisc @llvm.ptr.annotation.p0i8\n";
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          IntrinsicInst* intrinsicCall = dyn_cast<IntrinsicInst>(u.getUse().getUser());
          BasicBlock::iterator ii(intrinsicCall);
          ReplaceInstWithValue(intrinsicCall->getParent()->getInstList(), ii, intrinsicCall->getOperand(0));
        }   
      }   

      return false;
    }

    void processCmdLineArgs(Module& M) {
      // process ClVulnerableVendors
      for (StringRef vendor : ClVulnerableVendors) {
        DEBUG(dbgs() << "Vulnerable vendor: " << vendor << "\n");
        vulnerableVendors.push_back(vendor);
      }
    }

    void findCodeProvenanaceAnnotations(Module& M) {
      // provenance is recorded in compilation units with a variable called
      // __soaap_provenance_var. This variable has hidden visibility so that
      // the linker doesn't complain when linking multiple compilation units
      // together.
      string provenanceVarBaseName = "__soaap_provenance";
      SmallVector<DICompileUnit,16> CUs;
      if (NamedMDNode* CUMDNodes = M.getNamedMetadata("llvm.dbg.cu")) {
        for(unsigned i = 0, e = CUMDNodes->getNumOperands(); i != e; i++) {
          MDNode* CUMDNode = CUMDNodes->getOperand(i);
          DICompileUnit CU(CUMDNode);
          CUs.push_back(CU);
        }
      }

      // each __soaap_provenance global var is defined in exactly one CU,
      // so remove a CU from CUs once it has be attributed to a var
      for (GlobalVariable& G : M.getGlobalList()) {
        if (G.getName().startswith(provenanceVarBaseName)) {
          dbgs() << "Found global variable " << G.getName() << "\n";
          GlobalVariable* provenanceStrVar = dyn_cast<GlobalVariable>(G.getInitializer()->stripPointerCasts());
          ConstantDataArray* provenanceArr = dyn_cast<ConstantDataArray>(provenanceStrVar->getInitializer());
          StringRef provenanceStr = provenanceArr->getAsCString(); // getAsString adds '\0' as an additional character
          dbgs() << "  Provenance: " << provenanceStr << "\n";

          if (find(vulnerableVendors.begin(), vulnerableVendors.end(), provenanceStr) != vulnerableVendors.end()) {
            outs() << "   " << provenanceStr << " is a vulnerable vendor\n";
            // Find out what the containing compilation unit and all its functions
            for(unsigned i = 0, ei = CUs.size(); i != ei; i++) {
              DICompileUnit CU = CUs[i];
              DIArray CUGlobals = CU.getGlobalVariables();
              for (unsigned j = 0, ej = CUGlobals.getNumElements(); j != ej; j++) {
                DIGlobalVariable CUGlobal = static_cast<DIGlobalVariable>(CUGlobals.getElement(j));
                if (CUGlobal.getGlobal() == &G) {
                  outs() << "    Found containing compile unit for " << G.getName() << ", list functions:\n";
                  DIArray CUSubs = CU.getSubprograms();
                  for (unsigned k = 0, ek = CUSubs.getNumElements(); k != ek; k++) {
                    DISubprogram CUSub = static_cast<DISubprogram>(CUSubs.getElement(k));
                    if (Function* CUFunc = CUSub.getFunction()) {
                      outs() << "      " << CUFunc->getName() << "()\n";
                      // record that CUFunc is vulnerable
                      if (find(pastVulnAnnotatedFuncs.begin(), pastVulnAnnotatedFuncs.end(), CUFunc) == pastVulnAnnotatedFuncs.end()) {
                        pastVulnAnnotatedFuncs.push_back(CUFunc);
                      }
                    }
                  }
                  CUs.erase(CUs.begin()+i); // remove CU from CUs
                  goto outerloop;
                }
              }
            }
            outerloop:
            ;
          }
        }
      }
    }

    void findPastVulnerabilityAnnotations(Module& M) {
      // Find all annotated code blocks. Note, we do this by inserting calls to 
      // the function __soaap_past_vulnerability_at_point. This function is declared
      // static to avoid linking problems when linking multiple modules. However,
      // as a result of this, a number may be appended to its name to make unique.
      // We therefore have to search through all the functions in M and find those
      // that start with __soaap_past_vulnerability_at_point
      string pastVulnFuncBaseName = "__soaap_past_vulnerability_at_point";
      for (Function& F : M.getFunctionList()) {
        if (F.getName().startswith(pastVulnFuncBaseName)) {
          dbgs() << "   Found " << F.getName() << " function\n";
          for (User::use_iterator u = F.use_begin(), e = F.use_end(); e!=u; u++) {
            if (CallInst* call = dyn_cast<CallInst>(u.getUse().getUser())) {
              //call->dump();
              pastVulnAnnotatedPoints.push_back(call);
            }
          }
        }
      }

      // Find all annotated functions
      GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations");
      if (lga != NULL) {
        ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
        for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
          ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

          // get the annotation value first
          GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
          ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
          StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

          GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
          if (isa<Function>(annotatedVal)) {
            Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
            if (annotationStrArrayCString.startswith(PAST_VULNERABILITY)) {
              dbgs() << "   Found annotated function " << annotatedFunc->getName() << "\n";
              pastVulnAnnotatedFuncs.push_back(annotatedFunc);
              pastVulnAnnotatedFuncToCVE[annotatedFunc] = annotationStrArrayCString.substr(strlen(PAST_VULNERABILITY)+1);
            }
          }
        }
      }
    }

    void checkPrivilegedCalls(Module& M) {
      for (Function* F : sandboxedMethods) {
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            if (CallInst* C = dyn_cast<CallInst>(&I)) {
              if (Function* Target = C->getCalledFunction()) {
                if (find(privAnnotMethods.begin(), privAnnotMethods.end(), Target) != privAnnotMethods.end()) {
                  // check if this sandbox is allowed to call the privileged function
                  DEBUG(dbgs() << "   Found privileged call: "); 
                  DEBUG(C->dump());
                  int enclosingSandboxes = sandboxedMethodToNames[F];
                  if (callgateToSandboxes.find(Target) != callgateToSandboxes.end()) {
                    DEBUG(dbgs() << "   Allowed sandboxes: " << SandboxUtils::stringifySandboxNames(callgateToSandboxes[Target]) << "\n");
                    // check if at least all sandboxes that F could be in are allowed to execute this privileged function
                    int allowedSandboxes = callgateToSandboxes[Target];
                    if ((enclosingSandboxes & allowedSandboxes) != enclosingSandboxes) {
                      outs() << " *** Sandboxes " << SandboxUtils::stringifySandboxNames(enclosingSandboxes) << " call privileged function \"" << Target->getName() << "\" that they are not allowed to. If intended, annotate this permission using the __soaap_callgates annotation.\n";
                      if (MDNode *N = C->getMetadata("dbg")) {  // Here I is an LLVM instruction
                        DILocation Loc(N);                      // DILocation is in DebugInfo.h
                        unsigned Line = Loc.getLineNumber();
                        StringRef File = Loc.getFilename();
                        outs() << " +++ Line " << Line << " of file " << File << "\n";
                      }
                    }
                  }
                  else {
                    outs() << " *** Sandboxes " << SandboxUtils::stringifySandboxNames(enclosingSandboxes) << " call privileged function \"" << Target->getName() << "\" that they are not allowed to. If intended, annotate this permission using the __soaap_callgates annotation.\n";
                    if (MDNode *N = C->getMetadata("dbg")) {  // Here I is an LLVM instruction
                      DILocation Loc(N);                      // DILocation is in DebugInfo.h
                      unsigned Line = Loc.getLineNumber();
                      StringRef File = Loc.getFilename();
                      outs() << " +++ Line " << Line << " of file " << File << "\n";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    void checkLeakedRights(Module& M) {
      for (CallInst* C : pastVulnAnnotatedPoints) {
        // for each vulnerability, find out whether it is in a sandbox or not 
        // and what the leaked rights are
        /*dbgs() << "   past vuln annot point: ";
        C->dump();*/
        Function* F = C->getParent()->getParent();
        if (GlobalVariable* CVEGlobal = dyn_cast<GlobalVariable>(C->getArgOperand(0)->stripPointerCasts())) {
          ConstantDataArray* CVEGlobalArr = dyn_cast<ConstantDataArray>(CVEGlobal->getInitializer());
          StringRef CVE = CVEGlobalArr->getAsCString();
          DEBUG(dbgs() << "Enclosing function is " << F->getName() << "\n");

          if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) != sandboxedMethods.end()) {
            outs() << "\n";
            outs() << " *** Sandboxed function \"" << F->getName() << "\" has a past-vulnerability annotation for \"" << CVE << "\".\n";
            outs() << " *** Another vulnerability here would not grant ambient authority to the attacker but would leak the following restricted rights:\n"     ;
            // F may run in a sandbox
            // find out what was passed into the sandbox (shared global variables, file descriptors)
            for (pair<GlobalVariable*,int> varPermPair : varToPerms) {
              GlobalVariable* G = varPermPair.first;
              int varPerms = varPermPair.second;
              StringRef varPermsStr = "";
              if (varPerms == (VAR_READ_MASK | VAR_WRITE_MASK)) {
                varPermsStr = "Read and write";
              }
              else if (varPerms & VAR_READ_MASK) {
                varPermsStr = "Read";
              }
              else if (varPerms) {
                varPermsStr = "Write";
              }
              if (varPermsStr != "")
                outs () << " +++ " << varPermsStr << " access to global variable \"" << G->getName() << "\"\n";
            }
            
            outs() << "TODO: check leaking of capabilities\n";
            /*
            for(Function* entryPoint : funcToSandboxEntryPoint[F]) {
              for (pair<const Value*,int> fdPermPair : fdToPerms) {
                const Argument* fd = dyn_cast<const Argument>(fdPermPair.first);
                int perms = fdPermPair.second;
                for (Function::const_arg_iterator AI=entryPoint->arg_begin(), AE=entryPoint->arg_end(); AI!=AE; AI++) {
                  if (fd == AI) {
                    StringRef fdPerms = "";
                    if (perms == (FD_READ_MASK | FD_WRITE_MASK)) {
                      fdPerms = "Read and write";
                    }
                    else if (perms & FD_READ_MASK) {
                      fdPerms = "Read";
                    }
                    else if (perms) {
                      fdPerms = "Write";
                    }
                    if (fdPerms != "")
                      outs() << " +++ " << fdPerms << " access to file descriptor \"" << fd->getName() << "\" passed into sandbox entrypoint \"" << entryPoint->getName() << "\"\n";
                  }

                }
              }
            }
            */
          }
          if (find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end()) {
            // enclosingFunc may run with ambient authority
            outs() << "\n";
            outs() << " *** Function \"" << F->getName() << "\" has a past-vulnerability annotation for \"" << CVE << "\".\n";
            outs() << " *** Another vulnerability here would leak ambient authority to the attacker including full\n";
            outs() << " *** network and file system access.\n"; 
            outs() << " Possible trace:\n";
            PrettyPrinters::ppPrivilegedPathToFunction(F, M);
            outs() << "\n\n";
          }
        }

      }
      for (Function* P : privilegedMethods) {
        if (find(pastVulnAnnotatedFuncs.begin(), pastVulnAnnotatedFuncs.end(), P) != pastVulnAnnotatedFuncs.end()) {
          string CVE = pastVulnAnnotatedFuncToCVE[P];
          // enclosingFunc may run with ambient authority
          outs() << "\n";
          outs() << " *** Function " << P->getName() << " has a past-vulnerability annotation for " << CVE << ".\n";
          outs() << " *** Another vulnerability here would leak ambient authority to the attacker including full\n";
          outs() << " *** network and file system access.\n"; 
          outs() << " Possible trace:\n";
          PrettyPrinters::ppPrivilegedPathToFunction(P, M);
        }
      }

    }

    void loadDynamicCallEdges(Module& M) {
      if (ProfileInfo* PI = getAnalysisIfAvailable<ProfileInfo>()) {
        CallGraph& CG = getAnalysis<CallGraph>();
        for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
          if (F1->isDeclaration()) continue;
          for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
            if (CallInst* C = dyn_cast<CallInst>(&*I)) {
              C->dump();
              for (const Function* F2 : PI->getDynamicCallees(C)) {
                DEBUG(dbgs() << "F2: " << F2->getName() << "\n");
                CallGraphNode* F1Node = CG.getOrInsertFunction(F1);
                CallGraphNode* F2Node = CG.getOrInsertFunction(F2);
                DEBUG(dbgs() << "loadDynamicCallEdges: adding " << F1->getName() << " -> " << F2->getName() << "\n");
                F1Node->addCalledFunction(CallSite(C), F2Node);
              }
            }
          }
        }
      }
    }

    void calculateSandboxedMethods(Module& M) {
      sandboxedMethods = SandboxUtils::calculateSandboxedMethods(sandboxes);
    }

    /*
     * Find functions from which system calls are reachable.
     * This is so that when propagating capabilities through the callgraph
     * we can prune methods from which system calls are not reachable.
     * 
     * pre: calculateSandboxedMethods() has been run
     */
    void calculateSyscallReachableMethods(Module& M) {
      list<Function*> worklist;
      if (Function* read = M.getFunction("read"))
        worklist.push_back(read);
      if (Function* write = M.getFunction("write"))
        worklist.push_back(write);

      ProfileInfo* PI = getAnalysisIfAvailable<ProfileInfo>();

      /* process functions in worklist backwards from uses all the way back
         to outermost sandbox functions */
      while (!worklist.empty()) {
        Function* F = worklist.front();
        worklist.pop_front();

        // prune out functions not reachable from a sandbox
        if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) == sandboxedMethods.end())
          continue;

        // prune out functions already visited
        if (find(syscallReachableMethods.begin(), syscallReachableMethods.end(), F) != syscallReachableMethods.end()) 
          continue;

        if (!F->isDeclaration()) { // ignore the syscall itself (it will be declaration-only)
          DEBUG(dbgs() << "Adding " << F->getName() << " to syscallReachableMethods\n");
          syscallReachableMethods.push_back(F);
        }

        // Find all functions that call F and add them to the worklist.
        // Normally we could do this with the use_iterator, however there
        // may be dynamic edges too.
        for (Value::use_iterator I = F->use_begin(), E = F->use_end(); I != E; I++) {
          if (CallInst* C = dyn_cast<CallInst>(*I)) {
            Function* Caller = C->getParent()->getParent();
            if (find(worklist.begin(), worklist.end(), Caller) == worklist.end())
              worklist.push_back(Caller);
          }
        }
        if (PI) {
          for (const CallInst* C : PI->getDynamicCallers(F)) {
            Function* Caller = const_cast<Function*>(C->getParent()->getParent());
            if (find(worklist.begin(), worklist.end(), Caller) == worklist.end())
              worklist.push_back(Caller);
          }
        }
      }
    }

    void checkOriginOfAccesses(Module& M) {
      AccessOriginAnalysis analysis(allSandboxEntryPoints, privilegedMethods);
      analysis.doAnalysis(M);
    }

    /*
    void findSandboxCreationPoints(Module& M) {
      // look for calls to llvm.annotation.i32(NULL,"SOAAP_PERSISTENT_SANDBOX_CREATE",0,0)
      if (Function* AnnotationFn = M.getFunction("llvm.annotation.i32")) {
        for (Value::use_iterator I=AnnotationFn->use_begin(), E=AnnotationFn->use_end();
             (I != E) && isa<CallInst>(*I); I++) {
          CallInst* Call = cast<CallInst>(*I);
          // get name of sandbox in 2nd arg
          if (GlobalVariable* AnnotStrGlobal = dyn_cast<GlobalVariable>(Call->getArgOperand(1)->stripPointerCasts())) {
            ConstantDataArray* AnnotStrGlobalArr = dyn_cast<ConstantDataArray>(AnnotStrGlobal->getInitializer());
            StringRef AnnotStr = AnnotStrGlobalArr->getAsCString();
            if (AnnotStr.startswith(SOAAP_PERSISTENT_SANDBOX_CREATE)) {
              // sandbox-creation point
              StringRef sandboxName = AnnotStr.substr(strlen(SOAAP_PERSISTENT_SANDBOX_CREATE)+1);
              outs() << "      Sandbox name: " << sandboxName << "\n";
              SandboxUtils::assignBitIdxToSandboxName(sandboxName);
              sandboxCreationPointToName[Call] = (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
              sandboxCreationPoints.push_back(Call);
            }
          }
        }
      }
    }
    */

    void findPrivilegedAnnotations(Module& M) {
      if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
        ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
        for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
          ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

          // get the annotation value first
          GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
          ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
          StringRef annotationStrArrayCString = annotationStrArray->getAsCString();

          GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
          if (isa<Function>(annotatedVal)) {
            Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
            if (annotationStrArrayCString == SOAAP_PRIVILEGED) {
              outs() << "   Found function: " << annotatedFunc->getName() << "\n";
              privAnnotMethods.push_back(annotatedFunc);
            }
          }
        }
      }          
    }

    /*
     * Find functions that are annotated to be executed in persistent and
     * ephemeral sandboxes
     */
    void findSandboxes(Module& M) {
      sandboxes = SandboxUtils::findSandboxes(M);
//      Regex *sboxPerfRegex = new Regex("perf_overhead_\\(([0-9]{1,2})\\)",
//                                       true);
//      SmallVector<StringRef, 4> matches;
//
//      /*
//       * Function annotations are added to the global intrinsic array
//       * called llvm.global.annotations:
//       *
//       * @.str3 = private unnamed_addr constant [30 x i8] c"../../tests/test-param-decl.c\00", section "llvm.metadata"
//       * @.str5 = private unnamed_addr constant [8 x i8] c"sandbox_persistent\00", section "llvm.metadata"
//       *
//       * @llvm.global.annotations = appending global [1 x { i8*, i8*, i8*, i32 }]
//       *
//       * [{ i8*, i8*, i8*, i32 }
//       *  { i8* bitcast (void (i32, %struct.__sFILE*)* @sandboxed to i8*),  // function
//       *    i8* getelementptr inbounds ([8 x i8]* @.str5, i32 0, i32 0),  // function annotation
//       *    i8* getelementptr inbounds ([30 x i8]* @.str3, i32 0, i32 0),  // file
//       *    i32 5 }]    // line number
//       */
//      if (GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations")) {
//        ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
//        for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
//          ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());
//
//          // get the annotation value first
//          GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
//          ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
//          StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
//
//          GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
//          if (isa<Function>(annotatedVal)) {
//            Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
//            if (annotationStrArrayCString.startswith(SANDBOX_PERSISTENT)) {
//              outs() << "   Found persistent sandbox entry-point " << annotatedFunc->getName() << "\n";
//              persistentSandboxEntryPoints.push_back(annotatedFunc);
//              allSandboxEntryPoints.push_back(annotatedFunc);
//              // get name if one was specified
//              if (annotationStrArrayCString.size() > strlen(SANDBOX_PERSISTENT)) {
//                StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PERSISTENT)+1);
//                outs() << "      Sandbox name: " << sandboxName << "\n";
//                SandboxUtils::assignBitIdxToSandboxName(sandboxName);
//                sandboxEntryPointToName[annotatedFunc] = (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
//                DEBUG(dbgs() << "sandboxEntryPointToName[" << annotatedFunc->getName() << "]: " << sandboxEntryPointToName[annotatedFunc] << "\n");
//              }
//            }
//            else if (annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
//              outs() << "   Found ephemeral sandbox entry-point " << annotatedFunc->getName() << "\n";
//              ephemeralSandboxEntryPoints.push_back(annotatedFunc);
//              allSandboxEntryPoints.push_back(annotatedFunc);
//            }
//            else if (sboxPerfRegex->match(annotationStrArrayCString, &matches)) {
//              int overhead;
//              cout << "Threshold set to " << matches[1].str() <<
//                      "%\n";
//              matches[1].getAsInteger(0, overhead);
//              sandboxedMethodToOverhead[annotatedFunc] = overhead;
//            }
//            else if (annotationStrArrayCString.startswith(CLEARANCE)) {
//              StringRef className = annotationStrArrayCString.substr(strlen(CLEARANCE)+1);
//              outs() << "   Sandbox has clearance for \"" << className << "\"\n";
//              ClassifiedUtils::assignBitIdxToClassName(className);
//              sandboxedMethodToClearances[annotatedFunc] |= (1 << ClassifiedUtils::getBitIdxFromClassName(className));
//            }
//          }
//        }
//      }
    }

    /*
     * Find global variables that are annotated as being shared
     * and record how they are allowed to be accessed
     */
    void findSharedGlobalVariables(Module& M) {

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
      GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations");
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
              varToPerms[annotatedVar] |= VAR_READ_MASK;
              globalVarToSandboxNames[annotatedVar] |= (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
              dbgs() << "   Found annotated global var " << annotatedVar->getName() << "\n";
            }
            else if (annotationStrArrayCString.startswith(VAR_WRITE)) {
              StringRef sandboxName = annotationStrArrayCString.substr(strlen(VAR_WRITE)+1);
              varToPerms[annotatedVar] |= VAR_WRITE_MASK;
              globalVarToSandboxNames[annotatedVar] |= (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
              dbgs() << "   Found annotated global var " << annotatedVar->getName() << "\n";
            }
          }
        }
      }

    }

    void checkPropagationOfSandboxPrivateData(Module& M) {
      SandboxPrivateAnalysis analysis(privilegedMethods, sandboxedMethods, allReachableMethods, callgates, sandboxedMethodToNames);
      analysis.doAnalysis(M);
    }

    void checkPropagationOfClassifiedData(Module& M) {
      ClassifiedAnalysis analysis(sandboxedMethods, sandboxedMethodToClearances);
      analysis.doAnalysis(M);
    }

    void checkFileDescriptors(Module& M) {
      CapabilityAnalysis analysis(sandboxedMethods);
      analysis.doAnalysis(M);
    }

    /*
     * Find those functions that have been declared as being callgates
     * using the __callgates(...) macro. A callgate is a method whose
     * execution requires privileges different to those possessed by
     * the current sandbox.
     */
    void findCallgates(Module& M) {
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
      for (Function& F : M.getFunctionList()) {
        if (F.getName().startswith("__soaap_declare_callgates_helper_")) {
          DEBUG(dbgs() << "Found __soaap_declare_callgates_helper_\n");
          StringRef sandboxName = F.getName().substr(strlen("__soaap_declare_callgates_helper")+1);
          dbgs() << "   Sandbox name: " << sandboxName << "\n";
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
                outs() << "   Callgate " << i << " is " << callgate->getName() << "\n";
                SandboxUtils::assignBitIdxToSandboxName(sandboxName);
                callgateToSandboxes[callgate] |= (1 << SandboxUtils::getBitIdxFromSandboxName(sandboxName));
                callgates.push_back(callgate);
              }
            }
          }
        }
      }
    }

    void calculatePrivilegedMethods(Module& M) {
      CallGraph& CG = getAnalysis<CallGraph>();
      if (Function* MainFunc = M.getFunction("main")) {
        CallGraphNode* MainNode = CG[MainFunc];
        calculatePrivilegedMethodsHelper(MainNode);
      }
    }

    void calculatePrivilegedMethodsHelper(CallGraphNode* Node) {
      if (Function* F = Node->getFunction()) {
        // if a sandbox entry point, then ignore
        if (find(allSandboxEntryPoints.begin(), allSandboxEntryPoints.end(), F) != allSandboxEntryPoints.end())
          return;
        
        // if already visited this function, then ignore as cycle detected
        if (find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end())
          return;
  
        DEBUG(dbgs() << "Added " << F->getName() << " as privileged method\n");
        privilegedMethods.push_back(F);
        allReachableMethods.push_back(F);
  
        // recurse on callees
        for (CallGraphNode::iterator I=Node->begin(), E=Node->end(); I!=E; I++) {
          calculatePrivilegedMethodsHelper(I->second);
        }
      }
    }

    void checkGlobalVariables(Module& M) {

      // find all uses of global variables and check that they are allowed
      // as per the annotations
      for (Function* F : sandboxedMethods) {
        SmallVector<GlobalVariable*,10> alreadyReportedReads, alreadyReportedWrites;
        DEBUG(dbgs() << "   Sandbox-reachable function: " << F->getName().str() << "\n");
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
//            I.dump();
            if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
              if (GlobalVariable* gv = dyn_cast<GlobalVariable>(load->getPointerOperand())) {
                //outs() << "VAR_READ_MASK?: " << (varToPerms[gv] & VAR_READ_MASK) << ", sandbox-check: " << stringifySandboxNames(globalVarToSandboxNames[gv] & sandboxedMethodToNames[F]) << "\n";
                //if (gv->hasExternalLinkage()) continue; // not concerned with externs
                if (!(varToPerms[gv] & VAR_READ_MASK) || (globalVarToSandboxNames[gv] & sandboxedMethodToNames[F]) == 0) {
                  if (find(alreadyReportedReads.begin(), alreadyReportedReads.end(), gv) == alreadyReportedReads.end()) {
                    outs() << " *** Sandboxed method \"" << F->getName().str() << "\" read global variable \"" << gv->getName().str() << "\" but is not allowed to. If the access is intended, the variable needs to be annotated with __soaap_read_var.\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }
                    alreadyReportedReads.push_back(gv);
                    outs() << "\n";
                  }
                }
              }
            }
            else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
              if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
                if (gv->hasExternalLinkage()) continue; // not concerned with externs
                // check that the programmer has annotated that this
                // variable can be written to
                if (!(varToPerms[gv] & VAR_WRITE_MASK)) {
                  if (find(alreadyReportedWrites.begin(), alreadyReportedWrites.end(), gv) == alreadyReportedWrites.end()) {
                    outs() << " *** Sandboxed method \"" << F->getName().str() << "\" wrote to global variable \"" << gv->getName().str() << "\" but is not allowed to\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }
                    alreadyReportedWrites.push_back(gv);
                    outs() << "\n";
                  }
                }
              }
            }
//            I.dump();
//            cout << "Num operands: " << I.getNumOperands() << endl;
//            for (int i=0; i<I.getNumOperands(); i++) {
//              cout << "Operand " << i << ": " << endl;
//              I.getOperand(i)->dump();
//            }
          }
        }
      }

      // Look for writes to shared global variables in privileged methods
      // that will therefore not be seen by sandboxes (assuming that the
      // the sandbox process is forked at the start of main).
      for (Function* F : privilegedMethods) {
        DEBUG(dbgs() << "Privileged function: " << F->getName().str() << "\n");
        SmallVector<GlobalVariable*,10> alreadyReported;
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
              if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
                // check that the programmer has annotated that this
                // variable can be read from 
                if (varToPerms[gv] & VAR_READ_MASK) {
                  // check that this store is preceded by a sandbox_create annotation
                  int precedingSandboxCreations = 1;//findPrecedingSandboxCreations(store, F, M);
                  int varSandboxNames = globalVarToSandboxNames[gv];
                  int commonSandboxNames = precedingSandboxCreations & varSandboxNames;
                  DEBUG(dbgs() << "   Checking write to annotated variable " << gv->getName() << "\n");
                  DEBUG(dbgs() << "   preceding sandbox creations: " << SandboxUtils::stringifySandboxNames(precedingSandboxCreations) << ", varSandboxNames: " << SandboxUtils::stringifySandboxNames(varSandboxNames) << "\n");
                  if (commonSandboxNames) {
                    if (find(alreadyReported.begin(), alreadyReported.end(), gv) == alreadyReported.end()) {
                      outs() << " *** Write to shared variable \"" << gv->getName() << "\" outside sandbox in method \"" << F->getName() << "\" will not be seen by the sandboxes: " << SandboxUtils::stringifySandboxNames(commonSandboxNames) << ". Synchronisation is needed to to propagate this update to the sandbox.\n";
                      if (MDNode *N = I.getMetadata("dbg")) {
                        DILocation loc(N);
                        outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                      }
                      alreadyReported.push_back(gv);
                      outs() << "\n";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    /*
    int findPrecedingSandboxCreations(Instruction* I, Function* F, Module& M) {
      int result = 0;
      for (Instruction* J : sandboxCreationPoints) {
        Function* sandboxCreationFunc = J->getParent()->getParent();
        DEBUG(dbgs() << "   Sandbox creation point: ");
        DEBUG(J->dump());
        DEBUG(dbgs() << "   Enclosing function: " << sandboxCreationFunc->getName() << "\n");
        if (F == sandboxCreationFunc) {
          // check that J is reachable from I
          result |= (isReachableFrom(I, J, F) ? sandboxCreationPointToName[J] : 0);
        }
        else {
          // check that F is reachable from sandboxCreationFunc
          result |= (isReachableFrom(F, sandboxCreationFunc) ? sandboxCreationPointToName[J] : 0);
        }
      }
      return result;
    }
    */

    bool isReachableFrom(Instruction* I2, Instruction* I1, Function* F) {
      BasicBlock* B2 = I2->getParent();
      BasicBlock* B1 = I1->getParent();
      if (B1 == B2) {
        DEBUG(dbgs() << "   Same basic block\n");
        // same basic block
        bool I1Found = false;
        for (Instruction& I : B1->getInstList()) {
          //I.dump();
          if (&I == I1) {
            DEBUG(dbgs() << "   I1 found\n");
            I1Found = true;
          }
          else if (!I1Found && &I == I2) {
            return false;
          }
        }
        return true;
      }
      else {
        dbgs() << "   Different basic blocks\n";
        return isReachableFrom(B2, B1);
      }
    }

    bool isReachableFrom(BasicBlock* B2, BasicBlock* B1) {
      list<BasicBlock*> visited;
      return isReachableFromHelper(B2, B1, visited);
    }

    bool isReachableFromHelper(BasicBlock* B2, BasicBlock* Curr, list<BasicBlock*> visited) {
      if (Curr == B2)
        return true;
      else if (find(visited.begin(), visited.end(), Curr) != visited.end()) 
        return false;
      else {
        visited.push_back(Curr);
        for (succ_iterator SI = succ_begin(Curr), SE = succ_end(Curr); SI != SE; SI++) {
          BasicBlock* Succ = *SI;
          if (isReachableFromHelper(B2, Succ, visited))
            return true;
        }
        return false;
      }
    }

    bool isReachableFrom(Function* F2, Function* F1) {
      dbgs() << "   Checking if " << F2->getName() << " is reachable from " << F1->getName() << "\n";
      CallGraph& CG = getAnalysis<CallGraph>();
      CallGraphNode* F1Node = CG[F1];
      CallGraphNode* F2Node = CG[F2];
      list<CallGraphNode*> visited;
      return isReachableFromHelper(F1Node, F2Node, visited);
    }

    bool isReachableFromHelper(CallGraphNode* CurrNode, CallGraphNode* FinalNode, list<CallGraphNode*>& visited) {
      if (CurrNode == FinalNode)
        return true;
      else if (CurrNode->getFunction() == NULL) // non-function node (e.g. External node)
        return false;
      else if (find(visited.begin(), visited.end(), CurrNode) != visited.end()) // cycle
        return false;
      else {
        visited.push_back(CurrNode);
        for (CallGraphNode::iterator I = CurrNode->begin(), E = CurrNode->end(); I!=E; I++) {
          Value* V = I->first;
          if(CallInst* Call = dyn_cast_or_null<CallInst>(V)) {
            CallGraphNode* CalleeNode = I->second;
            if (Function* CalleeFunc = CalleeNode->getFunction()) {
              if (isReachableFromHelper(CalleeNode, FinalNode, visited)) {
                return true;
              }
            }
          }
        }
      }
      return false;
    }

		void instrumentPerfEmul(Module& M) {
			/* Get LLVM context */
			LLVMContext &C = M.getContext();

			/*
			 * Get the var.annotation intrinsic function from the current
			 * module.
			 */
			Function* FVA = M.getFunction("llvm.var.annotation");

			/* Insert instrumentation for emulating performance */
			Function* enterPersistentSandboxFn
				= M.getFunction("soaap_perf_enter_persistent_sbox");
			Function* enterEphemeralSandboxFn
				= M.getFunction("soaap_perf_enter_ephemeral_sbox");

			/*
			 * Iterate through sandboxed functions and apply the necessary
			 * instrumentation to emulate performance overhead.
			 */
			for (Function* F : allSandboxEntryPoints) {
				Argument* data_in = NULL;
				Argument* data_out = NULL;
				bool persistent = find(persistentSandboxEntryPoints.begin(),
					persistentSandboxEntryPoints.end(), F) !=
					persistentSandboxEntryPoints.end();
				Instruction* firstInst = F->getEntryBlock().getFirstNonPHI();

				/*
				 * Check if there are annotated parameters to sandboxed
				 * functions.
				 */
				if (FVA) {
					for (User::use_iterator u = FVA->use_begin(),
						e = FVA->use_end(); e!=u; u++) {
						User* user = u.getUse().getUser();
						if (isa<IntrinsicInst>(user)) {
							IntrinsicInst* annotateCall
								= dyn_cast<IntrinsicInst>(user);

							/* Get the enclosing function */
							Function* enclosingFunc
								= annotateCall->getParent()->getParent();

							/*
							 * If the enclosing function does not match the
							 * current sandboxed function in the outer loop
							 * continue.
							 */
							if (enclosingFunc != F)
								continue;

							/* Get the annotated variable as LLVM::Value */
							Value* annotatedVar
								= dyn_cast<Value>
								(annotateCall->getOperand(0)->stripPointerCasts());

							/* Get the annotation as string */
							GlobalVariable* annotationStrVar
								= dyn_cast<GlobalVariable>
								(annotateCall->getOperand(1)->stripPointerCasts());
							ConstantDataArray* annotationStrValArray
								= dyn_cast<ConstantDataArray>
								(annotationStrVar->getInitializer());
							StringRef annotationStrValCString
								= annotationStrValArray->getAsCString();

							/*
							 * Record which param was annotated. We have to do
							 * this because llvm creates a local var for the
							 * param by appending .addrN to the end of the param
							 * name and associates the annotation with the newly
							 * created local var i.e. see ifd and ifd.addr1
							 * above
							 */
							Argument* annotatedArg = NULL;

							for (Argument &arg : enclosingFunc->getArgumentList()) {
								if ((annotatedVar->getName().startswith(StringRef(Twine(arg.getName(), ".addr").str())))) {
									annotatedArg = &arg;
								}
							}

							if (annotatedArg != NULL) {
								if (annotationStrValCString == DATA_IN) {
									cout << "__DATA_IN annotated parameter"
										" found!\n";
									if (data_in) {
										errs() << "[XXX] Only one parameter "
											"should be annotated with __data_in"
											" attribute";
										return;
									}
									/* Get the data_in param */
									data_in = annotatedArg;
								}
								else if (annotationStrValCString == DATA_OUT) {
									cout << "__DATA_OUT annotated parameter"
										" found!\n";
									if (data_out) {
										errs() << "[XXX] Only one parameter "
											"should be annotated with __data_out"
											" attribute";
										return;
									}
									/* Get the data_in param */
									data_out = annotatedArg;
								}
							}
						}
					}
				}

				int perfThreshold = 0;
				if (sandboxedMethodToOverhead.find(F) !=
					sandboxedMethodToOverhead.end()) {
					perfThreshold = sandboxedMethodToOverhead[F];
				}

				/*
				 * Get type of "struct timespec" from the current module.
				 * Unfortunately, IRbuilder's CreateAlloca() method does not
				 * support inserting *before* a basic block and thus it is
				 * inconvenient to use it here.
				 *
				 */
				StructType* timespecTy = M.getTypeByName("struct.timespec");
				AllocaInst *start_ts = new AllocaInst(dyn_cast<Type>(timespecTy),
					Twine("soaap_start_ts"), firstInst);
				AllocaInst *sbox_ts = new AllocaInst(dyn_cast<Type>(timespecTy),
					Twine("soaap_sbox_ts"), firstInst);
				Value *argStartTs = dyn_cast <Value> (start_ts);
				Value *argSboxTs = dyn_cast <Value> (sbox_ts);

				/*
				 * Instrument block prologue to measure the sandboxing overhead.
				 */
				Function *perfOverheadFn
					= M.getFunction("soaap_perf_tic");
				CallInst *perfOverheadCall = CallInst::Create(perfOverheadFn,
					ArrayRef<Value*>(argStartTs));
				perfOverheadCall->insertBefore(firstInst);

				/*
				 * Pick the appropriate function to inject based on the
				 * annotations and perform the actual instrumentation in the
				 * sandboxed function prologue.
				 * NOTE: At the moment, we do not handle __data_out.
				 */
				CallInst* enterSandboxCall = NULL;
				if (data_in) {
					enterPersistentSandboxFn
						= M.getFunction("soaap_perf_enter_datain_persistent_sbox");
					enterEphemeralSandboxFn
						= M.getFunction("soaap_perf_enter_datain_ephemeral_sbox");
					enterSandboxCall = CallInst::Create(persistent
						? enterPersistentSandboxFn : enterEphemeralSandboxFn,
						ArrayRef<Value*>(data_in));
					enterSandboxCall->insertBefore(firstInst);
				} else {
					enterPersistentSandboxFn
						= M.getFunction("soaap_perf_enter_persistent_sbox");
					enterEphemeralSandboxFn
						= M.getFunction("soaap_perf_enter_ephemeral_sbox");
					enterSandboxCall = CallInst::Create(persistent
						? enterPersistentSandboxFn : enterEphemeralSandboxFn,
						ArrayRef<Value*>());
					enterSandboxCall->insertBefore(firstInst);
				}

				/*
				 * Inject instrumentation after the sandboxing emulation in
				 * order to measure the absolute overhead.
				 * Before that create the vector with the arguments needed.
				 */
				perfOverheadFn = M.getFunction("soaap_perf_overhead_toc");
				perfOverheadCall = CallInst::Create(perfOverheadFn,
					ArrayRef<Value*>(argSboxTs));
				perfOverheadCall->insertBefore(firstInst);

				ConstantInt *argPerfThreshold = NULL;
				if (perfThreshold)
					argPerfThreshold = ConstantInt::get(Type::getInt32Ty(C),
						perfThreshold, false);

				/*
				 * Inject instrumentation after the sandboxing emulation in
				 * order to measure the total execution time.
				 */
				SmallVector<Value*, 2> soaap_perf_overhead_args;
				soaap_perf_overhead_args.push_back(argStartTs);
				soaap_perf_overhead_args.push_back(argSboxTs);
				if (perfThreshold) {
					soaap_perf_overhead_args.push_back(dyn_cast<Value>
						(argPerfThreshold));
					perfOverheadFn = M.getFunction("soaap_perf_total_toc_thres");

				} else {
					perfOverheadFn = M.getFunction("soaap_perf_total_toc");
				}

				perfOverheadCall = CallInst::Create(perfOverheadFn,
					ArrayRef<Value*>(soaap_perf_overhead_args));

				// Get terminator instruction of the current basic block.
				for (BasicBlock& BB : F->getBasicBlockList()) {
					TerminatorInst* termInst = BB.getTerminator();
					if (isa<ReturnInst>(termInst)) {
						//BB is an exit block, instrument ret
						perfOverheadCall->insertBefore(termInst);
					}
				}
			}

			/*
			 * If there are running persistent sandboxed terminate them before
			 * exiting the program.  This is achieved when calling the
			 * appropriate library function with -1 as argument.
			 */
			if (!persistentSandboxEntryPoints.empty()) {
				Function* mainFn = M.getFunction("main");

				Function* createPersistentSandbox
					= M.getFunction("soaap_perf_create_persistent_sbox");
				Instruction* mainFirstInst = mainFn->getEntryBlock().getFirstNonPHI();
				if(mainFirstInst) {
					CallInst* createCall
						= CallInst::Create(createPersistentSandbox,
							ArrayRef<Value*>());
					createCall->insertBefore(mainFirstInst);
				}


				ConstantInt *arg = ConstantInt::get(Type::getInt32Ty(C),
					-1, true);

				Function* terminatePersistentSandbox
					= M.getFunction("soaap_perf_enter_datain_persistent_sbox");
				CallInst* terminateCall
					= CallInst::Create(terminatePersistentSandbox,
						ArrayRef<Value*>(dyn_cast<Value>(arg)));
				//CallInst* terminateCall
				//= CallInst::Create(terminatePersistentSandbox,
				//ArrayRef<Value*>());
				terminateCall->setTailCall();

				// Iterate over main's BasicBlocks and instrument all ret points
				for (BasicBlock& BB : mainFn->getBasicBlockList()) {
					TerminatorInst* mainLastInst = BB.getTerminator();
					if (isa<ReturnInst>(mainLastInst)) {
						//BB is an exit block, instrument ret
						terminateCall->insertBefore(mainLastInst);
					}
				}

			}
		}
	};

	char SoaapPass::ID = 0;
	static RegisterPass<SoaapPass> X("soaap", "Soaap Pass", false, false);

	void addPasses(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  		PM.add(new SoaapPass);
  	}

	RegisterStandardPasses S(PassManagerBuilder::EP_OptimizerLast, addPasses);

}
