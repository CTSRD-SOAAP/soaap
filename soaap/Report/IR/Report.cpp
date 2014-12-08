#include "Report/IR/Report.h"

using namespace soaap;

Report* Report::instance = nullptr;

Report* Report::v() {
  if (instance == nullptr) {
    instance = new Report;
  }
  return instance;
}

void Report::addRenderer(Renderer* r) {
  renderers.push_back(r);
}

void Report::setCallGraph(CallGraph* cg) {
  callgraph = cg;
}

void Report::addVulnerability(Vulnerability* v) {
  vulnerabilities.push_back(v);
  renderIfRealtime(v);
}

void Report::renderIfRealtime(Element* e) {
  for (Renderer* r : renderers) {
    if (r->isRealtime()) {
      e->render(r);
    }
  }
}

void Report::render() {
  for (Renderer* r : renderers) {
    if (!r->isRealtime()) {
      r->visit(this);
    }
  }
}
