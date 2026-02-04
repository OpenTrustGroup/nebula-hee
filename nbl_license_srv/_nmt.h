// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef HEE_TRUSTY_NBL_LICENSE_SRV__NMT_H_
#define HEE_TRUSTY_NBL_LICENSE_SRV__NMT_H_
#include <stdint.h>
#include <nbl_fwk/stream.h>
#include <nbl_fwk/service.h>

#include "lib/include/nbl_fwk/nmt_tee.h"

#define LICENSE_NMT_TEE_PORT "com.grt.license.srv.nmt_tee"
#define NMT_UUID_STR_LEN 36

#define NMT_API_VERSION 0x2
#define NMT_DEVICE_ID_SIZE 32
#define NMT_SIGNATURE_MAX_SIZE 256
#define NMT_DIGEST_MAX_SIZE 32
#define NMT_RSA_KEY_MAX_SIZE 2048
#define NMT_MAX_LICENSE_CODE_LEN 23
#define NMT_LICENSE_CODE_MASK ((0x1 << NMT_MAX_LICENSE_CODE_LEN) - 1)
#define NMT_MAX_LICENSE_CODE_DECIMAL_LEN 10
#define NMT_MAX_TRY_COUNT 10
#define NMT_FILENAME_MAX_LEN 256
#define NMT_MAX_TRANSFER_DATA_SIZE 3072

#define NMT_MAGIC_DEVICE_OPEN 0x99479bfef5826ec0ull
#define NMT_MAGIC_DEVICE_CLOSE 0xa5071783dcd8bd4eull

#define NMT_MAGIC_TRIAL_LICENSE 0x275c361a51caull

struct nmt_nebula_certs_v1 {
	uint8_t actk_cert[NMT_RSA_KEY_MAX_SIZE];
	uint32_t actk_size;
	uint8_t cmdk_cert[NMT_RSA_KEY_MAX_SIZE];
	uint32_t cmdk_size;
	uint32_t format;
};

struct nmt_license_req_v1 {
	uint32_t vid;
	uint32_t pid;
	uint8_t device_id[NMT_DEVICE_ID_SIZE];
};

struct nmt_license_v1 {
	uint8_t license[NMT_SIGNATURE_MAX_SIZE];
};

struct nmt_rsa_pub_v1 {
	uint32_t vid;
	uint32_t pid;
	uint8_t device_id[NMT_DEVICE_ID_SIZE];
	uint8_t rsa_pub[NMT_RSA_KEY_MAX_SIZE];
	uint8_t signature[NMT_SIGNATURE_MAX_SIZE];
};

typedef struct nmt_nebula_certs_v1 nmt_nebula_certs_t;
typedef struct nmt_license_v1 nmt_license_t;
typedef struct nmt_license_req_v1 nmt_license_req_t;
typedef struct nmt_rsa_pub_v1 nmt_rsa_pub_t;

enum {
	NMT_CRYPTO_ALGORITHM_NULL = 0,
	NMT_CRYPTO_ALGORITHM_RSA2048,
	NMT_CRYPTO_ALGORITHM_AES256,
	NMT_CRYPTO_ALGORITHM_SHA256
};

enum {
	NMT_CMD_NOP = 0,
	NMT_CMD_API_VERSION,
	NMT_CMD_ACT_PREPARE,
	NMT_CMD_ACT_COMMIT,
	NMT_CMD_KEYGEN,
	NMT_CMD_DATA_INJECT,
	NMT_CMD_ACT_MACHINE_CODE,
	NMT_CMD_ACT_LICENSE_CODE,
	NMT_CMD_ACT_STATUS,
	NMT_CMD_CLOSE_DEVICE,
	NMT_CMD_IS_DEVICE_CLOSE,
	NMT_CMD_ACT_FEATURE,
};

enum {
	NMT_TEE_CMD_NOP = 0,
	NMT_TEE_CMD_LOAD_INJECT_DATA,
};

struct nmt_cmd_req_v2 {
    uint32_t version;
    uint32_t cmd;
    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t product;
    uint32_t reserve[2];
};

struct nmt_cmd_rsp_v2 {
    uint32_t version;
    int32_t error;
    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t reserve[2];
};

struct nmt_data_inject_param_v1 {
	uint64_t flags;
	uint32_t slot;
	uint32_t dest_off;
	uint32_t data_off;
	uint32_t data_size;
	uint8_t uuid_str[NMT_UUID_STR_LEN + 1];
};

typedef struct nmt_data_inject_param_v1 nmt_data_inject_param_t;
typedef struct nmt_cmd_req_v2 nmt_cmd_req_t;
typedef struct nmt_cmd_rsp_v2 nmt_cmd_rsp_t;

int nmt_api_version();
int nmt_act_prepare(nmt_nebula_certs_t *cert, uint64_t flags,
		nmt_license_req_t **out_req);
int nmt_act_commit(const nmt_license_t *license);
int nmt_keygen(const uint8_t *ta_uuid, int slot, uint64_t flags, int alg,
		uint8_t **out_data, uint32_t *out_data_size);
int nmt_handle_cmd_ree(const nmt_cmd_req_t *req, int req_size,
		nmt_cmd_rsp_t **out_rsp, int *out_rsp_size);
int nmt_load_inject_data(const char *uuid, int slot,
		uint64_t flags, void *data, int size, int src_offset);
void start_ahyp_init_thread();
void stop_ahyp_init_thread();
#endif /* HEE_TRUSTY_NBL_LICENSE_SRV__NMT_H_ */
