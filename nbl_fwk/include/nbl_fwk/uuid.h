// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.

#ifndef NBL_UUID_H_
#define NBL_UUID_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \nbl_group{ nbl_fwk_hee }

typedef struct nbl_uuid
{
//	uint32_t time_low;
//	uint16_t time_mid;
//	uint16_t time_high_and_version;
//	uint16_t clock_seq;
//	uint8_t node[6];
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_high_and_version;
	uint8_t node[8];
} nbl_uuid_t;

#define NBL_ZERO_UUID \
	{ 0x0, 0x0, 0x0, \
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }

void nbl_uuid_to_str(const nbl_uuid_t *uuid, char *uuid_str);
void nbl_str_to_uuid(const char *str, nbl_uuid_t *uuid);
#ifdef __cplusplus
}
#endif

#endif /* NBL_UUID_H_ */
