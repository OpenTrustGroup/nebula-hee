// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
/**
 * Connect to NSC srv in HEE, and wait for NSC proxy connecting in REE.
 * So implement "obtain/recycle" functions.
 */

#include <errno.h>
#include <pthread.h>

#include "../../nbl_nsc/lib/fwk_ext.h"
#include "../../nbl_nsc/lib/inner.h"
#include "../server/nsc_interface.h"
#include "include/nsc_nss.h"

#define LOG_TAG "nbl_nsc.lib.nsc_srv"

static int nbl_nss_proxy_connect(nbl_srv_t* srv, nbl_uuid_t peer_uuid);
static void nbl_nss_proxy_open(nbl_chn_t* chn);
static void nbl_nss_proxy_close(nbl_chn_t* chn);

// Cannot transfer the nss from nbl_nss_proxy_open to
// start_wait_for_connect, so set it to thread-local data.
__thread nbl_nsc_t* g_nbl_nsc = NULL;

// Global errno about REE errno.
static __thread int g_nbl_nsc_errno = 0;

int nbl_nsc_nbl_errno_2_libc(int nbl_err) {
#define DEF_ERR ECOMM  // same with ERROR_FAIL

  if (nbl_err > 0) {
    return nbl_err;
  }

  switch (nbl_err) {
    case NBL_NO_ERROR:
      return 0;
    case NBL_ERR_GENERIC:
      return DEF_ERR;
    case NBL_ERR_NOT_FOUND:
      return DEF_ERR;
    case NBL_ERR_NOT_READY:
      return DEF_ERR;
    case NBL_ERR_NO_MSG:
      return DEF_ERR;
    case NBL_ERR_NO_MEMORY:
      return ENOMEM;
    case NBL_ERR_ALREADY_STARTED:
      return DEF_ERR;
    case NBL_ERR_NOT_VALID:
      return EINVAL;
    case NBL_ERR_INVALID_ARGS:
      return EINVAL;
    case NBL_ERR_NOT_ENOUGH_BUFFER:
      return ENOBUFS;
    case NBL_ERR_NOT_SUSPENDED:
      return DEF_ERR;
    case NBL_ERR_OBJECT_DESTROYED:
      return DEF_ERR;
    case NBL_ERR_NOT_BLOCKED:
      return ENOTBLK;
    case NBL_ERR_TIMED_OUT:
      return ETIMEDOUT;
    case NBL_ERR_ALREADY_EXISTS:
      return DEF_ERR;
    case NBL_ERR_CHANNEL_CLOSED:
      return DEF_ERR;
    case NBL_ERR_OFFLINE:
      return DEF_ERR;
    case NBL_ERR_NOT_ALLOWED:
      return EPERM;
    case NBL_ERR_BAD_PATH:
      return ENOENT;
    case NBL_ERR_ALREADY_MOUNTED:
      return DEF_ERR;
    case NBL_ERR_IO:
      return EIO;
    case NBL_ERR_NOT_DIR:
      return ENOTDIR;
    case NBL_ERR_NOT_FILE:
      return DEF_ERR;
    case NBL_ERR_RECURSE_TOO_DEEP:
      return DEF_ERR;
    case NBL_ERR_NOT_SUPPORTED:
      return EOPNOTSUPP;
    case NBL_ERR_TOO_BIG:
      return EFBIG;
    case NBL_ERR_CANCELLED:
      return ECANCELED;
    case NBL_ERR_NOT_IMPLEMENTED:
      return ENOSYS;
    case NBL_ERR_CHECKSUM_FAIL:
      return EPROTO;
    case NBL_ERR_CRC_FAIL:
      return EPROTO;
    case NBL_ERR_CMD_UNKNOWN:
      return EBADRQC;
    case NBL_ERR_BAD_STATE:
      return DEF_ERR;
    case NBL_ERR_BAD_LEN:
      return DEF_ERR;
    case NBL_ERR_BUSY:
      return EBUSY;
    case NBL_ERR_THREAD_DETACHED:
      return DEF_ERR;
    case NBL_ERR_I2C_NACK:
      return DEF_ERR;
    case NBL_ERR_ALREADY_EXPIRED:
      return DEF_ERR;
    case NBL_ERR_OUT_OF_RANGE:
      return DEF_ERR;
    case NBL_ERR_NOT_CONFIGURED:
      return DEF_ERR;
    case NBL_ERR_NOT_MOUNTED:
      return DEF_ERR;
    case NBL_ERR_FAULT:
      return DEF_ERR;
    case NBL_ERR_NO_RESOURCES:
      return DEF_ERR;
    case NBL_ERR_BAD_HANDLE:
      return DEF_ERR;
    case NBL_ERR_ACCESS_DENIED:
      return EACCES;
    case NBL_ERR_PARTIAL_WRITE:
      return DEF_ERR;
    case NBL_ERR_CREATE_HANDLE:
      return DEF_ERR;
    case NBL_ERR_CREATE_CHILD_HANDLE:
      return DEF_ERR;
    case NBL_ERR_MMAP:
      return DEF_ERR;
    case NBL_ERR_OUT_OF_MEM:
      return ENOMEM;
    case NBL_ERR_CONFLICT_ARGS:
      return DEF_ERR;
    case NBL_ERR_TA_RUN_FAILED:
      return DEF_ERR;
    default:
      return DEF_ERR;
  }
}

