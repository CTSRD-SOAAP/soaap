#ifndef SOAAP_REPORT_IR_REPORT_H
#define SOAAP_REPORT_IR_REPORT_H

#include "Report/IR/CallGraph.h"
#include "Report/IR/Element.h"
#include "Report/IR/Vulnerability.h"
#include "Report/Render/Renderer.h"

#include <vector>

using namespace std;

namespace soaap {
  class Report {
    public:
      static Report* v();
      void addRenderer(Renderer* r);
      void setCallGraph(CallGraph* cg);
      CallGraph* getCallGraph() { return callgraph; }
      void addVulnerability(Vulnerability* v);
      vector<Vulnerability*> getVulnerabilities() { return vulnerabilities; }
      void render();

    protected:  
      CallGraph* callgraph;
      vector<Vulnerability*> vulnerabilities;
      vector<Renderer*> renderers;
      void renderIfRealtime(Element* e);

    private:
      static Report* instance;
      Report() { } // singleton instance
  };
}
#endif
