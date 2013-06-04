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
#include "Analysis/GlobalVariableAnalysis.h"
#include "Analysis/VulnerabilityAnalysis.h"
#include "Analysis/PrivilegedCallAnalysis.h"
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

    SandboxVector sandboxes;

    FunctionIntMap sandboxedMethodToOverhead;
    FunctionVector persistentSandboxEntryPoints;
    FunctionVector allSandboxEntryPoints;

    FunctionVector callgates;
    FunctionVector privilegedMethods;

    // provenance
    StringVector vulnerableVendors;

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

      if (emPerf) {
        instrumentPerfEmul(M);
      }
      else {
        // do the checks statically
        outs() << "* Calculating privileged methods\n";
        calculatePrivilegedMethods(M);
        
        outs() << "* Checking global variable accesses\n";
        checkGlobalVariables(M);

        outs() << "* Checking file descriptor accesses\n";
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

    void checkPrivilegedCalls(Module& M) {
      PrivilegedCallAnalysis analysis;
      analysis.doAnalysis(M, sandboxes);
    }

    void checkLeakedRights(Module& M) {
      VulnerabilityAnalysis analysis(privilegedMethods, vulnerableVendors);
      analysis.doAnalysis(M, sandboxes);
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

    void checkOriginOfAccesses(Module& M) {
      AccessOriginAnalysis analysis(privilegedMethods);
      analysis.doAnalysis(M, sandboxes);
    }

    void findSandboxes(Module& M) {
      sandboxes = SandboxUtils::findSandboxes(M);
    }

    void checkPropagationOfSandboxPrivateData(Module& M) {
      SandboxPrivateAnalysis analysis(privilegedMethods, callgates);
      analysis.doAnalysis(M, sandboxes);
    }

    void checkPropagationOfClassifiedData(Module& M) {
      ClassifiedAnalysis analysis;
      analysis.doAnalysis(M, sandboxes);
    }

    void checkFileDescriptors(Module& M) {
      CapabilityAnalysis analysis;
      analysis.doAnalysis(M, sandboxes);
    }

    void calculatePrivilegedMethods(Module& M) {
      CallGraph& CG = getAnalysis<CallGraph>();
      if (Function* MainFunc = M.getFunction("main")) {
        CallGraphNode* MainNode = CG[MainFunc];
        calculatePrivilegedMethodsHelper(M, MainNode);
      }
    }

    void calculatePrivilegedMethodsHelper(Module& M, CallGraphNode* Node) {
      if (Function* F = Node->getFunction()) {
        // if a sandbox entry point, then ignore
        if (SandboxUtils::isSandboxEntryPoint(M, F))
          return;
        
        // if already visited this function, then ignore as cycle detected
        if (find(privilegedMethods.begin(), privilegedMethods.end(), F) != privilegedMethods.end())
          return;
  
        dbgs() << "Added " << F->getName() << " as privileged method\n";
        privilegedMethods.push_back(F);
  
        // recurse on callees
        for (CallGraphNode::iterator I=Node->begin(), E=Node->end(); I!=E; I++) {
          calculatePrivilegedMethodsHelper(M, I->second);
        }
      }
    }

    void checkGlobalVariables(Module& M) {
      GlobalVariableAnalysis analysis(privilegedMethods);
      analysis.doAnalysis(M, sandboxes);
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