void nbl_nsc_set_errno(int err) {
  g_nbl_nsc_errno = err;
}

int nbl_nsc_get_errno() {
  return nbl_nsc_nbl_errno_2_libc(g_nbl_nsc_errno);
}

nbl_chn_t* nbl_nsc_get_channel() {
  if (!g_nbl_nsc) {
    return NULL;
  }
  return g_nbl_nsc->chn;
}

static int nbl_nss_proxy_connect(nbl_srv_t* srv, nbl_uuid_t peer_uuid) {
  NBL_LOGD("NSC: Connect from REE proxy to HEE Client.\n");
  return NBL_NO_ERROR;
}

// If a channel is connected, the app main loop is stopped and keep the channel.
static void nbl_nss_proxy_open(nbl_chn_t* chn) {
  NBL_LOGD("NSC: Open channel between HEE client and REE proxy.\n");
  g_nbl_nsc->chn = chn;
  nbl_chn_set_priv(chn, g_nbl_nsc);

  // stop main loop, and then exit from main loop.
  // - So client can send message by this channel.
  nbl_app_stop(g_nbl_nsc->s_app);
}

/**
 * On channel from proxy to client is closed.
 * If HA is without other wait_any in main loop, this function will not be
 * invoked even channel is closed. But HA is in other wait_any (such as secmgr),
 * this function will be invoked on channel is closed. So this function cannot
 * be used for releasing resources and set g_ree to NULL. The channel from proxy
 * to client should be checked by real write/read.
 */
static void nbl_nss_proxy_close(nbl_chn_t* chn) {
  NBL_LOGD("NSC: Close channel between HEE client and REE proxy.\n");
}

/**
 * Get client name from HEE NSS server.
 * Callee should free the name.
 */
static int _nss_get_client_name(nbl_chn_t** chn, char** name) {
  int rc;

  // 1. Connect to HEE NSS srv.
  const char* nss_srv_port_name = "com.grt.nebula.nsc.srv";
  *name = NULL;

  rc = nbl_chn_open(nss_srv_port_name, NBL_TIMEOUT_INFINITY, chn);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Connect to %s failed (%d).\n", nss_srv_port_name, rc);
    goto conn_failed;
  }

  // 2. Send request to srv.
  nbl_stm_t* tx = nbl_stm_create();
  nbl_stm_t* rx = nbl_stm_create();

  nbl_stm_add_int(tx, NBL_NSC_CMD_START_CLIENT);

  rc = nbl_chn_write_stm(*chn, tx);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rc);
    goto failed;
  }

  // 3. Get client name.
  rc = nbl_chn_read_stm(*chn, rx, NBL_TIMEOUT_INFINITY);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rc);
    goto failed;
  }

  int name_size;
  void* name_ptr;

  // 3.1 Get name.
  rc = nbl_stm_next_buf_ptr(rx, &name_size, &name_ptr);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Cannot read client name 1. err=%d\n", rc);
    goto failed;
  }
  *name = strdup(name_ptr);

  // 4. Clear env.
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  return NBL_NO_ERROR;

