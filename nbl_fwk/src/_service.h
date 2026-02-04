// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NBL_INTERNAL_SERVICE_H_
#define NBL_INTERNAL_SERVICE_H_

#include <trusty_std.h>
#include <lk/list.h>
#include <zircon/types.h>
#include <nbl_fwk/service.h>

#define MAX_USER_HANDLES     64    /* max number of user handles */

#ifdef DEBUG
enum {
	NBL_DEBUG_STATE_NONE = 0,
	NBL_DEBUG_STATE_START_LEAK_DETECT = 1,
	NBL_DEBUG_STATE_STOP_LEAK_DETECT = 2,
};
#endif

typedef void (*nbl_fwk_event_handler_proc_t) (void *priv, const uevent_t *ev);
struct nbl_app;

typedef struct nbl_fwk_event_handler {
	nbl_fwk_event_handler_proc_t proc;
	void *priv;
} nbl_fwk_event_handler_t;

typedef struct nbl_chn {
	handle_t channel;
	nbl_uuid_t uuid;
	nbl_fwk_event_handler_t channel_handler;
	void *msg_buf;
	handle_t *handle_buf;
	nbl_stm_t *stm;
	nbl_srv_t *srv;
	void *priv;
	int auto_ack;
	list_node_t node;
} nbl_chn_t;

typedef struct nbl_srv_state {
	handle_t port;
	nbl_fwk_event_handler_t port_handler;
	list_node_t channels;
} nbl_srv_state_t;

typedef struct nbl_srv {
	char *name;
	struct nbl_app *app;
	uint32_t msg_num;
	uint32_t msg_size;
	uint32_t port_flags;
	nbl_conn_handler_t on_handle_port;
	nbl_chn_buf_handler_t on_handle_channel;
	nbl_chn_stm_handler_t on_handle_channel_cmd;
	nbl_chn_shm_handler_t on_handle_channel_shm;
	nbl_chn_open_handler_t on_handle_channel_open;
	nbl_chn_close_handler_t on_handle_channel_close;
	nbl_srv_state_t state;
	list_node_t node;
#ifdef DEBUG
	uint32_t debug_state;
#endif
} nbl_srv_t;

typedef struct nbl_app {
	int stopped;
	// Handle set for waiting for all event from ports and channels in application.
	handle_t hset;
	list_node_t services;
} nbl_app_t;

int nbl_fwk_wait_for_handle(handle_t handle);

/* SharedMemory definitions and strcuts MUST align with HIDL Tee.cpp */
#define NBL_SHM_MSG_MAGIC        0x4d48534e /* 'NSHM' */
#define NBL_SHM_MSG_FLAG_RO      0          /* registerSharedMemory() with writable = false */
#define NBL_SHM_MSG_FLAG_RW      1          /* registerSharedMemory() with writable = true  */
typedef struct nbl_shm_msg {
	uint32_t magic;
	uint64_t size;
	uint64_t flags;
} __attribute__((packed)) nbl_shm_msg_t;

static inline bool nbl_shm_check_header(void* data, size_t size) {
	return (data && size == sizeof(nbl_shm_msg_t) &&
			((nbl_shm_msg_t*)data)->magic == NBL_SHM_MSG_MAGIC)
				? NBL_NO_ERROR
				: NBL_ERR_CRC_FAIL;
}

#endif /* NBL_INTERNAL_SERVICE_H_ */
