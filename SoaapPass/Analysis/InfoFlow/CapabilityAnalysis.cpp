#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "Analysis/InfoFlow/CapabilityAnalysis.h"
#include "Utils/LLVMAnalyses.h"
#include "Utils/PrettyPrinters.h"
#include "soaap.h"

using namespace soaap;

void CapabilityAnalysis::initialise(ValueList& worklist, Module& M) {

  /*
   * Find those file descriptor parameters that are shared with the
   * sandboxed method.
   */
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

        DEBUG(dbgs() << "    annotation: " << annotationStrValCString << "\n");

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
            state[annotatedArg] |= FD_READ_MASK;
          }
          else if (annotationStrValCString == FD_WRITE) {
            state[annotatedArg] |= FD_WRITE_MASK;
          }
          worklist.push_back(annotatedArg);
          DEBUG(dbgs() << "   found annotated file descriptor " << annotatedArg->getName() << "\n");
        }
      }
    }
  }
}

int CapabilityAnalysis::performMeet(int fromVal, int toVal) {
  return fromVal & toVal;
}

void CapabilityAnalysis::postDataFlowAnalysis(Module& M) {
  validateDescriptorAccesses(M, "read", FD_READ_MASK);
  validateDescriptorAccesses(M, "write", FD_WRITE_MASK);
}

/*
 * Validate that the necessary permissions propagate to the syscall
 */
void CapabilityAnalysis::validateDescriptorAccesses(Module& M, string syscall, int requiredPerm) {
  if (Function* syscallFn = M.getFunction(syscall)) {
    for (Value::use_iterator I=syscallFn->use_begin(), E=syscallFn->use_end();
         (I != E) && isa<CallInst>(*I); I++) {
      CallInst* Call = cast<CallInst>(*I);
      Function* Caller = cast<Function>(Call->getParent()->getParent());
      if (find(sandboxedMethods.begin(), sandboxedMethods.end(), Caller) != sandboxedMethods.end()) {
        Value* fd = Call->getArgOperand(0);
        if (!(state[fd] & requiredPerm)) {
          outs() << " *** Insufficient privileges for \"" << syscall << "()\" in sandboxed method \"" << Caller->getName() << "\"\n";
          if (MDNode *N = Call->getMetadata("dbg")) {  // Here I is an LLVM instruction
            DILocation Loc(N);                      // DILocation is in DebugInfo.h
            unsigned Line = Loc.getLineNumber();
            StringRef File = Loc.getFilename();
            StringRef Dir = Loc.getDirectory();
            outs() << " +++ Line " << Line << " of file " << File << "\n";
          }
          outs() << "\n";
        }
      }
    }
  }
}
