// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.

// This is library for invoking Non-Secure(aka REE) functions.
// 1. nbl_nsc_* functions are like libc's function. But get errno by function
// "nbl_nsc_get_errno()". The error code is same with libc's errno.
// If return "EIO", cannot send message to proxy, so should start again.
// 2. All NSC functions will start nsc implicitly
//    (call nbl_nsc_start at head of functions).
//    But if want "start nsc" at a special place, call nbl_nsc_start().
// 3. If set start config, should invoke nb_nsc_start with special config at
// first.
// 4. Close ncs using nbl_nsc_stop().

#ifndef NSC_COMMON_H_
#define NSC_COMMON_H_

#include <nbl_fwk/service.h>

#ifdef __cplusplus
extern "C" {
#endif

// Same with "//third_party/ree-trusty/nbl_daemon/nsc_inner.h".
typedef enum {
  // If use shared memory at first, if not, use buf.
  NBL_NSC_MEM_ANY = 0,
  NBL_NSC_MEM_ONLY_SHARED_MEM = 1,
  NBL_NSC_MEM_ONLY_BUF = 2,
} NBL_NSC_MEM_TYPE;

typedef struct {
  NBL_NSC_MEM_TYPE mem_type; ///< Shared memory type.
  int retry_count; ///< Retry count for NSC proxy not connected case. 0 Never Retry. -1 Retry for ever.
  int retry_interval_ms; ///< Retry interval in milliseconds.
} nbl_nsc_config_t;

#define DEF_NBL_NSC_CONFIG \
  { .mem_type = NBL_NSC_MEM_ANY, \
    .retry_count = 0, \
    .retry_interval_ms = 0 \
  }

/**
 * Start NBL_NSC service of local thread.
 * If not call this function, other functions will return -1, and nbl_errno is
 * "ENOTCONN". If start already, it will do nothing, return OK.
 * 1. If return ETIMEDOUT, maybe proxy does NOT start.
 * \param config If started, return 0 with previous "config" whether or not set
 *               If set NULL, use default config.
 * "config" on this calling. \return 0 is OK, -1 is ERROR.
 */
int nbl_nsc_start(nbl_nsc_config_t* config);

/**
 * Stop NBL NSC service of local thread.
 * If want use this service again, should invoke nbl_nsc_start().
 */
void nbl_nsc_stop();

/**
 * Get errno from NBL NSC in local thread.
 * 1. Only when return -1, the errno returned has meaning.
 * 2. If there is error on transferring, then return "EIO".
 * 3. This errno is Thread Local Data, so cannot get other thread's errno.
 * \return errno, concrete value refers to libc.
 */
int nbl_nsc_get_errno();

/**
 * Get NSC channel in local thread.
 * \return NULL if NSC is not started.
 */
nbl_chn_t* nbl_nsc_get_channel();

/**
 * Replace the default NSC configuration.
 *
 * \param config A new NSC configuration.
 */
void nbl_nsc_set_config(nbl_nsc_config_t *config);

#ifdef __cplusplus
}
#endif

#endif  // NSC_COMMON_H_
