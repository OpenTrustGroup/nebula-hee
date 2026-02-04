// Copyright 2020 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NSC_TIME_H_
#define NSC_TIME_H_

// This is library for Non-Security TIME.
// - used for process time in REE.
// - Functions are like libc file, for examples: gettimeofday and etc.

#include <nbl_fwk/service.h>
#include "nsc_common.h"

#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get time.
 * \param tv get time value.
 * \param tz timezone.
 * \return -1: error, =0: OK.
 */
int nbl_nsc_gettimeofday(struct timeval *tv, struct timezone *tz);

#ifdef __cplusplus
}
#endif

#endif  // NSC_TIME_H_
