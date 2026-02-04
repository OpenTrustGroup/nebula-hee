// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <nbl_fwk/log.h>
#include <nbl_fwk/service.h>

#if NEBULA_CONFIG_UREE_SUPPORT
#include <tz_private/ta_sys_mem.h>
#endif

#include "_stream.h"
#include "_sysinfo.h"

#define PAGE_MASK (PAGE_SIZE - 1)

#define LOG_TAG "nbl_stream"

static int _get_trusty_shm_flags(uint64_t nflags, uint64_t *out_flags) {
	uint64_t tflags = 0;

	switch (nflags & NBL_SHM_TYPE_MASK) {
	case NBL_SHM_TYPE_NORMAL:
		tflags |= MMAP_FLAG_MEMREF_HANDLE;
		break;
	case NBL_SHM_TYPE_DEVICE:
	default:
		return NBL_ERR_NOT_SUPPORTED;
	}

	switch (nflags & NBL_SHM_SCOPE_MASK) {
	case NBL_SHM_SCOPE_DEFAULT:
	case NBL_SHM_SCOPE_HEE: {
		break;
	}
	case NBL_SHM_SCOPE_REE:
	case NBL_SHM_SCOPE_TEE:
	default:
		return NBL_ERR_NOT_SUPPORTED;
	}

	switch (nflags & NBL_SHM_PERM_MASK) {
	case NBL_SHM_PERM_RW:
		tflags |= MMAP_FLAG_PROT_RW;
		break;
	case NBL_SHM_PERM_RO:
		tflags |= MMAP_FLAG_PROT_RO;
		break;
	default:
		return NBL_ERR_NOT_SUPPORTED;
	}

	*out_flags = tflags;

	return NBL_NO_ERROR;
}

int nbl_shm_alloc(nbl_chn_t *chn, size_t size, uint64_t flags,
		nbl_shm_t **out_ref, void **out_base) {
	int err = NBL_NO_ERROR;
	nbl_shm_t *shrmem;
	size_t round_size;
	void *buf;
	handle_t root_handle;
	handle_t handle;
	uint64_t tflags = 0;

	if (!size) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!out_ref) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!out_base) {
		return NBL_ERR_INVALID_ARGS;
	}

	err = _get_trusty_shm_flags(flags, &tflags);
	if (err != NBL_NO_ERROR) {
		return err;
	}

	shrmem = calloc(1, sizeof(nbl_shm_t));
	if (!shrmem) {
		return NBL_ERR_NO_MEMORY;
	}

	shrmem->shmtype = NBL_SYS_INFO_SHMTYPE_INVALID;

	round_size = (size + PAGE_MASK) & ~PAGE_MASK;
	buf = memalign(PAGE_SIZE, round_size);
	if (!buf) {
		err = NBL_ERR_NO_MEMORY;
		goto alloc_buf_failed;
	}

	shrmem->buf = buf;
	shrmem->buf_size = round_size;
	shrmem->data_size = size;

	struct iovec iov = { buf, round_size };
	root_handle = _trusty_mref_create_handle(&iov, 1, tflags);
	if (root_handle == INVALID_IPC_HANDLE) {
		err = NBL_ERR_CREATE_HANDLE;
		goto create_handle_failed;
	}

	handle = _trusty_mref_create_child(root_handle, 0, round_size, tflags);
	if (handle == INVALID_IPC_HANDLE) {
		err = NBL_ERR_CREATE_CHILD_HANDLE;
		goto create_child_handle_failed;
	}

	shrmem->root_handle = root_handle;
	shrmem->handle = handle;
	shrmem->base = buf;
	shrmem->map_flags = tflags;

	*out_ref = shrmem;
	*out_base = buf;

	return err;

create_child_handle_failed:
	_trusty_close(root_handle);
create_handle_failed:
	free(buf);
alloc_buf_failed:
	free(shrmem);
	return err;
}

