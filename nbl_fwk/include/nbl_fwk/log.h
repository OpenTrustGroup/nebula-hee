// Copyright 2019 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NBL_LOG_H_
#define NBL_LOG_H_

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \nbl_group{ nbl_fwk_hee }

enum nbl_log_level {
  NBL_LOG_DEBUG = 0,
  NBL_LOG_INFO,
  NBL_LOG_WARN,
  NBL_LOG_ERROR,
  NBL_LOG_NUM_SEVERITIES,
};

#define NBL_DEFAULT_LOG_LEVEL NBL_LOG_INFO

struct nbl_log_settings {
  int min_log_level;
};

extern struct nbl_log_settings g_nbl_log_settings;

const char* nbl_log_strip_path(const char* path);
const char* nbl_log_get_severity_name(int log_level);

static inline void nbl_set_log_level(int min_log_level) {
  g_nbl_log_settings.min_log_level = min_log_level;
}

static inline int nbl_get_log_level(void) {
  return g_nbl_log_settings.min_log_level;
}

#define nbl_log_level_enabled(log_level) \
  (log_level >= g_nbl_log_settings.min_log_level)

#define NBL_LOG_PRINT(log_level, fmt, ...)  \
  do {                                      \
    if (nbl_log_level_enabled(log_level)) { \
      fprintf(stderr, fmt, ##__VA_ARGS__);  \
    }                                       \
  } while (0)

#define NBL_LOG(log_level, fmt, ...)                                  \
  do {                                                                \
    if (nbl_log_level_enabled(log_level)) {                           \
      fprintf(stderr, "[%s: %s: %s(%d)] " fmt,                        \
              nbl_log_get_severity_name(log_level), LOG_TAG,          \
              nbl_log_strip_path(__FILE__), __LINE__, ##__VA_ARGS__); \
    }                                                                 \
  } while (0)

#define NBL_LOGD(fmt, ...) NBL_LOG(NBL_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define NBL_LOGI(fmt, ...) NBL_LOG(NBL_LOG_INFO, fmt, ##__VA_ARGS__)
#define NBL_LOGW(fmt, ...) NBL_LOG(NBL_LOG_WARN, fmt, ##__VA_ARGS__)
#define NBL_LOGE(fmt, ...) NBL_LOG(NBL_LOG_ERROR, fmt, ##__VA_ARGS__)

#define NBL_HEXDUMP(data, data_size)                           \
  do {                                                         \
    if (nbl_log_level_enabled(NBL_LOG_DEBUG)) {                \
      uint8_t* p = (uint8_t*)data;                             \
      for (size_t count = 0; count < data_size; count += 16) { \
        for (size_t i = count; i < count + 16; i++) {          \
          if (i < data_size) {                                 \
            fprintf(stderr, "%02x ", p[i]);                    \
          }                                                    \
        }                                                      \
        fprintf(stderr, "\n");                                 \
      }                                                        \
    }                                                          \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NBL_LOG_H_ */
