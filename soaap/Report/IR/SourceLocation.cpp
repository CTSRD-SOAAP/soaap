#include "Report/IR/SourceLocation.h"

#include "Util/DebugUtils.h"

#include "llvm/IR/DebugInfo.h"

using namespace soaap;

SourceLocation::SourceLocation(Instruction* inst) : I(inst) {
  F = I->getParent()->getParent();
  if (MDNode* N = I->getMetadata("dbg")) {
    DILocation loc(N);
    filename = loc.getFilename();
    lineNumber = loc.getLineNumber();
  }
  else {
    filename = "";
    lineNumber = -1;
  }
}

SourceLocation::SourceLocation(Function* func) : F(func) {
  F = func;
  pair<string,int> loc = DebugUtils::getFunctionLocation(F);
  filename = loc.first;
  lineNumber = loc.second;
}

void SourceLocation::render(Renderer* r) {
  r->visit(this);
}
