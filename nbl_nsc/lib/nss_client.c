// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#include "../../nbl_nsc/lib/nss_client.h"

#include <errno.h>
#include <stdarg.h>

#include "../../nbl_nsc/lib/inner.h"
#include "../server/nsc_interface.h"
#include "include/nsc_nss.h"

#define LOG_TAG "nbl_nsc.lib.nss_client"

int _read_ree_rlt_errno(nbl_stm_t* rx, int* ree_rlt, int* ree_errno) {
  int rlt;
  rlt = nbl_stm_next_int(rx, ree_rlt);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read result (%d).\n", rlt);
    return rlt;
  }

  rlt = nbl_stm_next_int(rx, ree_errno);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read errno (%d).\n", rlt);
    return rlt;
  }

  return NBL_NO_ERROR;
}

/**
 * Initialize method of transferring big data from proxy to client.
 * At first use shared memory, if not, use buffer in message.
 * 1. Send message to client : allocate shared memory or buffer.
 * 2. If OK, here map shared memory or use buffer.
 * \return libc error.
 */
int _init_transfer_mem() {
  int rlt;
  int ree_rlt = 0;
  int ree_errno = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_INIT_TRANSFER_METHOD);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, g_nbl_nsc->config.mem_type);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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
  } else {
    int rsp;
    nbl_stm_next_int(rx, &rsp);
    if (rsp == NBL_NSC_RSP_OPEN_SHM) {
      rlt = nbl_stm_next_shm(rx, &g_nbl_nsc->shm);
      if (rlt != NBL_NO_ERROR) {
        NBL_LOGE("Get shared memory failed (%d).\n", rlt);
        goto failed;
      }

      rlt = nbl_shm_map(g_nbl_nsc->shm, NBL_SHM_PERM_RW, &g_nbl_nsc->shm_base);
      if (rlt != NBL_NO_ERROR) {
        NBL_LOGE("Map shared memory failed (%d).\n", rlt);
        goto failed;
      }

      rlt = nbl_shm_get_data_size(g_nbl_nsc->shm, &g_nbl_nsc->shm_size);
      if (rlt != NBL_NO_ERROR) {
        NBL_LOGE("Cannot get size of data (%d).\n", rlt);
        goto map_shm_failed;
      }
    } else {
      g_nbl_nsc->shm = NULL;
      g_nbl_nsc->shm_base = NULL;
      g_nbl_nsc->shm_size = 0;
    }
  }

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  return ree_rlt;

// NOT FATAL ERROR: can redo.
map_shm_failed:
  // TODO: If here free shm in client, the shm in REE is NOT freed !
  nbl_shm_free(g_nbl_nsc->shm);
  g_nbl_nsc->shm = NULL;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

// FATAL ERROR: should connect again.
fatal_error:
  HANDLE_FATAL_ERROR();
}

/**
 * Close method of transferring big data from proxy to client.
 * Method is "shared memory", so transfer data by buffer if close shared memory.
 * \return libc error.
 */
int _close_transfer_mem() {
  int rlt;
  int ree_rlt = 0;
  int ree_errno = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  if (!g_nbl_nsc->shm) {
    return 0;
  }

  CHECK_AND_START_NSC();

  nbl_shm_free(g_nbl_nsc->shm);
  g_nbl_nsc->shm = NULL;
  g_nbl_nsc->shm_base = NULL;
  g_nbl_nsc->shm_size = 0;

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_CLOSE_TRANSFER_METHOD);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, g_nbl_nsc->config.mem_type);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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

int nbl_nsc_open(const char* path, int flags, ...) {
  int rlt;
  int ree_rlt = 0;
  int ree_errno = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;
  mode_t mode = 0;

  CHECK_AND_START_NSC();

  // Get mode.
  va_list ap;
  va_start(ap, flags);

  if (flags & O_CREAT || flags & O_TMPFILE) {
    mode = va_arg(ap, uint32_t) & 0777;
  }
  va_end(ap);

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_OPEN);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, flags);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, mode);  // mode_t = unsigned int
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_buf(tx, strlen(path) + 1, path);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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

int nbl_nsc_close(int fd) {
  int rlt;
  int ree_rlt = 0;
  int ree_errno = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_CLOSE);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, fd);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ree_errno);
  return ree_rlt;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

fatal_error:
  HANDLE_FATAL_ERROR();
}

