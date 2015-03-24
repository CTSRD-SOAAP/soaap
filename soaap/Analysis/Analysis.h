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
        if (!CmdLineOpts::WarnLibs.empty() || !CmdLineOpts::NoWarnLibs.empty()) {
          string library = DebugUtils::getEnclosingLibrary(F);
          if (library.empty()) {
            return true;
          }
          if (!CmdLineOpts::WarnLibs.empty()) {
            return find(CmdLineOpts::WarnLibs.begin(), CmdLineOpts::WarnLibs.end(), library) != CmdLineOpts::WarnLibs.end();
          }
          else if (!CmdLineOpts::NoWarnLibs.empty()) {
            return find(CmdLineOpts::NoWarnLibs.begin(), CmdLineOpts::NoWarnLibs.end(), library) == CmdLineOpts::NoWarnLibs.end();
          }
        }
        return true;
      }
  };
}

#endif
