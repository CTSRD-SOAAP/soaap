#include "Report/Render/ConsoleRenderer.h"
#include "Common/Debug.h"
#include "Common/Typedefs.h"
#include "Report/IR/Vulnerability.h"
#include "Util/CallGraphUtils.h"
#include "Util/PrettyPrinters.h"
#include "Util/TypeUtils.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace soaap;

void ConsoleRenderer::visit(Vulnerability* v) {
  SourceLocation loc = v->getLocation();
  Function* F = loc.getFunction();
  if (v->isSandboxed()) {
    Sandbox* S = v->getSandbox();
    if (v->isVulnVendorLoc()) {
      // vulnerable vendor func
      outs() << " *** Sandboxed function \"" << F->getName() << "\" [" << S->getName() << "] is from a vulnerable vendor.\n";
      if (v->leaksLimitedRights()) {
        outs() << " *** A vulnerability here would not grant ambient authority to the attacker but would leak the following restricted rights:\n"     ;
      }
      else {
        outs() << " *** A vulnerability here would leak ambient authority to the attacker including full\n";
        outs() << " *** network and file system access.\n"; 
        goto outputTrace;
      }
    }
    else {
      // func with past vulnerabilities
      StringSet CVEs = v->getCVEs();
      outs() << " *** Sandboxed function \"" << F->getName() << "\" [" << S->getName() << "] has past-vulnerability annotations for \"" << TypeUtils::stringifyStringSet(CVEs) << "\".\n";
      if (v->leaksLimitedRights()) {
        outs() << " *** Another vulnerability here would not grant ambient authority to the attacker but would leak the following restricted rights:\n"     ;
      }
      else {
        outs() << " *** Another vulnerability here would leak ambient authority to the attacker including full\n";
        outs() << " *** network and file system access.\n"; 
        goto outputTrace;
      }
    }

    map<GlobalVariable*, vector<VariableRight> > varToPerms = v->getGlobals(); 
    if (!varToPerms.empty()) {
      outs() << " Global variables:\n";
      for (pair<GlobalVariable*,vector<VariableRight> > varPermPair : varToPerms) {
        GlobalVariable* G = varPermPair.first;
        vector<VariableRight> varPerms = varPermPair.second;
        outs() << " +++ ";
        for (VariableRight r : varPerms) {
          switch (r) {
            case READ: {
              outs() << "Read";
              break;
            }
            case WRITE: {
              outs() << "Write";
              break;
            }
            default: {
              errs() << "Error: unknown global variable right \"" << r << "\"\n";
            }
          }
          outs() << " access to \"" << G->getName() << "\"\n";
        }
      }
    }

    SDEBUG("soaap.analysis.vulnerability", 3, dbgs() << "Checking leaking of capabilities\n");
    ValueFunctionSetMap caps = v->getCapabilities();
    if (!caps.empty()) {
      outs() << " File descriptors:\n";
      for (pair<const Value*,FunctionSet> cap : caps) {
        const Argument* capArg = dyn_cast<const Argument>(cap.first);
        FunctionSet capPerms = cap.second;
        if (!capPerms.empty()) {
          outs() << " +++ Call " << CallGraphUtils::stringifyFunctionSet(capPerms) << " on file descriptor \"" << capArg->getName() << "\" passed into sandbox entrypoint \"" << S->getEntryPoint()->getName() << "\"\n";
        }
      }
    }

    SDEBUG("soaap.analysis.vulnerability", 3, dbgs() << "Checking callgates\n");
    FunctionVector callgates = v->getCallgates();
    if (!callgates.empty()) {
      outs() << " Call gates:\n";
      for (Function* F : callgates) {
        outs() << " +++ " << F->getName() << "\n";
      }
    }
    
    SDEBUG("soaap.analysis.vulnerability", 3, outs() << "Checking sandbox-private data\n");
    map<Value*, VariableType> privates = v->getPrivates();
    if (!privates.empty()) {
      outs() << " Private data:\n";
      for (pair<Value*,VariableType> p : privates) {
        Value* var = p.first;
        VariableType varType = p.second;
        outs() << " +++ ";
        switch (varType) {
          case LOCAL: {
            outs() << "Local variable";
            break;
          }
          case GLOBAL: {
            outs() << "Global variable";
            break;
          }
          case STRUCT_MEMBER: {
            outs() << "Struct member";
            break;
          }
          default: {
            errs() << "Error: unknown variable type\"" << varType << "\"\n";
            break;
          }
        }
        outs() << " \"" << var->getName() << "\"\n";
      }
    }
  }
  else {
    if (v->isVulnVendorLoc()) {
      outs() << " *** Function \"" << F->getName() << "\" is from a vulnerable vendor.\n";
      outs() << " *** A vulnerability here would leak ambient authority to the attacker including full\n";
      outs() << " *** network and file system access.\n"; 
    }
    else {
      StringSet CVEs = v->getCVEs();
      outs() << " *** Function \"" << F->getName() << "\" has past-vulnerability annotations for \"" << TypeUtils::stringifyStringSet(CVEs) << "\".\n";
      outs() << " *** Another vulnerability here would leak ambient authority to the attacker including full\n";
      outs() << " *** network and file system access.\n"; 
    }
  }

outputTrace:
  outs() << " Possible trace:\n";
  InstTrace trace;
  for (SourceLocation s : v->getStack()) {
    if (s.getInstruction() != nullptr) {
      trace.push_back(s.getInstruction());
    }
  }
  PrettyPrinters::ppTrace(trace);
  outs() << "\n\n";
}

void ConsoleRenderer::visit(SourceLocation* s) {
}
