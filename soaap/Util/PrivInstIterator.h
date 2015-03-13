#ifndef SOAAP_UTIL_PRIVINSTITERATOR_H
#define SOAAP_UTIL_PRIVINSTITERATOR_H

#include "Common/Sandbox.h"
#include "Util/SandboxUtils.h"

#include "llvm/IR/InstIterator.h"

using namespace llvm;

namespace soaap {
  class PrivInstIterator : public inst_iterator {
    public:
      PrivInstIterator(Function& F, SandboxVector& sboxes) : inst_iterator(F), sandboxes(sboxes) {
        M = F.getParent();  
      }
      
      PrivInstIterator(Function& F, bool b) : inst_iterator(F, b) { }

      InstIterator& operator++() {
        do {
          InstIterator::operator++();
          if (atEnd()) {
            break;
          }
          Instruction& I = operator*();
          if (SandboxUtils::isPrivilegedInstruction(&I, sandboxes, *M)) {
            break;
          }
        } while (true);
        return *this;
      }

    private:
      SandboxVector sandboxes;
      Module* M;
  };
  
  inline PrivInstIterator priv_inst_begin(Function *F, SandboxVector& sandboxes) {
    return PrivInstIterator(*F, sandboxes);
  }
  
  inline PrivInstIterator priv_inst_end(Function *F) {
    return PrivInstIterator(*F, true);
  }
}

#endif
