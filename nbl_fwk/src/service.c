// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#include <string.h>

#include <nbl_fwk/service.h>
#include <nbl_fwk/common.h>
#include <nbl_fwk/debug.h>

#include <assert.h>
#include <uapi/err.h>
#include <stdlib.h>

#include <zircon/types.h>
#include <zircon/syscalls.h>

#include "_stream.h"
#include "_service.h"
#include "_sysinfo.h"

#define LOG_TAG "trusty_fwk_srv"

#ifdef DEBUG
extern void nbl_start_trace_heap(void);
extern int nbl_end_trace_heap(int try_fix);
static int s_debug_state = NBL_DEBUG_STATE_NONE;
#endif

static void handle_port_event (void *priv, const uevent_t *ev);
static int add_ipc_handle_2_hset(nbl_app_t* app, handle_t handle, void* cookie);

#define ALIGN_TO(value, align) (((value) + (align) - 1) & ~((align) - 1))
#define PAGE_ALIGN(value) ALIGN_TO((value), PAGE_SIZE)

static nbl_shm_t *_alloc_shrmem(handle_t shrmem_handle, size_t shrmem_size) {
	nbl_shm_t *shrmem = (nbl_shm_t *)calloc(1, sizeof(nbl_shm_t));
	if (shrmem) {
		shrmem->root_handle = INVALID_IPC_HANDLE;
		shrmem->handle = shrmem_handle;
		shrmem->buf_size = PAGE_ALIGN(shrmem_size);
		shrmem->data_size = shrmem_size;
		shrmem->shmtype = NBL_SYS_INFO_SHMTYPE_INVALID;
	}
	return shrmem;
}

static uint64_t _to_nbl_shm_perm(uint64_t flags) {
	if (flags == NBL_SHM_MSG_FLAG_RW)
		return NBL_SHM_PERM_RW;
	return NBL_SHM_PERM_RO;
}

/*
 *  Close service port.
 */
static void close_port(nbl_srv_t *srv) {
	nbl_srv_state_t *state;

	if (!srv) {
		NBL_LOGI("non-null service expected\n");
		return;
	}

	state = &srv->state;

	/* close port */
	if (state->port != INVALID_IPC_HANDLE) {
		int rc = _trusty_close(state->port);
		if (rc != NO_ERROR) {
			NBL_LOGI("Failed (%d) to close port %d\n",
				   rc, state->port);
		}
		state->port = INVALID_IPC_HANDLE;
	}
}

static int reg_cookie(nbl_srv_t *srv) {
	int rc;

	if (!srv) {
		NBL_LOGI("null services specified\n");
		return ERR_INVALID_ARGS;
	}

	/* attach handler to port handle */
	srv->state.port_handler.proc = handle_port_event;
	srv->state.port_handler.priv = srv;
	rc = _trusty_set_cookie(srv->state.port, &srv->state.port_handler);
	if (rc < 0) {
		NBL_LOGI("Failed (%d) to set cookie on port %d\n",
			  rc, srv->state.port);
		return rc;
	}

	rc = add_ipc_handle_2_hset(srv->app, srv->state.port, &srv->state.port_handler);
	if (rc < 0) {
		return rc;
	}

	return NO_ERROR;
}

static int open_port(nbl_srv_t *srv, nbl_app_t *app) {
	int rc;

	if (!srv) {
		NBL_LOGI("null services specified\n");
		return ERR_INVALID_ARGS;
	}

	/* create port */
	rc = _trusty_port_create(srv->name, srv->msg_num, srv->msg_size,
				 srv->port_flags);
	if (rc < 0) {
		NBL_LOGI("Failed (%d) to create port\n", rc);
		return rc;
	}

	/* setup port state  */
	srv->state.port = (handle_t)rc;

	srv->app = app;

	return NO_ERROR;
}

/*
 * Close channel
 */
