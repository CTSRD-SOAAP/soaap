#include "soaap.h"
#include "avcodec.h"

void decode_audio(int in, int out);
void encode_audio(int in, int out);

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("decoder");
  decode_audio(0, 1);
  encode_audio(0, 1);
  return 0;
}

__soaap_sandbox_persistent("decoder")
void decode_audio(__soaap_fd_read int in, __soaap_fd_write int out) {
  avcodec_decode_audio4();
}

void encode_audio(int in, int out) {
  avcodec_encode_audio();
}