failed:
  if (*name) {
    free(*name);
  }

  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  if (*chn) {
    nbl_chn_close(*chn);
  }

conn_failed:
  return rc;
}

// send "finish start" message to server, and server will make somebody to
// connect this port.
static int _send_finish_start_port(nbl_chn_t** chn) {
  int rc;

  nbl_stm_t* tx = nbl_stm_create();
  nbl_stm_t* rx = nbl_stm_create();

  nbl_stm_add_int(tx, NBL_NSC_CMD_FINISH_START_CLIENT);

  rc = nbl_chn_write_stm(*chn, tx);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rc);
  }

  rc = nbl_chn_read_stm(*chn, rx, NBL_TIMEOUT_INFINITY);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Read stream failed (%d).\n", rc);
    goto failed;
  }

  nbl_stm_next_int(rx, &rc);

failed:
  nbl_stm_destroy(tx);
  nbl_stm_destroy(rx);

  return rc;
}

/**
 * \return nbl error
 */
static int _start_wait_for_connect(nbl_chn_t* srv_chn,
                                   const char* service_name) {
  int rc;
  int retry_count;
  int retry_interval_ms;
  int need_retry;

  g_nbl_nsc->s_app = nbl_app_create();

  // nss proxy in REE <--> client.
  g_nbl_nsc->s_services[0].name = service_name;
  g_nbl_nsc->s_services[0].flags = NBL_SRV_FLAG_SCOPE_REE;
  g_nbl_nsc->s_services[0].buf_num = NBL_SRV_DEFAULT_BUF_NUM;
  g_nbl_nsc->s_services[0].buf_size = NBL_SRV_MAX_BUF_SIZE;
  g_nbl_nsc->s_services[0].conn_handler = nbl_nss_proxy_connect;
  g_nbl_nsc->s_services[0].open_handler = nbl_nss_proxy_open;
  g_nbl_nsc->s_services[0].stm_handler = NULL;
  g_nbl_nsc->s_services[0].buf_handler = NULL;
  g_nbl_nsc->s_services[0].close_handler = nbl_nss_proxy_close;

  for (size_t i = 0; i < sizeof(g_nbl_nsc->s_services) / sizeof(nbl_srv_desc_t);
       i++) {
    rc = nbl_srv_create(&g_nbl_nsc->s_services[i], NULL);
    if (rc != NBL_NO_ERROR) {
      NBL_LOGE("Create service %s failed!", g_nbl_nsc->s_services[i].name);
      goto failed;
    }

    rc = nbl_app_reg_srv(g_nbl_nsc->s_app, g_nbl_nsc->s_services[i].srv);
    if (rc != NBL_NO_ERROR) {
      goto failed;
    }

    NBL_LOGI("Start port of nsc client:%s\n", g_nbl_nsc->s_services[i].name);
  }

  // Notify server that server of me is started, and then
  // server will notify client to connect the server(async return).
  retry_count = g_nbl_nsc->config.retry_count;
  retry_interval_ms = g_nbl_nsc->config.retry_interval_ms;
  need_retry = 0;
  do {
    rc = _send_finish_start_port (&srv_chn);
    if (retry_count > 0) {
      retry_count--;
    }

    need_retry = (retry_count > 0 || retry_count == -1)
        && rc == NBL_ERR_NOT_READY;

    if (need_retry && retry_interval_ms > 0) {
      struct timespec req = {
          .tv_sec = retry_interval_ms / 1000,
          .tv_nsec = (retry_interval_ms % 1000) * 1000000
      };
      nanosleep(&req, NULL);
    }
  }
  while (need_retry);

  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Notify nsc proxy failed (%d)\n", rc);
    goto failed;
  }

  // If proxy connects to this port, the loop will exit.
  // - But channel is kept for file operations later.
  // - If wait some time, should exit from this loop because proxy may crashes.
  rc = _app_wait_channel (g_nbl_nsc->s_app, g_nbl_nsc->s_services[0].srv,
                          NBL_TIMEOUT_INFINITY);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Wait nsc proxy connection failed (%d)\n", rc);
    goto failed;
  }

  NBL_LOGI("Start NSC.\n");

  return rc;

