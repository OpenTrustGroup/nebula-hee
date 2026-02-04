// Copyright 2018 GoldenRiver Technology Co., Ltd.
// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "lib/fxl/files/file.h"
#include "third_party/rapidjson/rapidjson/document.h"

#include <nbl_fwk/log.h>

#include "launcher/gnapp_launcher_delegate.h"

#define LOG_TAG "LORDER_CONFIG"

constexpr char kApps[] = "apps";
constexpr char kLoaderConfigFile[] = "/system/data/grt_loader/apps.config";
constexpr char kVendorConfigFile[] = "/system/vendor/config/apps.config";

static void load_app_config(const char *config_file, component::GnappLauncherDelegate *launcher_delegate) {
  std::string data;
  rapidjson::Document document;
  std::string url;

  files::ReadFileToString(config_file, &data);
  document.Parse(data);
  if (document.HasParseError()) {
    NBL_LOGE("Can't parse the grt loader config file %s.\n", config_file);
    return;
  }

  auto apps_it = document.FindMember(kApps);
  if (apps_it != document.MemberEnd()) {
    const auto &value = apps_it->value;
    if (!value.IsArray()) {
      NBL_LOGE("Trusty loader config file format error.\n");
      return;
    }
    for (const auto &app : value.GetArray()) {
      if (app.IsString()) {
        url = app.GetString();
        launcher_delegate->LaunchApplication(url.c_str(), NULL, 0, 0, 0, 0);
      } else {
        NBL_LOGE("Now trusty loader don't support arguments.\n");
      }
    }
  }
}

void start_app_by_config(component::GnappLauncherDelegate *launcher_delegate) {
  load_app_config(kLoaderConfigFile, launcher_delegate);
  load_app_config(kVendorConfigFile, launcher_delegate);
}