void close_channel(nbl_chn_t *chan)
{
	if (chan) {
		if (chan->srv && chan->srv->on_handle_channel_close) {
			chan->srv->on_handle_channel_close(chan);
		}

		if (chan->channel != INVALID_IPC_HANDLE) {
			int rc = _trusty_close(chan->channel);
			if (rc != NO_ERROR) {
				NBL_LOGI("Failed (%d) to close channel %d\n",
								   rc, chan->channel);
			}
			chan->channel = INVALID_IPC_HANDLE;
		}

		chan->msg_buf = 0;
		chan->handle_buf = 0;

		if (chan->stm) {
			nbl_stm_destroy(chan->stm);
			chan->stm = NULL;
		}

		if (list_in_list(&chan->node)) {
			list_delete(&chan->node);
		}
		free(chan);
	}
}

/*
 *  Restart specified service
 */
static int restart_service(nbl_srv_t *srv)
{
	int rc;

	if (!srv) {
		NBL_LOGI("non-null service expected\n");
		return ERR_INVALID_ARGS;
	}

	close_port(srv);

	rc = open_port(srv, srv->app);
	if (rc) {
		NBL_LOGI("Restarting service open port (%s) failed\n", srv->name);
		goto err_open_port;
	}

	rc = reg_cookie(srv);
	if (rc) {
		NBL_LOGI("Restarting service reg port (%s) cookie failed\n", srv->name);
		goto err_reg_cookie;
	}

	return NO_ERROR;

err_reg_cookie:
err_open_port:
	close_port(srv);
	return rc;
}

static int handle_channel_msg(nbl_chn_t *chan, const uevent_t *ev)
{
	int err = NO_ERROR;
	int rc;
	ipc_msg_info_t inf;

	/* for all messages */
	for (;;) {
		/* get message */
		rc = _trusty_get_msg(ev->handle, &inf);
		if (rc == ERR_NO_MSG)
			break; /* no new messages */

		if (rc != NO_ERROR) {
			NBL_LOGI("failed (%d) to get_msg for chan (%d)\n",
				  rc, ev->handle);
			return rc;
		}

		if (inf.len > chan->srv->msg_size || inf.len > NBL_SRV_MAX_BUF_SIZE) {
			NBL_LOGI("%s: message too large %ld\n", __func__, inf.len);
			_trusty_put_msg(ev->handle, inf.id);
			return ERR_NOT_ENOUGH_BUFFER;
		}

		/* read msg content */
		iovec_t iov = {
			.base = chan->msg_buf,
			.len = inf.len,
		};
		ipc_msg_t msg = {
			.iov = &iov,
			.num_iov = 1,
			.handles = chan->handle_buf,
			.num_handles = inf.num_handles
		};

		rc = _trusty_read_msg(ev->handle, inf.id, 0, &msg);
		_trusty_put_msg(ev->handle, inf.id);
		if (rc < 0) {
			NBL_LOGI("failed to read msg (%d, %d)\n", rc, ev->handle);
			return rc;
		}

		if (((size_t) rc) < inf.len) {
			NBL_LOGI("invalid message of size (%d, %d)\n", rc, ev->handle);
			err = ERR_NOT_VALID;
			break;
		}

		if (chan && chan->srv) {
			if (chan->srv->on_handle_channel_shm && inf.num_handles == 1 &&
				nbl_shm_check_header(chan->msg_buf, inf.len) == NBL_NO_ERROR) {
				nbl_shm_msg_t* msg = chan->msg_buf;
				size_t shrmem_size = msg->size;
				nbl_shm_t *shrmem_ptr = _alloc_shrmem(*chan->handle_buf, shrmem_size);
				if (!shrmem_ptr) {
					NBL_LOGE("failed to allocate nbl_shm_t.\n");
					err = ERR_FAULT;
					break;
				}
				uint64_t shrmem_perm = _to_nbl_shm_perm(msg->flags);
				chan->srv->on_handle_channel_shm(chan, shrmem_ptr, shrmem_perm);
			} else if (chan->srv->on_handle_channel) {
				rc = chan->srv->on_handle_channel(chan, chan->msg_buf, inf.len);
				if (rc != NO_ERROR) {
					NBL_LOGI("failed (%d) to handle channel (%d) message\n",
									  rc, ev->handle);
					err = ERR_FAULT;
					break;
				}
			} else if (chan->srv->on_handle_channel_cmd) {
				// TODO use stm ipc_msg directly.
				chan->stm->ipc_msg.num_handles = inf.num_handles;
				nbl_stm_reset(chan->stm);

				rc = nbl_stm_check_header(chan->msg_buf);
				if (rc != NBL_NO_ERROR) {
					NBL_LOGE("failed (%d) to check stream CRC.\n", rc);
					err = rc;
					break;
				}

				rc = chan->srv->on_handle_channel_cmd(chan, chan->stm);
				if (rc != NO_ERROR) {
					NBL_LOGI("failed (%d) to handle channel (%d) cmd\n",
									  rc, ev->handle);
					err = ERR_FAULT;
					break;
				}
			}
		}
	}

	if (nbl_get_auto_poison_cache()) {
	    nbl_poison_cache();
	}

	return err;
}

