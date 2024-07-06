#include "rknn_runner.h"
#include "postprocess.h"
#include "rknn_api.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <vector>

// 获取当前时间（微秒级）
static long long get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void rknn_runner_dump_tensor_attr(rknn_tensor_attr *attrs, int num) {
  printf("|%-6s|%-10s|%-6s|%-15s|%-8s|%-8s|%-4s|%-8s|%-8s|%-4s|%-4s|\n",
         "Index", "Name", "Dims", "Dimensions", "NElems", "Size", "Fmt", "Type",
         "QntType", "ZP", "Scale");
  printf("+--------------------------------------------------------------------"
         "---------------------------+\n");

  for (int attr_i = 0; attr_i < num; attr_i++) {
    rknn_tensor_attr *attr = &(attrs[attr_i]);
    char dims[128] = {0};
    for (uint32_t i = 0; i < attr->n_dims; ++i) {
      int idx = strlen(dims);
      sprintf(&dims[idx], "%d%s", attr->dims[i],
              (i == attr->n_dims - 1) ? "" : ", ");
    }

    printf("|%-6d|%-10s|%-6d|%-15s|%-8d|%-8d|%-4s|%-8s|%-8s|%-4d|%-4f|\n",
           attr->index, attr->name, attr->n_dims, dims, attr->n_elems,
           attr->size, get_format_string(attr->fmt),
           get_type_string(attr->type), get_qnt_type_string(attr->qnt_type),
           attr->zp, attr->scale);
    printf(
        "+--------------------------------------------------------------------"
        "---------------------------+\n");
  }
}

static void rknn_runner_internal_release(rknn_runner_t *runner) {
  if (!runner) {
    return;
  }

  if (!runner->rknn_ctx) {
    free(runner);
    return;
  }

  if (runner->input_mems) {
    for (uint32_t i = 0; i < runner->io_num.n_input; ++i) {
      rknn_destroy_mem(runner->rknn_ctx, runner->input_mems[i]);
    }

    free(runner->input_mems);
  }

  if (runner->output_mems) {
    for (uint32_t i = 0; i < runner->io_num.n_output; ++i) {
      rknn_destroy_mem(runner->rknn_ctx, runner->output_mems[i]);
    }

    free(runner->output_mems);
  }

  if (runner->input_attrs) {
    free(runner->input_attrs);
  }

  if (runner->output_attrs) {
    free(runner->output_attrs);
  }

  rknn_destroy(runner->rknn_ctx);

  free(runner);
}

rknn_runner_t *rknn_runner_create(char *model_path, rknn_cb_func func) {
  return rknn_runner_create(model_path, RKNN_TENSOR_UINT8, RKNN_TENSOR_NHWC,
                            func);
}

