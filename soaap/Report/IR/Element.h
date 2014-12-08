#ifndef SOAAP_REPORT_IR_ELEMENT_H
#define SOAAP_REPORT_IR_ELEMENT_H

#include "Report/Render/Renderer.h"

namespace soaap {
  class Element {
    public:
      virtual void render(Renderer* r) = 0;
  };
}
#endif