static int _nsc_read(int fd, void* buf, size_t size) {
  int rlt;
  nbl_stm_t* tx;
  nbl_stm_t* rx;
  int ree_rlt = 0;
  int ree_errno = 0;

  // Max size of read once.
  size_t once_size;
  size_t remain_size = size;
  size_t total_read_size = 0;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  once_size = g_nbl_nsc->shm ? g_nbl_nsc->shm_size : NBL_NSS_MIN_PACKET_SIZE;

  // split read to some times by data buffer and size.
  do {
    // calculate read data on this time.
    size_t read_size = 0;
    if (remain_size > once_size) {
      read_size = once_size;
    } else {
      read_size = remain_size;
    }

    // read once.
    rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_READ);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
      goto failed;
    }
    rlt = nbl_stm_add_int(tx, fd);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
      goto failed;
    }
    rlt = nbl_stm_add_buf(tx, sizeof(read_size), &read_size);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
      goto failed;
    }

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

    // get result and read buffer.
    rlt = _read_ree_rlt_errno(rx, &ree_rlt, &ree_errno);
    if (rlt != NBL_NO_ERROR) {
      goto failed;
    }

    // "ree_rlt" is size of REAL read on this time.
    if (ree_rlt == 0) {
      // Reach file end.
      break;
    } else if (ree_rlt < 0) {
      // meet error.
      rlt = -1;
      break;
    }

    // The last is ok, so read data to input buf.
    if (g_nbl_nsc->shm) {
      memcpy(buf + total_read_size, g_nbl_nsc->shm_base, ree_rlt);
    } else {
      int out_size;
      rlt = nbl_stm_next_buf(rx, &out_size, buf + total_read_size);
      if (rlt != NBL_NO_ERROR) {
        NBL_LOGE("Cannot get buffer, nbl_err=%d.\n", rlt);
        rlt = -1;
        break;
      }

      // buffer's size is NOT same with real read, meat a error.
      if (ree_rlt != out_size) {
        NBL_LOGE("Read file size mismatch (return size(%d) != buf size(%d).\n",
                 ree_rlt, out_size);
        rlt = NBL_ERR_CONFLICT_ARGS;
        break;
      }
    }

    total_read_size += ree_rlt;
    remain_size -= ree_rlt;

    // On size read is less than expect, that means: reach end of file.
    if (ree_rlt < (int)once_size) {
      break;
    }

  } while (remain_size > 0);

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  if (rlt >= 0) {
    return total_read_size;
  } else {
    nbl_nsc_set_errno(ree_errno);
    return -1;
  }

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

fatal_error:
  HANDLE_FATAL_ERROR();
}

int nbl_nsc_read(int fd, void* buf, size_t size) {
  int rc;
  int close_rc;

  rc = _init_transfer_mem();
  if (rc != 0) {
    return rc;
  }
  NBL_LOGD("Before nsc read, init shm=%s.\n", g_nbl_nsc->shm ? "Y" : "N");

  rc = _nsc_read(fd, buf, size);

  // Whether or not write successes, should close shared meory.
  close_rc = _close_transfer_mem();
  if (close_rc != 0 && rc == 0) {
    rc = close_rc;
  }
  NBL_LOGD("After nsc read, close shm.\n");

  // At first, return first error in write or close.
  return rc;
}

static int _nsc_write(int fd, const void* buf, size_t size) {
  int rlt;
  nbl_stm_t* tx;
  nbl_stm_t* rx;
  int ree_rlt = 0;
  int ree_errno = 0;
  size_t once_size;
  size_t remain_size = size;
  size_t total_write_size = 0;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  once_size = g_nbl_nsc->shm ? g_nbl_nsc->shm_size : NBL_NSS_MIN_PACKET_SIZE;
  do {
    // calculate write data on this time.
    size_t write_size = 0;
    if (remain_size > once_size) {
      write_size = once_size;
    } else {
      write_size = remain_size;
    }

    // Send message.
    rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_WRITE);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
      goto failed;
    }
    rlt = nbl_stm_add_int(tx, fd);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
      goto failed;
    }
    rlt = nbl_stm_add_buf(tx, sizeof(write_size), &write_size);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
      goto failed;
    }

    if (g_nbl_nsc->shm) {
      memcpy(g_nbl_nsc->shm_base, buf + total_write_size, write_size);
    } else {
      nbl_stm_add_buf(tx, write_size, buf + total_write_size);
    }

    rlt = nbl_chn_write_stm(g_nbl_nsc->chn, tx);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Write stream failed (%d).\n", rlt);
      CHECK_FATAL_ERROR_AND_GOTO();
      goto failed;
    }

    // Receive message.
    rlt = nbl_chn_read_stm(g_nbl_nsc->chn, rx, NBL_TIMEOUT_INFINITY);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Read stream failed (%d).\n", rlt);
      CHECK_FATAL_ERROR_AND_GOTO();
      goto failed;
    }

    rlt = _read_ree_rlt_errno(rx, &ree_rlt, &ree_errno);
    if (rlt != NBL_NO_ERROR) {
      NBL_LOGE("Write file failed (%d).\n", rlt);
      goto failed;
    }

    if (ree_rlt < 0) {
      rlt = -1;
      break;
    }

    total_write_size += write_size;
    remain_size -= write_size;

  } while (remain_size > 0);

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  if (rlt >= 0) {
    return total_write_size;
  } else {
    nbl_nsc_set_errno(ree_errno);
    return -1;
  }

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

