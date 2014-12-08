#ifndef SOAAP_REPORT_RENDER_RENDERER_H
#define SOAAP_REPORT_RENDER_RENDERER_H

namespace soaap {
  // to avoid header include cycles
  class Report;
  class SourceLocation;
  class Vulnerability;
  class Renderer {
    public:
      Renderer(bool rtime) : realtime(rtime) { }
      virtual bool isRealtime() { return realtime; }
      virtual void visit(Report* r) = 0;
      virtual void visit(Vulnerability* v) = 0;
      virtual void visit(SourceLocation* s) = 0;

    protected:
      bool realtime;
  };
}
#endif
