// Copyright 2018 GoldenRiver Technology Co., Ltd.
// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>
#include <stdbool.h>

#include <nbl_fwk/common.h>
#include <nbl_fwk/service.h>
#include <nbl_fwk/stream.h>

__BEGIN_CDECLS

int loader_start_app_by_uuid(const nbl_uuid_t *uuid, const char *arg,
                             bool wait_for_end);

__END_CDECLS