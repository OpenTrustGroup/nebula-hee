// Copyright 2020 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NBL_NTIME_H_
#define NBL_NTIME_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \nbl_group{ nbl_fwk_hee }

/**
 * Nebula time includes second and millisecond.
 */
typedef struct {
  uint32_t seconds;
  uint32_t millis;
} nbl_time_t;

/**
 * \brief Get REE time.
 *
 * It's NOT secure, because it can be changed by program or users in REE.
 *
 * \param[out] time REE time.
 * \return #NBL_NO_ERROR if success. Other is error.
 */
int nbl_time_get_ree(nbl_time_t* time);

/**
 * \brief Get system time of TEE/HEE.
 *
 * The origin of system time is from power on.
 * It's monotonic and is not reset or rolled back during the life of a given TA
 * instance. But if TEE system is reset, this time will start from 0.
 *
 * \param[out] time system time
 * \return #NBL_NO_ERROR if success. Other is error.
 */
int nbl_time_get_system(nbl_time_t* time);

/**
 * \brief Set TA persistent time of this TA.
 *
 * User can set any time as start into this persistent time.
 * And then use "nbl_time_get_ta_persistent()" to get this time.
 * Even if TEE system is reset, this time will be kept.
 *
 * \param[in] time start time
 * \return #NBL_NO_ERROR if success.
 * #NBL_ERR_NOT_ALLOWED is "TA has NO uuid, so don't allow".
 * Other is error.
 */
int nbl_time_set_ta_persistent(nbl_time_t* time);

/**
 * \brief Get TA persistent time of this TA.
 *
 * Get TA persistent time if set a start time by "nbl_time_set_ta_persistent".
 * It's monotonic from setting start to resetting new start, even if TEE system
 * is reset.
 * Only if TA has UUID, it can set this time.
 *
 * \param[out] time ta persistent time
 * \return #NBL_NO_ERROR if success.
 *    #NBL_ERR_NOT_FOUND is "NOT SET persistent time".
 *    #NBL_ERR_OBJECT_DESTROYED is "time is corrupted".
 *    #NBL_ERR_NOT_ALLOWED is "TA has NO uuid, so don't allow".
 *    And other is error.
 *
 */
int nbl_time_get_ta_persistent(nbl_time_t* time);

#ifdef __cplusplus
}
#endif

#endif /* NBL_NTIME_H_ */