void nbl_shm_free(nbl_shm_t *ref) {
	if (ref) {
		if (ref->root_handle == INVALID_IPC_HANDLE) {
			// If not the shared memory owner, unmap first.
			nbl_shm_unmap(ref);
		}

		if (ref->shmtype != NBL_SYS_INFO_SHMTYPE_HANDLECHN) {
			if (ref->handle != INVALID_IPC_HANDLE) {
				_trusty_close(ref->handle);
			}
		}

		if (ref->root_handle != INVALID_IPC_HANDLE) {
			_trusty_close(ref->root_handle);
		}

		if (ref->buf) {
			free(ref->buf);
		}

		memset(ref, 0, sizeof(nbl_shm_t));
		ref->root_handle = INVALID_IPC_HANDLE;
		ref->handle = INVALID_IPC_HANDLE;

		free(ref);
	}
}

int nbl_shm_get_data_size(nbl_shm_t *ref, size_t *out_size) {
	int err = NBL_NO_ERROR;

	if (!ref) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!out_size) {
		return NBL_ERR_INVALID_ARGS;
	}

	*out_size = ref->data_size;

	return err;
}

int nbl_shm_get_data_ptr(nbl_shm_t *ref, void **out_ptr) {
	int err = NBL_NO_ERROR;

	if (!ref) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!out_ptr) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!ref->base) {
		return NBL_ERR_NOT_READY;
	}

	*out_ptr = ref->base;

	return err;
}

int nbl_shm_map(nbl_shm_t *ref, uint64_t flags, void **out_base) {
	int err;
	void *base;
	uint64_t tflags;

	if (!ref) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (ref->root_handle != INVALID_IPC_HANDLE) {
		return NBL_ERR_NOT_VALID;
	}

	if (ref->base) {
		return NBL_ERR_ALREADY_EXISTS;
	}

	if (ref->handle == INVALID_IPC_HANDLE) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!out_base) {
		return NBL_ERR_INVALID_ARGS;
	}

	err = _get_trusty_shm_flags(flags, &tflags);
	if (err != NBL_NO_ERROR) {
		return err;
	}

	if (ref->shmtype == NBL_SYS_INFO_SHMTYPE_HANDLECHN) {
#if NEBULA_CONFIG_UREE_SUPPORT
		MTEE_SHAREDMEM_PARAM mem_param;

		err = TA_Mem_QuerySharedmem(ref->handle, &mem_param);
		if (err != TZ_RESULT_SUCCESS) {
			return NBL_ERR_MMAP;
		}

		base = mem_param.buffer;
#else
		return NBL_ERR_NOT_SUPPORTED;
#endif
	} else {
		base = (void *)_trusty_mmap(NULL, ref->buf_size, tflags, ref->handle);
		if (!base) {
			return NBL_ERR_MMAP;
		}
	}


	ref->base = base;
	ref->map_flags = tflags;

	*out_base = base;

	return NBL_NO_ERROR;
}

#if NEBULA_CONFIG_UREE_SUPPORT
extern TZ_RESULT MTEE_UnmmapRegion(void *va, unsigned long size);
#endif

int nbl_shm_unmap(nbl_shm_t *ref) {
	if (!ref) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (ref->shmtype == NBL_SYS_INFO_SHMTYPE_HANDLECHN) {
#if NEBULA_CONFIG_UREE_SUPPORT
		if (ref->base) {
			MTEE_UnmmapRegion(ref->base, ref->buf_size);
			ref->base = NULL;
			ref->map_flags = 0;
		}
#else
		return NBL_ERR_NOT_SUPPORTED;
#endif
	} else {
		if (ref->root_handle != INVALID_IPC_HANDLE) {
			return NBL_ERR_NOT_VALID;
		}

		if (ref->base && ref->buf_size) {
			_trusty_munmap(ref->base, ref->buf_size);
			ref->base = NULL;
			ref->map_flags = 0;
		}
	}


	return NBL_NO_ERROR;
}

