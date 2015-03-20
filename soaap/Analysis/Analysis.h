#ifndef SOAAP_ANALYSIS_ANALYSIS_H
#define SOAAP_ANALYSIS_ANALYSIS_H

#include "llvm/IR/Module.h"
#include "Common/CmdLineOpts.h"
#include "Common/Sandbox.h"
#include "Util/DebugUtils.h"

using namespace llvm;

namespace soaap {
  class Analysis {
    public:
      virtual void doAnalysis(Module& M, SandboxVector& sandboxes) = 0;

      virtual bool shouldOutputWarningFor(Instruction* I) {
        return shouldOutputWarningFor(I->getParent()->getParent());
      }

      virtual bool shouldOutputWarningFor(Function* F) {
        if (!CmdLineOpts::WarnModules.empty() || !CmdLineOpts::NoWarnModules.empty()) {
          string module = DebugUtils::getEnclosingModule(F);
          if (module.empty()) {
            return true;
          }
          if (!CmdLineOpts::WarnModules.empty()) {
            return find(CmdLineOpts::WarnModules.begin(), CmdLineOpts::WarnModules.end(), module) != CmdLineOpts::WarnModules.end();
          }
          else if (!CmdLineOpts::NoWarnModules.empty()) {
            return find(CmdLineOpts::NoWarnModules.begin(), CmdLineOpts::NoWarnModules.end(), module) == CmdLineOpts::NoWarnModules.end();
          }
        }
        return true;
      }
  };
}

#endif
