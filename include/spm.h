/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Secure Partition Manager header.
 */

#ifndef SPM_H_
#define SPM_H_

/**
 * @defgroup secure_partition_manager Secure Partition Manager
 * @{
 * @brief Secure Partition Manager (SPM).
 *
 * The Secure Partition Manager (SPM) provides functions for configuring
 * the security attributes of flash, RAM, and peripherals.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Prototype of the function that is called in non-secure context from
 *	   secure fault handler context.
 *
 * Function can be used to print pending logging data before reboot.
 */
typedef void (*spm_ns_on_fatal_error_t)(void);

/** @brief Jump to non-secure partition.
 *
 * This function extracts the VTOR_NS from
 * DT_FLASH_AREA_IMAGE_0_NONSECURE_OFFSET_0 and configures the MSP
 * accordingly before jumping to VTOR_NS[1].
 */
void spm_jump(void);


/** @brief Configure security attributes of flash, RAM, and peripherals.
 *
 * This function reads the security attribute options set for peripherals in
 * Kconfig. The RAM and flash partitioning is configured statically.
 */
void spm_config(void);

/** @brief Set handler which is called by the SPM fault handler.
 *
 * Handler is intended to be used to print out any pending log data before reset.
 *
 * @note It is only for debugging purposes!
 *
 * @param handler Handler.
 *
 * @retval -ENOTSUP if feature is disabled.
 * @retval 0 on success.
 */
int spm_set_ns_fatal_error_handler(spm_ns_on_fatal_error_t handler);

/** @brief Call non-secure fatal error handler.
 *
 * Must be called from fatal error handler.
 */
void spm_ns_fatal_error_handler(void);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SPM_H_ */
