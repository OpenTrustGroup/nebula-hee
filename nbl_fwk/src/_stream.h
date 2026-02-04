// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NBL_INTERNAL_COMMON_H_
#define NBL_INTERNAL_COMMON_H_
#include <nbl_fwk/stream.h>
#include <trusty_std.h>

typedef struct nbl_shm {
	void *buf;
	size_t buf_size;
	size_t data_size;
	handle_t root_handle;
	handle_t handle;
	void *base;
	uint64_t map_flags;
	uint32_t shmtype;
} nbl_shm_t;

typedef struct nbl_stm {
	ipc_msg_t ipc_msg;
	uint32_t msg_index;
	uint32_t handle_index;
	uint32_t msg_buf_offset;
} nbl_stm_t;

enum {
	NBL_CMD_PARAM_INT = 0,
	NBL_CMD_PARAM_DATA = 1,
	NBL_CMD_PARAM_SHRMEM = 2,
	NBL_CMD_PARAM_STRING = 3,
};

#define NBL_CMD_VERSION 0

#define NBL_CMD_VERSION_MASK 0xFFFULL
#define NBL_CMD_VERSION_SHIFT 0

#define NBL_MAX_CMD_COUNT 256
#define NBL_CMD_ID_MASK 0xFF
#define NBL_CMD_ID_SHIFT 12

#define NBL_MAX_INDEX_COUNT 256
#define NBL_CMD_INDEX_MASK 0xFF
#define NBL_CMD_INDEX_SHIFT 20

#define NBL_MAX_PARAM_COUNT 8
#define NBL_CMD_PARAM_MASK 0xF
#define NBL_CMD_PARAM_SHIFT 28

#define NBL_CMD_PARAM_TYPE_MASK 0xF
#define NBL_CMD_PARAM_TYPE_BIT_WIDTH 4
#define NBL_CMD_PARAM_TYPE_SHIFT 32

#define NBL_STM_HEADER_MAGIC 0x3d61e8e7
#define NBL_STM_VERSION 1

typedef struct nbl_fwk_cmd {
	uint64_t cmd;
} nbl_fwk_cmd_t;

typedef struct nbl_stm_header {
	uint32_t magic;
	uint16_t version;
	uint16_t handle_table;
	uint32_t reserves[2];
} nbl_stm_header_t;

typedef struct nbl_fwk_cmd_param_data {
	uint32_t data_size;
} nbl_fwk_cmd_param_data_t;

typedef struct nbl_fwk_cmd_param_shrmem {
	uint32_t id;
	uint32_t data_size;
	uint32_t buf_size;
} nbl_fwk_cmd_param_shrmem_t;

static inline uint32_t _get_cmd_field(nbl_fwk_cmd_t *cmd,
		uint32_t shift, uint32_t mask) {
	uint32_t field = (uint32_t)((cmd->cmd >> shift) & mask);
	return field;
}

static inline void _set_cmd_field(nbl_fwk_cmd_t *cmd,
		uint32_t shift, uint32_t mask, uint32_t value) {
	cmd->cmd = (cmd->cmd & (~(((uint64_t)mask) << shift))) | (((uint64_t)(value & mask)) << shift);
}

static inline void nbl_fwk_init_cmd(nbl_fwk_cmd_t *cmd, uint32_t cmd_id, uint32_t index) {
	cmd->cmd = (NBL_CMD_VERSION & NBL_CMD_VERSION_MASK)
			| ((cmd_id & NBL_CMD_ID_MASK) << NBL_CMD_ID_SHIFT)
			| ((index & NBL_CMD_INDEX_MASK) << NBL_CMD_INDEX_SHIFT);
}

static inline nbl_fwk_cmd_t *nbl_fwk_get_cmd(void *data) {
	nbl_fwk_cmd_t *cmd = (nbl_fwk_cmd_t *)data;
	return cmd;
}

static inline void *nbl_fwk_get_cmd_param_base(void *data) {
	void *param_base = (void *)nbl_fwk_get_cmd(data) + sizeof(nbl_fwk_cmd_t);
	return param_base;
}

static inline uint32_t nbl_fwk_get_cmd_version(nbl_fwk_cmd_t *cmd) {
	return _get_cmd_field(cmd, NBL_CMD_VERSION_SHIFT, NBL_CMD_VERSION_MASK);
}

static inline uint32_t nbl_fwk_get_cmd_id(nbl_fwk_cmd_t *cmd) {
	return _get_cmd_field(cmd, NBL_CMD_ID_SHIFT, NBL_CMD_ID_MASK);
}

static inline uint32_t nbl_fwk_get_cmd_index(nbl_fwk_cmd_t *cmd) {
	return _get_cmd_field(cmd, NBL_CMD_INDEX_SHIFT, NBL_CMD_INDEX_MASK);
}

static inline void nbl_fwk_set_cmd_index(nbl_fwk_cmd_t *cmd, uint32_t cmd_index) {
	_set_cmd_field(cmd, NBL_CMD_INDEX_SHIFT, NBL_CMD_INDEX_MASK, cmd_index);
}

static inline uint32_t nbl_fwk_get_cmd_param_count(nbl_fwk_cmd_t *cmd) {
	return _get_cmd_field(cmd, NBL_CMD_PARAM_SHIFT, NBL_CMD_PARAM_MASK);
}

