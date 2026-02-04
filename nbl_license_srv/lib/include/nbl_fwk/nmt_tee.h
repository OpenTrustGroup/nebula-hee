// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NBL_LICENSE_SRV_NMT_TEE_H_
#define NBL_LICENSE_SRV_NMT_TEE_H_
#include <stdint.h>

/**
 * \addtogroup nbl_fwk_hee
 */

/**
 * \file
 * \ingroup nbl_fwk_hee
 */

/// Max data size of injecting data at a time.
#define NMT_MAX_TRANSFER_DATA_SIZE 3072

enum nmt_inject_flag{
	NMT_INJECT_FLAG_ANYWHERE = 0x0ull, ///< Load data from all possible places.
	NMT_INJECT_FLAG_TD = 0x1ull, ///< Use storage TD port to save or load data.
	NMT_INJECT_FLAG_TDEA = 0x2ull ///< Use storage TDEA port to save or load data.
};

enum nmt_act_product {
	NMT_ACT_PRODUCT_TRIAL = 0, ///< Trial edition of the product.
	NMT_ACT_PRODUCT_BASIC = 1, ///< Basic edition of the product.
};

enum nmt_act_feature {
	NMT_ACT_FEAT_INSTALL_TA = 1, ///< install trusted apps, 0: not allow, 1: allow
	NMT_ACT_FEAT_MAX_VM = 2, ///< max. number of VMs can be startup
	NMT_ACT_FEAT_TRACE_TOOL = 3, ///< use trace tool, 0: not allow, 1: allow
	NMT_ACT_FEAT_MONITOR_TOOL = 4, ///< use monitor tool, 0: not allow, 1: allow
	NMT_ACT_FEAT_EXPIRE_AFTER_DAYS = 5, ///< product license expired days
	NMT_ACT_FEAT_BOOT_UP_DAYS = 6, ///< product boot up days
	NMT_ACT_FEAT_REMAINING_DAYS = 7, ///< Bate license remaining days. 0: Not activated or expired.
	NMT_ACT_FEAT_VERSION = 8, ///< license data version
	NMT_ACT_FEAT_INVALID = 0xFFFFFFFF
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Gets product activation status.
 *
 * \param[in] product Product ID #nmt_act_product.
 *
 * \return #NBL_NO_ERROR if the product is activated. #NBL_ERR_NOT_READY if tee has
 * not completely booted. #NBL_ERR_NOT_VALID if the product is not activated.
 */
int nbl_act_status(int product);

/**
 * \brief Gets license feature status.
 *
 * \param[in] feature Feature ID #nmt_act_feature.
 *
 * \return >= 0 if the feature is valid. #NBL_ERR_NOT_READY if nbl_license_srv has
 * not ready. #NBL_ERR_NOT_VALID if the feature is not valid.
 */
int nbl_act_feature(int feature);

/**
 * \brief Loads data which is injected to this TA.
 *
 * \param[in] slot Slot of data.
 * \param[in] flags Flags #nmt_inject_flag for loading data.
 * \param[in] data Data buffer used for loading data.
 * \param[in] size Data buffer size.
 * \param[in] src_offset Offset of source data.
 * \return The number of bytes read. #NBL_ERR_INVALID_ARGS if data is not NULL
 *         and size is 0.
 * \note If data is set to NULL and size is set to 0, then total size of data
 *       will be returned.
 */
int nbl_plt_load_inject_data(int slot, uint64_t flags, void *data, int size,
		int src_offset);

/**
 * \brief Check whether the device is closed or not.
 *
 * \return 1 if the device is closed, 0 otherwise.
 */
int nbl_plt_is_device_close();

#ifdef __cplusplus
}
#endif

#endif /* NBL_LICENSE_SRV_NMT_TEE_H_ */
