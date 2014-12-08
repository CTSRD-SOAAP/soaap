#ifndef SOAAP_REPORT_IR_SOURCELOCATION_H
#define SOAAP_REPORT_IR_SOURCELOCATION_H

#include "Report/IR/Element.h"

#include "llvm/IR/Function.h"
#include <string>

using namespace llvm;
using namespace std;

namespace soaap {
  class SourceLocation : public Element {
    public:
      SourceLocation(Instruction* inst);
      SourceLocation(Function* func);
      virtual void render(Renderer* r);
      Instruction* getInstruction() { return I; }
      Function* getFunction() { return F; }
      string getFilename() { return filename; }
      int getLineNumber() { return lineNumber; }

    protected:
      Instruction* I;
      Function* F;
      string filename;
      int lineNumber;
  };
}
#endif