fatal_error:
  HANDLE_FATAL_ERROR();
}

int nbl_nsc_write(int fd, const void* buf, size_t size) {
  int rc;
  int close_rc;

  rc = _init_transfer_mem();
  if (rc != 0) {
    return rc;
  }
  NBL_LOGD("Before nsc write, init shm=%s.\n", g_nbl_nsc->shm ? "Y" : "N");

  rc = _nsc_write(fd, buf, size);

  // Whether or not write successes, should close shared meory.
  close_rc = _close_transfer_mem();
  if (close_rc != 0 && rc == 0) {
    rc = close_rc;
  }
  NBL_LOGD("After nsc write, close shm.\n");

  // At first, return first error in write or close.
  return rc;
}

int nbl_nsc_fsize(int fd) {
  int rlt;
  int ree_rlt;
  int ree_errno;
  nbl_stm_t* tx;
  nbl_stm_t* rx;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_FSIZE);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, fd);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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
  if (rlt != NBL_NO_ERROR) {
    goto failed;
  }

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ree_errno);
  return ree_rlt;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

fatal_error:
  HANDLE_FATAL_ERROR();
}

// In libc, unlink (file) != remove(file and dir).
// But in zirocn, unlink = remove, all can remove file and dir.
int nbl_nsc_unlink(const char* path) {
  return nbl_nsc_remove(path);
}

int nbl_nsc_remove(const char* path) {
  int rlt;
  int ree_rlt;
  int ree_errno;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_REMOVE);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_buf(tx, strlen(path) + 1, path);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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
  if (rlt != NBL_NO_ERROR) {
    goto failed;
  }

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ree_errno);
  return ree_rlt;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

fatal_error:
  HANDLE_FATAL_ERROR();
}

off_t nbl_nsc_lseek(int fd, off_t offset, int whence) {
  int rlt;
  off_t ree_rlt = 0;
  int ree_errno = 0;
  nbl_stm_t* tx = NULL;
  nbl_stm_t* rx = NULL;

  CHECK_AND_START_NSC();

  tx = nbl_stm_create();
  rx = nbl_stm_create();

  rlt = nbl_stm_add_int(tx, NBL_NSC_CMD_LSEEK);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, fd);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_buf(tx, sizeof(offset), &offset);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }
  rlt = nbl_stm_add_int(tx, whence);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed adding value in stream (%d).\n", rlt);
    goto failed;
  }

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

  // result of "lseek" is "long" type, so use buffer to store and restore.
  int size_ree_rlt;
  rlt = nbl_stm_next_buf(rx, &size_ree_rlt, &ree_rlt);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read argument from stream, rlt=%d.\n", rlt);
    goto failed;
  }

  rlt = nbl_stm_next_int(rx, &ree_errno);
  if (rlt != NBL_NO_ERROR) {
    NBL_LOGE("Failed to read argument from stream, rlt=%d.\n", rlt);
    goto failed;
  }

  // Only on ree's rlt is NOT ok, the ree's errno has meaning.
  // If this invoke succeed, NO need to set local errno actually.
  nbl_nsc_set_errno(ree_errno);
  return ree_rlt;

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  nbl_nsc_set_errno(ERROR_FAIL);
  return -1;

fatal_error:
  HANDLE_FATAL_ERROR();
}