nbl_stm_t *nbl_stm_create() {
	int rc;
	nbl_stm_t *stream;
	void *msg_buf;
	handle_t *handle_buf;
	int i;

	stream = (nbl_stm_t *)calloc(1, sizeof(nbl_stm_t));
	if (!stream) {
		return NULL;
	}

	stream->ipc_msg.iov = (iovec_t *)calloc(1, sizeof(iovec_t));
	if (!stream->ipc_msg.iov) {
		goto alloc_iov_failed;
	}

	msg_buf = calloc(1, NBL_SRV_MAX_BUF_SIZE);
	if (!msg_buf) {
		goto alloc_msg_buf_failed;
	}

	stream->ipc_msg.iov[0].base = msg_buf;
	stream->ipc_msg.iov[0].len = NBL_SRV_MAX_BUF_SIZE;
	stream->ipc_msg.num_iov = 1;

	handle_buf = (handle_t *)malloc(NBL_STM_MAX_SHM_COUNT * sizeof(handle_t));
	if (!handle_buf) {
		goto alloc_handle_buf_failed;
	}

	for(i = 0; i < NBL_STM_MAX_SHM_COUNT; i++) {
		handle_buf[i] = INVALID_IPC_HANDLE;
	}

	stream->ipc_msg.handles = (handle_t *)handle_buf;
	stream->ipc_msg.num_handles = 0;

	rc = nbl_stm_reset(stream);
	if (rc != NBL_NO_ERROR) {
		goto reset_stream_failed;
	}

	return stream;

reset_stream_failed:
alloc_handle_buf_failed:
alloc_msg_buf_failed:
alloc_iov_failed:
	nbl_stm_destroy(stream);
	return NULL;
}

void nbl_stm_destroy(nbl_stm_t *stream) {
	unsigned int i;

	if (stream) {
		if (stream->ipc_msg.iov) {
			for(i = 0; i < stream->ipc_msg.num_iov; i++) {
				if (stream->ipc_msg.iov[i].base) {
					free(stream->ipc_msg.iov[i].base);
				}
			}

			if (stream->ipc_msg.handles) {
				free(stream->ipc_msg.handles);
			}

			free(stream->ipc_msg.iov);
		}

		memset(stream, 0, sizeof(nbl_stm_t));
		free(stream);
	}
}

int nbl_stm_add_cmd(nbl_stm_t *stream, nbl_fwk_cmd_t cmd) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	size_t write_size;
	nbl_fwk_cmd_t *cur_cmd;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];
	write_size = sizeof(nbl_fwk_cmd_t);

	if (stream->msg_buf_offset + write_size > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	cur_cmd = (nbl_fwk_cmd_t *)cur_pos;
	*cur_cmd = cmd;
	stream->msg_buf_offset += write_size;

	return err;
}

int nbl_stm_add_int(nbl_stm_t *stream, int value) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	void *next_pos;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	if (stream->msg_buf_offset + sizeof(int) > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_add_cmd_param_int(cur_pos, value);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return err;
}

int nbl_stm_add_buf(nbl_stm_t *stream, int size, const void *data) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	void *next_pos;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!size) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	if (stream->msg_buf_offset + size + sizeof(nbl_fwk_cmd_param_data_t) > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_add_cmd_param_data(cur_pos, size, data);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return err;
}

int nbl_stm_add_buf_ptr(nbl_stm_t *stream, int size, void **out_data_ptr) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	void *next_pos;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!out_data_ptr) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!size) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	if (stream->msg_buf_offset + size + sizeof(nbl_fwk_cmd_param_data_t) > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_add_cmd_param_data_ptr(cur_pos, size, out_data_ptr);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return err;
}

