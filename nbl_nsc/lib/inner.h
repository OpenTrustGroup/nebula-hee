// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#pragma once

#include <lk/list.h>
#include <stdlib.h>

#include <nbl_fwk/common.h>
#include <nbl_fwk/log.h>
#include <nbl_fwk/service.h>
#include <nbl_fwk/stream.h>

#include <nsc_common.h>

#ifdef __cplusplus
extern "C"
#endif

#define NBL_NSS_MIN_PACKET_SIZE 4000

// If not connected, return ENOTCONN(Transport endpoint is not connected).
// If fatal error: need to connect again, return EIO(Input/output error).
// -- because trusty_write/read is SAME name error.
// If failed, can redo, return ECOMM (Communication error on send)
#define ERROR_CONN ENOTCONN
#define ERROR_FATAL EIO
#define ERROR_FAIL ECOMM

/**
 * REE context which is a valid connect to daemon in REE.
 */
typedef struct nbl_nsc {
	nbl_chn_t* chn;
	nbl_shm_t* shm;
	void* shm_base;
	size_t shm_size;
	nbl_srv_desc_t s_services[1];
	nbl_app_t* s_app;
	nbl_nsc_config_t config;
} nbl_nsc_t;

#define CHECK_AND_START_NSC()        \
  rlt = nbl_nsc_start(&_nsc_config); \
  if (rlt != 0) {                    \
    return rlt;                      \
  }

#define CHECK_FATAL_ERROR_AND_GOTO()                        \
  if (rlt == NBL_ERR_IO || rlt == NBL_ERR_CHANNEL_CLOSED) { \
    goto fatal_error;                                       \
  }

#define HANDLE_FATAL_ERROR()      \
  nbl_stm_destroy(tx);            \
  nbl_stm_destroy(rx);            \
  nbl_nsc_stop();                 \
  nbl_nsc_set_errno(ERROR_FATAL); \
  return -1;

void nbl_nsc_set_errno(int err);

extern __thread nbl_nsc_t* g_nbl_nsc;
extern nbl_nsc_config_t _nsc_config;

int _init_transfer_mem();
/**
 * Read result and errno by stream from REE.
 */
int _read_ree_rlt_errno(nbl_stm_t* rx, int* ree_rlt, int* ree_errno);

#ifdef __cplusplus
}
#endif
