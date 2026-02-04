// Copyright 2020 GoldenRiver Technology Co., Ltd. All rights reserved.
#include "../../nbl_nsc/lib/nss_client.h"

#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>

#include "../../nbl_nsc/lib/inner.h"
#include "../server/nsc_interface.h"
#include "include/nsc_nss.h"

#define LOG_TAG "nbl_nsc.lib.nss_client"

int nbl_nsc_gettimeofday(struct timeval* tv, struct timezone* tz) {
  int rlt;
  int ree_rlt = 0;
  int ree_errno = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_GETTIMEOFDAY);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  // NOT send tv, get buffer from REE and copy buffer's data to "tv".
  // NOT send tz, because it's obsolete, and it should be NULL.

  rlt = nbl_chn_write_stm(g_nbl_nsc->chn, tx);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rlt);
    CHECK_FATAL_ERROR_AND_GOTO();
    goto failed;
  }

  rlt = nbl_chn_read_stm(g_nbl_nsc->chn, rx, NBL_TIMEOUT_INFINITY);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rlt);
    CHECK_FATAL_ERROR_AND_GOTO();
    goto failed;
  }

  rlt = _read_ree_rlt_errno(rx, &ree_rlt, &ree_errno);
  if (rlt != 0) {
    goto failed;
  }

  if (ree_rlt < 0) {  // ree_rlt is "file descriptor".
    nbl_nsc_set_errno(ree_errno);
  }

  if (rlt == 0 && ree_rlt == 0) {
    int out_size;
    struct timeval ree_tv;
    rlt = nbl_stm_next_buf(rx, &out_size, &ree_tv);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Read stream failed (%d).\n", rlt);
      CHECK_FATAL_ERROR_AND_GOTO();
      goto failed;
    }

    if (out_size != sizeof(struct timeval)) {
      NBL_LOGE("Read buffer size is %d, but expect is %lu.\n", out_size,
               sizeof(struct timeval));
      goto failed;
    }

    memcpy(tv, &ree_tv, sizeof(struct timeval));
  }

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  return ree_rlt;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

// FATAL ERROR: should connect again.
fatal_error:
  HANDLE_FATAL_ERROR();
}
