#include <nbl_fwk/service.h>
#include <nbl_fwk/common.h>
#include <nbl_fwk/debug.h>

#include <lib/zx/tipc_chan.h>

#include <zircon/types.h>
#include <trusty_std.h>

#define LOG_TAG "lifecycle_launcher_client"

static const char kLifecycleLauncherService[] =
	"com.nebula.grtloader.lifecycle_launcher";

typedef struct __attribute__((packed)) launcher_request {
	nbl_uuid_t uuid;
	int64_t timeout_ms;
} launcher_request_t;

int nbl_ta_open(nbl_uuid_t uuid, long timeout_ms,
		nbl_ta_handle_t *out_handle)
{
	if (!out_handle) {
		return NBL_ERR_INVALID_ARGS;
	}

	zx::tipc_chan tipc_chan;
	zx_status_t status = zx::tipc_chan::create(
			    IPC_CONNECT_ASYNC, kLifecycleLauncherService,
			    sizeof(kLifecycleLauncherService), &tipc_chan);
	if (status != ZX_OK) {
		NBL_LOGE("failed to connect lifecycle launcher service: %d\n", status);
		return NBL_ERR_NOT_SUPPORTED;
	}

	launcher_request_t req = {
		.uuid = uuid,
		.timeout_ms = timeout_ms,
	};
	status = tipc_chan.write(&req, sizeof(req));
	if (status != ZX_OK) {
		NBL_LOGE("failed to write launch request: %d\n", status);
		return NBL_ERR_IO;
	}

	zx_signals_t pending;
	status = tipc_chan.wait_one(IPC_HANDLE_POLL_MSG, zx::time::infinite(), &pending);
	if (status != ZX_OK && pending != IPC_HANDLE_POLL_MSG) {
		NBL_LOGE("failed to wait launch response: %d\n", status);
		return NBL_ERR_IO;
	}

	uint32_t resp;
	status = tipc_chan.read(&resp, sizeof(resp));
	if (status != ZX_OK) {
		NBL_LOGE("failed to read launch response: %d\n", status);
		return NBL_ERR_IO;
	}

	if (resp != NBL_NO_ERROR) {
		return resp;
	}

	*out_handle = tipc_chan.release();
	return NBL_NO_ERROR;
}

void nbl_ta_close(nbl_ta_handle_t handle)
{
	zx_handle_close(handle);
}