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
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "soaap.h"
#include "soaap_perf.h"

#include <iostream>
#include <vector>
#include <climits>
#include <functional>

using namespace llvm;
using namespace std;
namespace soaap {

  struct SoaapPass : public ModulePass {

    static char ID;
    static const int UNINITIALISED = INT_MAX;
    static const int ORIGIN_PRIV = 0;
    static const int ORIGIN_SANDBOX = 1;
    bool modified;
    bool dynamic;
    bool emPerf;

    map<GlobalVariable*,int> varToPerms;
    map<const Value*,int> fdToPerms;
    
    map<Function*, int> sandboxedMethodToOverhead;
    SmallVector<Function*,16> persistentSandboxFuncs;
    SmallVector<Function*,16> ephemeralSandboxFuncs;
    SmallVector<Function*,32> sandboxEntryPoints;
    map<Function*,int> sandboxEntryPointToName;
    map<Function*,int> sandboxedMethodToNames;
    map<StringRef,int> sandboxNameToBitIdx;
    map<int,StringRef> bitIdxToSandboxName;
    int nextSandboxNameBitIdx = 0;
    SmallVector<Function*,16> callgates;
    SmallVector<Function*,16> privilegedMethods;
    SmallSet<Function*,32> allReachableMethods;
    SmallSet<Function*,16> sandboxedMethods;
    SmallSet<Function*,16> syscallReachableMethods;

    map<const Value*,int> origin;
    SmallVector<CallInst*,16> untrustedSources;

    // classification stuff
    map<StringRef,int> classToBitIdx;
    map<int,StringRef> bitIdxToClass;
    int nextClassBitIdx = 0;
    map<Function*,int> sandboxedMethodToClearances;
    map<const Value*,int> valueToClasses;
    map<GlobalVariable*,int> varToClasses;

    // sandbox-private stuff
    map<const Value*,int> valueToSandboxNames;
    map<GlobalVariable*,int> varToSandboxNames;

    SoaapPass() : ModulePass(ID) {
      modified = false;
      dynamic = false;
      emPerf = false;
    }

    // inner classes for propagate functions
    class PropagateFunction {
      public:
        virtual bool propagate(const Value* From, const Value* To) = 0; 
      protected:
        SoaapPass* parent;
        PropagateFunction(SoaapPass* p) : parent(p) { }
      };

    class OriginPropagateFunction : public PropagateFunction {
      public:
        OriginPropagateFunction(SoaapPass* p) : PropagateFunction(p) { }

        bool propagate(const Value* From, const Value* To) {
          DEBUG(dbgs() << "propagate called\n");
          if (parent->origin.find(To) == parent->origin.end()) {
            parent->origin[To] = parent->origin[From];
            return true; // return true to allow perms to propagate through
                         // regardless of whether the value was non-zero
          }
          else {
            int old = parent->origin[To];
            parent->origin[To] = parent->origin[From];
            return parent->origin[To] != old;
          }
        };
    };

    class FDPermsPropagateFunction : public PropagateFunction {
      public:
        FDPermsPropagateFunction(SoaapPass* p) : PropagateFunction(p) { }

        bool propagate(const Value* From, const Value* To) {
          if (parent->fdToPerms.find(To) == parent->fdToPerms.end()) {
            parent->fdToPerms[To] = parent->fdToPerms[From];
            return true; // return true to allow perms to propagate through
                         // regardless of whether the value was non-zero
          }                   
          else {
            int old = parent->fdToPerms[To];
            parent->fdToPerms[To] &= parent->fdToPerms[From];
            return parent->fdToPerms[To] != old;
          }      
        };
    };
    
    class ClassPropagateFunction : public PropagateFunction {
      public:
        ClassPropagateFunction(SoaapPass* p) : PropagateFunction(p) { }

        bool propagate(const Value* From, const Value* To) {
          if (parent->valueToClasses.find(To) == parent->valueToClasses.end()) {
            parent->valueToClasses[To] = parent->valueToClasses[From];
            return true; // return true to allow classes to propagate through
                         // regardless of whether the value was non-zero
          }                   
          else {
            int old = parent->valueToClasses[To];
            parent->valueToClasses[To] |= parent->valueToClasses[From];
            return parent->valueToClasses[To] != old;
          }      
        };
    };

    class SandboxPrivatePropagateFunction : public PropagateFunction {
      public:
        SandboxPrivatePropagateFunction(SoaapPass* p) : PropagateFunction(p) { }

        bool propagate(const Value* From, const Value* To) {
          if (parent->valueToSandboxNames.find(To) == parent->valueToSandboxNames.end()) {
            parent->valueToSandboxNames[To] = parent->valueToSandboxNames[From];
            return true; // return true to allow sandbox names to propagate through
                         // regardless of whether the value was non-zero
          }                   
          else {
            int old = parent->valueToSandboxNames[To];
            parent->valueToSandboxNames[To] |= parent->valueToSandboxNames[From];
            return parent->valueToSandboxNames[To] != old;
          }      
        };
    };

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      if (!dynamic) {
        AU.setPreservesCFG();
      }
      AU.addRequired<CallGraph>();
      AU.addRequired<ProfileInfo>();
    }