int nbl_stm_add_shm(nbl_stm_t *stream, nbl_shm_t *shrmem) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	handle_t *cur_handle;
	void *cur_pos;
	void *next_pos;
	nbl_fwk_cmd_param_shrmem_t param_shrmem;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	if (stream->msg_buf_offset + sizeof(nbl_fwk_cmd_param_shrmem_t) > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	if (!stream->ipc_msg.handles) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->handle_index >= NBL_STM_MAX_SHM_COUNT) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!shrmem) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (shrmem->handle == INVALID_IPC_HANDLE || !shrmem->data_size || !shrmem->buf_size) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (shrmem->shmtype == NBL_SYS_INFO_SHMTYPE_HANDLECHN) {
#if NEBULA_CONFIG_UREE_SUPPORT
		param_shrmem.id = shrmem->handle;
#else
		return NBL_ERR_NOT_SUPPORTED;
#endif
	} else {
		cur_handle = stream->ipc_msg.handles + stream->handle_index;
		*cur_handle = shrmem->handle;

		param_shrmem.id = stream->handle_index;
		stream->handle_index++;
		stream->ipc_msg.num_handles = stream->handle_index;
	}

	param_shrmem.data_size = shrmem->data_size;
	param_shrmem.buf_size = shrmem->buf_size;

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_add_cmd_param_shrmem(cur_pos, param_shrmem);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return err;
}

int nbl_stm_next_cmd(nbl_stm_t *stream, nbl_fwk_cmd_t *out_cmd) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	size_t write_size;
	nbl_fwk_cmd_t *cur_cmd;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];
	write_size = sizeof(nbl_fwk_cmd_t);

	if (stream->msg_buf_offset + write_size > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	if (!out_cmd) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	cur_cmd = (nbl_fwk_cmd_t *)cur_pos;
	*out_cmd = *cur_cmd;
	stream->msg_buf_offset += write_size;

	return err;
}

int nbl_stm_next_int(nbl_stm_t *stream, int *out_value) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	void *next_pos;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!out_value) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	if (stream->msg_buf_offset + sizeof(int) > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_next_cmd_param_int(cur_pos, out_value);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return err;
}

