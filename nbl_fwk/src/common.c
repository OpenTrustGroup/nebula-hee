// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#include <nbl_fwk/common.h>
#include <zircon/types.h>
#include <zircon/syscalls.h>

static int g_auto_poison_cache = 1;

int nbl_api_level() {
	return NBL_API_LEVEL;
}

int nbl_set_cache_protection(uint32_t flags) {
    return zx_set_cache_protection(flags);
}

int nbl_poison_cache() {
    return zx_poison_cache();
}

void nbl_set_auto_poison_cache(int enabled) {
    g_auto_poison_cache = enabled;
}

int nbl_get_auto_poison_cache() {
    return g_auto_poison_cache;
}