static void handle_channel_event(void *priv, const uevent_t *ev)
{
	nbl_chn_t *ctx = (nbl_chn_t *)priv;

	if ((ev->event & IPC_HANDLE_POLL_ERROR) ||
		(ev->event & IPC_HANDLE_POLL_SEND_UNBLOCKED)) {
		/* close it as it is in an error state */
		NBL_LOGI("error event (0x%x) for chan (%d)\n",
			   ev->event, ev->handle);
		close_channel(ctx);
		return;
	}

	if (ev->event & IPC_HANDLE_POLL_MSG) {
		if (handle_channel_msg(ctx, ev) != NO_ERROR) {
			NBL_LOGI("failed on handle channel (%d) msg\n",
						   ev->handle);
			close_channel(ctx);
			return;
		}
	}

	if (ev->event & IPC_HANDLE_POLL_HUP) {
#ifdef DEBUG
		int need_stop_trace = s_debug_state == NBL_DEBUG_STATE_STOP_LEAK_DETECT;
#endif
		/* closed by peer */
		close_channel(ctx);
#ifdef DEBUG
		if (need_stop_trace) {
			s_debug_state = NBL_DEBUG_STATE_NONE;
			if(nbl_end_trace_heap(0)) {
				assert(0);
			}
		}
#endif
		return;
	}
}

/*
 *  Handle common port errors
 */
static int handle_port_errors(const uevent_t *ev)
{
	if ((ev->event & IPC_HANDLE_POLL_ERROR) ||
	    (ev->event & IPC_HANDLE_POLL_HUP) ||
	    (ev->event & IPC_HANDLE_POLL_MSG) ||
	    (ev->event & IPC_HANDLE_POLL_SEND_UNBLOCKED)) {
		nbl_srv_t *srv = NULL;
		nbl_fwk_event_handler_t *handler = (nbl_fwk_event_handler_t *)ev->cookie;

		if (handler) {
			srv = (nbl_srv_t *)handler->priv;

			/* recreate service */
			restart_service(srv);
		}

		/* should never happen with port handles */
		NBL_LOGI("error event (0x%x) for port (%d)\n",
		       ev->event, ev->handle);

		return 1;
	}

	return 0;
}

static int add_ipc_handle_2_hset(nbl_app_t* app, handle_t handle, void* cookie)
{
  int rc;
  uevent_t evt = {
      .event = ~0,
  };

  if (app->hset == INVALID_IPC_HANDLE) {
    NBL_LOGE("Handle set is NONE.\n");
    return ERR_BAD_STATE;
  }

  evt.handle = handle;
  evt.cookie = cookie;
  rc = _trusty_handle_set_ctrl(app->hset, HSET_ADD, &evt);
  if (rc < 0) {
    NBL_LOGE("Failed to add handle(%d) to handle set(%d), err=%d\n", handle, app->hset, rc);
    return rc;
  }

  return NO_ERROR;
}

