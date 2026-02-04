// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#include <nbl_fwk/err.h>
#include <nbl_fwk/log.h>
#include <trusty_std.h>
#include <platform_defs.h>

#include "_sysinfo.h"
#define LOG_TAG "nbl_sys_info"

static nbl_sys_info_t _sys_info;

typedef struct {
	uint32_t save_type;
	uint32_t shm_type;
} dcfg_hyp_nss_t;

typedef struct {
	uint32_t base; /* in bytes */
	uint32_t size; /* in bytes */
	uint32_t programmed;
} dcfg_hyp_rpmb_t;

int nbl_sys_get_info(int32_t version, nbl_sys_info_t *out_info) {
	int rc;
	uint32_t vid;
	uint32_t pid;
	dcfg_hyp_nss_t nss_info;
	dcfg_hyp_rpmb_t rpmb_info;

	if (!out_info) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (version <= NBL_SYS_INFO_CUR_VERSION) {
		if (_sys_info.version == 0) {
			rc = _trusty_ioctl(SYS_PLATFORM_FD, SYS_PLATFORM_GET_VENDOR_ID, &vid);
			if (rc != NBL_NO_ERROR) {
				NBL_LOGE("Get vid failed (%d).\n", vid);
				rc = NBL_ERR_FAULT;
				goto failed;
			}

			rc = _trusty_ioctl(SYS_PLATFORM_FD, SYS_PLATFORM_GET_PRODUCT_ID, &pid);
			if (rc != NBL_NO_ERROR) {
				NBL_LOGE("Get pid failed (%d).\n", pid);
				rc = NBL_ERR_FAULT;
				goto failed;
			}

			rc = _trusty_ioctl(SYS_PLATFORM_FD, SYS_PLATFORM_GET_NSS_INFO, &nss_info);
			if (rc != NBL_NO_ERROR) {
				NBL_LOGE("Get nss info failed (%d).\n", rc);
				rc = NBL_ERR_FAULT;
				goto failed;
			}

			rc = _trusty_ioctl(SYS_PLATFORM_FD, SYS_PLATFORM_GET_RPMB_INFO, &rpmb_info);
			if (rc != NBL_NO_ERROR) {
				NBL_LOGE("Get rpmb info failed (%d).\n", rc);
				rc = NBL_ERR_FAULT;
				goto failed;
			}

			_sys_info.version = NBL_SYS_INFO_CUR_VERSION;
			_sys_info.vid = vid;
			_sys_info.pid = pid;
			_sys_info.shmtype = nss_info.shm_type;
			_sys_info.rpmb_programmed = rpmb_info.programmed;
		} else {
			rc = NBL_NO_ERROR;
		}

		out_info->version = _sys_info.version;
		out_info->vid = _sys_info.vid;
		out_info->pid = _sys_info.pid;
		out_info->shmtype = _sys_info.shmtype;
		out_info->rpmb_programmed = _sys_info.rpmb_programmed;
	} else {
		rc = NBL_ERR_NOT_SUPPORTED;
	}

failed:
	return rc;
}
