#include "Util/InstUtils.h"

#include "Common/XO.h"
#include "Util/DebugUtils.h"

#include "llvm/IR/DebugInfo.h"

using namespace soaap;

void InstUtils::emitInstLocation(Instruction* I) {
  if (MDNode *N = I->getMetadata("dbg")) {
    XO::open_container("location");
    DILocation loc(N);
    XO::emit(
      " +++ Line {:line/%d} of file {:file/%s}",
      loc.getLineNumber(),
      loc.getFilename().str().c_str());
    string module = DebugUtils::getEnclosingModule(I);
    if (!module.empty()) {
      XO::emit(" ({:module/%s} module)", module.c_str());
    }
    XO::emit("\n");
    XO::close_container("location");
  }
}