static void handle_port_event (void *priv, const uevent_t *ev)
{
	nbl_srv_t *srv = (nbl_srv_t *)priv;
	nbl_uuid_t peer_uuid;

	if (handle_port_errors(ev))
		return;

	if (ev->event & IPC_HANDLE_POLL_READY) {
		handle_t chan;
		nbl_stm_t *stm;

		/* incomming connection: accept it */
		int rc = _trusty_accept(ev->handle, (struct uuid *)&peer_uuid);
		if (rc < 0) {
			NBL_LOGI("failed (%d) to accept on port %d\n",
				   rc, ev->handle);
			return;
		}
		chan = (handle_t) rc;

#ifdef DEBUG
		if (s_debug_state == NBL_DEBUG_STATE_START_LEAK_DETECT) {
			nbl_start_trace_heap();
			s_debug_state = NBL_DEBUG_STATE_NONE;
		}
#endif

		rc = NO_ERROR;
		if (srv && srv->on_handle_port) {
			rc = srv->on_handle_port(srv, peer_uuid);
			if (rc != NO_ERROR) {
				NBL_LOGI("failed (%d) to on handle port %d\n",
								   rc, ev->handle);
				goto failed_on_handle_port;
			}
		}

		nbl_chn_t *channel_ctx = (nbl_chn_t *)calloc(1, sizeof(nbl_chn_t));
		if (!channel_ctx) {
			NBL_LOGI("failed to alloc channel (%d) context memory\n", chan);
			goto failed_alloc_channel;
		}

		stm = nbl_stm_create();
		if (!stm) {
			NBL_LOGI("failed to alloc channel (%d) stream\n", chan);
			goto failed_alloc_stm;
		}

		channel_ctx->channel = chan;
		channel_ctx->channel_handler.priv = channel_ctx;
		channel_ctx->channel_handler.proc = handle_channel_event;
		channel_ctx->uuid = peer_uuid;
		channel_ctx->stm = stm;
		channel_ctx->msg_buf = nbl_stm_get_msg_buf(stm, 0);
		channel_ctx->handle_buf = nbl_stm_get_handle_buf(stm);
		channel_ctx->srv = srv;
		list_initialize(&channel_ctx->node);

		/* attach it to handle */
		rc = _trusty_set_cookie(chan, &channel_ctx->channel_handler);
		if (rc) {
			NBL_LOGI("failed (%d) to set_cookie on chan %d\n",
				   rc, chan);
			goto failed_set_cookie;
		}

		// Add to handle set.
		rc = add_ipc_handle_2_hset(srv->app, chan, &channel_ctx->channel_handler);
		if (rc < 0) {
			goto failed_add_handle_2_hset;
		}

		list_add_tail(&srv->state.channels, &channel_ctx->node);
		if(srv->on_handle_channel_open) {
		  srv->on_handle_channel_open(channel_ctx);
		}

		return;

failed_add_handle_2_hset:
failed_set_cookie:
		nbl_stm_destroy(stm);
failed_alloc_stm:
		free(channel_ctx);
failed_on_handle_port:
failed_alloc_channel:
		_trusty_close(chan);
	}
}

static void unreg_srv(nbl_srv_t *srv) {
	nbl_srv_state_t *state = &srv->state;

	close_port(srv);

	/* reset port handler */
	state->port_handler.proc = NULL;
	state->port_handler.priv = NULL;

	/* close channels */
	list_node_t *node = NULL;
	list_node_t *temp_node = NULL;
	list_for_every_safe(&state->channels, node, temp_node) {
		nbl_chn_t *ctx = containerof(node, nbl_chn_t, node);
		close_channel(ctx);
	}

	if (list_in_list(&srv->node)) {
		list_delete(&srv->node);
	}
}

/*
 *  Create service
 */
void nbl_srv_destroy(nbl_srv_t *srv)
{

	if (!srv) {
		NBL_LOGI("non-null service expected\n");
		return;
	}

	if (srv->name) {
		free(srv->name);
		srv->name = NULL;
	}

	unreg_srv(srv);

	free(srv);
}

/*
 *  Create service
 */
int nbl_app_reg_srv(nbl_app_t *app, nbl_srv_t *srv)
{
	int rc;

	if (!app) {
		return ERR_INVALID_ARGS;
	}

	rc = open_port(srv, app);
	if (rc) {
		NBL_LOGI("Failed (%d) to open port %s\n", rc, srv->name);
		return rc;
	}

	rc = reg_cookie(srv);
	if (rc) {
		NBL_LOGI("Restarting service reg port (%s) cookie failed\n", srv->name);
		goto err_reg_cookie;
	}

	list_add_tail(&app->services, &srv->node);

	return NO_ERROR;

err_reg_cookie:
	close_port(srv);
	return rc;
}

