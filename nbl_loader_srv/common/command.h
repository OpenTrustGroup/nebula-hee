/*
 * command.h
 *
 *  Created on: May 22, 2018
 *      Author: zhoubo
 */

#ifndef THIRD_PARTY_HEE_TRUSTY_GRT_TRUSTY_LOADER_COMMON_COMMAND_H_
#define THIRD_PARTY_HEE_TRUSTY_GRT_TRUSTY_LOADER_COMMON_COMMAND_H_
#include <stdint.h>
#include <trusty_uuid.h>

// MAX length of application argument.
#define ARG_MAX_LEN 64

enum {
	GRT_LOADER_SEND_FILE_DATA = 1,
	GRT_LOADER_SEND_FILE_NAME = 2,
	GRT_LOADER_REE_TA_REQ = 3,
	GRT_LOADER_SEND_ABORT = 4,
	GRT_LOADER_SEND_RESPONSE = 5,
	GRT_LOADER_LOAD_TA = 6,
	GRT_LOADER_LOAD_SECMGR_TA = 7,
	// On end of TA run, return info to callee.
	GRT_LOADER_TA_RUN_END = 8,

	GRT_LOADER_LOAD_CHANNEL_TEST = 99,
	GRT_LOADER_START_LEAK_DETECT = 100,
	GRT_LOADER_STOP_LEAK_DETECT = 101
};

typedef struct grt_loader_cmd {
	uint32_t cmd;
	uint32_t param_size;
} grt_loader_cmd_t;

typedef struct grt_load_path_info {
	uint32_t path_len;
	uint32_t use_ion_as_heap;
	uint64_t heap_base;
	uint32_t shm_handle;
	uint32_t with_tracer;
	uint64_t heap_size;
	char arg[ARG_MAX_LEN];
} grt_load_path_info_t;

typedef struct grt_load_uuid_info {
	uuid_t uuid;
	uint32_t use_ion_as_heap;
	uint64_t heap_base;
	uint32_t shm_handle;
	uint64_t heap_size;
	char arg[ARG_MAX_LEN];
} grt_load_uuid_info_t;

typedef struct grt_loader_file_info {
	uint32_t file_offset;
	uint32_t file_len;
	uint32_t data_handle;
	uint32_t data_size;
} grt_loader_file_info_t;

typedef struct grt_loader_ta_param {
	uint32_t path_len;
} grt_loader_ta_param_t;

static inline grt_loader_cmd_t *grt_loader_get_cmd(void *data) {
	grt_loader_cmd_t *cmd = (grt_loader_cmd_t *)data;
	return cmd;
}

static inline grt_loader_file_info_t *grt_loader_get_file_info(void *data) {
	grt_loader_file_info_t *file_info = (grt_loader_file_info_t *)((char *)grt_loader_get_cmd(data) + sizeof(grt_loader_cmd_t));
	return file_info;
}

static inline void *grt_loader_get_data(void *data) {
	char *d = (char *)grt_loader_get_file_info(data) + sizeof(grt_loader_file_info_t);
	return d;
}
#endif /* THIRD_PARTY_HEE_TRUSTY_GRT_TRUSTY_LOADER_COMMON_COMMAND_H_ */
