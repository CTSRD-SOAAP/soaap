#include "Report/IR/CallGraph.h"

using namespace soaap;

void CallGraph::render(Renderer* r) {
  r->visit(this);
}
