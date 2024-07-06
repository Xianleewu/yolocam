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

#ifndef __RKNN_RUNNER_H__
#define __RKNN_RUNNER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rknn_api.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
}
#endif

typedef void (*rknn_cb_func)(void *);

typedef struct __rknn_runner_s {
  rknn_context rknn_ctx;
  rknn_tensor_type input_type;
  rknn_tensor_format input_layout;
  rknn_sdk_version sdk_ver;
  rknn_input_output_num io_num;
  rknn_tensor_attr *input_attrs;
  rknn_tensor_attr *output_attrs;
  rknn_custom_string custom_string;
  rknn_tensor_mem **input_mems;
  rknn_tensor_mem **output_mems;
  rknn_cb_func post;
} rknn_runner_t;

rknn_runner_t *rknn_runner_create(char *model_path, rknn_cb_func func);

rknn_runner_t *rknn_runner_create(char *model_path,
                                  rknn_tensor_type tensor_type,
                                  rknn_tensor_format tensor_fmt,
                                  rknn_cb_func func);

int rknn_runner_destroy(rknn_runner_t *runner);
int rknn_runner_process(rknn_runner_t *runner, uint8_t *input_data);

#endif /*__RKNN_RUNNER_H__*/
