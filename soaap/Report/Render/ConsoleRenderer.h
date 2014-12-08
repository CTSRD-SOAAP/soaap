#ifndef SOAAP_REPORT_RENDER_CONSOLERENDERER_H
#define SOAAP_REPORT_RENDER_CONSOLERENDERER_H

#include "Report/Render/Renderer.h"

namespace soaap {
  class ConsoleRenderer : public Renderer {
    public:
      ConsoleRenderer() : Renderer(true) { }
      virtual void visit(CallGraph* cg) { }
      virtual void visit(Report* r) { }
      virtual void visit(Vulnerability* v);
      virtual void visit(SourceLocation* s);
  };
}
#endif