rknn_runner_t *rknn_runner_create(char *model_path,
                                  rknn_tensor_type tensor_type,
                                  rknn_tensor_format tensor_fmt,
                                  rknn_cb_func func) {
  int ret = -1;
  rknn_runner_t *runner = NULL;
  rknn_tensor_type input_type = tensor_type;
  rknn_tensor_format input_layout = tensor_fmt;

  if (model_path == NULL) {
    printf("input model path is invalid\n");
    return NULL;
  }

  runner = (rknn_runner_t *)malloc(sizeof(rknn_runner_t));
  if (runner == NULL) {
    printf("allocate rknn runner oom\n");
    return NULL;
  }

  runner->post = func;

  ret = rknn_init(&runner->rknn_ctx, model_path, 0, 0, NULL);
  if (ret < 0) {
    printf("rknn_init fail! ret=%d\n", ret);
    free(runner);
    return NULL;
  }

  // Get sdk and driver version
  rknn_sdk_version sdk_ver;
  ret = rknn_query(runner->rknn_ctx, RKNN_QUERY_SDK_VERSION, &sdk_ver,
                   sizeof(sdk_ver));
  if (ret != RKNN_SUCC) {
    printf("rknn_query fail! ret=%d\n", ret);
    return NULL;
  }

  printf("rknn_api/rknnrt version: %s, driver version: %s\n",
         sdk_ver.api_version, sdk_ver.drv_version);

  // Get Model Input Output Info
  ret = rknn_query(runner->rknn_ctx, RKNN_QUERY_IN_OUT_NUM, &runner->io_num,
                   sizeof(runner->io_num));
  if (ret != RKNN_SUCC) {
    printf("rknn_query fail! ret=%d\n", ret);
    return NULL;
  }
  printf("model input num: %d, output num: %d\n", runner->io_num.n_input,
         runner->io_num.n_output);

  printf("input tensors:\n");
  runner->input_attrs = (rknn_tensor_attr *)malloc(runner->io_num.n_input *
                                                   sizeof(rknn_tensor_attr));
  if (runner->input_attrs == NULL) {
    printf("allocat input_attrs falied\n");
    goto release;
  }
  memset(runner->input_attrs, 0,
         runner->io_num.n_input * sizeof(rknn_tensor_attr));
  for (uint32_t i = 0; i < runner->io_num.n_input; i++) {
    runner->input_attrs[i].index = i;
    // query info
    ret = rknn_query(runner->rknn_ctx, RKNN_QUERY_INPUT_ATTR,
                     &(runner->input_attrs[i]), sizeof(rknn_tensor_attr));
    if (ret < 0) {
      printf("rknn_init error! ret=%d\n", ret);
      return NULL;
    }
  }
  rknn_runner_dump_tensor_attr(runner->input_attrs, runner->io_num.n_input);

  printf("output tensors:\n");
  runner->output_attrs = (rknn_tensor_attr *)malloc(runner->io_num.n_output *
                                                    sizeof(rknn_tensor_attr));
  if (runner->output_attrs == NULL) {
    printf("allocat output_attrs falied\n");
    goto release;
  }

  memset(runner->output_attrs, 0,
         runner->io_num.n_output * sizeof(rknn_tensor_attr));
  for (uint32_t i = 0; i < runner->io_num.n_output; i++) {
    runner->output_attrs[i].index = i;
    // query info
    ret = rknn_query(runner->rknn_ctx, RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR,
                     &(runner->output_attrs[i]), sizeof(rknn_tensor_attr));
    if (ret != RKNN_SUCC) {
      printf("rknn_query fail! ret=%d\n", ret);
      return NULL;
    }
  }
  rknn_runner_dump_tensor_attr(runner->output_attrs, runner->io_num.n_output);

  // Get custom string
  ret = rknn_query(runner->rknn_ctx, RKNN_QUERY_CUSTOM_STRING,
                   &runner->custom_string, sizeof(rknn_custom_string));
  if (ret != RKNN_SUCC) {
    printf("rknn_query fail! ret=%d\n", ret);
    return NULL;
  }
  printf("custom string: %s\n", runner->custom_string.string);

  // default input type is int8 (normalize and quantize need compute in
  // outside) if set uint8, will fuse normalize and quantize to npu
  runner->input_attrs[0].type = input_type;
  // default fmt is NHWC, npu only support NHWC in zero copy mode
  runner->input_attrs[0].fmt = input_layout;

  // Create input tensor memory
  runner->input_mems = (rknn_tensor_mem **)malloc(runner->io_num.n_input *
                                                  sizeof(rknn_tensor_mem *));
  if (runner->input_mems == NULL) {
    printf("allocat input_mems falied\n");
    goto release;
  }

  runner->input_mems[0] = rknn_create_mem(
      runner->rknn_ctx, runner->input_attrs[0].size_with_stride);

  // Create output tensor memory
  runner->output_mems = (rknn_tensor_mem **)malloc(runner->io_num.n_output *
                                                   sizeof(rknn_tensor_mem *));
  if (runner->output_mems == NULL) {
    printf("allocat output_mems falied\n");
    goto release;
  }

  for (uint32_t i = 0; i < runner->io_num.n_output; ++i) {
    runner->output_mems[i] = rknn_create_mem(
        runner->rknn_ctx, runner->output_attrs[i].size_with_stride);
  }
  // Set input tensor memory
  ret = rknn_set_io_mem(runner->rknn_ctx, runner->input_mems[0],
                        &runner->input_attrs[0]);
  if (ret < 0) {
    printf("rknn_set_io_mem fail! ret=%d\n", ret);
    goto release;
  }
  // Set output tensor memory
  for (uint32_t i = 0; i < runner->io_num.n_output; ++i) {
    // set output memory and attribute
    ret = rknn_set_io_mem(runner->rknn_ctx, runner->output_mems[i],
                          &runner->output_attrs[i]);
    if (ret < 0) {
      printf("rknn_set_io_mem fail! ret=%d\n", ret);
      goto release;
    }
  }

  return runner;

release:
  rknn_runner_internal_release(runner);
  return NULL;
}

int rknn_runner_process(rknn_runner_t *runner, uint8_t *input_data) {
  int ret = -1;
  if (!runner) {
    printf("invalid rknn runner\n");
    return -1;
  }

  // Copy input data to input tensor memory
  int width = runner->input_attrs[0].dims[2];
  int stride = runner->input_attrs[0].w_stride;
  if (input_data) {
    if (width == stride) {
      memcpy(runner->input_mems[0]->virt_addr, input_data,
             width * runner->input_attrs[0].dims[1] *
                 runner->input_attrs[0].dims[3]);
    } else {
      int height = runner->input_attrs[0].dims[1];
      int channel = runner->input_attrs[0].dims[3];
      // copy from src to dst with stride
      uint8_t *src_ptr = input_data;
      uint8_t *dst_ptr = (uint8_t *)runner->input_mems[0]->virt_addr;
      // width-channel elements
      int src_wc_elems = width * channel;
      int dst_wc_elems = stride * channel;
      for (int h = 0; h < height; ++h) {
        memcpy(dst_ptr, src_ptr, src_wc_elems);
        src_ptr += src_wc_elems;
        dst_ptr += dst_wc_elems;
      }
    }
  }

  ret = rknn_run(runner->rknn_ctx, NULL);
  if (ret < 0) {
    printf("rknn run error %d\n", ret);
    return -1;
  }

  if (runner->post) {
    runner->post(runner);
  }

  return 0;
}

int rknn_runner_destroy(rknn_runner_t *runner) {
  rknn_runner_internal_release(runner);
  return 0;
}

