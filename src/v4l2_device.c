// Copyright 2022 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v4l2_device.h"
#include "image_pkt.h"
#include "string.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define FMT_NUM_PLANES 1

struct buffer {
  void *start;
  size_t length;
  int export_fd;
  int sequence;
};

struct v4l2_buffer buf;

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define xcam_mem_clear(v_stack) memset(&(v_stack), 0, sizeof(v_stack))
#define CHECK_ARG(pointer)                                                     \
  {                                                                            \
    if (NULL == pointer) {                                                     \
      printf("invalid arguments!\n");                                          \
      return -1;                                                               \
    }                                                                          \
  }

typedef struct __v4l2_device_priv {
  int fd;
  enum v4l2_buf_type buf_type;
  struct buffer *buffers;
  unsigned int n_buffers;
} v4l2_device_priv_t;

#define ERR(...)                                                               \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
  } while (0)

static void errno_exit(const char *s) {
  ERR("[IMAGE v4l2_device]: %s error %d, %s\n", s, errno, strerror(errno));
}

static int xioctl(int fh, int request, void *arg) {
  int r;
  do {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);
  return r;
}

static void v4l2_stop_capture(v4l2_device_priv_t *v4l2_device_priv) {
  enum v4l2_buf_type type;

  type = v4l2_device_priv->buf_type;
  if (-1 == xioctl(v4l2_device_priv->fd, VIDIOC_STREAMOFF, &type))
    errno_exit("VIDIOC_STREAMOFF");
}

static int v4l2_start_capture(v4l2_device_priv_t *v4l2_priv) {
  unsigned int i;
  enum v4l2_buf_type type;

  for (i = 0; i < v4l2_priv->n_buffers; ++i) {
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = v4l2_priv->buf_type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_priv->buf_type) {
      struct v4l2_plane planes[FMT_NUM_PLANES];
      buf.m.planes = planes;
      buf.length = FMT_NUM_PLANES;
    }
    if (-1 == xioctl(v4l2_priv->fd, VIDIOC_QBUF, &buf))
      errno_exit("VIDIOC_QBUF");
  }
  type = v4l2_priv->buf_type;
  if (-1 == xioctl(v4l2_priv->fd, VIDIOC_STREAMON, &type))
    errno_exit("VIDIOC_STREAMON");

  return 0;
}

static void v4l2_uninit_device(v4l2_device_priv_t *v4l2_device_priv) {
  unsigned int i;

  for (i = 0; i < v4l2_device_priv->n_buffers; ++i) {
    if (-1 == munmap(v4l2_device_priv->buffers[i].start,
                     v4l2_device_priv->buffers[i].length))
      errno_exit("munmap");

    close(v4l2_device_priv->buffers[i].export_fd);
  }

  free(v4l2_device_priv->buffers);
}

static int v4l2_init_mmap(v4l2_device_t *v4l2_device) {
  v4l2_device_priv_t *v4l2_priv = (v4l2_device_priv_t *)v4l2_device->priv;
  struct v4l2_requestbuffers req;
  int fd_tmp = -1;
  int buffer_index = 0;

  CLEAR(req);

  fd_tmp = v4l2_priv->fd;

  req.count = v4l2_device->buf_count;
  req.type = v4l2_priv->buf_type;
  req.memory = V4L2_MEMORY_MMAP;

  struct buffer *tmp_buffers = NULL;

  if (-1 == xioctl(fd_tmp, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      ERR("%s does not support memory mapping\n", v4l2_device->dev_name);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < v4l2_device->buf_count) {
    ERR("Insufficient buffer memory on %s\n", v4l2_device->dev_name);
  }

  if (req.count) {
    v4l2_priv->n_buffers = req.count;
    printf("required buffers:[%d]\n", req.count);
  }

  tmp_buffers =
      (struct buffer *)calloc(v4l2_priv->n_buffers, sizeof(struct buffer));

  if (!tmp_buffers) {
    ERR("Out of memory\n");
    return -1;
  }

  v4l2_priv->buffers = tmp_buffers;

  for (buffer_index = 0; buffer_index < req.count; ++buffer_index) {
    struct v4l2_buffer buf;
    struct v4l2_plane planes[FMT_NUM_PLANES];
    CLEAR(buf);
    CLEAR(planes);

    buf.type = v4l2_priv->buf_type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = buffer_index;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_priv->buf_type) {
      buf.m.planes = planes;
      buf.length = FMT_NUM_PLANES;
    }

    if (-1 == xioctl(fd_tmp, VIDIOC_QUERYBUF, &buf))
      errno_exit("VIDIOC_QUERYBUF");

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_priv->buf_type) {
      tmp_buffers[buffer_index].length = buf.m.planes[0].length;
      tmp_buffers[buffer_index].start = mmap(
          NULL /* start anywhere */, buf.m.planes[0].length,
          PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */,
          fd_tmp, buf.m.planes[0].m.mem_offset);
    } else {
      tmp_buffers[buffer_index].length = buf.length;
      tmp_buffers[buffer_index].start =
          mmap(NULL /* start anywhere */, buf.length,
               PROT_READ | PROT_WRITE /* required */,
               MAP_SHARED /* recommended */, fd_tmp, buf.m.offset);
    }

    if (MAP_FAILED == tmp_buffers[buffer_index].start)
      errno_exit("mmap");

    // export buf dma fd
    struct v4l2_exportbuffer expbuf;
    CLEAR(expbuf);
    expbuf.type = v4l2_priv->buf_type;
    expbuf.index = buffer_index;
    expbuf.flags = O_CLOEXEC;
    if (xioctl(fd_tmp, VIDIOC_EXPBUF, &expbuf) < 0) {
      errno_exit("get dma buf failed\n");
    }
    tmp_buffers[buffer_index].export_fd = expbuf.fd;
  }

  return 0;
}

