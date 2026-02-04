// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.

#ifndef NBL_COMMON_H_
#define NBL_COMMON_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \nbl_group{ nbl_fwk_hee }

/// Current API level.
#define NBL_API_LEVEL 1

/// Microsecond count of a second.
#define NBL_MSEC 1000000ULL

/// Poison cache at context switching.
#define NBL_CACHE_PROT_FLAG_AUTO = 0x1;

/**
 * \brief Get current API level.
 *
 *
 * \return Current API level.
 */
int nbl_api_level();

/**
 * \brief Set cache protection flags.
 *
 * This flags is used by #nbl_poison_cache.
 *
 * \param[in] flags Cache protection flags.
 * \return #NBL_NO_ERROR if success.
 * \see NBL_CACHE_PROT_FLAG_AUTO
 */
int nbl_set_cache_protection(uint32_t flags);

/**
 * \brief Poison the cache of this process.
 *
 * This operation can prevent from leaking sensitive data which is in cache side
 * channel.
 *
 * \return #NBL_NO_ERROR if success.
 */
int nbl_poison_cache();

/**
 * \brief Set automatically poison cache enabled flag.
 *
 * Specified 1 will make #nbl_poison_cache automatically invoked.
 *
 * \param[in] enabled Specify 1 to enable automatically poison the cache.
 * \see nbl_get_auto_poison_cache
 * \see nbl_poison_cache
 */
void nbl_set_auto_poison_cache(int enabled);

/**
 * \brief Get automatically poison cache enabled flag.
 *
 * \return 1:enable or 0:disable
 * \see nbl_set_auto_poison_cache
 */
int nbl_get_auto_poison_cache();

#ifdef __cplusplus
}
#endif

#endif /* NBL_COMMON_H_ */
