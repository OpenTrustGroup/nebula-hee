// Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
//
// The present software is the confidential and proprietary information of
// GoldenRiver Technologies Co., Ltd. You shall not disclose the present
// software and shall use it only in accordance with the terms of the license
// agreement you entered into with GoldenRiver Technologies Co., Ltd.
// This software may be subject to export or import laws in certain countries.

#include "nbl_fwk/uuid.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <nbl_fwk/log.h>

#define UUID_SUB_STR_LEN 32
#define LOG_TAG "trusty_fwk_srv"

static int tolower(int c) {
	if (c >= 'A' && c <= 'Z') {
		return c + 'a' - 'A';
	} else {
		return c;
	}
}

static int htoi(char s[]) {
	int i;
	int n = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		i = 2;
	} else {
		i = 0;
	}
	for (;
			(s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z')
					|| (s[i] >= 'A' && s[i] <= 'Z'); ++i) {
		if (tolower(s[i]) > '9') {
			n = 16 * n + (10 + tolower(s[i]) - 'a');
		} else {
			n = 16 * n + (tolower(s[i]) - '0');
		}
	}
	return n;
}

void _delete_char_from_str(char *str, char target) {
	int i = 0;
	int j = 0;
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] != target) {
			str[j++] = str[i];
		}
	}
	str[j] = '\0';
}

void _substring(char* str, int pos, int length, char* substr) {
	char* pch = str;
	int i;
	pch = pch + pos;
	for (i = 0; i < length; i++) {
		substr[i] = *(pch++);
	}
	substr[length] = '\0';
}
/**
 * Convert string which format is UUID to uuid structure.
 */
void nbl_str_to_uuid(const char *str, nbl_uuid_t *uuid) {
	char substr[UUID_SUB_STR_LEN];
	char *uuid_str = strdup(str);
	_delete_char_from_str(uuid_str, '-');

	memset(substr, 0, sizeof(substr));
	if (strlen(uuid_str) != 32) {
		NBL_LOGE("UUID length error\n");
	} else {
		_substring(uuid_str, 0, 8, substr);
		uuid->time_low = htoi(substr);
		memset(substr, 0, sizeof(substr));

		_substring(uuid_str, 8, 4, substr);
		uuid->time_mid = htoi(substr);
		memset(substr, 0, sizeof(substr));

		_substring(uuid_str, 12, 4, substr);
		uuid->time_high_and_version = htoi(substr);
		memset(substr, 0, sizeof(substr));

		int i = 0;
		for (i = 0; i < 8; i++) {
			_substring(uuid_str, 16 + i * 2, 2, substr);
			uuid->node[i] = htoi(substr);
			memset(substr, 0, sizeof(substr));
		}
	}
	free(uuid_str);
}

void nbl_uuid_to_str(const nbl_uuid_t *uuid, char *uuid_str) {
	sprintf(uuid_str, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			uuid->time_low, uuid->time_mid, uuid->time_high_and_version,
			uuid->node[0], uuid->node[1], uuid->node[2], uuid->node[3],
			uuid->node[4], uuid->node[5], uuid->node[6], uuid->node[7]);
}
