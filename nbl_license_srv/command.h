// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.

#pragma once

#include <stdint.h>

#define LICENSE_FILE_PORT "com.grt.license.srv.license_file"
#define LICENSE_NOTIFY_PORT "com.grt.license.srv.notify"

enum {

	// REE send license file to HEE
	GRT_LICENSE_SEND_FILE_DATA = 1,
	GRT_LICENSE_SEND_NMT_DATA = 2
};
