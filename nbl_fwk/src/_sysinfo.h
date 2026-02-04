// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#pragma once
#include <stdint.h>

#define NBL_SYS_INFO_VERSION_V1 1
#define NBL_SYS_INFO_CUR_VERSION NBL_SYS_INFO_VERSION_V1

enum {
	NBL_SYS_INFO_SHMTYPE_INVALID = 0,
	NBL_SYS_INFO_SHMTYPE_TIPC = 1,
	NBL_SYS_INFO_SHMTYPE_LINKBUF,
	NBL_SYS_INFO_SHMTYPE_HANDLECHN,
};

typedef struct nbl_sys_info_v1 {
	int32_t version;
	uint32_t shmtype;
	uint32_t vid;
	uint32_t pid;
	uint32_t rpmb_programmed;
} nbl_sys_info_v1_t;

typedef nbl_sys_info_v1_t nbl_sys_info_t;

int nbl_sys_get_info(int32_t version, nbl_sys_info_t *out_info);
