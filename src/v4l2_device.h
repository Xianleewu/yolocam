#ifndef __V4L2_DEVICE_H__
#define __V4L2_DEVICE_H__

#include "image_pkt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __v4l2_device {
  char dev_name[255];
  int buf_count;
  int width;
  int height;
  int format;
  void *priv;
} v4l2_device_t;

void v4l2_device_deinit(v4l2_device_t *v4l2_device);
int v4l2_device_init(v4l2_device_t *v4l2_device);
v4l2_device_t *v4l2_device_create(const char *vdev_name, const char *format,
                                  int width, int height, int buf_num);
void v4l2_device_destroy(v4l2_device_t *v4l2_device);
int v4l2_device_read(v4l2_device_t *v4l2_device, image_pkt_t *image_pkt);

#ifdef __cplusplus
}
#endif

#endif /*__V4L2_DEVICE_H__*/
