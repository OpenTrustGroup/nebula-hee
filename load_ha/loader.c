// Copyright 2018 GoldenRiver Technology Co., Ltd.
// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <load_ha/loader.h>

#include <errno.h>
#include <trusty_ipc.h>
#include <trusty_std.h>
#include <uapi/err.h>

#include "nbl_loader_srv/common/command.h"

int loader_start_app_by_uuid(const nbl_uuid_t *uuid, const char *arg,
                             bool wait_for_end) {
  typedef struct Data {
    uint32_t cmd;
    int rc;
  } Data;

  const char *port_name = "com.grt.loader.srv.main";

  int rc;

  Data resp_data;
  grt_loader_cmd_t *command;
  grt_load_uuid_info_t *info;
  uint32_t buf_size = sizeof(grt_loader_cmd_t) + sizeof(grt_load_uuid_info_t);
  char req_data[buf_size];

  // open channel
  nbl_chn_t *channel = NULL;
  rc = nbl_chn_open(port_name, NBL_TIMEOUT_INFINITY, &channel);
  if (rc) {
    fprintf(stderr, "Failed to connect to %s, rc=%d\n", port_name, rc);
    goto error;
  }

  command = (grt_loader_cmd_t *)req_data;
  command->cmd = GRT_LOADER_LOAD_SECMGR_TA;
  command->param_size = sizeof(grt_load_uuid_info_t);

  info = (grt_load_uuid_info_t *)(command + 1);
  memcpy(&info->uuid, uuid, sizeof(uuid_t));
  info->use_ion_as_heap = 0;
  info->heap_base = 0;
  info->shm_handle = 0;
  info->heap_size = 0;
  memset(info->arg, 0, sizeof(info->arg));
  if (arg) strcpy(info->arg, arg);

  // invoke the application in secmgr.
  rc = nbl_chn_write_buf(channel, req_data, buf_size);
  if (rc < 0) {
    printf("Failed to send command to loader (%d)\n", rc);
    goto error;
  }

  // read the result for loading program, no running.
  rc = nbl_chn_read_buf(channel, &resp_data, sizeof(resp_data), -1);
  if (rc < 0) {
    printf("Failed to read response from loader (%d)\n", rc);
    goto error;
  }
  if (resp_data.cmd != GRT_LOADER_SEND_RESPONSE) {
    printf("The command is %d, not GRT_LOADER_SEND_RESPONSE.\n", resp_data.cmd);
    rc = ENOMSG;
    goto error;
  }
  rc = resp_data.rc;
  if (rc < 0) {
    printf("After loading, return error (%d)\n", rc);
    goto error;
  }

  // if need, read the result until program finishes.
  if (wait_for_end) {
    rc = nbl_chn_read_buf(channel, &resp_data, sizeof(resp_data),
                          NBL_TIMEOUT_INFINITY);
    if (rc < 0) {
      printf("Failed to read response from loader (%d:%d)\n", rc, errno);
      goto error;
    }
    if (resp_data.cmd != GRT_LOADER_TA_RUN_END) {
      printf("The command is %d, not GRT_LOADER_TA_RUN_END.\n", resp_data.cmd);
      rc = ENOMSG;
      goto error;
    }
    rc = resp_data.rc;
    if (rc < 0) {
      printf("After run, return error (%d)\n", rc);
      goto error;
    }
  }

error:
  if (channel) nbl_chn_close(channel);

  return rc;
}