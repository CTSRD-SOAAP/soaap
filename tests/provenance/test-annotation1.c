#include "soaap.h"

__soaap_provenance("org.gnu.libc")

int b();

int a() {
  return 3;
}

int main() {
  a();
  b();
}
