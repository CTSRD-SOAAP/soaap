#include "Util/InstUtils.h"

#include "Common/XO.h"
#include "Util/DebugUtils.h"

#include "llvm/IR/DebugInfo.h"

using namespace soaap;

void InstUtils::emitInstLocation(Instruction* I) {
  if (DILocation* loc = dyn_cast_or_null<DILocation>(I->getMetadata("dbg"))) {
    XO::Container locationContainer("location");
    XO::emit(
      " +++ Line {:line/%d} of file {:file/%s}",
      loc->getLine(),
      loc->getFilename().str().c_str());
    string library = DebugUtils::getEnclosingLibrary(I);
    if (!library.empty()) {
      XO::emit(" ({:library/%s} library)", library.c_str());
    }
    XO::emit("\n");
  }
}
