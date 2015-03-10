#include "Util/InstUtils.h"

#include "Common/XO.h"

#include "llvm/IR/DebugInfo.h"

using namespace soaap;

void InstUtils::emitInstLocation(Instruction* I) {
  if (MDNode *N = I->getMetadata("dbg")) {
    XO::open_container("location");
    DILocation loc(N);
    XO::emit(
      " +++ Line {:line/%d} of file {:file/%s}\n",
      loc.getLineNumber(),
      loc.getFilename().str().c_str());
    XO::close_container("location");
  }
}
