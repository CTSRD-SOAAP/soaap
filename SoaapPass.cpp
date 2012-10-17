#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/ilist.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/IRBuilder.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/DebugInfo.h"

#include "soaap.h"

#include <iostream>
#include <vector>

using namespace llvm;
using namespace std;
namespace soaap {

	struct SoaapPass : public ModulePass {

		static char ID;

		bool modified;
		bool dynamic;
		map<GlobalVariable*,int> varToPerms;
		map<Argument*,int> fdToPerms;
		SmallVector<Function*,16> persistentSandboxFuncs;
		SmallVector<Function*,16> ephemeralSandboxFuncs;
		SmallVector<Function*,32> sandboxFuncs;
		SmallVector<Function*,16> callgates;

		SmallSet<Function*,16> sandboxReachableMethods;

		SoaapPass() : ModulePass(ID) {
			modified = false;
			dynamic = true;
		}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			if (!dynamic) {
				AU.setPreservesCFG();
			}
			AU.addRequired<CallGraph>();
		}

		virtual bool runOnModule(Module& M) {

			findSandboxedMethods(M);

			findSharedGlobalVariables(M);

			findSharedFileDescriptors(M);

			findCallgates(M);

			if (dynamic) {
				// use valgrind
				instrumentValgrindClientRequests(M);
				generateCallgateValgrindWrappers(M);
			}
			else {
				// do the checks statically
				calculateSandboxReachableMethods(M);
				checkGlobalVariables(M);
			}

			return modified;

		}

		/*
		 * Find functions that are annotated to be executed in persistent and
		 * ephemeral sandboxes
		 */
		void findSandboxedMethods(Module& M) {

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
			 *    i32 5 }]		// line number
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
						if (annotationStrArrayCString == SANDBOX_PERSISTENT) {
							persistentSandboxFuncs.push_back(annotatedFunc);
							sandboxFuncs.push_back(annotatedFunc);
						}
						else if (annotationStrArrayCString == SANDBOX_EPHEMERAL) {
							ephemeralSandboxFuncs.push_back(annotatedFunc);
							sandboxFuncs.push_back(annotatedFunc);
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
			 * 	  [{ i8*, i8*, i8*, i32 }
			 * 	   { i8* bitcast (i32* @fd to i8*),		// annotated global variable
			 * 	     i8* getelementptr inbounds ([9 x i8]* @.str, i32 0, i32 0),  // annotation
			 * 	     i8* getelementptr inbounds ([7 x i8]* @.str1, i32 0, i32 0), // file name
			 * 	     i32 1 }]	// line number
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
		     *   i8* %ifd.addr1,			// param (llvm creates a local var for the param by appending .addrN to the end of the param name)
			 *   i8* getelementptr inbounds ([8 x i8]* @.str2, i32 0, i32 0),  // annotation
			 *   i8* getelementptr inbounds ([30 x i8]* @.str3, i32 0, i32 0),  // file name
			 *   i32 5)	             // line number
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
			 *	  	__declare_callgates_helper(0, fns); \
			 *    }
			 *
			 * Hence, we must find the "call @__declare_callgates_helper"
			 * instruction and obtain the list of functions from its arguments
			 */
			if (Function* F = M.getFunction("__declare_callgates_helper")) {
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
							outs() << "Callgate " << i << " is " << callgate->getName() << "\n";
							callgates.push_back(callgate);
						}
					}
				}
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
			 * 		return x+y;
			 * }
			 *
			 * The corresponding valgrind function wrapper would be:
			 *
			 * int _vgw00000ZU_NONE_example(int x, int y) { // wraps calls to example(x,y)
			 *     int    result = -1;
			 *	   OrigFn fn;
			 *	   valgrind_get_orig_fn(&fn);
			 *	   call_unwrapped_function_w_ww(&fn, &result, &x, &y);
			 *	   return result;
			 * }
			 *
			 * where:
			 *     _vgw00000: is a special prefix that tells valgrind this is a
			 *     			  wrapper function
			 *     ZU:		  tells valgrind that the soname is Z-encoded
			 *                whereas the function name is not. Z-encoding is
			 *                used to write unusual characters as valid C
			 *                function names (e.g. *, +, :, etc.)
			 *     NONE:	  Name of the ELF shared object containing the
			 *                function to be wrapped (i.e. example). This name
			 *                is stored in the soname field of the shared object.
			 *                If the so file does not have a soname then it is
			 *                given the name NONE by default.
			 *     example:	  name of the function to wrap. NONE and example
			 *     			  together identify the exact function to wrap.
			 *     OrigFn:	  valgrind struct for holding a pointer to the
			 *     			  original function to execute (i.e. example).
			 *     			  The definition of it is:
			 *     			  typedef
   	   	   	 *					  struct {
      	  	 *  	  	  	  	    unsigned int nraddr;
			 *					  }
			 *					  OrigFn;
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
				Function* getOrigFn = M.getFunction("valgrind_get_orig_fn");
				builder.CreateCall(getOrigFn, origFnAllocInst);

				/* debug begin */
				Function* printfFn = M.getFunction("soaap_printf");
				Value* printfFormatString = builder.CreateGlobalStringPtr(StringRef(Twine(callgate->getName()," wrapper\n").str()));
				builder.CreateCall(printfFn, printfFormatString);
				/* debug end */

				// Add calls to soaap_enter_callgate and soaap_exit_callgate
				// after valgrind_get_orig_fn(...).
				Function* enterCallGateFn = M.getFunction("soaap_enter_callgate");
				Function* exitCallGateFn = M.getFunction("soaap_exit_callgate");

				builder.CreateCall(enterCallGateFn);

				// Add call to the correct call_unwrappedfunction_w_ variant
				int numCallgateParams = callgate->getFunctionType()->getNumParams();
				string params_code = "";
				if (numCallgateParams == 0) {
					params_code = "v";
				}
				else {
					for (int i=0; i<numCallgateParams; i++) {
						params_code += "w";
					}
				}

				Function* callUnwrappedFn = M.getFunction(StringRef("call_unwrapped_function_w_" + params_code));

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

