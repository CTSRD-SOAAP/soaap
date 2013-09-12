#ifndef SOAAP_ANALYSIS_INFOFLOW_CONTEXT_H
#define SOAAP_ANALYSIS_INFOFLOW_CONTEXT_H

#include "llvm/Support/Casting.h"

namespace soaap {
  class Context {
    public:
      enum ContextKind {
        CK_CONTEXT,
        CK_SANDBOX
      };
      ContextKind getKind() const { return Kind; }
      Context(ContextKind K) : Kind(K) { }
      Context() : Kind(CK_CONTEXT) { }
      static bool classof(const Context* C) { return C->getKind() == CK_CONTEXT; }

    private:
      const ContextKind Kind;
  };
  static Context* const NO_CONTEXT = new Context();
  static Context* const PRIV_CONTEXT = new Context();
  static Context* const SINGLE_CONTEXT = new Context();
}

#endif
