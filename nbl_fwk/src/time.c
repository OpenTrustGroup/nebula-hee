// Copyright 2020 GoldenRiver Technology Co., Ltd. All rights reserved.

#include <nbl_fwk/common.h>
#include <nbl_fwk/log.h>
#include <nbl_fwk/service.h>
#include <nbl_fwk/stream.h>
#include "../include/nbl_fwk/ntime.h"

#include "time_interface.h"

#define LOG_TAG "nbl_time.client"

static int _time_connect(nbl_chn_t** out_channel) {
  return nbl_chn_open(TIME_SRV_CHANNEL_NAME, NBL_TIMEOUT_INFINITY, out_channel);
}
static void _time_disconnect(nbl_chn_t* chan) {
  nbl_chn_close(chan);
}

// Connect time server, it must be before using channel and notice resource
// release.
#define CNN_TIME_SRV()            \
  nbl_chn_t* time_chn = NULL;     \
  rlt = _time_connect(&time_chn); \
  if (rlt != 0) {                 \
    return rlt;                   \
  }

#define DISCNN_TIME_SRV() _time_disconnect(time_chn)

int nbl_time_get_ree(nbl_time_t* time) {
  int rlt;
  int srv_rlt = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  // check parameters.
  if (time == NULL) {
    // "time" should be a valid pointer to buffer.
    return NBL_ERR_INVALID_ARGS;
  }

  CNN_TIME_SRV();

  // Transfer parameters and call it.
  tx = nbl_stm_create();
  rx = nbl_stm_create();

  nbl_stm_add_int(tx, NBL_TIME_CMD_GET_REE_TIME);

  rlt = nbl_chn_write_stm(time_chn, tx);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rlt);
    goto failed;
  }

  rlt = nbl_chn_read_stm(time_chn, rx, NBL_TIMEOUT_INFINITY);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rlt);
    goto failed;
  }

  // read result from time server.
  rlt = nbl_stm_next_int(rx, &srv_rlt);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read result (%d).\n", rlt);
    goto failed;
  }

  if (srv_rlt == NBL_NO_ERROR) {
    int out_size = 0;
    rlt = nbl_stm_next_buf(rx, &out_size, time);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed to read, result=%d.\n", rlt);
      goto failed;
    }

    if (out_size != sizeof(nbl_time_t)) {
      NBL_LOGE("Received buffer size is %d, but should be %lu\n.", out_size,
               sizeof(nbl_time_t));
      rlt = NBL_ERR_BAD_LEN;
      goto failed;
    }
  }

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  DISCNN_TIME_SRV();

  return rlt;
}

int nbl_time_get_system(nbl_time_t* time) {
  int rlt;
  int srv_rlt = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  // check parameters.
  if (time == NULL) {
    // "time" should be a valid pointer to buffer.
    return NBL_ERR_INVALID_ARGS;
  }

  CNN_TIME_SRV();

  // Transfer parameters and call it.
  tx = nbl_stm_create();
  rx = nbl_stm_create();

  nbl_stm_add_int(tx, NBL_TIME_CMD_GET_SYSTEM_TIME);

  rlt = nbl_chn_write_stm(time_chn, tx);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rlt);
    goto failed;
  }

  rlt = nbl_chn_read_stm(time_chn, rx, NBL_TIMEOUT_INFINITY);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rlt);
    goto failed;
  }

  // read result from time server.
  rlt = nbl_stm_next_int(rx, &srv_rlt);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read result (%d).\n", rlt);
    goto failed;
  }

  if (srv_rlt == NBL_NO_ERROR) {
    int out_size = 0;
    rlt = nbl_stm_next_buf(rx, &out_size, time);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed to read, result=%d.\n", rlt);
      goto failed;
    }

    if (out_size != sizeof(nbl_time_t)) {
      NBL_LOGE("Received buffer size is %d, but should be %lu\n.", out_size,
               sizeof(nbl_time_t));
      rlt = NBL_ERR_BAD_LEN;
      goto failed;
    }
  }

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  DISCNN_TIME_SRV();

  return rlt;
}

int nbl_time_set_ta_persistent(nbl_time_t* time) {
  int rlt;
  int srv_rlt = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  // check parameters.
  if (time == NULL) {
    // "time" should be a valid pointer to buffer.
    return NBL_ERR_INVALID_ARGS;
  }

  CNN_TIME_SRV();

  // Transfer parameters and call it.
  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_TIME_CMD_SET_TA_PERSISTENT_TIME);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Add int to stream failed (%d).\n", rlt);
    goto failed;
  }

  rlt = nbl_stm_add_buf(tx, sizeof(nbl_time_t), time);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Add buffer to stream failed (%d).\n", rlt);
    goto failed;
  }

  rlt = nbl_chn_write_stm(time_chn, tx);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rlt);
    goto failed;
  }

  rlt = nbl_chn_read_stm(time_chn, rx, NBL_TIMEOUT_INFINITY);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rlt);
    goto failed;
  }

  // read result from time server.
  rlt = nbl_stm_next_int(rx, &srv_rlt);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read result (%d).\n", rlt);
    goto failed;
  }

  rlt = srv_rlt;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  DISCNN_TIME_SRV();

  return rlt;
}

int nbl_time_get_ta_persistent(nbl_time_t* time) {
  int rlt;
  int srv_rlt = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  // check parameters.
  if (time == NULL) {
    // "time" should be a valid pointer to buffer.
    return NBL_ERR_INVALID_ARGS;
  }

  CNN_TIME_SRV();

  // Transfer parameters and call it.
  tx = nbl_stm_create();
  rx = nbl_stm_create();

  nbl_stm_add_int(tx, NBL_TIME_CMD_GET_TA_PERSISTENT_TIME);

  rlt = nbl_chn_write_stm(time_chn, tx);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rlt);
    goto failed;
  }

  rlt = nbl_chn_read_stm(time_chn, rx, NBL_TIMEOUT_INFINITY);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rlt);
    goto failed;
  }

  // read result from time server.
  rlt = nbl_stm_next_int(rx, &srv_rlt);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read result (%d).\n", rlt);
    goto failed;
  }

  if (srv_rlt != NBL_NO_ERROR) {
    rlt = srv_rlt;
    goto failed;
  }
  int out_size = 0;
  rlt = nbl_stm_next_buf(rx, &out_size, time);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read, result=%d.\n", rlt);
    goto failed;
  }

  if (out_size != sizeof(nbl_time_t)) {
    NBL_LOGE("Received buffer size is %d, but should be %lu\n.", out_size,
             sizeof(nbl_time_t));
    rlt = NBL_ERR_BAD_LEN;
    goto failed;
  }

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  DISCNN_TIME_SRV();

  return rlt;
}