int nbl_app_unreg_srv(nbl_srv_t *srv) {
	if (!srv) {
		return ERR_INVALID_ARGS;
	}

	// TODO something with app?

	unreg_srv(srv);

	return NO_ERROR;
}

int nbl_chn_write_buf(nbl_chn_t *chan, void *data, uint32_t data_size)
{
	int rc = NO_ERROR;

	if (!chan) {
		return ERR_INVALID_ARGS;
	}

	if (!data) {
		return ERR_INVALID_ARGS;
	}

	if (data_size) {
		iovec_t iov = {
			.base = data,
			.len = data_size,
		};

		ipc_msg_t msg = {
			.iov = &iov,
			.num_iov = 1,
			.num_handles = 0,
			.handles = NULL
		};

		rc = _trusty_send_msg(chan->channel, &msg);
	}

	return rc;
}

int nbl_chn_read_buf(nbl_chn_t *chan, void *data, uint32_t data_size,
		long timeout) {
	int rc = NO_ERROR;
	int err = NO_ERROR;
	int size = 0;
	uevent_t evt;
	ipc_msg_info_t mi;
	iovec_t iov;
	ipc_msg_t msg;

	if (!chan) {
		return ERR_INVALID_ARGS;
	}

	if (!data) {
		return ERR_INVALID_ARGS;
	}

	rc = _trusty_wait(chan->channel, &evt, timeout);
	if (rc) {
		return rc;
	}

	if (!(evt.event & IPC_HANDLE_POLL_MSG)) {
		return ERR_IO;
	}

	rc = _trusty_get_msg(chan->channel, &mi);
	if (rc) {
		return rc;
	}

	iov.base = data;
	iov.len = data_size;
	msg.iov = &iov;
	msg.num_iov = 1;
	msg.handles = NULL;
	msg.num_handles = 0;

	rc = _trusty_read_msg(chan->channel, mi.id, 0, &msg);
	if (rc < 0) {
		err = rc;
		goto read_msg_failed;
	}

	size = rc;

	rc = _trusty_put_msg(chan->channel, mi.id);
	if (rc != NBL_NO_ERROR) {
		return rc;
	}

	return size;

read_msg_failed:
	_trusty_put_msg(chan->channel, mi.id);
	return err;
}

int nbl_chn_write_stm(nbl_chn_t *chan, nbl_stm_t *stream) {
	int rc = NO_ERROR;

	if (!chan) {
		return ERR_INVALID_ARGS;
	}

	if (!stream) {
		return ERR_INVALID_ARGS;
	}

	nbl_stm_set_header(stream->ipc_msg.iov[0].base);

	iovec_t iov = stream->ipc_msg.iov[0];
	iov.len = stream->msg_buf_offset;

	ipc_msg_t msg = {
		.iov = &iov,
		.num_iov = 1,
		.handles = stream->ipc_msg.handles,
		.num_handles = stream->handle_index
	};

	rc = _trusty_send_msg(chan->channel, &msg);
	if (rc < 0) {
		return rc;
	}

	nbl_stm_reset(stream);

	return NBL_NO_ERROR;
}

int nbl_chn_read_stm(nbl_chn_t *chan,
		nbl_stm_t *stream, long timeout) {
	int rc = NO_ERROR;
	int err = NO_ERROR;
	uevent_t evt;
	ipc_msg_info_t mi;
	iovec_t iov;
	ipc_msg_t msg;

	if (!chan) {
		return ERR_INVALID_ARGS;
	}

	if (!stream) {
		return ERR_INVALID_ARGS;
	}

	rc = _trusty_wait(chan->channel, &evt, timeout);
	if (rc) {
		return rc;
	}

	if (!(evt.event & IPC_HANDLE_POLL_MSG)) {
		return ERR_IO;
	}

	rc = _trusty_get_msg(chan->channel, &mi);
	if (rc) {
		return rc;
	}

	if (mi.len > stream->ipc_msg.iov[0].len) {
		err = ERR_OUT_OF_RANGE;
		goto read_out_of_range;
	}

	nbl_stm_reset(stream);

	iov.base = stream->ipc_msg.iov[0].base;
	iov.len = mi.len;
	msg.iov = &iov;
	msg.num_iov = 1;
	msg.handles = stream->ipc_msg.handles;
	msg.num_handles = mi.num_handles;

	rc = _trusty_read_msg(chan->channel, mi.id, 0, &msg);
	if (rc < 0) {
		err = rc;
		goto read_msg_failed;
	}

	stream->ipc_msg.num_handles = mi.num_handles;

	rc = _trusty_put_msg(chan->channel, mi.id);
	if (rc != NBL_NO_ERROR) {
		return rc;
	}

	rc = nbl_stm_check_header(stream->ipc_msg.iov[0].base);
	if (rc != NBL_NO_ERROR) {
		return rc;
	}

	return NBL_NO_ERROR;

read_msg_failed:
read_out_of_range:
	_trusty_put_msg(chan->channel, mi.id);
	return err;
}

