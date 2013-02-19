#include "soaap.h"

int a();

__soaap_callgates(a);

int a() {
  return 0;
}
