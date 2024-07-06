#include <stdio.h>
#include "image_pkt.h"

void image_pkt_unref(image_pkt_t *img_pkt) {
  if (!img_pkt) {
    return;
  }
  if (__sync_sub_and_fetch(&img_pkt->ref_count, 1) == 0) {
    if (img_pkt->unref) {
      img_pkt->unref(img_pkt->creator, img_pkt);
    }
    free(img_pkt);
  }
}

void image_pkt_ref(image_pkt_t *img_pkt) {
  __sync_add_and_fetch(&img_pkt->ref_count, 1);
}