int nbl_stm_next_buf(nbl_stm_t *stream, int *out_size, void *out_data) {
	iovec_t *cur_iov;
	void *cur_pos;
	void *next_pos;
	size_t cur_size;
	size_t total_size;
	nbl_fwk_cmd_param_data_t *param_data;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!out_size && !out_data) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	cur_size = stream->msg_buf_offset + sizeof(nbl_fwk_cmd_param_data_t);
	if (cur_size > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	param_data = (nbl_fwk_cmd_param_data_t *)(cur_iov->base + stream->msg_buf_offset);
	total_size = cur_size + param_data->data_size;

	if (out_size && !out_data) {
		*out_size = param_data->data_size;
		return NBL_NO_ERROR;
	}

	if (total_size <= cur_size || total_size <= param_data->data_size || total_size > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_next_cmd_param_data(cur_pos, out_size, out_data);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return NBL_NO_ERROR;
}

int nbl_stm_next_buf_ptr(nbl_stm_t *stream, int *out_size, void **out_data_ptr) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	void *cur_pos;
	void *next_pos;
	size_t cur_size;
	size_t total_size;
	nbl_fwk_cmd_param_data_t *param_data;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!out_size || !out_data_ptr) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	cur_size = stream->msg_buf_offset + sizeof(nbl_fwk_cmd_param_data_t);
	if (cur_size > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	param_data = (nbl_fwk_cmd_param_data_t *)(cur_iov->base + stream->msg_buf_offset);
	total_size = cur_size + param_data->data_size;

	if (total_size <= cur_size || total_size <= param_data->data_size || total_size > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_next_cmd_param_data_ptr(cur_pos, out_size, out_data_ptr);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	return err;
}

int nbl_stm_next_shm(nbl_stm_t *stream, nbl_shm_t **out_shrmem) {
	int err = NBL_NO_ERROR;
	iovec_t *cur_iov;
	handle_t *handle;
	void *cur_pos;
	void *next_pos;
	nbl_fwk_cmd_param_shrmem_t param_shrmem;
	nbl_shm_t *shrmem;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	if (!stream->ipc_msg.iov || !stream->ipc_msg.num_iov) {
		return NBL_ERR_NOT_READY;
	}

	if (stream->msg_index >= stream->ipc_msg.num_iov) {
		return NBL_ERR_OUT_OF_RANGE;
	}

	if (!out_shrmem) {
		return NBL_ERR_INVALID_ARGS;
	}

	cur_iov = &stream->ipc_msg.iov[stream->msg_index];

	if (stream->msg_buf_offset + sizeof(nbl_fwk_cmd_param_shrmem_t) > cur_iov->len) {
		return NBL_ERR_OUT_OF_MEM;
	}

	if (!stream->ipc_msg.handles) {
		return NBL_ERR_NOT_READY;
	}

	cur_pos = cur_iov->base + stream->msg_buf_offset;
	next_pos = nbl_fwk_next_cmd_param_shrmem(cur_pos, &param_shrmem);
	stream->msg_buf_offset += (uint32_t)(next_pos - cur_pos);

	if (!param_shrmem.data_size || !param_shrmem.buf_size) {
		return NBL_ERR_BAD_HANDLE;
	}

	if (stream->ipc_msg.num_handles == 0) { // TODO use shmtype
#if NEBULA_CONFIG_UREE_SUPPORT
		shrmem = (nbl_shm_t *)calloc(1, sizeof(nbl_shm_t));
		if (!shrmem) {
			return NBL_ERR_NO_MEMORY;
		}

		shrmem->root_handle = INVALID_IPC_HANDLE;
		shrmem->handle = param_shrmem.id;
		shrmem->buf_size = param_shrmem.buf_size;
		shrmem->data_size = param_shrmem.data_size;
		shrmem->shmtype = NBL_SYS_INFO_SHMTYPE_HANDLECHN;
#else
		return NBL_ERR_OUT_OF_RANGE;
#endif
	} else {
		if (stream->ipc_msg.num_handles > NBL_STM_MAX_SHM_COUNT) {
			return NBL_ERR_OUT_OF_RANGE;
		}

		if (param_shrmem.id >= stream->ipc_msg.num_handles) {
			return NBL_ERR_OUT_OF_RANGE;
		}

		handle = stream->ipc_msg.handles + param_shrmem.id;
		if ((*handle) == INVALID_IPC_HANDLE) {
			return NBL_ERR_BAD_HANDLE;
		}

		shrmem = (nbl_shm_t *)calloc(1, sizeof(nbl_shm_t));
		if (!shrmem) {
			return NBL_ERR_NO_MEMORY;
		}

		shrmem->root_handle = INVALID_IPC_HANDLE;
		shrmem->handle = *handle;
		shrmem->buf_size = param_shrmem.buf_size;
		shrmem->data_size = param_shrmem.data_size;
		// TODO read from param_shrmem.
		shrmem->shmtype = NBL_SYS_INFO_SHMTYPE_INVALID;

		// Delegate handle to nbl_shm_t.
		*handle = 0;
	}

	*out_shrmem = shrmem;

	return err;
}

int nbl_stm_reset(nbl_stm_t *stream) {
	int err = NBL_NO_ERROR;

	if (!stream) {
		return NBL_ERR_INVALID_ARGS;
	}

	stream->handle_index = 0;
	stream->msg_buf_offset = sizeof(nbl_stm_header_t);
	stream->msg_index = 0;

	return err;
}

void *nbl_stm_get_msg_buf(nbl_stm_t *stream, int msg_index) {
	void *msg_buf = NULL;

	if (stream && stream->ipc_msg.iov) {
		msg_buf = stream->ipc_msg.iov[0].base;
	}

	return msg_buf;
}
void *nbl_stm_get_handle_buf(nbl_stm_t *stream) {
	void *handle_buf = NULL;

	if (stream) {
		handle_buf = stream->ipc_msg.handles;
	}

	return handle_buf;
}