static int v4l2_init_device(v4l2_device_t *v4l2_device) {
  struct v4l2_capability cap;
  struct v4l2_format fmt;
  v4l2_device_priv_t *v4l2_priv = NULL;
  CHECK_ARG(v4l2_device);
  v4l2_priv = (v4l2_device_priv_t *)v4l2_device->priv;

  if (-1 == xioctl(v4l2_priv->fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      ERR("%s is no V4L2 device\n", v4l2_device->dev_name);
    } else {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
      !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
    ERR("%s is not a video capture device, capabilities: %x\n",
        v4l2_device->dev_name, cap.capabilities);
    return -1;
  }

  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    ERR("%s does not support streaming i/o\n", v4l2_device->dev_name);
    return -1;
  }

  if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
    v4l2_priv->buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    CLEAR(fmt);
    fmt.type = v4l2_priv->buf_type;
    fmt.fmt.pix_mp.width = v4l2_device->width;
    fmt.fmt.pix_mp.height = v4l2_device->height;
    fmt.fmt.pix_mp.pixelformat = v4l2_device->format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
  } else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
    v4l2_priv->buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CLEAR(fmt);
    fmt.type = v4l2_priv->buf_type;
    fmt.fmt.pix_mp.width = v4l2_device->width;
    fmt.fmt.pix_mp.height = v4l2_device->height;
    fmt.fmt.pix_mp.pixelformat = v4l2_device->format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
  }

  if (-1 == xioctl(v4l2_priv->fd, VIDIOC_S_FMT, &fmt))
    errno_exit("VIDIOC_S_FMT");

  v4l2_init_mmap(v4l2_device);

  return 0;
}

static void v4l2_close_device(v4l2_device_t *v4l2_device) {
  v4l2_device_priv_t *v4l2_device_priv =
      (v4l2_device_priv_t *)v4l2_device->priv;
  if (-1 == close(v4l2_device_priv->fd))
    errno_exit("close");

  v4l2_device_priv->fd = -1;
}

static int v4l2_open_device(v4l2_device_t *v4l2_device, const char *dev_name) {
  v4l2_device_priv_t *v4l2_priv = NULL;
  CHECK_ARG(v4l2_device);
  v4l2_priv = (v4l2_device_priv_t *)v4l2_device->priv;
  v4l2_priv->fd = open(dev_name, O_RDWR /*| O_NONBLOCK */, 0);
  if (-1 == v4l2_priv->fd) {
    ERR("Cannot open '%s': %d, %s\n", dev_name, errno, strerror(errno));
    return -1;
  }
  return 0;
}

/*
 * external funtions
 * */

void v4l2_device_deinit(v4l2_device_t *v4l2_device) {
  v4l2_device_priv_t *v4l2_priv = NULL;
  if(!v4l2_device) {
      return;
  }
  v4l2_priv = (v4l2_device_priv_t *)v4l2_device->priv;
  v4l2_stop_capture(v4l2_priv);
  v4l2_uninit_device(v4l2_priv);
}