/*
 *  Dispatch event
 */
static void dispatch_event(const uevent_t *ev)
{
	assert(ev);

	if (ev->event == IPC_HANDLE_POLL_NONE) {
		/* not really an event, do nothing */
		NBL_LOGI("got an empty event\n");
		return;
	}

	if (ev->handle == INVALID_IPC_HANDLE) {
		/* not a valid handle  */
		NBL_LOGI("got an event (0x%x) with invalid handle (%d)",
		      ev->event, ev->handle);
		return;
	}

	/* check if we have handler */
	nbl_fwk_event_handler_t *handler = ev->cookie;
	if (handler && handler->proc) {
		/* invoke it */
		handler->proc(handler->priv, ev);
		return;
	}

	/* no handler? close it */
	NBL_LOGI("no handler for event (0x%x) with handle %d\n",
	       ev->event, ev->handle);
	_trusty_close(ev->handle);

	return;
}

int nbl_fwk_wait_for_handle(handle_t handle) {
	int rc;
	uevent_t event;

	event.handle = INVALID_IPC_HANDLE;
	event.event = 0;
	event.cookie = NULL;
	rc = _trusty_wait(handle, &event, -1);
	if (rc < 0) {
		NBL_LOGI("wait_for_port failed (%d)", rc);
		return rc;
	}
	if (rc == NO_ERROR) { /* got an event */
		dispatch_event(&event);
	}
	return rc;
}

// hooks to be overriden by libnbl_ta_controller.a
int __WEAK __enter_main_loop(nbl_app_t *app) { return NBL_NO_ERROR; };
void __WEAK __leave_main_loop(nbl_app_t *app) {};

int nbl_app_main_loop(nbl_app_t *app)
{
  int rc;
  uevent_t event;

  if (!app) {
    NBL_LOGI("service context null\n");
    return ERR_INVALID_ARGS;
  }

  rc = __enter_main_loop(app);
  if (rc < 0) {
    return rc;
  }

  /* handle events */
  while (!app->stopped) {
    event.handle = INVALID_IPC_HANDLE;
    event.event = 0;
    event.cookie = NULL;
    rc = _trusty_wait(app->hset, &event, -1);
    if (rc < 0) {
      NBL_LOGE("Failed to wait for trusty event. err=%d.\n", rc);
      continue;
    }
    if (rc == NO_ERROR) { /* got an event */
      dispatch_event(&event);
    }
  }

  __leave_main_loop(app);

  return NO_ERROR;
}

int nbl_chn_open(const char *port_name, long timeout, nbl_chn_t **out_channel) {
	int rc;
	uevent_t evt;
	handle_t chan;
	nbl_chn_t *channel;

	if (!port_name) {
		return ERR_INVALID_ARGS;
	}

	if (!out_channel) {
		return ERR_INVALID_ARGS;
	}

	rc = _trusty_connect(port_name, IPC_CONNECT_ASYNC | IPC_CONNECT_WAIT_FOR_PORT);
	if ((handle_t)rc == INVALID_IPC_HANDLE) {
		return rc;
	}

	chan = (handle_t) rc;

	rc = _trusty_wait(chan, &evt, timeout);
	if (rc) {
		goto wait_failed;
	}

	if (evt.handle != chan) {
		rc = ERR_BAD_STATE;
		goto handle_mismatch_failed;
	}

	if (evt.event & IPC_HANDLE_POLL_HUP) {
		rc = ERR_CHANNEL_CLOSED;
		goto channel_closed;
	}

	if (!(evt.event & IPC_HANDLE_POLL_READY)) {
		rc = ERR_BAD_STATE;
		goto bad_state;
	}

	channel = calloc(1, sizeof(nbl_chn_t));
	if (!channel) {
		rc = ERR_NO_MEMORY;
		goto no_memory;
	}

	channel->channel = chan;
	*out_channel = channel;

	return NO_ERROR;

no_memory:
bad_state:
channel_closed:
handle_mismatch_failed:
wait_failed:
	_trusty_close(chan);
	return rc;
}