static inline uint32_t nbl_fwk_get_cmd_param_type(nbl_fwk_cmd_t *cmd, uint32_t index) {
	return _get_cmd_field(cmd,
			NBL_CMD_PARAM_TYPE_SHIFT
					+ index * NBL_CMD_PARAM_TYPE_BIT_WIDTH,
			NBL_CMD_PARAM_TYPE_MASK);
}

static inline int nbl_fwk_add_cmd_param_type(nbl_fwk_cmd_t *cmd,
		uint32_t param_type) {
	int rc = -1;
	uint32_t param_count = nbl_fwk_get_cmd_param_count(cmd);

	if (param_count < NBL_MAX_PARAM_COUNT) {
		_set_cmd_field(cmd,
				NBL_CMD_PARAM_TYPE_SHIFT
						+ param_count * NBL_CMD_PARAM_TYPE_BIT_WIDTH,
				NBL_CMD_PARAM_TYPE_MASK, param_type);
		_set_cmd_field(cmd, NBL_CMD_PARAM_SHIFT,
				NBL_CMD_PARAM_MASK, ++param_count);
		rc = 0;
	}

	return rc;
}

static inline void nbl_stm_set_header(void *base) {
	nbl_stm_header_t *header = (nbl_stm_header_t *)base;
	header->magic = NBL_STM_HEADER_MAGIC;
	header->version = NBL_STM_VERSION;
	header->handle_table = 0;
	header->reserves[0] = 0;
	header->reserves[1] = 0;
}

static inline int nbl_stm_check_header(void *base) {
	nbl_stm_header_t *header = (nbl_stm_header_t *)base;

	if (header->magic != NBL_STM_HEADER_MAGIC) {
		return NBL_ERR_CRC_FAIL;
	}

	return NBL_NO_ERROR;
}

static inline void *nbl_fwk_add_cmd_param_int(void *cur_pos, int value) {
	void *next_pos = cur_pos;

	*(int *)cur_pos = value;
	next_pos += sizeof(int);

	return next_pos;
}

static inline void *nbl_fwk_add_cmd_param_data(void *cur_pos, int size, const void *data) {
	void *next_pos = cur_pos;

	nbl_fwk_cmd_param_data_t *data_info = (nbl_fwk_cmd_param_data_t *)cur_pos;
	data_info->data_size = size;
	next_pos += sizeof(nbl_fwk_cmd_param_data_t);

	memcpy(next_pos, data, size);
	next_pos += size;

	return next_pos;
}

static inline void *nbl_fwk_add_cmd_param_data_ptr(void *cur_pos, int size, void **out_data_ptr) {
	void *next_pos = cur_pos;

	nbl_fwk_cmd_param_data_t *data_info = (nbl_fwk_cmd_param_data_t *)cur_pos;
	data_info->data_size = size;
	next_pos += sizeof(nbl_fwk_cmd_param_data_t);

	*out_data_ptr = next_pos;
	next_pos += size;

	return next_pos;
}

static inline void *nbl_fwk_add_cmd_param_shrmem(void *cur_pos, nbl_fwk_cmd_param_shrmem_t value) {
	void *next_pos = cur_pos;

	*(nbl_fwk_cmd_param_shrmem_t *)cur_pos = value;
	next_pos += sizeof(nbl_fwk_cmd_param_shrmem_t);

	return next_pos;
}

static inline void *nbl_fwk_next_cmd_param_int(void *cur_pos, int *out_value) {
	void *next_pos = cur_pos;

	*out_value = *(int *)cur_pos;
	next_pos += sizeof(int);

	return next_pos;
}

static inline void *nbl_fwk_next_cmd_param_data(void *cur_pos, int *out_size, void *out_data) {
	void *next_pos = cur_pos;

	nbl_fwk_cmd_param_data_t *data_info = (nbl_fwk_cmd_param_data_t *)cur_pos;
	if (out_size) {
		*out_size = data_info->data_size;
	}
	next_pos += sizeof(nbl_fwk_cmd_param_data_t);

	memcpy(out_data, next_pos, data_info->data_size);
	next_pos += data_info->data_size;

	return next_pos;
}

static inline void *nbl_fwk_next_cmd_param_data_ptr(void *cur_pos, int *out_size, void **out_data_ptr) {
	void *next_pos = cur_pos;

	nbl_fwk_cmd_param_data_t *data_info = (nbl_fwk_cmd_param_data_t *)cur_pos;
	*out_size = data_info->data_size;
	next_pos += sizeof(nbl_fwk_cmd_param_data_t);

	*out_data_ptr = next_pos;
	next_pos += data_info->data_size;

	return next_pos;
}

static inline void *nbl_fwk_next_cmd_param_shrmem(void *cur_pos, nbl_fwk_cmd_param_shrmem_t *out_value) {
	void *next_pos = cur_pos;

	*out_value = *(nbl_fwk_cmd_param_shrmem_t *)cur_pos;
	next_pos += sizeof(nbl_fwk_cmd_param_shrmem_t);

	return next_pos;
}

int nbl_stm_add_cmd(nbl_stm_t *stream, nbl_fwk_cmd_t cmd);
int nbl_stm_next_cmd(nbl_stm_t *stream, nbl_fwk_cmd_t *out_cmd);
void *nbl_stm_get_msg_buf(nbl_stm_t *stream, int msg_index);
void *nbl_stm_get_handle_buf(nbl_stm_t *stream);

#endif /* NBL_INTERNAL_COMMON_H_ */
