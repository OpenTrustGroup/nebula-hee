// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.

#include <nbl_fwk/log.h>

const char* const log_severity_names[NBL_LOG_NUM_SEVERITIES] = {
    "DEBUG", "INFO", "WARN", "ERROR"};

struct nbl_log_settings g_nbl_log_settings = {
    .min_log_level = NBL_DEFAULT_LOG_LEVEL,
};

const char* nbl_log_strip_path(const char* path) {
  char* p = strrchr(path, '/');

  if (p) {
    return p + 1;
  }
  return path;
}

const char* nbl_log_get_severity_name(int log_level) {
  if (log_level >= NBL_LOG_DEBUG && log_level < NBL_LOG_NUM_SEVERITIES) {
    return log_severity_names[log_level];
  }
  return "UNKNOWN";
}