failed:
  for (size_t i = 0; i < sizeof(g_nbl_nsc->s_services) / sizeof(nbl_srv_desc_t);
       i++) {
    nbl_srv_destroy(g_nbl_nsc->s_services[i].srv);
    free((void*)g_nbl_nsc->s_services[0].name);
  }
  nbl_app_destroy(g_nbl_nsc->s_app);

  return rc;
}

int nbl_nsc_start(nbl_nsc_config_t* config) {
  int rc;
  char* client_name = NULL;
  nbl_chn_t* chn_srv = NULL;

  if (g_nbl_nsc) {
    return 0;
  }

  g_nbl_nsc = calloc(1, sizeof(nbl_nsc_t));
  if (!g_nbl_nsc) {
    NBL_LOGE("Failed to allocate NBL REE context.\n");
    rc = errno;
    goto failed;
  }

  if (config == NULL) {
    g_nbl_nsc->config = (nbl_nsc_config_t)DEF_NBL_NSC_CONFIG;
  } else {
    g_nbl_nsc->config = *config;
  }

  rc = _nss_get_client_name(&chn_srv, &client_name);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Cannot connect HEE NSS srv. err=%d\n", rc);
    goto failed;
  }
  NBL_LOGI("Get client name '%s'.\n", client_name);

  // start own server and wait for connection from proxy.
  rc = _start_wait_for_connect(chn_srv, client_name);
  nbl_chn_close(chn_srv);

  if (rc != NBL_NO_ERROR) {
    goto failed;
  }  // Only rc = 0, will continue.

  return rc;

failed:
  if (g_nbl_nsc) {
    free(g_nbl_nsc);
    g_nbl_nsc = NULL;
  }
  nbl_nsc_set_errno(rc);
  return -1;
}

static void _free_ree(nbl_nsc_t* ree) {
  if (!ree) {
    return;
  }
  // NOTICE: Must not close channel here, because it will be closed later.
  ree->chn = NULL;

  if (ree->shm) {
    nbl_shm_free(ree->shm);
  }

  ree->shm = NULL;
  ree->shm_size = 0;

  free(ree);
}

void nbl_nsc_stop() {
  int rc;

  if (!g_nbl_nsc) {
    return;
  }

  // Order of freeing shm is :
  // 1, free In HEE. 2, free In REE. 3, HEE close channel.
  if (g_nbl_nsc->shm) {
    nbl_shm_free(g_nbl_nsc->shm);
    g_nbl_nsc->shm = NULL;
    g_nbl_nsc->shm_base = NULL;
    g_nbl_nsc->shm_size = 0;
  }

  // Send "stop" message to ree nss proxy.
  nbl_stm_t* tx = nbl_stm_create();
  nbl_stm_add_int(tx, NBL_NSC_CMD_STOP);
  rc = nbl_chn_write_stm(g_nbl_nsc->chn, tx);
  if (rc != NBL_NO_ERROR) {
    NBL_LOGE("Write stream failed (%d).\n", rc);
  }
  nbl_stm_destroy(tx);

  // Free app/srv.
  for (size_t i = 0; i < sizeof(g_nbl_nsc->s_services) / sizeof(nbl_srv_desc_t);
       i++) {
    nbl_srv_destroy(g_nbl_nsc->s_services[i].srv);
    free((void*)g_nbl_nsc->s_services[i].name);
  }

  if (g_nbl_nsc->s_app) {
    nbl_app_destroy(g_nbl_nsc->s_app);
    g_nbl_nsc->s_app = NULL;
  }

  _free_ree(g_nbl_nsc);
  g_nbl_nsc = NULL;
}

nbl_nsc_config_t _nsc_config = DEF_NBL_NSC_CONFIG;

void nbl_nsc_set_config(nbl_nsc_config_t *config) {
	if (config) {
		memcpy(&_nsc_config, config, sizeof(nbl_nsc_config_t));
	}
}
