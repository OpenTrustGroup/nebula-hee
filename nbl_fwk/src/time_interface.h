// Copyright 2020 GoldenRiver Technology Co., Ltd. All rights reserved.
#pragma once

/**
 * Common interface for client and server of "time".
 */

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_SRV_CHANNEL_NAME "com.grt.nebula.time.srv"

/**
 * Commands between TIME client and server.
 */
enum {
  NBL_TIME_CMD_GET_REE_TIME = 1,
  NBL_TIME_CMD_GET_SYSTEM_TIME = 2,
  NBL_TIME_CMD_SET_TA_PERSISTENT_TIME = 3,
  NBL_TIME_CMD_GET_TA_PERSISTENT_TIME = 4,
};

#ifdef __cplusplus
}
#endif
