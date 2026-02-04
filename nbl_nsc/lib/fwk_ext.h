// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "nbl_fwk/service.h"

int _app_wait_channel(nbl_app_t *app, nbl_srv_t *srv, uint64_t timeout_msecs);

#ifdef __cplusplus
}
#endif
