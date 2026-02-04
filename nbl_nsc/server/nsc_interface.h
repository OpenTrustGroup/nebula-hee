// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#pragma once

/**
 * Keep the common interface
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Commands between NSC client and server in HEE.
 */
enum {
  NBL_NSC_CMD_START_CLIENT = 1,
  NBL_NSC_CMD_FINISH_START_CLIENT = 2,
};

/**
 * Commands between NSC server and proxy in REE.
 * same with "//third_party/ree-trusty/nbl_daemon/nss_proxy.c".
 * Note: This definition cannot be merge to REE's source or inversely, because
 * the REE source should be compiled alone.
 */
enum {
  NBL_NSC_SRV_CMD_CONNECT = 1,
};

/**
 * Common commands between NSC proxy in REE and client in HEE.
 * Same with "//third_party/ree-trusty/nbl_daemon/nsc_inner.h".
 * Note: This definition cannot be merge to REE's source or inversely, because
 * the REE source should be compiled alone.
 */
enum {
  NBL_NSC_CMD_NULL = 0,
  NBL_NSC_CMD_OPEN = 1,
  NBL_NSC_CMD_CLOSE = 2,
  NBL_NSC_CMD_READ = 3,
  NBL_NSC_CMD_WRITE = 4,
  NBL_NSC_CMD_FSIZE = 5,
  NBL_NSC_CMD_REMOVE = 6,
  NBL_NSC_CMD_LSEEK = 7,
  NBL_NSC_CMD_GETTIMEOFDAY = 8,

  NBL_NSC_CMD_STOP = 500,
  // initialize transfer method by buffer or shared memory.
  NBL_NSC_CMD_INIT_TRANSFER_METHOD = 501,
  NBL_NSC_CMD_CLOSE_TRANSFER_METHOD = 502,

  NBL_NSC_RSP_OPEN_TIPC = 1000,  // use buffer, not shared memory
  NBL_NSC_RSP_OPEN_SHM = 1001,   // use shared memory
  NBL_NSC_RSP_CLOSE_SHM = 1002,  // close shared memory
};

#ifdef __cplusplus
}
#endif
