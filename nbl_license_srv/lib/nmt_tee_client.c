#include <nbl_fwk/service.h>
#include <assert.h>
#include "../_nmt.h"
#include "../command.h"

#define LOG_TAG "nmt_tee_client"

static void *_init_nmt_req(void *buf_ptr, uint32_t cmd, uint32_t product,
		const uint8_t *data, uint32_t size) {
	void *next = NULL;
	nmt_cmd_req_t *nmt_req = (nmt_cmd_req_t *)buf_ptr;
	nmt_req->version = NMT_API_VERSION;
	nmt_req->cmd = cmd;
	nmt_req->product = product;
	nmt_req->payload_offset = sizeof(nmt_cmd_req_t);
	nmt_req->payload_size = size;

	next = nmt_req + 1;

	if (data && size) {
		void *dst_data = next;
		memcpy(dst_data, data, size);
		next = (char *)next + size;
	}

	return next;
}

static int _nbl_act_license_status(uint32_t cmd, int product) {
	int rc, rc_hee = NBL_ERR_GENERIC;
	nbl_chn_t *chn = NULL;
	nbl_stm_t *cmd_request = NULL;
	nbl_stm_t *cmd_response = NULL;
	void *buf_ptr = NULL;
	int buf_size = 0;
	nmt_cmd_rsp_t *nmt_rsp = NULL;

	rc = nbl_chn_open(LICENSE_FILE_PORT, NBL_TIMEOUT_500_MS, &chn);
	if (rc < 0) {
		NBL_LOGE("Failed to connect to service\"%s\" (%d)\n", LICENSE_FILE_PORT,
				rc);
		return NBL_ERR_NOT_READY;
	}

	cmd_request = nbl_stm_create();
	nbl_stm_add_int(cmd_request, GRT_LICENSE_SEND_NMT_DATA);
	nbl_stm_add_buf_ptr(cmd_request, sizeof(nmt_cmd_req_t), &buf_ptr);

	_init_nmt_req(buf_ptr, cmd, product, NULL, 0);

	cmd_response = nbl_stm_create();

	rc = nbl_chn_write_stm_sync(chn, cmd_request, cmd_response,
			NBL_TIMEOUT_500_MS);
	if (rc != NBL_NO_ERROR) {
		NBL_LOGE("Failed to write or read info of HA. (%d)\n", rc);
		rc = NBL_ERR_CHANNEL_CLOSED;
		goto release;
	}

	nbl_stm_next_int(cmd_response, &rc_hee);
	if (rc_hee != NBL_NO_ERROR) {
		NBL_LOGE("Handle cmd failed (%d)\n", rc_hee);
		rc = NBL_ERR_GENERIC;
		goto release;
	}

	nbl_stm_next_buf_ptr(cmd_response, &buf_size, &buf_ptr);

	assert((size_t)buf_size >= sizeof(nmt_cmd_rsp_t));

	nmt_rsp = (nmt_cmd_rsp_t *)buf_ptr;
	rc = nmt_rsp->error;

	if ((size_t)buf_size > sizeof(nmt_cmd_rsp_t)) {
		// Handle additional data if needed
		void *data_ptr = (char *)buf_ptr + sizeof(nmt_cmd_rsp_t);
		// Process data_ptr as needed
		NBL_LOGI("Received message: %s\n", (char*)data_ptr);
	}

release:
	nbl_stm_destroy(cmd_request);
	nbl_stm_destroy(cmd_response);
	nbl_chn_close(chn);

	return rc;
}

int nbl_act_status(int product) {
	return _nbl_act_license_status(NMT_CMD_ACT_STATUS, product);
}

int nbl_act_feature(int feature) {
	return _nbl_act_license_status(NMT_CMD_ACT_FEATURE, feature);
}