//				callgateWrapper->dump();
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

			Function* mainFn = M.getFunction("main");
			Instruction* mainFnFirstInst = NULL;

			if (mainFn != NULL) {

				mainFnFirstInst = mainFn->getEntryBlock().getFirstNonPHI();

				/*
				 * 1. Create sandbox at the start of main
				 */
				if (!sandboxFuncs.empty()) {
					Function* createSandboxFn = M.getFunction("soaap_create_sandbox");
					CallInst* createSandboxCall = CallInst::Create(createSandboxFn, ArrayRef<Value*>());
					createSandboxCall->insertBefore(mainFnFirstInst);
				}

			}

			/*
			 * 2. Insert calls to enter and exit sandbox at the entry and exit
			 *    of sandboxed methods respectively and also tell valgrind which
			 *    file descriptors are shared at the entry.
			 */
			Function* enterPersistentSandboxFn = M.getFunction("soaap_enter_persistent_sandbox");
			Function* exitPersistentSandboxFn = M.getFunction("soaap_exit_persistent_sandbox");
			Function* enterEphemeralSandboxFn = M.getFunction("soaap_enter_ephemeral_sandbox");
			Function* exitEphemeralSandboxFn = M.getFunction("soaap_exit_ephemeral_sandbox");

			for (Function* F : sandboxFuncs) {
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
//				for (User::use_iterator u = F->use_begin(), e = F->use_end(); e!=u; u++) {
//					User* user = u.getUse().getUser();
//					if (isa<CallInst>(user)) {
//						CallInst* caller = dyn_cast<CallInst>(user);
//						CallInst* enterSandboxCall = CallInst::Create(persistent ? enterPersistentSandboxFn : enterEphemeralSandboxFn, ArrayRef<Value*>());
//						CallInst* exitSandboxCall = CallInst::Create(persistent ? exitPersistentSandboxFn : exitEphemeralSandboxFn, ArrayRef<Value*>());
//						enterSandboxCall->insertBefore(caller);
//						exitSandboxCall->insertAfter(caller);
//
//						/*
//						 * Before each call to enter_sandbox, also instrument
//						 * calls to tell valgrind which file descriptors are
//						 * shared and what accesses are allowed on them
//						 */
//						for (Argument& A : F->getArgumentList()) {
//							if (fdToPerms.find(&A) != fdToPerms.end()) {
//								instrumentSharedFileValgrindClientRequest(M, caller, &A, fdToPerms[&A], enterSandboxCall);
//							}
//						}
//					}
//				}

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
				Function* fileSharedFn = M.getFunction("soaap_shared_file");
				CallInst* fileSharedCall = CallInst::Create(fileSharedFn, args);
				fileSharedCall->insertAfter(predInst);
			}
			else {
				Function* fdSharedFn = M.getFunction("soaap_shared_fd");
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

			Function* varSharedFn = M.getFunction("soaap_shared_var");
			CallInst* varSharedCall = CallInst::Create(varSharedFn, Args);
			varSharedCall->insertBefore(predInst);
		}

		void calculateSandboxReachableMethods(Module& M) {
			CallGraphNode* cgRoot = getAnalysis<CallGraph>().getRoot();
			calculateSandboxReachableMethodsHelper(cgRoot, false, SmallVector<CallGraphNode*,16>());
		}


		void calculateSandboxReachableMethodsHelper(CallGraphNode* node, bool collect, SmallVector<CallGraphNode*,16> visited) {
//			cout << "Visiting " << node->getFunction()->getName().str() << endl;
			if (collect) {
//				cout << "-- Collecting" << endl;
				sandboxReachableMethods.insert(node->getFunction());
			}
			else if (find(visited.begin(), visited.end(), node) != visited.end()) {
				// cycle detected
				return;
			}
//			cout << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
			visited.push_back(node);
			for (CallGraphNode::iterator I=node->begin(), E=node->end(); I != E; I++) {
				CallGraphNode* calleeNode = I->second;
				if (Function* calleeFunc = calleeNode->getFunction()) {
					bool sandboxFunc = find(sandboxFuncs.begin(), sandboxFuncs.end(), calleeFunc) != sandboxFuncs.end();
					calculateSandboxReachableMethodsHelper(calleeNode, collect || sandboxFunc, visited);
				}
			}
		}

		void checkGlobalVariables(Module& M) {

			// find all uses of global variables and check that they are allowed
			// as per the annotations
			for (Function* F : sandboxReachableMethods) {
				cout << "Sandboxed function: " << F->getName().str() << endl;
				for (BasicBlock& BB : F->getBasicBlockList()) {
					for (Instruction& I : BB.getInstList()) {
//						I.dump();
						if (LoadInst* load = dyn_cast<LoadInst>(&I)) {
							if (GlobalVariable* gv = dyn_cast<GlobalVariable>(load->getPointerOperand())) {
								if (!(varToPerms[gv] & VAR_READ_MASK)) {
									cout << " *** Sandboxed method " << F->getName().str() << " read global variable " << gv->getName().str() << " but is not allowed to" << endl;
									if (MDNode *N = I.getMetadata("dbg")) {
									   DILocation loc(N);
									   cout << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getDirectory().str() << "/" << loc.getFilename().str() << endl;
									}
									cout << endl;
								}

							}
						}
						else if (StoreInst* store = dyn_cast<StoreInst>(&I)) {
							if (GlobalVariable* gv = dyn_cast<GlobalVariable>(store->getPointerOperand())) {
								// check that the programmer has annotated that this
								// variable can be written to
								if (!(varToPerms[gv] & VAR_WRITE_MASK)) {
									cout << " *** Sandboxed method " << F->getName().str() << " wrote to global variable " << gv->getName().str() << " but is not allowed to" << endl;
									if (MDNode *N = I.getMetadata("dbg")) {
									   DILocation loc(N);
									   cout << " +++ Line " << loc.getLineNumber() << " of file "<< loc.getDirectory().str() << "/" << loc.getFilename().str() << endl;
									}
									cout << endl;
								}
							}
						}
//						I.dump();
//						cout << "Num operands: " << I.getNumOperands() << endl;
//						for (int i=0; i<I.getNumOperands(); i++) {
//							cout << "Operand " << i << ": " << endl;
//							I.getOperand(i)->dump();
//						}
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
