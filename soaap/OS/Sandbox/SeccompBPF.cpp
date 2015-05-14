#include "OS/Sandbox/SeccompBPF.h"

#include "Common/CmdLineOpts.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <fstream>

using namespace soaap;
using namespace std;

SeccompBPF::SeccompBPF() {
  if (!CmdLineOpts::SandboxPolicy.empty()) {
    dbgs() << "Opening policy ifle " << CmdLineOpts::SandboxPolicy << "\n";
    ifstream policyFile(CmdLineOpts::SandboxPolicy);
    if (policyFile.is_open()) {
      string line;
      while (getline(policyFile,line)) {
        dbgs() << "Adding allowed sys call " << line << "\n";
        addPermittedSysCall(line);
      }
    }
    else {
      errs() << "ERROR: unable to open sandbox policy file \"" << CmdLineOpts::SandboxPolicy << "\"\n";
    }
  }
}
