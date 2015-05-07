#include "Instrument/PerformanceEmulationInstrumenter.h"

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

#include "Common/Debug.h"
#include "Util/DebugUtils.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"
#define IN_SOAAP_INSTRUMENTER
#include "soaap_perf.h"

using namespace soaap;

void PerformanceEmulationInstrumenter::instrument(Module& M, SandboxVector& sandboxes) {
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
  bool persistentSandboxExists = false; 
  for (Sandbox* S : sandboxes) {
    bool persistent = S->isPersistent();
    int perfThreshold = S->getOverhead();
    for (Function* F : S->getEntryPoints()) {
      Argument* data_in = NULL;
      Argument* data_out = NULL;
      persistentSandboxExists = persistentSandboxExists || persistent;
      Instruction* firstInst = F->getEntryBlock().getFirstNonPHI();

      /*
       * Check if there are annotated parameters to sandboxed
       * functions.
       */
      if (FVA) {
        for (User* U : FVA->users()) {
          if (IntrinsicInst* annotateCall
              = dyn_cast<IntrinsicInst>(U)) {

            /* Get the enclosing function */
            Function* enclosingFunc
              = annotateCall->getParent()->getParent();

            /*
             * If the enclosing function does not match the
             * current sandbox entry point in the outer loop
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
            if (DbgDeclareInst* dbgDecl = FindAllocaDbgDeclare(annotatedVar)) {
              DILocalVariable* varDbg = dbgDecl->getVariable();
              string annotatedVarName = varDbg->getName().str();
              Argument* annotatedArg = NULL;

              for (Argument &arg : enclosingFunc->getArgumentList()) {
                if (arg.getName().str() == annotatedVarName) {
                  annotatedArg = &arg;
                }
              }

              if (annotatedArg != NULL) {
                if (annotationStrValCString == DATA_IN) {
                  outs() << "__DATA_IN annotated parameter"
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
                  outs() << "__DATA_OUT annotated parameter"
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
     * XXX IM: Make this cleaner after SOAAP deadline.
       */
      CallInst* enterSandboxCall = NULL;
      if (data_in && data_out) {
        SmallVector<Value*, 2> soaap_perf_datainout_args;
        soaap_perf_datainout_args.push_back(data_in);
        soaap_perf_datainout_args.push_back(data_out);

        enterPersistentSandboxFn
          = M.getFunction("soaap_perf_enter_datainout_persistent_sbox");
        enterEphemeralSandboxFn
          = M.getFunction("soaap_perf_enter_datainout_ephemeral_sbox");
        enterSandboxCall = CallInst::Create(persistent
          ? enterPersistentSandboxFn : enterEphemeralSandboxFn,
          ArrayRef<Value*>(soaap_perf_datainout_args));
        enterSandboxCall->insertBefore(firstInst);
      } else if (data_in) {
        enterPersistentSandboxFn
          = M.getFunction("soaap_perf_enter_datain_persistent_sbox");
        enterEphemeralSandboxFn
          = M.getFunction("soaap_perf_enter_datain_ephemeral_sbox");
        enterSandboxCall = CallInst::Create(persistent
          ? enterPersistentSandboxFn : enterEphemeralSandboxFn,
          ArrayRef<Value*>(data_in));
        enterSandboxCall->insertBefore(firstInst);
      } else if (data_out) {
        enterPersistentSandboxFn
          = M.getFunction("soaap_perf_enter_dataout_persistent_sbox");
        enterEphemeralSandboxFn
          = M.getFunction("soaap_perf_enter_dataout_ephemeral_sbox");
        enterSandboxCall = CallInst::Create(persistent
          ? enterPersistentSandboxFn : enterEphemeralSandboxFn,
          ArrayRef<Value*>(data_out));
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


      // Get terminator instruction of the current basic block.
      for (BasicBlock& BB : F->getBasicBlockList()) {
        TerminatorInst* termInst = BB.getTerminator();
        if (isa<ReturnInst>(termInst)) {
          //BB is an exit block, instrument ret
          perfOverheadCall = CallInst::Create(perfOverheadFn,
            ArrayRef<Value*>(soaap_perf_overhead_args));
          dbgs() << "Inserting call to " << perfOverheadFn->getName() << "\n";
          perfOverheadCall->insertBefore(termInst);
        }
      }
    }

    bool cgDebugOutput = false;
    SDEBUG("soaap.instr.perf", 3, cgDebugOutput = true);
    Function* invokeCallgateFn = M.getFunction("soaap_perf_callgate");
    for (Function* CGF : S->getCallgates()) {
      // Precede each call to a callgate function within the sandbox with
      // an RPC send and receive 
      for (User* U : CGF->users()) {
        if (CallInst* C = dyn_cast<CallInst>(U)) {
          if (S->containsInstruction(C)) {
            if (cgDebugOutput) {
              dbgs() << "Adding call to soaap_perf_callgate invocation of callgate \"" << CGF->getName() << "\" in sandbox: ";
              if (DILocation* loc = dyn_cast_or_null<DILocation>(C->getMetadata("dbg"))) {
                dbgs() << loc->getFilename() << ":" << loc->getLine();
              }
              dbgs() << "\n";
            }
            CallInst* invokeCallgateCall = CallInst::Create(invokeCallgateFn, ArrayRef<Value*>());
            invokeCallgateCall->insertBefore(C);
          }
        }
      }
    }
  }

  /*
   * If there are running persistent sandboxed terminate them before
   * exiting the program.  This is achieved when calling the
   * appropriate library function with -1 as argument.
   */
  if (persistentSandboxExists) {
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

    Function* terminatePersistentSandbox
      = M.getFunction("soaap_perf_enter_datain_persistent_sbox");
    //CallInst* terminateCall
    //= CallInst::Create(terminatePersistentSandbox,
    //ArrayRef<Value*>());

    // Iterate over main's BasicBlocks and instrument all ret points
    for (BasicBlock& BB : mainFn->getBasicBlockList()) {
      TerminatorInst* mainLastInst = BB.getTerminator();
      if (isa<ReturnInst>(mainLastInst)) {
        //BB is an exit block, instrument ret
        ConstantInt *arg = ConstantInt::get(Type::getInt32Ty(C),
          -1, true);
        CallInst* terminateCall
          = CallInst::Create(terminatePersistentSandbox,
            ArrayRef<Value*>(dyn_cast<Value>(arg)));
        terminateCall->setTailCall();
        terminateCall->insertBefore(mainLastInst);
      }
    }
  }
}