    virtual bool runOnModule(Module& M) {

      outs() << "* Running " << getPassName() << "\n";

      outs() << "* Finding sandbox methods\n";
      findSandboxedMethods(M);

      outs() << "* Finding global variables\n";
      findSharedGlobalVariables(M);

      outs() << "* Finding file descriptors\n";
      findSharedFileDescriptors(M);

      outs() << "* Finding callgates\n";
      findCallgates(M);

      outs() << "* Finding classifications\n";
      findClassifications(M);

      outs() << "* Finding sandbox-private data\n";
      findSandboxPrivateAnnotations(M);

      if (!sandboxEntryPoints.empty()) {
        if (dynamic && !emPerf) {
          // use valgrind
          instrumentValgrindClientRequests(M);
          generateCallgateValgrindWrappers(M);
        }
        else if (!dynamic && !emPerf) {
          // do the checks statically
          outs() << "* Adding dynamic call edges to callgraph (if available)\n";
          loadDynamicCallEdges(M);

          outs() << "* Calculating sandboxed methods\n";
          calculateSandboxedMethods(M);
          outs() << "   " << sandboxedMethods.size() << " methods found\n";

          outs() << "* Checking global variable accesses\n";
          checkGlobalVariables(M);

          outs() << "* Checking file descriptor accesses\n";
          outs() << "   Calculating syscall-reachable methods\n";
          calculateSyscallReachableMethods(M);
          outs() << "   Found " << syscallReachableMethods.size() << " methods\n";
          checkFileDescriptors(M);

          outs() << "* Calculating privileged methods\n";
          calculatePrivilegedMethods(M);

          outs() << "* Checking propagation of data from sandboxes to privileged components\n";
          checkOriginOfAccesses(M);
          
          outs() << "* Checking propagation of classified data\n";
          checkPropagationOfClassifiedData(M);

          outs() << "* Checking propagation of sandbox-private data\n";
          checkPropagationOfSandboxPrivateData(M);
        }
        else if (!dynamic && emPerf) {
          instrumentPerfEmul(M);
        }
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

      return modified;
    }

    void printPrivilegedPathToInstruction(Instruction* I, Module& M) {
      if (Function* MainFn = M.getFunction("main")) {
        // Find privileged path to instruction I, via a function that calls a sandboxed callee
        Function* Target = I->getParent()->getParent();
        CallGraph& CG = getAnalysis<CallGraph>();
        CallGraphNode* TargetNode = CG[Target];

        outs() << "  Possible causes\n";
        for (CallInst* C : untrustedSources ) {
          Function* Via = C->getParent()->getParent();
          DEBUG(outs() << MainFn->getName() << " -> " << Via->getName() << " -> " << Target->getName() << "\n");
          list<Instruction*> trace1 = findPathToFunc(MainFn, Via, NULL, -1);
          list<Instruction*> trace2 = findPathToFunc(Via, Target, &origin, ORIGIN_SANDBOX);
          // check that we have successfully been able to find a full trace!
          if (!trace1.empty() && !trace2.empty()) {
            printTaintSource(C);
            // append target instruction I and trace1, to the end of trace2
            trace2.push_front(I);
            trace2.insert(trace2.end(), trace1.begin(), trace1.end());
            prettyPrintTrace(trace2);
            outs() << "\n";
          }
        }

        outs() << "Unable to find a trace\n";
      }
    }

    list<Instruction*> findPathToFunc(Function* From, Function* To, map<const Value*,int>* shadow, int taint) {
      CallGraph& CG = getAnalysis<CallGraph>();
      CallGraphNode* FromNode = CG[From];
      CallGraphNode* ToNode = CG[To];
      list<CallGraphNode*> visited;
      list<Instruction*> trace;
      findPathToFuncHelper(FromNode, ToNode, trace, visited, shadow, taint);
      return trace;
    }

    bool findPathToFuncHelper(CallGraphNode* CurrNode, CallGraphNode* FinalNode, list<Instruction*>& trace, list<CallGraphNode*>& visited, map<const Value*,int>* shadow, int taint) {
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
            bool proceed = true;
            if (shadow) {
              // check that Call has at least one tainted arg
              int idx;
              for (idx = 0; idx < Call->getNumArgOperands(); idx++) {
                if((proceed = ((*shadow)[Call->getArgOperand(idx)->stripPointerCasts()] == taint))) {
                  break;
                }
              }
            }
            if (proceed && findPathToFuncHelper(CalleeNode, FinalNode, trace, visited, shadow, taint)) {
              // CurrNode is on a path to FinalNode, so prepend to the trace
              trace.push_back(Call);
              return true;
            }
          }
        }
        return false;
      }
    }

    void printTaintSource(CallInst* C) {
      outs() << "    Source of untrusted data:\n";
      Function* EnclosingFunc = cast<Function>(C->getParent()->getParent());
      if (MDNode *N = C->getMetadata("dbg")) {
        DILocation Loc(N);
        unsigned Line = Loc.getLineNumber();
        StringRef File = Loc.getFilename();
        unsigned FileOnlyIdx = File.find_last_of("/");
        StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
        outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
      }
    }

    void prettyPrintTrace(list<Instruction*>& trace) {
      outs() << "    Possible trace to untrusted function pointer call:\n";
      for (Instruction* I : trace) {
        Function* EnclosingFunc = cast<Function>(I->getParent()->getParent());
        if (MDNode *N = I->getMetadata("dbg")) {
          DILocation Loc(N);
          unsigned Line = Loc.getLineNumber();
          StringRef File = Loc.getFilename();
          unsigned FileOnlyIdx = File.find_last_of("/");
          StringRef FileOnly = FileOnlyIdx == -1 ? File : File.substr(FileOnlyIdx+1);
          outs() << "      " << EnclosingFunc->getName() << "(" << FileOnly << ":" << Line << ")\n";
        }
      }
    }

    void loadDynamicCallEdges(Module& M) {
      ProfileInfo* PI = getAnalysisIfAvailable<ProfileInfo>();
      if (PI) {
        CallGraph& CG = getAnalysis<CallGraph>();
        for (Module::iterator F1 = M.begin(), E1 = M.end(); F1 != E1; ++F1) {
          if (F1->isDeclaration()) continue;
          for (inst_iterator I = inst_begin(F1), E = inst_end(F1); I != E; ++I) {
            if (CallInst* C = dyn_cast<CallInst>(&*I)) {
              for (const Function* F2 : PI->getDynamicCallees(C)) {
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
          syscallReachableMethods.insert(F);
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

    void performDataflowAnalysis(Module& M, PropagateFunction& propagate, list<const Value*>& worklist) {

      // perform propagation until fixed point is reached
      CallGraph& CG = getAnalysis<CallGraph>();
      while (!worklist.empty()) {
        const Value* V = worklist.front();
        worklist.pop_front();
        DEBUG(outs() << "*** Popped " << V->getName() << "\n");
        DEBUG(V->dump());
        for (Value::const_use_iterator VI=V->use_begin(), VE=V->use_end(); VI != VE; VI++) {
          const Value* V2;
          DEBUG(VI->dump());
          if (const StoreInst* SI = dyn_cast<const StoreInst>(*VI)) {
            if (V == SI->getPointerOperand()) // to avoid infinite looping
              continue;
            V2 = SI->getPointerOperand();
          }
          else if (const CallInst* CI = dyn_cast<const CallInst>(*VI)) {
            // propagate to the callee function's argument
            // find the index of the use position
            //outs() << "CALL: ";
            //CI->dump();
            //Function* Caller = const_cast<Function*>(CI->getParent()->getParent());
            //outs() << "CALLER: " << Caller->getName() << "\n";
            if (Function* Callee = CI->getCalledFunction()) {
              propagateToCallee(CI, Callee, worklist, V, propagate);
            }
            else if (const Value* FP = CI->getCalledValue())  { // dynamic callees
              ProfileInfo* PI = getAnalysisIfAvailable<ProfileInfo>();
              if (PI) {
                DEBUG(dbgs() << "dynamic call edge propagation\n");
                DEBUG(CI->dump());
                list<const Function*> Callees = PI->getDynamicCallees(CI);
                DEBUG(dbgs() << "number of dynamic callees: " << Callees.size() << "\n");
                for (const Function* Callee : Callees) {
                  DEBUG(dbgs() << "  " << Callee->getName() << "\n");
                  propagateToCallee(CI, Callee, worklist, V, propagate);
                }
              }
            } 
            continue;
          }
          else {
            V2 = *VI;
          }
          if (propagate.propagate(V,V2)) { // propagate permissions
            if (find(worklist.begin(), worklist.end(), V2) == worklist.end()) {
              worklist.push_back(V2);
            }
          }
          DEBUG(outs() << "Propagating " << V->getName() << " to " << V2->getName() << "\n");
        }
      }
    }

    void checkOriginOfAccesses(Module& M) {

      // initialise worklist with values returned from sandboxes
      list<const Value*> worklist;
      for (Function* F : sandboxEntryPoints) {
        // find calls of F, if F actually returns something!
        if (!F->getReturnType()->isVoidTy()) {
          for (Value::use_iterator I=F->use_begin(), E=F->use_end(); I!=E; I++) {
            if (CallInst* C = dyn_cast<CallInst>(*I)) {
              worklist.push_back(C);
              origin[C] = ORIGIN_SANDBOX;
              untrustedSources.push_back(C);
            }
          }
        }
      }

      // transfer function
      OriginPropagateFunction propagateOrigin(this);
      
      // compute fixed point
      performDataflowAnalysis(M, propagateOrigin, worklist);

      // look for untrusted function pointer calls
      checkPrivilegedFunctionPointerCalls(M);
    }

    // check that no untrusted function pointers are called in privileged methods
    void checkPrivilegedFunctionPointerCalls(Module& M) {
      for (Function* F : privilegedMethods) {
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I!=E; ++I) {
          if (CallInst* C = dyn_cast<CallInst>(&*I)) {
            if (C->getCalledFunction() == NULL) {
              if (origin[C->getCalledValue()] == ORIGIN_SANDBOX) {
                Function* Caller = cast<Function>(C->getParent()->getParent());
                outs() << "\n *** Untrusted function pointer call in " << Caller->getName() << "\n";
                if (MDNode *N = C->getMetadata("dbg")) {  // Here I is an LLVM instruction
                  DILocation Loc(N);                      // DILocation is in DebugInfo.h
                  unsigned Line = Loc.getLineNumber();
                  StringRef File = Loc.getFilename();
                  StringRef Dir = Loc.getDirectory();
                  outs() << " +++ Line " << Line << " of file " << File << "\n";
                }
                printPrivilegedPathToInstruction(C, M);
              }
            }
          }
        }
      }
      outs() << "\n";
    }

    /*
     * Find functions that are annotated to be executed in persistent and
     * ephemeral sandboxes
     */
    void findSandboxedMethods(Module& M) {

      Regex *sboxPerfRegex = new Regex("perf_overhead_\\(([0-9]{1,2})\\)",
                                       true);
      SmallVector<StringRef, 4> matches;

      /*
       * Function annotations are added to the global intrinsic array
       * called llvm.global.annotations:
       *
       * @.str3 = private unnamed_addr constant [30 x i8] c"../../tests/test-param-decl.c\00", section "llvm.metadata"
       * @.str5 = private unnamed_addr constant [8 x i8] c"sandbox_persistent\00", section "llvm.metadata"
       *
       * @llvm.global.annotations = appending global [1 x { i8*, i8*, i8*, i32 }]
       *
       * [{ i8*, i8*, i8*, i32 }
       *  { i8* bitcast (void (i32, %struct.__sFILE*)* @sandboxed to i8*),  // function
       *    i8* getelementptr inbounds ([8 x i8]* @.str5, i32 0, i32 0),  // function annotation
       *    i8* getelementptr inbounds ([30 x i8]* @.str3, i32 0, i32 0),  // file
       *    i32 5 }]    // line number
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
          if (isa<Function>(annotatedVal)) {
            Function* annotatedFunc = dyn_cast<Function>(annotatedVal);
            if (annotationStrArrayCString.startswith(SANDBOX_PERSISTENT)) {
              outs() << "   Found persistent sandbox entry-point " << annotatedFunc->getName() << "\n";
              persistentSandboxFuncs.push_back(annotatedFunc);
              sandboxEntryPoints.push_back(annotatedFunc);
              // get name if one was specified
              if (annotationStrArrayCString.size() > strlen(SANDBOX_PERSISTENT)) {
                StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PERSISTENT)+1);
                outs() << "      Sandbox name: " << sandboxName << "\n";
                assignBitIdxToSandboxName(sandboxName);
                sandboxEntryPointToName[annotatedFunc] = (1 << sandboxNameToBitIdx[sandboxName]);
                DEBUG(dbgs() << "sandboxEntryPointToName[" << annotatedFunc->getName() << "]: " << sandboxEntryPointToName[annotatedFunc] << "\n");
              }
            }
            else if (annotationStrArrayCString.startswith(SANDBOX_EPHEMERAL)) {
              outs() << "   Found ephemeral sandbox entry-point " << annotatedFunc->getName() << "\n";
              persistentSandboxFuncs.push_back(annotatedFunc);
              ephemeralSandboxFuncs.push_back(annotatedFunc);
              sandboxEntryPoints.push_back(annotatedFunc);
            }
            else if (sboxPerfRegex->match(annotationStrArrayCString, &matches)) {
              int overhead;
              cout << "Threshold set to " << matches[1].str() <<
                      "%\n";
              matches[1].getAsInteger(0, overhead);
              sandboxedMethodToOverhead[annotatedFunc] = overhead;
            }
            else if (annotationStrArrayCString.startswith(CLEARANCE)) {
              StringRef className = annotationStrArrayCString.substr(strlen(CLEARANCE)+1);
              outs() << "   Sandbox has clearance for \"" << className << "\"\n";
              assignBitIdxToClassName(className);
              sandboxedMethodToClearances[annotatedFunc] |= (1 << classToBitIdx[className]);
            }
          }
        }
      }
      
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
            if (annotationStrArrayCString == VAR_READ) {
              varToPerms[annotatedVar] |= VAR_READ_MASK;
            }
            else if (annotationStrArrayCString == VAR_WRITE) {
              varToPerms[annotatedVar] |= VAR_WRITE_MASK;
            }
          }
        }
      }

    }

    /*
     * Find those file descriptor parameters that are shared with the
     * sandboxed method.
     */
    void findSharedFileDescriptors(Module& M) {

      /*
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
      if (Function* F = M.getFunction("llvm.var.annotation")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();

            /*
             * Find out the enclosing function and record which
             * param was annotated. We have to do this because
             * llvm creates a local var for the param by appending
             * .addrN to the end of the param name and associates
             * the annotation with the newly created local var
             * i.e. see ifd and ifd.addr1 above
             */
            Argument* annotatedArg = NULL;
            Function* enclosingFunc = annotateCall->getParent()->getParent();
            for (Argument &arg : enclosingFunc->getArgumentList()) {
              if ((annotatedVar->getName().startswith(StringRef(Twine(arg.getName(), ".addr").str())))) {
                annotatedArg = &arg;
              }
            }

            if (annotatedArg != NULL) {
              if (annotationStrValCString == FD_READ) {
                fdToPerms[annotatedArg] |= FD_READ_MASK;
              }
              else if (annotationStrValCString == FD_WRITE) {
                fdToPerms[annotatedArg] |= FD_WRITE_MASK;
              }
            }
          }
        }
      }

    }

    void checkPropagationOfSandboxPrivateData(Module& M) {

      // initialise with pointers to annotated fields and uses of annotated global variables
      list<const Value*> worklist;
      if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();
            
            if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
              StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
              int bitIdx = sandboxNameToBitIdx[sandboxName];
            
              DEBUG(dbgs() << "   Sandbox-private annotation " << annotationStrValCString << " found:\n");
            
              worklist.push_back(annotatedVar);
              valueToSandboxNames[annotatedVar] |= (1 << bitIdx);
            }
          }
        }
      }
      
      if (Function* F = M.getFunction("llvm.var.annotation")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();
            if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
              StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
              int bitIdx = sandboxNameToBitIdx[sandboxName];
            
              DEBUG(dbgs() << "   Sandbox-private annotation " << annotationStrValCString << " found:\n");
              DEBUG(annotatedVar->dump());
              worklist.push_back(annotatedVar);
              valueToSandboxNames[annotatedVar] |= (1 << bitIdx);
            }
          }
        }
      }

      for (map<GlobalVariable*,int>::iterator I=varToSandboxNames.begin(), E=varToSandboxNames.end(); I != E; I++) {
        GlobalVariable* var = I->first;
        int sandboxNames = I->second;
        // find all users of var and taint them
        for (User::use_iterator u = var->use_begin(), e = var->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (LoadInst* load = dyn_cast<LoadInst>(user)) {
            Value* v = load->getPointerOperand();
            DEBUG(dbgs() << "   Load of sandbox-private global variable " << var->getName() << " found; associated sandboxes are: " << stringifySandboxNames(sandboxNames) << "\n");
            DEBUG(load->dump());
            valueToSandboxNames[v] = sandboxNames; // could v have already been initialised above? (NO)
          }
        }
      }

      // transfer function
      SandboxPrivatePropagateFunction propagateSandboxPrivate(this);

      // compute fixed point
      performDataflowAnalysis(M, propagateSandboxPrivate, worklist);
      
      // validate that sandbox-private data is never accessed in other sandboxed contexts
      for (Function* F : allReachableMethods) {
        DEBUG(dbgs() << "Function: " << F->getName());
        if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) != sandboxedMethods.end()) {
          DEBUG(dbgs() << ", sandbox names: " << stringifySandboxNames(sandboxedMethodToNames[F]) << "\n");
        }
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            DEBUG(dbgs() << "   Instruction:\n");
            DEBUG(I.dump());
            if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
              Value* v = load->getPointerOperand();
              DEBUG(dbgs() << "      Value:\n");
              DEBUG(v->dump());
              DEBUG(dbgs() << "      Value names: " << valueToSandboxNames[v] << ", " << stringifySandboxNames(valueToSandboxNames[v]) << "\n");
              if (find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end()) {
                if (valueToSandboxNames[v] != 0) {
                  outs() << "\n *** Privileged method " << F->getName() << " read data value private to sandboxes: " << stringifySandboxNames(valueToSandboxNames[v]) << "\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                  }
                }
              }
              if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) != sandboxedMethods.end()) {
                if (!(valueToSandboxNames[v] == 0 || valueToSandboxNames[v] == sandboxedMethodToNames[F] || (valueToSandboxNames[v] & sandboxedMethodToNames[F]))) {
                  outs() << "\n *** Sandboxed method " << F->getName() << " read data value belonging to sandboxes: " << stringifySandboxNames(valueToSandboxNames[v]) << " but it executes in sandboxes: " << stringifySandboxNames(sandboxedMethodToNames[F]) << "\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                    DILocation loc(N);
                    outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                  }
                }
              }
            }
          }
        }
      }

      // Validate that data cannot leak out of a sandbox.
      // Currently, SOAAP looks for escapement via:
      //   1) Assignments to global variables.
      //   2) Arguments to functions for which there is no body (due to incomplete call graph).
      //   3) Arguments to functions of callgates.
      //   4) Arguments to functions that are executed in a different sandbox
      //      (i.e. cross-domain calls).
      //   5) Assignments to environment variables.
      //   6) Arguments to system calls
      for (Function* F : allReachableMethods) {
        DEBUG(dbgs() << "Function: " << F->getName());
        int sandboxNames = sandboxedMethodToNames[F];
        if (sandboxNames != 0) {
          DEBUG(dbgs() << ", sandbox names: " << stringifySandboxNames(sandboxedMethodToNames[F]) << "\n");

          for (BasicBlock& BB : F->getBasicBlockList()) {
            for (Instruction& I : BB.getInstList()) {
              DEBUG(dbgs() << "   Instruction:\n");
              DEBUG(I.dump());
              // if assignment to a global variable, then check taint of value
              // being assigned
              if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
                Value* lhs = store->getPointerOperand();
                if (GlobalVariable* gv = dyn_cast<GlobalVariable>(lhs)) {
                  Value* rhs = store->getValueOperand();
                  // if the rhs is private to the current sandbox, then flag an error
                  if (valueToSandboxNames[rhs] & sandboxNames) {
                    outs() << "\n *** Sandboxed method " << F->getName() << " executing in sandboxes: " << stringifySandboxNames(sandboxNames) << " may leak private data through global variable " << gv->getName() << "\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }
                  }
                }
              }
              else if (CallInst* call = dyn_cast<CallInst>(&I)) {
                // if this is a call to setenv, check the taint of the second argument
                if (Function* Callee = call->getCalledFunction()) {
                  if (Callee->isIntrinsic()) continue;
                  if (Callee->getName() == "setenv") {
                    Value* arg = call->getArgOperand(1);
                  
                    if (valueToSandboxNames[arg] & sandboxNames) {
                      outs() << "\n *** Sandboxed method " << F->getName() << " executing in sandboxes: " << stringifySandboxNames(sandboxNames) << " may leak private data through env var ";
                      if (GlobalVariable* envVarGlobal = dyn_cast<GlobalVariable>(call->getArgOperand(0)->stripPointerCasts())) {
                        ConstantDataArray* envVarArray = dyn_cast<ConstantDataArray>(envVarGlobal->getInitializer());
                        StringRef envVarName = envVarArray->getAsString();
                        outs() << "\"" << envVarName << "\"";
                      }
                      outs() << "\n";
                      if (MDNode *N = I.getMetadata("dbg")) {
                        DILocation loc(N);
                        outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                      }
                    }
                  }
                  else if (Callee->getBasicBlockList().empty()) {
                    // extern function
                    DEBUG(dbgs() << "Extern callee: " << Callee->getName() << "\n");
                    for (User::op_iterator AI=call->op_begin(), AE=call->op_end(); AI!=AE; AI++) {
                      Value* arg = dyn_cast<Value>(AI->get());
                      if (valueToSandboxNames[arg] & sandboxNames) {
                        outs() << "\n *** Sandboxed method " << F->getName() << " executing in sandboxes: " <<      stringifySandboxNames(sandboxNames) << " may leak private data through the extern function " << Callee->getName() << "\n";
                        if (MDNode *N = I.getMetadata("dbg")) {
                          DILocation loc(N);
                          outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                        }
                      }
                    }
                  }
                  else if (find(callgates.begin(), callgates.end(), Callee) != callgates.end()) {
                    // cross-domain call to callgate
                    outs() << "\n *** Sandboxed method " << F->getName() << " executing in sandboxes: " <<      stringifySandboxNames(sandboxNames) << " may leak private data through callgate " << Callee->getName() << "\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }

                  }
                  else if (sandboxedMethodToNames[Callee] != sandboxNames) { // possible cross-sandbox call
                    outs() << "\n *** Sandboxed method " << F->getName() << " executing in sandboxes: " <<      stringifySandboxNames(sandboxNames) << " may leak private data through a cross-sandbox call into: " << stringifySandboxNames(sandboxedMethodToNames[Callee]) << "\n";
                    if (MDNode *N = I.getMetadata("dbg")) {
                      DILocation loc(N);
                      outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                    }
                  }
                }
              }
            }
          }
        }
        else {
          DEBUG(dbgs() << "\n");
        }
      }

    }

    void checkPropagationOfClassifiedData(Module& M) {

      // initialise with pointers to annotated fields and uses of annotated global variables
      list<const Value*> worklist;
      if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();
            
            if (annotationStrValCString.startswith(CLASSIFY)) {
              StringRef className = annotationStrValCString.substr(strlen(CLASSIFY)+1); //+1 because of _
              int bitIdx = classToBitIdx[className];
            
              dbgs() << "   Classification annotation " << annotationStrValCString << " found:\n";
            
              worklist.push_back(annotatedVar);
              valueToClasses[annotatedVar] |= (1 << bitIdx);
            }
          }
        }
      }
      
      for (map<GlobalVariable*,int>::iterator I=varToClasses.begin(), E=varToClasses.end(); I != E; I++) {
        GlobalVariable* var = I->first;
        int classes = I->second;
        // find all users of var and taint them
        for (User::use_iterator u = var->use_begin(), e = var->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (LoadInst* load = dyn_cast<LoadInst>(user)) {
            Value* v = load->getPointerOperand();
            dbgs() << "   Load of classified global variable " << var->getName() << " found with classifications: " << stringifyClassifications(classes) << "\n";
            DEBUG(load->dump());
            valueToClasses[v] = classes; // could v have already been initialised above? (NO)
          }
        }

      }
                  
      // transfer function
      ClassPropagateFunction propagateClassifications(this);

      // compute fixed point
      performDataflowAnalysis(M, propagateClassifications, worklist);
      
      // validate that classified data is never accessed inside sandboxed contexts that
      // don't have clearance for its class.
      for (Function* F : sandboxedMethods) {
        DEBUG(dbgs() << "Function: " << F->getName() << ", clearances: " << stringifyClassifications(sandboxedMethodToClearances[F]) << "\n");
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
            DEBUG(dbgs() << "   Instruction:\n");
            DEBUG(I.dump());
            if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
              Value* v = load->getPointerOperand();
              DEBUG(dbgs() << "      Value:\n");
              DEBUG(v->dump());
              DEBUG(dbgs() << "      Value classes: " << valueToClasses[v] << ", " << stringifyClassifications(valueToClasses[v]) << "\n");
              if (!(valueToClasses[v] == 0 || valueToClasses[v] == sandboxedMethodToClearances[F] || (valueToClasses[v] & sandboxedMethodToClearances[F]))) {
                outs() << "\n *** Sandboxed method " << F->getName() << " read data value of class: " << stringifyClassifications(valueToClasses[v]) << " but only has clearances for: " << stringifyClassifications(sandboxedMethodToClearances[F]) << "\n";
                if (MDNode *N = I.getMetadata("dbg")) {
                  DILocation loc(N);
                  outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                }
              }
            }
          }
        }
      }
      
    }

    string stringifySandboxNames(int sandboxNames) {
      string sandboxNamesStr = "[";
      int currIdx = 0;
      bool first = true;
      for (currIdx=0; currIdx<=31; currIdx++) {
        if (sandboxNames & (1 << currIdx)) {
          StringRef sandboxName = bitIdxToSandboxName[currIdx];
          if (!first) 
            sandboxNamesStr += ",";
          sandboxNamesStr += sandboxName;
          first = false;
        }
      }
      sandboxNamesStr += "]";
      return sandboxNamesStr;
    }

    string stringifyClassifications(int classes) {
      string classStr = "[";
      int currIdx = 0;
      bool first = true;
      for (currIdx=0; currIdx<=31; currIdx++) {
        if (classes & (1 << currIdx)) {
          StringRef className = bitIdxToClass[currIdx];
          if (!first) 
            classStr += ",";
          classStr += className;
          first = false;
        }
      }
      classStr += "]";
      return classStr;
    }

    /*
     * Propagate the file descriptor annotations using def-use chains.
     * Iterate through each def-use chain starting from each annotated arg.
     * We use a worklist based algorithm.
     *
     * Start with the annotated parameters, and then iteratively propagate to 
     * all defs that use them. If the user is a call instruction, then propagate
     * to the corresponding parameter Argument object.
     *
     * Carry on this iteration until the worklist is empty.
     */
    void checkFileDescriptors(Module& M) {

      // initialise
      list<const Value*> worklist;
      for (map<const Value*,int>::iterator I=fdToPerms.begin(), E=fdToPerms.end(); I != E; I++) {
        worklist.push_back(I->first);
      }

      // transfer function
      FDPermsPropagateFunction propagateFDPerms(this);

      // compute fixed point
      performDataflowAnalysis(M, propagateFDPerms, worklist);

      // find all calls to read 
      validateDescriptorAccesses(M, "read", FD_READ_MASK);
      validateDescriptorAccesses(M, "write", FD_WRITE_MASK);
    }

    void propagateToCallee(const CallInst* CI, const Function* Callee, list<const Value*>& worklist, const Value* V, PropagateFunction& propagateTaint){
      // propagate only to methods from which sys call are reachable from 
      if (find(syscallReachableMethods.begin(), syscallReachableMethods.end(), Callee) != syscallReachableMethods.end()) {
        DEBUG(dbgs() << "Propagating to callee " << Callee->getName() << "\n");
        int idx;
        for (idx = 0; idx < CI->getNumArgOperands(); idx++) {
          if (CI->getArgOperand(idx)->stripPointerCasts() == V) {
          // now find the parameter object. Annoyingly there is no way
          // of getting the Argument at a particular index, so...
            for (Function::const_arg_iterator AI=Callee->arg_begin(), AE=Callee->arg_end(); AI!=AE; AI++) {
            //outs() << "arg no: " << AI->getArgNo() << "\n";
              if (AI->getArgNo() == idx) {
                //outs() << "Pushing arg " << AI->getName() << "\n";
                if (find(worklist.begin(), worklist.end(), AI) == worklist.end()) {
                  worklist.push_back(AI);
                  propagateTaint.propagate(V,AI); // propagate taint
                }
              }
            }
          }
        }
      }
    }

    /*
     * Validate that the necessary permissions propagate to the syscall
     */
    void validateDescriptorAccesses(Module& M, string syscall, int required_perm) {
      if (Function* syscallFn = M.getFunction(syscall)) {
        for (Value::use_iterator I=syscallFn->use_begin(), E=syscallFn->use_end(); I != E; I++) {
          CallInst* Call = cast<CallInst>(*I);
          Value* fd = Call->getArgOperand(0);
          if (!(fdToPerms[fd] & required_perm)) {
            Function* Caller = cast<Function>(Call->getParent()->getParent());
            outs() << "\n *** Insufficient privileges for " << syscall << "() in " << Caller->getName() << "\n";
            if (MDNode *N = Call->getMetadata("dbg")) {  // Here I is an LLVM instruction
              DILocation Loc(N);                      // DILocation is in DebugInfo.h
              unsigned Line = Loc.getLineNumber();
              StringRef File = Loc.getFilename();
              StringRef Dir = Loc.getDirectory();
              outs() << " +++ Line " << Line << " of file " << File << "\n";
            }
          }
        }
      }
      outs() << "\n";
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
       * to the function __declare_callgates_helper:
       *
       * #define __callgates(fns...) \
       *    void __declare_callgates() { \
       *      __declare_callgates_helper(0, fns); \
       *    }
       *
       * Hence, we must find the "call @__declare_callgates_helper"
       * instruction and obtain the list of functions from its arguments
       */
      if (Function* F = M.getFunction("__declare_callgates_helper")) {
        DEBUG(dbgs() << "Found __declare_callgates_helper\n");
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
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
              callgates.push_back(callgate);
            }
          }
        }
      }
    }

    void findSandboxPrivateAnnotations(Module& M) {

      // struct field annotations are stored in LLVM IR as arguments to calls 
      // to the intrinsic @llvm.ptr.annotation.p0i8
      if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();
            if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
              StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
              assignBitIdxToSandboxName(sandboxName);
            }
          }
        }
      }

      if (Function* F = M.getFunction("llvm.var.annotation")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();
            if (annotationStrValCString.startswith(SANDBOX_PRIVATE)) {
              StringRef sandboxName = annotationStrValCString.substr(strlen(SANDBOX_PRIVATE)+1); //+1 because of _
              assignBitIdxToSandboxName(sandboxName);
            }
          }
        }
      }

      // annotations on variables are stored in the llvm.global.annotations global
      // array
      GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations");
      if (lga != NULL) {
        ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
        for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
          ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

          // get the annotation value first
          GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
          ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
          StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
          if (annotationStrArrayCString.startswith(SANDBOX_PRIVATE)) {
            GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
            if (isa<GlobalVariable>(annotatedVal)) {
              GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
              if (annotationStrArrayCString.startswith(SANDBOX_PRIVATE)) {
                StringRef sandboxName = annotationStrArrayCString.substr(strlen(SANDBOX_PRIVATE)+1);
                DEBUG(dbgs() << "    Found sandbox-private global variable " << annotatedVar->getName() << "; belongs to \"" << sandboxName << "\"\n");
                assignBitIdxToSandboxName(sandboxName);
                varToSandboxNames[annotatedVar] |= (1 << sandboxNameToBitIdx[sandboxName]);
              }
            }
          }
        }
      }

    }
    void findClassifications(Module& M) {

      // struct field annotations are stored in LLVM IR as arguments to calls 
      // to the intrinsic @llvm.ptr.annotation.p0i8
      if (Function* F = M.getFunction("llvm.ptr.annotation.p0i8")) {
        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
          User* user = u.getUse().getUser();
          if (isa<IntrinsicInst>(user)) {
            IntrinsicInst* annotateCall = dyn_cast<IntrinsicInst>(user);
            Value* annotatedVar = dyn_cast<Value>(annotateCall->getOperand(0)->stripPointerCasts());

            GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(annotateCall->getOperand(1)->stripPointerCasts());
            ConstantDataArray* annotationStrValArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
            StringRef annotationStrValCString = annotationStrValArray->getAsCString();
            if (annotationStrValCString.startswith(CLASSIFY)) {
              StringRef className = annotationStrValCString.substr(strlen(CLASSIFY)+1); //+1 because of _
              assignBitIdxToClassName(className);
            }
          }
        }
      }

      // annotations on variables are stored in the llvm.global.annotations global
      // array
      GlobalVariable* lga = M.getNamedGlobal("llvm.global.annotations");
      if (lga != NULL) {
        ConstantArray* lgaArray = dyn_cast<ConstantArray>(lga->getInitializer()->stripPointerCasts());
        for (User::op_iterator i=lgaArray->op_begin(), e = lgaArray->op_end(); e!=i; i++) {
          ConstantStruct* lgaArrayElement = dyn_cast<ConstantStruct>(i->get());

          // get the annotation value first
          GlobalVariable* annotationStrVar = dyn_cast<GlobalVariable>(lgaArrayElement->getOperand(1)->stripPointerCasts());
          ConstantDataArray* annotationStrArray = dyn_cast<ConstantDataArray>(annotationStrVar->getInitializer());
          StringRef annotationStrArrayCString = annotationStrArray->getAsCString();
          if (annotationStrArrayCString.startswith(CLASSIFY)) {
            GlobalValue* annotatedVal = dyn_cast<GlobalValue>(lgaArrayElement->getOperand(0)->stripPointerCasts());
            if (isa<GlobalVariable>(annotatedVal)) {
              GlobalVariable* annotatedVar = dyn_cast<GlobalVariable>(annotatedVal);
              if (annotationStrArrayCString.startswith(CLASSIFY)) {
                StringRef className = annotationStrArrayCString.substr(strlen(CLASSIFY)+1);
                assignBitIdxToClassName(className);
                varToClasses[annotatedVar] |= (1 << classToBitIdx[className]);
              }
            }
          }
        }
      }

    }

    void assignBitIdxToClassName(StringRef className) {
      if (classToBitIdx.find(className) == classToBitIdx.end()) {
        dbgs() << "    Assigning bit index " << nextClassBitIdx << " to class \"" << className << "\"\n";
        classToBitIdx[className] = nextClassBitIdx;
        bitIdxToClass[nextClassBitIdx] = className;
        nextClassBitIdx++;
      }
    }

    void assignBitIdxToSandboxName(StringRef sandboxName) {
      if (sandboxNameToBitIdx.find(sandboxName) == sandboxNameToBitIdx.end()) {
        dbgs() << "    Assigning bit index " << nextSandboxNameBitIdx << " to sandbox name \"" << sandboxName << "\"\n";
        sandboxNameToBitIdx[sandboxName] = nextSandboxNameBitIdx;
        bitIdxToSandboxName[nextSandboxNameBitIdx] = sandboxName;
        nextSandboxNameBitIdx++;
      }
    }

    /*
     * Insert wrappers for callgates that make client requests before and
     * after calls to the callgate letting valgrind know that we're entering
     * and exiting a callgate respectively.
     *
     * This seems to be the only way to interpose callgate calls
     * in valgrind.
     */
    void generateCallgateValgrindWrappers(Module& M) {
      /*
       * See http://valgrind.org/docs/manual/manual-core-adv.html#manual-core-adv.wrapping
       * for information about valgrind function wrappers.
       *
       * Suppose we have the function:
       *
       * int example(int x, int y) {
       *    return x+y;
       * }
       *
       * The corresponding valgrind function wrapper would be:
       *
       * int _vgw00000ZU_NONE_example(int x, int y) { // wraps calls to example(x,y)
       *     int    result = -1;
       *     OrigFn fn;
       *     valgrind_get_orig_fn(&fn);
       *     call_unwrapped_function_w_ww(&fn, &result, &x, &y);
       *     return result;
       * }
       *
       * where:
       *     _vgw00000: is a special prefix that tells valgrind this is a
       *            wrapper function
       *     ZU:      tells valgrind that the soname is Z-encoded
       *                whereas the function name is not. Z-encoding is
       *                used to write unusual characters as valid C
       *                function names (e.g. *, +, :, etc.)
       *     NONE:    Name of the ELF shared object containing the
       *                function to be wrapped (i.e. example). This name
       *                is stored in the soname field of the shared object.
       *                If the so file does not have a soname then it is
       *                given the name NONE by default.
       *     example:   name of the function to wrap. NONE and example
       *            together identify the exact function to wrap.
       *     OrigFn:    valgrind struct for holding a pointer to the
       *            original function to execute (i.e. example).
       *            The definition of it is:
       *            typedef
             *            struct {
             *                    unsigned int nraddr;
       *            }
       *            OrigFn;
       *
       *     call_unwrapped_function_w_ww: calls the actual function and
       *                stores the result. The naming is of the form:
       *                call_unwrapped_function_RETURNTYPE_ARGTYPES.
       *                The first w refers to whether the return value is
       *                non-void (v is used instead of w if it is). The
       *                next two w's (ww) indicate that the function takes
       *                two arguments. If the function takes no arguments,
       *                then v is used instead of any w's.
       *
       *     To simplify things, we assume any soname, i.e. we use a
       *     soname of * represented in Z-encoding as Za. Also, all
       *     wrapper functions return a value. In the case of a void
       *     wrapped function, no result value would be stored and so the
       *     wrapper would return -1.
       */
      for (Function* callgate : callgates) {
        // Za encodes * and means any soname
        Function* callgateWrapper = dyn_cast<Function>(M.getOrInsertFunction(StringRef(Twine("_vgw00000ZU_Za_", callgate->getName()).str()), callgate->getFunctionType()));

        // create body of wrapper function
        BasicBlock* entryBlock = BasicBlock::Create(M.getContext(), "entry", callgateWrapper);
        IRBuilder<> builder(entryBlock);

        // create result and OrigFn local vars
        Type* callgateReturnType = callgate->getReturnType();
        Type* resultType = callgateReturnType->isVoidTy() ? IntegerType::get(M.getContext(), 32) : callgateReturnType;
        AllocaInst* resultAllocInst = builder.CreateAlloca(resultType);

        StructType* origFnType = M.getTypeByName("struct.OrigFn");
        AllocaInst* origFnAllocInst = builder.CreateAlloca(origFnType);

        // valgrind_get_orig_fn must be the first call or it doesn't work!!
        FunctionType* getOrigFnType = FunctionType::get(Type::getVoidTy(M.getContext()), ArrayRef<Type*>(origFnType), false);
        Function* getOrigFn = cast<Function>(M.getOrInsertFunction("valgrind_get_orig_fn", getOrigFnType));
        builder.CreateCall(getOrigFn, origFnAllocInst);

        /* debug begin */
        FunctionType* printfFnType = FunctionType::get(Type::getInt32Ty(M.getContext()), ArrayRef<Type*>(Type::getInt8PtrTy(M.getContext())), true);
        Function* printfFn = cast<Function>(M.getOrInsertFunction("printf", printfFnType));
        Value* printfFormatString = builder.CreateGlobalStringPtr(StringRef(Twine(callgate->getName()," wrapper\n").str()));
        builder.CreateCall(printfFn, printfFormatString);
        /* debug end */

        // Add calls to soaap_enter_callgate and soaap_exit_callgate
        // after valgrind_get_orig_fn(...).
        FunctionType* callGateFnType = FunctionType::get(Type::getVoidTy(M.getContext()), false);
        Function* enterCallGateFn = cast<Function>(M.getOrInsertFunction("soaap_enter_callgate", callGateFnType));
        Function* exitCallGateFn = cast<Function>(M.getOrInsertFunction("soaap_exit_callgate", callGateFnType));

        builder.CreateCall(enterCallGateFn);

        // Add call to the correct call_unwrappedfunction_w_ variant and construct
        // function type simultaneously
        SmallVector<Type*,6> params;
        params.push_back(PointerType::getUnqual(origFnType));
        params.push_back(Type::getInt64PtrTy(M.getContext()));
        int numCallgateParams = callgate->getFunctionType()->getNumParams();
        string params_code = "";
        if (numCallgateParams == 0) {
          params_code = "v";
        }
        else {
          for (int i=0; i<numCallgateParams; i++) {
            params_code += "w";
            params.push_back(Type::getInt64Ty(M.getContext()));
          }
        }
      
        FunctionType* callUnwrappedFnType = FunctionType::get(Type::getVoidTy(M.getContext()), ArrayRef<Type*>(params), false);
        Function* callUnwrappedFn = cast<Function>(M.getOrInsertFunction(StringRef("call_unwrapped_function_w_" + params_code), callUnwrappedFnType));

        // bitcast result var to int for the func call
        Value* resultAllocCast = builder.CreateCast(Instruction::BitCast, resultAllocInst, IntegerType::getInt32PtrTy(M.getContext()));
        SmallVector<Value*, 10> callUnwrappedFnCallArgs;
        callUnwrappedFnCallArgs.push_back(origFnAllocInst);
        callUnwrappedFnCallArgs.push_back(resultAllocCast);
        // bitcast all args to int for the func call
        Function::ArgumentListType& arguments = callgateWrapper->getArgumentList();
        for (Argument& arg : arguments) {
          Value* argCast = builder.CreatePointerCast(&arg, IntegerType::getInt32Ty(M.getContext()));
          callUnwrappedFnCallArgs.push_back(argCast);
        }

        builder.CreateCall(callUnwrappedFn, ArrayRef<Value*>(callUnwrappedFnCallArgs));

        builder.CreateCall(exitCallGateFn);

        if (callgate->getReturnType()->isVoidTy()) {
          builder.CreateRetVoid();
        }
        else {
          Value* returnValue = builder.CreateLoad(resultAllocInst);
          builder.CreateRet(returnValue);
        }

//        callgateWrapper->dump();
      }
    }

    /*
     * Instrument valgrind client requests. This is the mechanism valgrind
     * provides for communicating information to the underlying valgrind
     * execution engine.
     *
     * See http://valgrind.org/docs/manual/manual-core-adv.html#manual-core-adv.clientreq
     * for more details.
     */
    void instrumentValgrindClientRequests(Module& M) {
  
      if (sandboxEntryPoints.empty()) {
        return;
      }  

      Function* mainFn = M.getFunction("main");
      Instruction* mainFnFirstInst = NULL;
      FunctionType* VoidNoArgsFuncType = FunctionType::get(Type::getVoidTy(M.getContext()), false);

      if (mainFn != NULL) {

        mainFnFirstInst = mainFn->getEntryBlock().getFirstNonPHI();

        /*
         * 1. Create sandbox at the start of main
         */
          Function* createSandboxFn = cast<Function>(M.getOrInsertFunction("soaap_create_sandbox", VoidNoArgsFuncType));
          CallInst* createSandboxCall = CallInst::Create(createSandboxFn, ArrayRef<Value*>());
          createSandboxCall->insertBefore(mainFnFirstInst);

      }

      /*
       * 2. Insert calls to enter and exit sandbox at the entry and exit
       *    of sandboxed methods respectively and also tell valgrind which
       *    file descriptors are shared at the entry.
       */
      Function* enterPersistentSandboxFn = cast<Function>(M.getOrInsertFunction("soaap_enter_persistent_sandbox", VoidNoArgsFuncType));
      Function* exitPersistentSandboxFn = cast<Function>(M.getOrInsertFunction("soaap_exit_persistent_sandbox", VoidNoArgsFuncType));
      Function* enterEphemeralSandboxFn = cast<Function>(M.getOrInsertFunction("soaap_enter_ephemeral_sandbox", VoidNoArgsFuncType));
      Function* exitEphemeralSandboxFn = cast<Function>(M.getOrInsertFunction("soaap_exit_ephemeral_sandbox", VoidNoArgsFuncType));

      for (Function* F : sandboxEntryPoints) {
        bool persistent = find(persistentSandboxFuncs.begin(), persistentSandboxFuncs.end(), F) != persistentSandboxFuncs.end();
        Instruction* firstInst = F->getEntryBlock().getFirstNonPHI();
        CallInst* enterSandboxCall = CallInst::Create(persistent ? enterPersistentSandboxFn : enterEphemeralSandboxFn, ArrayRef<Value*>());
        enterSandboxCall->insertBefore(firstInst);
        /*
         * Before each call to enter_sandbox, also instrument
         * calls to tell valgrind which file descriptors are
         * shared and what accesses are allowed on them
         */
        for (Argument& A : F->getArgumentList()) {
          if (fdToPerms.find(&A) != fdToPerms.end()) {
            instrumentSharedFileValgrindClientRequest(M, &A, fdToPerms[&A], enterSandboxCall);
          }
        }
        for (BasicBlock& BB : F->getBasicBlockList()) {
          TerminatorInst* termInst = BB.getTerminator();
          if (isa<ReturnInst>(termInst)) {
            //BB is an exit block, insert an exit_sandbox() call
            CallInst* exitSandboxCall = CallInst::Create(persistent ? exitPersistentSandboxFn : exitEphemeralSandboxFn, ArrayRef<Value*>());
            exitSandboxCall->insertBefore(termInst);
          }
        }
//        for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
//          User* user = u.getUse().getUser();
//          if (isa<CallInst>(user)) {
//            CallInst* caller = dyn_cast<CallInst>(user);
//            CallInst* enterSandboxCall = CallInst::Create(persistent ? enterPersistentSandboxFn : enterEphemeralSandboxFn, ArrayRef<Value*>());
//            CallInst* exitSandboxCall = CallInst::Create(persistent ? exitPersistentSandboxFn : exitEphemeralSandboxFn, ArrayRef<Value*>());
//            enterSandboxCall->insertBefore(caller);
//            exitSandboxCall->insertAfter(caller);
//
//            /*
//             * Before each call to enter_sandbox, also instrument
//             * calls to tell valgrind which file descriptors are
//             * shared and what accesses are allowed on them
//             */
//            for (Argument& A : F->getArgumentList()) {
//              if (fdToPerms.find(&A) != fdToPerms.end()) {
//                instrumentSharedFileValgrindClientRequest(M, caller, &A, fdToPerms[&A], enterSandboxCall);
//              }
//            }
//          }
//        }

      }

      /*
       * 3. Insert client requests for shared vars
       */
      if (mainFn != NULL) {
        for (pair<GlobalVariable*,int> varPermPair : varToPerms) {
          instrumentSharedVarValgrindClientRequest(M, varPermPair.first, varPermPair.second, mainFnFirstInst);
        }
      }
    }

    /*
     * Inserts calls to soaap_shared_fd(fdvar, perms) or
     * soaap_shared_file(filevar, perms) that in turn tells valgrind that
     * the file descriptor value of fdvar or filevar is shared with
     * sandboxes and the permissions as defined by perms are allowed on it.
     */
    void instrumentSharedFileValgrindClientRequest(Module& M, Argument* arg, int perms, Instruction* predInst) {
      Value* args[] = { arg, ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), perms) };
      if (arg->getType()->isPointerTy()) {
        Type* fileStructType = M.getTypeByName("struct.FILE");
        SmallVector<Type*,2> fileSharedFnParamTypes;
        fileSharedFnParamTypes.push_back(PointerType::getUnqual(fileStructType));
        fileSharedFnParamTypes.push_back(Type::getInt32Ty(M.getContext()));
        FunctionType* fileSharedFnType = FunctionType::get(Type::getVoidTy(M.getContext()), ArrayRef<Type*>(fileSharedFnParamTypes), false);
        Function* fileSharedFn = cast<Function>(M.getOrInsertFunction("soaap_shared_file", fileSharedFnType));
        CallInst* fileSharedCall = CallInst::Create(fileSharedFn, args);
        fileSharedCall->insertAfter(predInst);
      }
      else {
        SmallVector<Type*,2> fdSharedFnParamTypes;
        fdSharedFnParamTypes.push_back(Type::getInt32Ty(M.getContext()));
        fdSharedFnParamTypes.push_back(Type::getInt32Ty(M.getContext()));
        FunctionType* fdSharedFnType = FunctionType::get(Type::getVoidTy(M.getContext()), ArrayRef<Type*>(    fdSharedFnParamTypes), false);

        Function* fdSharedFn = cast<Function>(M.getOrInsertFunction("soaap_shared_fd", fdSharedFnType));
        CallInst* fdSharedCall = CallInst::Create(fdSharedFn, args);
        fdSharedCall->insertAfter(predInst);
      }
    }

    /*
     * Inserts calls to soaap_shared_var(varname, perms) that in turn tells
     * valgrind that varname is shared with sandboxes and the permissions
     * as defined by perms are allowed on it.
     */
    void instrumentSharedVarValgrindClientRequest(Module& M, GlobalVariable* grv, int perms, Instruction* predInst) {
      /*
       * Create a global string variable to hold the name of the
       * shared variable.
       *
       * Note: When constructing a global variable, if you specify a
       * module parent, then the global variable is added automatically
       * to its list of global variables
       */
      StringRef varName = grv->getName();
      Constant* varNameArray = ConstantDataArray::getString(M.getContext(), varName);
      GlobalVariable* varNameGlobal = new GlobalVariable(M, varNameArray->getType(), true, GlobalValue::PrivateLinkage, varNameArray, "__soaap__shared_var_" + varName);

      Constant* Idxs[2] = {
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 0),
          ConstantInt::get(Type::getInt32Ty(M.getContext()), 0)
      };

      Constant* varCastConst = ConstantExpr::getGetElementPtr(varNameGlobal, Idxs);
      Constant* permConst = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), perms);
      Value* Args[] = { varCastConst, permConst };

      SmallVector<Type*,2> varSharedFnParamTypes;
      varSharedFnParamTypes.push_back(Type::getInt8PtrTy(M.getContext()));
      varSharedFnParamTypes.push_back(Type::getInt32Ty(M.getContext()));
      FunctionType* varSharedFnType = FunctionType::get(Type::getVoidTy(M.getContext()), ArrayRef<Type*>(varSharedFnParamTypes), false);
      Function* varSharedFn = cast<Function>(M.getOrInsertFunction("soaap_shared_var", varSharedFnType));
      CallInst* varSharedCall = CallInst::Create(varSharedFn, Args);
      varSharedCall->insertBefore(predInst);
    }

    void calculateSandboxedMethods(Module& M) {
      CallGraph& CG = getAnalysis<CallGraph>();
      for (Function* F : sandboxEntryPoints) {
        CallGraphNode* Node = CG.getOrInsertFunction(F);
        int sandboxName = 0;
        if (sandboxEntryPointToName.find(F) != sandboxEntryPointToName.end()) {
          sandboxName = sandboxEntryPointToName[F]; // sandbox entry point will only have one name
        }
        calculateSandboxedMethodsHelper(Node, sandboxedMethodToClearances[F], sandboxName);
      }
    }

    void calculateSandboxedMethodsHelper(CallGraphNode* node, int clearances, int sandboxName) {

      Function* F = node->getFunction();

      DEBUG(dbgs() << "Visiting " << F->getName() << "\n");
       
      if (find(sandboxedMethods.begin(), sandboxedMethods.end(), F) != sandboxedMethods.end()) {
        // cycle detected
        return;
      }

      sandboxedMethods.insert(F);
      allReachableMethods.insert(F);
      sandboxedMethodToClearances[F] |= clearances;

      if (sandboxName != 0) {
        DEBUG(dbgs() << "   Assigning name: " << sandboxName << "\n");
        sandboxedMethodToNames[F] |= sandboxName;
      }

//      cout << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
      for (CallGraphNode::iterator I=node->begin(), E=node->end(); I != E; I++) {
        Value* V = I->first;
        CallGraphNode* calleeNode = I->second;
        if (Function* calleeFunc = calleeNode->getFunction()) {
          if (sandboxEntryPointToName.find(calleeFunc) != sandboxEntryPointToName.end()) {
            DEBUG(dbgs() << "   Encountered sandbox entry point, changing sandbox name to: " << stringifySandboxNames(sandboxName));
            sandboxName = sandboxEntryPointToName[calleeFunc];
          }
          calculateSandboxedMethodsHelper(calleeNode, clearances, sandboxName);
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
        if (find(sandboxEntryPoints.begin(), sandboxEntryPoints.end(), F) != sandboxEntryPoints.end())
          return;
        
        // if already visited this function, then ignore as cycle detected
        if (find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end())
          return;
  
        privilegedMethods.push_back(F);
        allReachableMethods.insert(F);
  
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
        DEBUG(dbgs() << "Sandbox-reachable function: " << F->getName().str() << "\n");
        for (BasicBlock& BB : F->getBasicBlockList()) {
          for (Instruction& I : BB.getInstList()) {
//            I.dump();
            if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
              if (GlobalVariable* gv = dyn_cast<GlobalVariable>(load->getPointerOperand())) {
                if (!(varToPerms[gv] & VAR_READ_MASK)) {
                  outs() << "\n *** Sandboxed method " << F->getName().str() << " read global variable " << gv->getName().str() << " but is not allowed to\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                     DILocation loc(N);
                     outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
                  }
                }

              }
            }
            else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
              if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
                // check that the programmer has annotated that this
                // variable can be written to
                if (!(varToPerms[gv] & VAR_WRITE_MASK)) {
                  outs() << "\n *** Sandboxed method " << F->getName().str() << " wrote to global variable " << gv->getName().str() << " but is not allowed to\n";
                  if (MDNode *N = I.getMetadata("dbg")) {
                     DILocation loc(N);
                     outs() << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getFilename().str() << "\n";
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
      outs() << "\n";
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
			for (Function* F : sandboxedMethods) {
				Argument* data_in = NULL;
				Argument* data_out = NULL;
				bool persistent = find(persistentSandboxFuncs.begin(),
					persistentSandboxFuncs.end(), F) !=
					persistentSandboxFuncs.end();
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
			if (!persistentSandboxFuncs.empty()) {
				Function* mainFn = M.getFunction("main");

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
