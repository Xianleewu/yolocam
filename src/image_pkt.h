#ifndef __IMAGE_PKT_H__
#define __IMAGE_PKT_H__

#include <stdint.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*img_pkt_unref)(void*,void*);

typedef struct {
  int dma_fd;
  void *vir_addr;
  struct timeval ts;
  size_t size;
  int width;
  int width_strip;
  int height;
  void *creator;
  int ref_count;
  img_pkt_unref unref;
} image_pkt_t;

void image_pkt_unref(image_pkt_t *img_pkt);

void image_pkt_ref(image_pkt_t *img_pkt);

#ifdef __cplusplus
}
#endif

#endif /*__IMAGE_PKT_H__*/
