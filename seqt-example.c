#include <riv.h>
#define SEQT_IMPL
#include "seqt.h"

int main() {
  seqt_init();
  seqt_play(seqt_make_source_from_file("songs/01.seqt.rivcard"), -1);
  while(riv_present()) {
    seqt_poll();
  }
}
