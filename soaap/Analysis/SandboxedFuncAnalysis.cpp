#include "Analysis/SandboxedFuncAnalysis.h"

#include "soaap.h"
#include "Common/Debug.h"
#include "Common/XO.h"
#include "Util/CallGraphUtils.h"
#include "Util/DebugUtils.h"
#include "Util/SandboxUtils.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

using namespace soaap;

void SandboxedFuncAnalysis::doAnalysis(Module& M, SandboxVector& sandboxes) {
  // first find all methods annotated as being sandboxed and then check calls within sandboxes
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
        if (annotationStrArrayCString.startswith(SOAAP_SANDBOXED)) {
          SandboxVector annotatedSandboxes;
          string sandboxListCsv = annotationStrArrayCString.substr(strlen(SOAAP_SANDBOXED)+1); //+1 because of _
          SDEBUG("soaap.analysis.sandboxed", 3, dbgs() << INDENT_1 << "sandboxed annotation " << annotationStrArrayCString << " found: " << annotatedFunc->getName() << ", sandboxList: " << sandboxListCsv << "\n");
          istringstream ss(sandboxListCsv);
          string sandbox;
          while(getline(ss, sandbox, ',')) {
            // trim leading and trailing spaces and quotes ("")
            size_t start = sandbox.find_first_not_of(" \"");
            size_t end = sandbox.find_last_not_of(" \"");
            sandbox = sandbox.substr(start, end-start+1);
            SDEBUG("soaap.analysis.sandboxed", 3, dbgs() << INDENT_2 << "Sandbox: " << sandbox << "\n");
            if (Sandbox* S = SandboxUtils::getSandboxWithName(sandbox, sandboxes)) {
              SDEBUG("soaap.analysis.sandboxed", 3, dbgs() << INDENT_3 << "Adding sandbox\n");
              annotatedSandboxes.push_back(S);
            }
          }

          funcToSandboxes[annotatedFunc] = annotatedSandboxes;
        }
      }
    }
  }

  // now check calls within sandboxes
  XO::List sandboxedFuncList("sandboxed_func");
  for (pair<Function*,SandboxVector> p : funcToSandboxes) {
    Function* F = p.first;
    if (shouldOutputWarningFor(F)) {
      SandboxVector& sandboxingRestriction = p.second;
      SDEBUG("soaap.analysis.sandboxed", 3, dbgs() << "Processing " << F->getName() << " with sandboxing restrictions: " << SandboxUtils::stringifySandboxVector(sandboxingRestriction) << "\n");
      
      // first determine if we will be outputting a warning
      SandboxVector containingSandboxes = SandboxUtils::getSandboxesContainingMethod(F, sandboxes);
      SandboxVector disallowedSandboxes;
      if (!sandboxingRestriction.empty() && !containingSandboxes.empty()) {
        for (Sandbox* S : containingSandboxes) {
          if (find(sandboxingRestriction.begin(), sandboxingRestriction.end(), S) == sandboxingRestriction.end()) {
            disallowedSandboxes.push_back(S);
          }
        }
      }
      bool privileged = SandboxUtils::isPrivilegedMethod(F, M);
      bool outputWarning = privileged || !disallowedSandboxes.empty();

      // now output the warning
      if (outputWarning) {
        XO::Instance sandboxedFuncInstance(sandboxedFuncList);
        XO::emit("\n");
        XO::emit("{e:function}", F->getName().str().c_str());
        
        // output first bit of warning, as it is common
        if (sandboxingRestriction.empty()) {
          XO::emit("{e:sandbox_restriction/some_sandbox}");
          XO::emit(" *** Function \"{d:function}\" has been annotated as only being allowed to execute in a sandbox but ",
                   F->getName().str().c_str());
        }
        else {
          XO::List sandboxRestrictionList("sandbox_restriction");
          for (Sandbox* S : sandboxingRestriction) {
            XO::Instance sandboxRestrictionInstance(sandboxRestrictionList);
            XO::emit("{e:name/%s}", S->getName().c_str());
          }
          sandboxRestrictionList.close();
          XO::emit(" *** Function \"{d:function}\" has been annotated as only being allowed to execute in the sandboxes: {d:restricted_sandboxes} but ",
                   F->getName().str().c_str(),
                   SandboxUtils::stringifySandboxVector(sandboxingRestriction).c_str());
        }

        XO::List sandboxViolationList("sandbox_violation");
        if (privileged) {
          XO::Instance sandboxViolationInstance(sandboxViolationList);
          XO::emit("{e:type/%s}","privileged");
          XO::emit("it may execute in a privileged context\n");
          if (CmdLineOpts::isSelected(SoaapAnalysis::SandboxedFuncs, CmdLineOpts::OutputTraces)) {
            CallGraphUtils::emitCallTrace(F, NULL, M);
          }
        }

        if (!disallowedSandboxes.empty()) {
          XO::emit("{d:additonal_text}it executes in the sandboxes: {d:containing_sandboxes} of which {d:disallowed_sandboxes} are disallowed\n",
                   privileged ? "\n Additionally, " : "",
                   SandboxUtils::stringifySandboxVector(containingSandboxes).c_str(),
                   SandboxUtils::stringifySandboxVector(disallowedSandboxes).c_str());
          bool first = true;
          for (Sandbox* S : disallowedSandboxes) {
            XO::Instance sandboxViolationInstance(sandboxViolationList);
            XO::emit("{e:type/%s}{e:name/%s}", "sandbox", S->getName().c_str());
            if (CmdLineOpts::isSelected(SoaapAnalysis::SandboxedFuncs, CmdLineOpts::OutputTraces)) {
              if (!first) {
                XO::emit("\n");
              }
              //XO::emit(" Sandbox: {d:sandbox}\n", S->getName().c_str());
              CallGraphUtils::emitCallTrace(F, S, M);
            }
            first = false;
          }
        }
        XO::emit("\n");
      }
    }
  }
}