void nbl_chn_set_priv(nbl_chn_t *chan, void *priv) {
	chan->priv = priv;
}

void *nbl_chn_get_priv(nbl_chn_t *chan) {
	return chan->priv;
}

nbl_uuid_t nbl_chn_get_peer_uuid(nbl_chn_t *chan) {
	return chan->uuid;
}

static int nbl_chn_write_stm_sync_with_handle(handle_t handle, nbl_stm_t *request, nbl_stm_t *response, uint32_t timeout) {
	int err = NO_ERROR;
	int rc;
	uevent_t evt;
	ipc_msg_info_t mi;
	iovec_t iov;
	ipc_msg_t msg;

	if (handle == INVALID_IPC_HANDLE) {
		return ERR_INVALID_ARGS;
	}

	if (!request || !request->ipc_msg.iov || !request->ipc_msg.num_iov) {
		return ERR_INVALID_ARGS;
	}

	if (request->handle_index > NBL_STM_MAX_SHM_COUNT) {
		return NBL_ERR_BAD_LEN;
	}

	nbl_stm_set_header(request->ipc_msg.iov[0].base);

	iov.base = request->ipc_msg.iov[0].base;
	iov.len = request->msg_buf_offset;
	msg.iov = &iov;
	msg.num_iov = 1;
	msg.handles = request->ipc_msg.handles;
	msg.num_handles = request->handle_index;

	rc = _trusty_send_msg(handle, &msg);
	if (rc < 0) {
		return rc;
	}

	nbl_stm_reset(request);
	nbl_stm_reset(response);

	rc = _trusty_wait(handle, &evt, timeout);
	if (rc) {
		return rc;
	}

	if (!(evt.event & IPC_HANDLE_POLL_MSG)) {
		return ERR_IO;
	}

	rc = _trusty_get_msg(handle, &mi);
	if (rc) {
		return rc;
	}

	if (mi.len > response->ipc_msg.iov[0].len) {
		err = ERR_OUT_OF_RANGE;
		goto read_out_of_range;
	}

	iov.base = response->ipc_msg.iov[0].base;
	iov.len = mi.len;
	msg.iov = &iov;
	msg.num_iov = 1;
	msg.handles = response->ipc_msg.handles;
	msg.num_handles = mi.num_handles;

	rc = _trusty_read_msg(handle, mi.id, 0, &msg);
	if (rc < 0) {
		err = rc;
		goto read_msg_failed;
	}

	response->ipc_msg.num_handles = mi.num_handles;

	rc = _trusty_put_msg(handle, mi.id);
	if (rc != NBL_NO_ERROR) {
		return rc;
	}

	rc = nbl_stm_check_header(response->ipc_msg.iov[0].base);
	if (rc != NBL_NO_ERROR) {
		return rc;
	}

	return NBL_NO_ERROR;

read_msg_failed:
read_out_of_range:
	_trusty_put_msg(handle, mi.id);
	return err;
}

int nbl_chn_write_stm_sync(nbl_chn_t *channel,
		nbl_stm_t *request, nbl_stm_t *response, long timeout) {
	if (!channel) {
		return ERR_INVALID_ARGS;
	}
	return nbl_chn_write_stm_sync_with_handle(channel->channel, request,
			response, timeout);
}

void nbl_chn_close(nbl_chn_t *chan) {
	close_channel(chan);
}

