#ifndef SOAAP_INSTRUMENT_PERFORMANCEEMULATIONINSTRUMENTER_H
#define SOAAP_INSTRUMENT_PERFORMANCEEMULATIONINSTRUMENTER_H

#include "Instrument/Instrumenter.h"

namespace soaap {
  class PerformanceEmulationInstrumenter : public Instrumenter {
    public:
      virtual void instrument(Module& M, SandboxVector& sandboxes);
  };
}

#endif
