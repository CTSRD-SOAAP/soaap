#include "soaap.h"

void parse(int ifd, void* t);
void not_sandboxed();

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("parser");
  parse(argc, NULL);
  not_sandboxed();
}

 __soaap_sandbox_persistent("parser") 
void parse(__soaap_fd_read int ifd, void* t) {
  if (ifd == 3) {
    __soaap_vuln_pt("CVE-2005-ABC");
  }
}

__soaap_vuln_fn("CVE-2005-DEF")
void not_sandboxed() {
}
