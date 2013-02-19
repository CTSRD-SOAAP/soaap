#include "soaap.h"

int b();

__soaap_callgates(b);

int b() {
  return 1;
}
