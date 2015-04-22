#include "Util/InstUtils.h"

#include "Common/XO.h"
#include "Util/DebugUtils.h"

#include "llvm/IR/DebugInfo.h"

using namespace soaap;

void InstUtils::emitInstLocation(Instruction* I) {
  if (MDLocation* loc = dyn_cast_or_null<MDLocation>(I->getMetadata("dbg"))) {
    XO::open_container("location");
    XO::emit(
      " +++ Line {:line/%d} of file {:file/%s}",
      loc->getLine(),
      loc->getFilename().str().c_str());
    string library = DebugUtils::getEnclosingLibrary(I);
    if (!library.empty()) {
      XO::emit(" ({:library/%s} library)", library.c_str());
    }
    XO::emit("\n");
    XO::close_container("location");
  }
}
