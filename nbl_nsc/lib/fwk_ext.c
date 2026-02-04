// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#include "../../nbl_nsc/lib/fwk_ext.h"

#include <errno.h>
#include <sys/types.h>

#include "../../nbl_fwk/src/_service.h"
#include "trusty_ipc.h"
#include "trusty_std.h"

#define LOG_TAG "nbl_nsc.lib.fwk_ext"

// same with "//third_party/hee-trusty/nbl_fwk/src/service.c"
static void _inner_dispatch_event(const uevent_t* ev) {
  if (ev->event == IPC_HANDLE_POLL_NONE) {
    /* not really an event, do nothing */
    NBL_LOGI("got an empty event\n");
    return;
  }

  if (ev->handle == INVALID_IPC_HANDLE) {
    /* not a valid handle  */
    NBL_LOGI("got an event (0x%x) with invalid handle (%d)", ev->event,
             ev->handle);
    return;
  }

  /* check if we have handler */
  nbl_fwk_event_handler_t* handler = ev->cookie;
  if (handler && handler->proc) {
    /* invoke it */
    handler->proc(handler->priv, ev);
    return;
  }

  /* no handler? close it */
  NBL_LOGI("no handler for event (0x%x) with handle %d\n", ev->event,
           ev->handle);
  _trusty_close(ev->handle);

  return;
}

int _app_wait_channel(nbl_app_t* app, nbl_srv_t* srv, uint64_t timeout_msecs) {
  int rc = 0;
  uevent_t event;

  int port_id = srv->state.port;

  if (!app) {
    NBL_LOGI("service context null\n");
    return NBL_ERR_INVALID_ARGS;
  }

  /* handle events */
  while (!app->stopped) {
    event.handle = INVALID_IPC_HANDLE;
    event.event = 0;
    event.cookie = NULL;
    rc = _trusty_wait(port_id, &event, timeout_msecs);
    if (rc == NBL_ERR_TIMED_OUT) {
      NBL_LOGE("Wait for proxy connection, but timeout.\n");
      rc = ETIMEDOUT;
      break;
    } else if (rc < 0) {
      NBL_LOGI("wait_any failed (%d)\n", rc);
      continue;
    }
    if (rc == NBL_NO_ERROR) { /* got an event */
      _inner_dispatch_event(&event);
    }
  }
  return rc;
}
