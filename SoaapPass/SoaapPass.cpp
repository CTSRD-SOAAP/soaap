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
#include "llvm/Support/Debug.h"

#include "soaap.h"
#include "soaap_perf.h"

#include <iostream>
#include <vector>

using namespace llvm;
using namespace std;
namespace soaap {

  struct SoaapPass : public ModulePass {

    static char ID;

    bool modified;
    bool dynamic;
    bool emPerf;

    map<GlobalVariable*,int> varToPerms;
    map<Value*,int> fdToPerms;
    SmallVector<Function*,16> persistentSandboxFuncs;
    SmallVector<Function*,16> ephemeralSandboxFuncs;
    SmallVector<Function*,32> sandboxFuncs;
    SmallVector<Function*,16> callgates;

    SmallSet<Function*,16> sandboxReachableMethods;

    SoaapPass() : ModulePass(ID) {
      modified = false;
      dynamic = false;
      emPerf = false;
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

      if (dynamic && !emPerf) {
        // use valgrind
        instrumentValgrindClientRequests(M);
        generateCallgateValgrindWrappers(M);
      }
      else if (!dynamic && !emPerf)
      {
        // do the checks statically
        calculateSandboxReachableMethods(M);
        checkGlobalVariables(M);
        checkFileDescriptors(M);
      }
      else if (!dynamic && emPerf) {
        instrumentPerfEmul(M);
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
            if (annotationStrArrayCString == SANDBOX_PERSISTENT) {
              dbgs() << "Found persistent sandbox " << annotatedFunc->getName() << "\n";
              persistentSandboxFuncs.push_back(annotatedFunc);
              sandboxFuncs.push_back(annotatedFunc);
            }
            else if (annotationStrArrayCString == SANDBOX_EPHEMERAL) {
              dbgs() << "Found ephemeral sandbox " << annotatedFunc->getName() << "\n";
              persistentSandboxFuncs.push_back(annotatedFunc);
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
      std::list<Value*> worklist;
      for (map<Value*,int>::iterator I=fdToPerms.begin(), E=fdToPerms.end(); I != E; I++) {
        worklist.push_back(I->first);
      }

      // perform propagation until fixed point is reached
      while (!worklist.empty()) {
        Value* V = worklist.front();
        worklist.pop_front();
        DEBUG(outs() << "** Popped " << V->getName() << "\n");
        for (Value::use_iterator VI=V->use_begin(), VE=V->use_end(); VI != VE; VI++) {
          Value* V2;
          DEBUG(VI->dump());
          if (StoreInst* SI = dyn_cast<StoreInst>(*VI)) {
            if (V == SI->getPointerOperand()) // to avoid infinite looping
              continue;
            V2 = SI->getPointerOperand();
          }
          else if (CallInst* CI = dyn_cast<CallInst>(*VI)) {
            // propagate to the callee function's argument
            // find the index of the use position
            int idx;
            if (Function* callee = CI->getCalledFunction()) {
              for (idx = 0; idx < CI->getNumArgOperands(); idx++) {
                if (CI->getArgOperand(idx)->stripPointerCasts() == V) {
                // now find the parameter object. Annoyingly there is no way
                // of getting the Argument at a particular index, so...
                  for (Function::arg_iterator AI=callee->arg_begin(), AE=callee->arg_end(); AI!=AE; AI++) {
                  //outs() << "arg no: " << AI->getArgNo() << "\n";
                    if (AI->getArgNo() == idx) {
                      //outs() << "Pushing arg " << AI->getName() << "\n";
                      worklist.push_back(AI);
                      fdToPerms[AI] = fdToPerms[V]; // propagate permissions
                    }
                  }
                }
              }
            }
            continue;
          }
          else {
            V2 = *VI;
          }
          worklist.push_back(V2);
          fdToPerms[V2] = fdToPerms[V]; // propagate permissions
          //outs() << "Propagating " << V->getName() << " to " << V2->getName() << "\n";
        }
      }

      // find all calls to read 
      validateDescriptorAccesses(M, "read", FD_READ_MASK);
      validateDescriptorAccesses(M, "write", FD_WRITE_MASK);
    }

    /*
     * Validate that the necessary permissions propagate to the syscall
     */
    void validateDescriptorAccesses(Module& M, string syscall, int required_perm) {
      if (Function* syscallFn = M.getFunction(syscall)) {
        for (Value::use_iterator I=syscallFn->use_begin(), E=syscallFn->use_end(); I != E; I++) {
          CallInst* call = cast<CallInst>(*I);
          Value* fd = call->getArgOperand(0);
          if (fdToPerms[fd] & required_perm) {
            outs() << syscall << ": OK!\n";
          }
          else {
            outs() << syscall << ": Insufficient privileges\n";
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
       *      __declare_callgates_helper(0, fns); \
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
  
      if (sandboxFuncs.empty()) {
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

    void calculateSandboxReachableMethods(Module& M) {
      CallGraphNode* cgRoot = getAnalysis<CallGraph>().getRoot();
      calculateSandboxReachableMethodsHelper(cgRoot, false, SmallVector<CallGraphNode*,16>());
    }


    void calculateSandboxReachableMethodsHelper(CallGraphNode* node, bool collect, SmallVector<CallGraphNode*,16> visited) {
//      cout << "Visiting " << node->getFunction()->getName().str() << endl;
      if (collect) {
//        cout << "-- Collecting" << endl;
        sandboxReachableMethods.insert(node->getFunction());
      }
      else if (find(visited.begin(), visited.end(), node) != visited.end()) {
        // cycle detected
        return;
      }
//      cout << "Adding " << node->getFunction()->getName().str() << " to visited" << endl;
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
//            I.dump();
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
//            I.dump();
//            cout << "Num operands: " << I.getNumOperands() << endl;
//            for (int i=0; i<I.getNumOperands(); i++) {
//              cout << "Operand " << i << ": " << endl;
//              I.getOperand(i)->dump();
//            }
          }
        }
      }

    }

    void instrumentPerfEmul(Module& M) {
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
      for (Function* F : sandboxFuncs) {
        CallInst* enterSandboxCall = NULL;
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

        /*
         * Pick the appropriate function to inject based on the
         * annotations and perform the actual instrumentation in the
         * sandboxed function prologue.
         * NOTE: At the moment, we do not handle __data_out.
         */
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