int nbl_plt_load_inject_data(int slot, uint64_t flags, void *data, int size,
		int src_offset) {
	int rc, rc_hee = NBL_ERR_GENERIC;
	nbl_chn_t *chn = NULL;
	nbl_stm_t *cmd_request = NULL;
	nbl_stm_t *cmd_response = NULL;
	nbl_shm_t *shm = NULL;
	void *shm_base = NULL;

	rc = nbl_chn_open(LICENSE_NMT_TEE_PORT, NBL_TIMEOUT_500_MS, &chn);
	if (rc < 0) {
		NBL_LOGE("Failed to connect to service\"%s\" (%d)\n",
				LICENSE_NMT_TEE_PORT, rc);
		return NBL_ERR_NOT_READY;
	}

	if (size > 0) {
		rc = nbl_shm_alloc(chn, size, NBL_SHM_PERM_RW, &shm, &shm_base);
		if (rc != NBL_NO_ERROR) {
			NBL_LOGE("Failed to alloc shm (%d)\n", rc);
			goto release;
		}
	}

	cmd_request = nbl_stm_create();
	nbl_stm_add_int(cmd_request, NMT_TEE_CMD_LOAD_INJECT_DATA);
	nbl_stm_add_int(cmd_request, slot);
	nbl_stm_add_buf(cmd_request, sizeof(flags), &flags);
	nbl_stm_add_int(cmd_request, size);
	nbl_stm_add_int(cmd_request, src_offset);
	nbl_stm_add_shm(cmd_request, shm);

	cmd_response = nbl_stm_create();

	rc = nbl_chn_write_stm_sync(chn, cmd_request, cmd_response,
			NBL_TIMEOUT_500_MS);
	if (rc != NBL_NO_ERROR) {
		NBL_LOGE("Failed to write or read info of TA (%d).\n", rc);
		rc = NBL_ERR_IO;
		goto release;
	}

	nbl_stm_next_int(cmd_response, &rc_hee);
	if (rc_hee <= 0) {
		goto release;
	}

	if (data && shm_base) {
		memcpy(data, shm_base, rc_hee);
	}

	rc = rc_hee;

release:
	nbl_shm_free(shm);
	nbl_stm_destroy(cmd_request);
	nbl_stm_destroy(cmd_response);
	nbl_chn_close(chn);

	return rc;
}

int nbl_plt_is_device_close() {
	int rc, rc_hee = NBL_ERR_GENERIC;
	nbl_chn_t *chn = NULL;
	nbl_stm_t *cmd_request = NULL;
	nbl_stm_t *cmd_response = NULL;
	void *buf_ptr = NULL;
	int buf_size = 0;
	nmt_cmd_rsp_t *nmt_rsp = NULL;

	rc = nbl_chn_open(LICENSE_FILE_PORT, NBL_TIMEOUT_500_MS, &chn);
	if (rc < 0) {
		NBL_LOGE("Failed to connect to service\"%s\" (%d)\n", LICENSE_FILE_PORT,
				rc);
		return NBL_ERR_NOT_READY;
	}

	cmd_request = nbl_stm_create();
	nbl_stm_add_int(cmd_request, GRT_LICENSE_SEND_NMT_DATA);
	nbl_stm_add_buf_ptr(cmd_request, sizeof(nmt_cmd_req_t), &buf_ptr);

	_init_nmt_req(buf_ptr, NMT_CMD_IS_DEVICE_CLOSE, NMT_ACT_PRODUCT_BASIC, NULL,
			0);

	cmd_response = nbl_stm_create();

	rc = nbl_chn_write_stm_sync(chn, cmd_request, cmd_response,
			NBL_TIMEOUT_500_MS);
	if (rc != NBL_NO_ERROR) {
		NBL_LOGE("Failed to write or read info of HA. (%d)\n", rc);
		rc = NBL_ERR_CHANNEL_CLOSED;
		goto release;
	}

	nbl_stm_next_int(cmd_response, &rc_hee);
	if (rc_hee != NBL_NO_ERROR) {
		NBL_LOGE("Handle cmd failed (%d)\n", rc_hee);
		rc = NBL_ERR_GENERIC;
		goto release;
	}

	nbl_stm_next_buf_ptr(cmd_response, &buf_size, &buf_ptr);

	assert(buf_size == sizeof(nmt_cmd_rsp_t));

	nmt_rsp = (nmt_cmd_rsp_t *)buf_ptr;
	rc = nmt_rsp->error;

release:
	nbl_stm_destroy(cmd_request);
	nbl_stm_destroy(cmd_response);
	nbl_chn_close(chn);

	return rc;
}
