#include "soaap.h"

void gz_compress(int ifd, int ofd);

__soaap_var_read("compress") int some_flag = 0;
int some_other_flag = 1;
 
int main() {
  __soaap_create_persistent_sandbox("compress");
  some_flag = 1;
  gz_compress(0, 1);
  return 0;
}

__soaap_sandbox_persistent("compress")
void gz_compress(int ifd, int ofd) {
  if (some_flag && some_other_flag) {
    printf("gz_compress\n");
  }
}

