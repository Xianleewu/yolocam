/* Copyright (C)
 * 2023 - Xianlee xianleewu@163.com
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <string.h>
extern "C" {
#include "ptr_queue.h"
#include "rkdrm_display.h"
#include "rknn_api.h"
#include "serial_comm.h"
#include "v4l2_device.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
}

#include "dma_alloc.h"
#include "log.h"
#include "rga/RgaApi.h"
#include "rga/RgaUtils.h"
#include "rga/drmrga.h"
#include "rga/im2d.hpp"
#include <fcntl.h>
#include <linux/videodev2.h>

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/opencv.hpp"
#include "postprocess.h"
#include "src/rknn_runner.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "DRMCAM"

enum { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

int enable_minilog = 0;
int rkipc_log_level = LOG_DEBUG;

long long start_time;
long long end_time;

static int main_loop_run = 1;

int input_width = 1920;
int input_height = 1080;
int output_width = 480;
int output_height = 480;

static char RKNN_YOLO_MODEL[] = {"/oem/model/yolov5s-640-640.rknn"};
static char ini_config_file[] = {"/usr/share/yolocam/default.ini"};
static char remote_host[] = {"192.168.100.11"};
static int remote_port = 9900;
static char output_tty[] = {"/dev/ttyS1"};
static serialport_t serial_port;
static float serial_min_prop = 0.35;

typedef struct {
  pthread_t rknn_thread;
  pthread_t v4l2_thread;
  pthread_t disp_thread;
  pthread_t uart_thread;
  pthread_attr_t attr;
  ptr_queue_t disp_queue;
  ptr_queue_t rknn_queue;
  ptr_queue_t info_queue;
  ptr_queue_t uart_queue;
  v4l2_device_t *v4l2_device;
  rknn_runner_t *runner;
  struct display drm_disp;
} smart_cam_t;

smart_cam_t smart_cam;

static void sig_proc(int signo) {
  LOG_INFO("received signo %d \n", signo);
  main_loop_run = 0;
}

static int check_sololinker_device() {
  FILE *fp;
  char model[256];

  fp = fopen("/proc/device-tree/model", "r");
  if (fp == NULL) {
    perror("Error opening file");
    return -1;
  }

  if (fscanf(fp, "%255s", model) != 1) {
    perror("Error reading file");
    fclose(fp);
    return -1;
  }

  fclose(fp);

  if (strstr(model, "Hinlink") == NULL) {
    return -1;
  } else {
    return 0;
  }
}

void v4l2_buffer_recycling(smart_cam_t *cam_ctx) {
  image_pkt_t *img_pkt_rknn = NULL;
  image_pkt_t *img_pkt_disp = NULL;

  img_pkt_rknn = (image_pkt_t *)ptr_queue_dequeue(&cam_ctx->rknn_queue, 1);
  if (img_pkt_rknn) {
    image_pkt_unref(img_pkt_rknn);
  }

  img_pkt_disp = (image_pkt_t *)ptr_queue_dequeue(&cam_ctx->disp_queue, 1);
  if (img_pkt_disp) {
    image_pkt_unref(img_pkt_disp);
  }
}

static void format_timeval(char *buffer, size_t buffer_size) {
  struct timeval tv;
  struct tm *tm_info;
  char time_string[40];

  gettimeofday(&tv, NULL);

  tm_info = localtime(&tv.tv_sec);

  strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", tm_info);

  snprintf(buffer, buffer_size, "%s.%06ld", time_string, tv.tv_usec);
}

int detect_result_to_serialport(serialport_t *port,
                                detect_result_group_t *det_grp) {
  char time_str[64] = {0};
  char detect_result_str[256] = {0};

  if (!port) {
    return -1;
  }

  if (!det_grp || det_grp->count <= 0) {
    return -1;
  }

  format_timeval(time_str, 64);

  // group start
  serial_send(port, ">\n", sizeof(">\n"));
  for (int i = 0; i < det_grp->count; i++) {
    detect_result_t *det_result = &(det_grp->results[i]);
    if (det_result->prop < serial_min_prop) {
      continue;
    }
    int formated_len = snprintf(
        detect_result_str, 256, "$%s,%s,%.1f,%d,%d,%d,%d#\n", time_str,
        det_result->name, det_result->prop * 100, det_result->box.left,
        det_result->box.top, det_result->box.right, det_result->box.bottom);
    serial_send(port, detect_result_str, formated_len);
  }
  // group end
  serial_send(port, "<\n", sizeof("<\n"));

  return 0;
}

void *thread_func_v4l2(void *arg) {
  smart_cam_t *cam_ctx = (smart_cam_t *)(arg);
  while (main_loop_run) {
    image_pkt_t *img_pkt = (image_pkt_t *)malloc(sizeof(image_pkt_t));
    memset(img_pkt, 0, sizeof(image_pkt_t));

    if (0 != v4l2_device_read(smart_cam.v4l2_device, img_pkt)) {
      // v4l2_buffer_recycling(cam_ctx);
      continue;
    }

    image_pkt_ref(img_pkt);
    if (0 != ptr_queue_enqueue(&cam_ctx->disp_queue, img_pkt, 10)) {
      image_pkt_unref(img_pkt);
    }

    image_pkt_ref(img_pkt);
    if (0 != ptr_queue_enqueue(&cam_ctx->rknn_queue, img_pkt, 10)) {
      image_pkt_unref(img_pkt);
    }
  }

  v4l2_device_destroy(cam_ctx->v4l2_device);

  return NULL;
}

// 获取当前时间（微秒级）
long long get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

void *thread_func_rknn(void *arg) {
  smart_cam_t *cam_ctx = (smart_cam_t *)(arg);
  int ret = 0;
  im_rect crop_rect;
  crop_rect.x = 0;
  crop_rect.y = 0;
  crop_rect.width = (input_width < input_height) ? input_width : input_height;
  crop_rect.height = (input_width < input_height) ? input_width : input_height;

  while (main_loop_run) {
    image_pkt_t *img_pkt = NULL;
    int rknn_width = 0;
    int rknn_height = 0;
    int rknn_format = 0;
    rga_buffer_t rknn_img;
    rga_buffer_t src_img;
    rga_buffer_handle_t src_handle = 0;
    rga_buffer_handle_t rknn_handle = 0;
    int src_format = 0;
    rknn_format = RK_FORMAT_RGB_888;

    memset(&src_img, 0, sizeof(src_img));
    memset(&rknn_img, 0, sizeof(rknn_img));

    img_pkt = (image_pkt_t *)ptr_queue_dequeue(&cam_ctx->rknn_queue, 1000);
    if (!img_pkt) {
      continue;
    }

    if (!cam_ctx->runner || !cam_ctx->runner->input_attrs) {
      image_pkt_unref(img_pkt);
      continue;
    }

    rknn_width = cam_ctx->runner->input_attrs[0].dims[2];
    rknn_height = cam_ctx->runner->input_attrs[0].dims[1];
    rknn_handle = importbuffer_fd(cam_ctx->runner->input_mems[0]->fd,
                                  cam_ctx->runner->input_mems[0]->size);
    if (rknn_handle == 0) {
      printf("import rknn buffer failed!\n");
      goto release_buffer;
    }

    rknn_img =
        wrapbuffer_handle(rknn_handle, rknn_width, rknn_height, rknn_format);

    src_format = RK_FORMAT_YCbCr_420_SP;

    src_handle = importbuffer_fd(img_pkt->dma_fd, img_pkt->size);
    if (src_handle == 0) {
      printf("import src buffer in rknn failed!\n");
      goto release_buffer;
    }

    src_img = wrapbuffer_handle(src_handle, img_pkt->width, img_pkt->height,
                                src_format);

    ret = improcess(src_img, rknn_img, {}, crop_rect, {}, {}, IM_SYNC);
    if (ret != IM_STATUS_SUCCESS) {
      printf("%s running failed, %s\n", LOG_TAG, imStrError((IM_STATUS)ret));
      goto release_buffer;
    }

    image_pkt_unref(img_pkt);

    start_time = get_timestamp();
    rknn_runner_process(cam_ctx->runner, NULL);
    end_time = get_timestamp();
    // printf("---->RKNN run time:%lld\n", end_time - start_time);

  release_buffer:
    if (src_handle) {
      releasebuffer_handle(src_handle);
      src_handle = 0;
    }
    if (rknn_handle) {
      releasebuffer_handle(rknn_handle);
      rknn_handle = 0;
    }
  }

  rknn_runner_destroy(cam_ctx->runner);

  return NULL;
}

void *thread_func_disp(void *arg) {
  smart_cam_t *cam_ctx = (smart_cam_t *)(arg);
  int v4l2_index = 0;
  detect_result_group_t *det_grp = NULL;
  int det_lifespan = 15;

  while (main_loop_run) {
    image_pkt_t *img_pkt = NULL;
    detect_result_group_t *new_grp = NULL;
    int src_width, src_height, src_format;
    int disp_width, disp_height, disp_format;
    int src_buf_size, disp_buf_size;
    int ret = -1;
    im_rect crop_rect;
    rga_buffer_t src_img, disp_img;
    rga_buffer_handle_t src_handle = 0;
    rga_buffer_handle_t disp_handle = 0;

    memset(&src_img, 0, sizeof(src_img));
    memset(&disp_img, 0, sizeof(disp_img));

    img_pkt = (image_pkt_t *)ptr_queue_dequeue(&cam_ctx->disp_queue, 100);
    if (!img_pkt) {
      continue;
    }

    new_grp =
        (detect_result_group_t *)ptr_queue_dequeue(&cam_ctx->info_queue, 10);

    src_width = img_pkt->width;
    src_height = img_pkt->height;
    src_format = RK_FORMAT_YCbCr_420_SP;

    disp_width = output_width;
    disp_height = output_height;
    disp_format = RK_FORMAT_BGRA_8888;

    cv::Mat disp_mat(disp_height, disp_width, CV_8UC4,
                     cam_ctx->drm_disp.buf[v4l2_index].map);

    crop_rect.x = 0;
    crop_rect.y = 0;
    crop_rect.width = 1080;
    crop_rect.height = 1080;

    src_buf_size = src_width * src_height * get_bpp_from_format(src_format);
    disp_buf_size = disp_width * disp_height * get_bpp_from_format(disp_format);

    src_handle = importbuffer_fd(img_pkt->dma_fd, src_buf_size);
    if (src_handle == 0) {
      printf("import src buffer failed!\n");
      goto release_buffer;
    }

    disp_handle = importbuffer_fd(cam_ctx->drm_disp.buf[v4l2_index].dmabuf_fd,
                                  disp_buf_size);
    if (disp_handle == 0) {
      printf("import disp buffer failed!\n");
      goto release_buffer;
    }

    src_img = wrapbuffer_handle(src_handle, src_width, src_height, src_format);
    disp_img =
        wrapbuffer_handle(disp_handle, disp_width, disp_height, disp_format);

    ret = imcheck(src_img, disp_img, {}, {});
    if (IM_STATUS_NOERROR != ret) {
      printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
      goto release_buffer;
    }

    ret = improcess(src_img, disp_img, {}, crop_rect, {}, {}, IM_SYNC);
    if (ret != IM_STATUS_SUCCESS) {
      printf("%s running failed, %s\n", LOG_TAG, imStrError((IM_STATUS)ret));
      goto release_buffer;
    }

    if (new_grp) {
      if (det_grp) {
        delete det_grp;
      }
      det_grp = new_grp;
      det_lifespan = 15;
      new_grp = NULL;
    }

    if (det_grp) {
      det_lifespan--;
      if (--det_lifespan <= 0) {
        delete det_grp;
        det_grp = NULL;
      } else {
        char text[256];
        for (int i = 0; i < det_grp->count; i++) {
          detect_result_t *det_result = &(det_grp->results[i]);
          if (det_result->prop < 0.35) {
            continue;
          }
          sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
          printf("%s @ (%d %d %d %d) %f\n", det_result->name,
                 det_result->box.left, det_result->box.top,
                 det_result->box.right, det_result->box.bottom,
                 det_result->prop);
          cv::putText(disp_mat, text,
                      cv::Point(det_result->box.left, det_result->box.top),
                      cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 204, 0));
          cv::rectangle(
              disp_mat, cv::Point(det_result->box.left, det_result->box.top),
              cv::Point(det_result->box.right, det_result->box.bottom),
              cv::Scalar(0, 0, 255), 3, 1, 0); //绘制矩形
        }
      }
    }

    drmCommit(&cam_ctx->drm_disp.buf[v4l2_index], disp_width, disp_height, 0, 0,
              &cam_ctx->drm_disp.dev, cam_ctx->drm_disp.plane_type);

    v4l2_index++;
    if (v4l2_index == BUF_COUNT)
      v4l2_index = 0;

    image_pkt_unref(img_pkt);

  release_buffer:
    if (src_handle)
      releasebuffer_handle(src_handle);
    if (disp_handle)
      releasebuffer_handle(disp_handle);
  }

  return NULL;
}

void *thread_func_uart(void *arg) {
  smart_cam_t *cam_ctx = (smart_cam_t *)(arg);

  while (main_loop_run) {
    detect_result_group_t *new_grp = NULL;

    new_grp =
        (detect_result_group_t *)ptr_queue_dequeue(&cam_ctx->info_queue, 10);
    detect_result_to_serialport(&serial_port, new_grp);

    delete new_grp;
  }

  return NULL;
}

void rknn_runner_post(void *arg) {
  rknn_runner_t *runner = NULL;
  const float nms_threshold = NMS_THRESH;
  const float box_conf_threshold = BOX_THRESH;

  if (!arg) {
    return;
  }
  runner = (rknn_runner_t *)arg;

  int model_width = 0;
  int model_height = 0;
  if (runner->input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
    model_width = runner->input_attrs[0].dims[2];
    model_height = runner->input_attrs[0].dims[3];
  } else {
    model_width = runner->input_attrs[0].dims[1];
    model_height = runner->input_attrs[0].dims[2];
  }
  float scale_w = (float)model_width / output_width;
  float scale_h = (float)model_height / output_height;

  detect_result_group_t *detect_result_group = new detect_result_group_t;
  detect_result_group_t *detect_result_group_s = new detect_result_group_t;
  std::vector<float> out_scales;
  std::vector<int32_t> out_zps;
  for (uint32_t i = 0; i < runner->io_num.n_output; ++i) {
    out_scales.push_back(runner->output_attrs[i].scale);
    out_zps.push_back(runner->output_attrs[i].zp);
  }

  post_process((int8_t *)runner->output_mems[0]->virt_addr,
               (int8_t *)runner->output_mems[1]->virt_addr,
               (int8_t *)runner->output_mems[2]->virt_addr, 640, 640,
               box_conf_threshold, nms_threshold, scale_w, scale_h, out_zps,
               out_scales, detect_result_group);
  memcpy(detect_result_group_s, detect_result_group,
         sizeof(detect_result_group_t));
  ptr_queue_enqueue(&smart_cam.info_queue, detect_result_group, 10);
  ptr_queue_enqueue(&smart_cam.uart_queue, detect_result_group_s, 10);
}

int main(int argc, char **argv) {
  int ret = -1;

  if (0 != check_sololinker_device()) {
    LOG_ERROR("Envirement init failed!\n");
    LOG_ERROR("Please run on sololinker-a Board\n");
    return -1;
  }

  memset(&serial_port, 0, sizeof(serialport_t));

  if (serial_init(&serial_port, output_tty, B115200) != 0) {
    return 1;
  }

  serial_send(&serial_port, "yolocam init...\n", sizeof("yolocam init...\n"));

  ret = c_RkRgaInit();
  if (ret) {
    printf("c_RkRgaInit error : %s\n", strerror(errno));
    return ret;
  }

  smart_cam.runner = rknn_runner_create(RKNN_YOLO_MODEL, rknn_runner_post);
  signal(SIGINT, sig_proc);

  LOG_INFO("input_width is %d, input_height is %d\n", input_width,
           input_height);

  memset(&smart_cam.drm_disp, 0, sizeof(struct display));
  smart_cam.drm_disp.fmt = DRM_FORMAT_ARGB8888;
  smart_cam.drm_disp.width = output_width;
  smart_cam.drm_disp.height = output_height;
  smart_cam.drm_disp.plane_type = DRM_PLANE_TYPE_PRIMARY;
  smart_cam.drm_disp.buf_cnt = BUF_COUNT;
  ret = drm_display_init(&smart_cam.drm_disp);
  if (ret) {
    LOG_ERROR("drm display init failed!\n");
    return ret;
  }

  smart_cam.v4l2_device =
      v4l2_device_create("/dev/video11", "NV12", input_width, input_height, 4);
  v4l2_device_init(smart_cam.v4l2_device);
  if (smart_cam.v4l2_device == NULL) {
    LOG_ERROR("create v4l2 device failed!\n");
    return -1;
  }

  ptr_queue_init(&smart_cam.disp_queue, 4);
  ptr_queue_init(&smart_cam.rknn_queue, 2);
  ptr_queue_init(&smart_cam.info_queue, 5);
  ptr_queue_init(&smart_cam.uart_queue, 10);

  struct sched_param param;
  pthread_attr_init(&smart_cam.attr);
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_attr_setschedparam(&smart_cam.attr, &param);
  pthread_attr_setschedpolicy(&smart_cam.attr, SCHED_FIFO);

  pthread_create(&smart_cam.v4l2_thread, &smart_cam.attr, thread_func_v4l2,
                 &smart_cam);
  pthread_create(&smart_cam.rknn_thread, NULL, thread_func_rknn, &smart_cam);
  pthread_create(&smart_cam.disp_thread, NULL, thread_func_disp, &smart_cam);
  pthread_create(&smart_cam.uart_thread, NULL, thread_func_uart, &smart_cam);

  while (main_loop_run) {
    sleep(1);
  }

  pthread_join(smart_cam.v4l2_thread, NULL);
  pthread_join(smart_cam.rknn_thread, NULL);
  pthread_join(smart_cam.disp_thread, NULL);
  pthread_join(smart_cam.uart_thread, NULL);

  ptr_queue_cleanup(&smart_cam.disp_queue);
  ptr_queue_cleanup(&smart_cam.rknn_queue);
  ptr_queue_cleanup(&smart_cam.info_queue);
  ptr_queue_cleanup(&smart_cam.uart_queue);

  return 0;
}
