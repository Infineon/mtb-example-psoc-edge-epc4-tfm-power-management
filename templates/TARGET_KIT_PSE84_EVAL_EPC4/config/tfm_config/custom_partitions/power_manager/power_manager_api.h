/*
 * Copyright (c) 2025 Cypress Semiconductor Corporation (an Infineon company)
 * or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#if !defined(POWER_MANAGER_API_H)
#define POWER_MANAGER_API_H

#include <stddef.h>
#include <stdint.h>

#include "psa/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calls the POWER_MANAGER to clear the wake-up source.
 *
 * @retval PSA_SUCCESS                  The operation completed successfully.
 * @retval other PSA error codes are indicating failure.
 */
psa_status_t power_manager_clr_wakeup_src(void);

/**
 * @brief Calls the POWER_MANAGER to get the wake-up source.
 *
 * @param[out] wakeup_src  Pointer to a uint32_t where the result will be stored.
 *
 * @retval PSA_SUCCESS                  The operation completed successfully.
 * @retval other PSA error codes are indicating failure.
 */
psa_status_t power_manager_get_wakeup_src(uint32_t *wakeup_src);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MANAGER_API_H */