int v4l2_device_init(v4l2_device_t *v4l2_device) {
  v4l2_device_priv_t *v4l2_priv = NULL;
  CHECK_ARG(v4l2_device);
  v4l2_priv = (v4l2_device_priv_t *)v4l2_device->priv;
  if (v4l2_open_device(v4l2_device, v4l2_device->dev_name) != 0) {
    return -1;
  }
  if (v4l2_init_device(v4l2_device) != 0) {
    return -1;
  }
  if (v4l2_start_capture(v4l2_priv) != 0) {
    return -1;
  }
  return 0;
}

v4l2_device_t *v4l2_device_create(const char *vdev_name, const char *format,
                                  int width, int height, int buf_num) {
  v4l2_device_t *v4l2_device = NULL;

  if (!vdev_name || width <= 0 || height <= 0 || !format) {
    errno_exit("invalid argument");
  }

  v4l2_device = (v4l2_device_t *)malloc(sizeof(v4l2_device_t));

  v4l2_device->format = v4l2_fourcc(format[0], format[1], format[2], format[3]);
  v4l2_device->width = width;
  v4l2_device->height = height;
  strcpy(v4l2_device->dev_name, vdev_name);
  v4l2_device->priv = (v4l2_device_priv_t *)malloc(sizeof(v4l2_device_priv_t));
  v4l2_device->buf_count = buf_num;

  return v4l2_device;
}

void v4l2_device_destroy(v4l2_device_t *v4l2_device) {
  v4l2_device_deinit(v4l2_device);
  if (v4l2_device) {
    if (v4l2_device->priv) {
      free(v4l2_device->priv);
    }
    free(v4l2_device);
  }
}

static void v4l2_device_buffer_unref(void *priv, void *pkt) {
  v4l2_device_priv_t *v4l2_priv = (v4l2_device_priv_t *)priv;
  image_pkt_t *img_pkt = (image_pkt_t *)pkt;
  int i = 0;
  for (i = 0; i < v4l2_priv->n_buffers; ++i) {
    if (img_pkt->dma_fd == v4l2_priv->buffers[i].export_fd) {
      struct v4l2_buffer buf;
      CLEAR(buf);
      buf.type = v4l2_priv->buf_type;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;
      buf.m.fd = img_pkt->dma_fd;

      if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_priv->buf_type) {
        struct v4l2_plane planes[FMT_NUM_PLANES];
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
      }
      if (-1 == xioctl(v4l2_priv->fd, VIDIOC_QBUF, &buf)) {
        errno_exit("VIDIOC_QBUF");
      }
      return;
    }
  }
  printf("[V4l2 Device] unref v4l2 buffer failed!\n");
}

int v4l2_device_read(v4l2_device_t *v4l2_device, image_pkt_t *image_pkt) {
  v4l2_device_priv_t *v4l2_priv = NULL;
  struct v4l2_buffer buf;
  int bytesused;

  CHECK_ARG(v4l2_device);
  v4l2_priv = (v4l2_device_priv_t *)v4l2_device->priv;
  buf.type = v4l2_priv->buf_type;
  buf.memory = V4L2_MEMORY_MMAP;

  struct v4l2_plane planes[FMT_NUM_PLANES];
  if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_priv->buf_type) {
    buf.m.planes = planes;
    buf.length = FMT_NUM_PLANES;
  }

  if (-1 == xioctl(v4l2_priv->fd, VIDIOC_DQBUF, &buf)) {
    printf("VIDIOC_DQBUF Failed!\n");
    return -1;
  }

  if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_priv->buf_type)
    bytesused = buf.m.planes[0].bytesused;
  else
    bytesused = buf.bytesused;

  image_pkt->creator = v4l2_priv;
  image_pkt->dma_fd = v4l2_priv->buffers[buf.index].export_fd;
  image_pkt->ts = buf.timestamp;
  image_pkt->vir_addr = v4l2_priv->buffers[buf.index].start;
  image_pkt->size = bytesused;
  image_pkt->width = v4l2_device->width;
  image_pkt->height = v4l2_device->height;
  image_pkt->unref = v4l2_device_buffer_unref;

  return 0;
}