int nbl_srv_create(nbl_srv_desc_t *desc, nbl_srv_t **out_srv) {
	int err = NO_ERROR;
	nbl_srv_t *srv;
	size_t name_len;
	uint32_t port_flags = 0;
	uint32_t scope;

	if (!desc) {
		return ERR_INVALID_ARGS;
	}

	scope = desc->flags & NBL_SRV_FLAG_SCOPE_MASK;

	if (!desc->name) {
		return ERR_INVALID_ARGS;
	}

	if (!desc->buf_size || desc->buf_size > NBL_SRV_MAX_BUF_SIZE) {
		return ERR_INVALID_ARGS;
	}

	if (!desc->buf_num || desc->buf_num > NBL_SRV_DEFAULT_BUF_NUM) {
		return ERR_INVALID_ARGS;
	}

	switch(scope) {
	case NBL_SRV_FLAG_SCOPE_HEE:
		port_flags = IPC_PORT_ALLOW_TA_CONNECT;
		break;
	case NBL_SRV_FLAG_SCOPE_REE:
		port_flags = IPC_PORT_ALLOW_TA_CONNECT | IPC_PORT_ALLOW_NS_CONNECT;
		break;
	case NBL_SRV_FLAG_SCOPE_SECURE_DOMAIN:
	case NBL_SRV_FLAG_SCOPE_TEE:
		return ERR_NOT_IMPLEMENTED;
	default:
		return ERR_NOT_SUPPORTED;
	}

	if ((desc->buf_handler || desc->shm_handler) && desc->stm_handler) {
		return NBL_ERR_CONFLICT_ARGS;
	}

	srv = (nbl_srv_t *)calloc(1, sizeof(nbl_srv_t));
	if (!srv) {
		return ERR_NO_MEMORY;
	}

	name_len = strnlen(desc->name, NBL_SRV_MAX_NAME_LEN);
	srv->name = (char *)calloc(1, name_len + 1);
	if (!srv->name) {
		err = ERR_NO_MEMORY;
		goto alloc_name_failed;
	}
	memcpy(srv->name, desc->name, name_len);
	srv->name[name_len] = '\0';

	srv->msg_num = desc->buf_num;
	srv->msg_size = desc->buf_size;
	srv->port_flags = port_flags;
	srv->state.port = INVALID_IPC_HANDLE;
	srv->on_handle_port = desc->conn_handler;
	srv->on_handle_channel_open = desc->open_handler;
	srv->on_handle_channel = desc->buf_handler;
	srv->on_handle_channel_shm = desc->shm_handler;
	srv->on_handle_channel_cmd = desc->stm_handler;
	srv->on_handle_channel_close = desc->close_handler;
	list_initialize(&srv->state.channels);

	desc->srv = srv;

	if (out_srv) {
		*out_srv = srv;
	}

	return err;

alloc_name_failed:
	nbl_srv_destroy(srv);
	return err;
}

nbl_app_t *nbl_app_create() {
	int rc;
	nbl_app_t *app;

	app = (nbl_app_t *)calloc(1, sizeof(nbl_app_t));
	if (!app) {
		return NULL;
	}

	list_initialize(&app->services);

	// Create a handle set.
	rc = _trusty_handle_set_create();
	if (rc < 0) {
		NBL_LOGE("Failed to create handle set. error=%d\n", rc);
		return NULL;
	}
	app->hset = rc;

	return app;
}

int nbl_app_stop(nbl_app_t *app) {
	if (!app) {
		return ERR_INVALID_ARGS;
	}

	app->stopped = 1;

	return NO_ERROR;
}

void nbl_app_destroy(nbl_app_t* app) {
  if (!app) {
    return;
  }

  app->stopped = 1;

  if (app->hset != INVALID_IPC_HANDLE) {
    _trusty_close(app->hset);
    app->hset = INVALID_IPC_HANDLE;
  }

  free(app);
}

void nbl_start_leak_trace() {
#ifdef DEBUG
	s_debug_state = NBL_DEBUG_STATE_START_LEAK_DETECT;
#endif
}

void nbl_stop_leak_trace() {
#ifdef DEBUG
	s_debug_state = NBL_DEBUG_STATE_STOP_LEAK_DETECT;
#endif
}