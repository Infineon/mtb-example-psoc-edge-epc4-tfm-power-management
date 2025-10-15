/*
 * Copyright (c) 2025 Cypress Semiconductor Corporation (an Infineon company)
 * or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "psa_manifest/sid.h"
#include "psa/client.h"

#include "power_manager_api.h"
#include "power_manager_defs.h"

psa_status_t power_manager_clr_wakeup_src(void)
{
    psa_invec in_vec[] = {
        { .base = NULL, .len = 0 }
    };

    psa_outvec out_vec[] = {
        { .base = NULL, .len = 0 }
    };

    return psa_call(POWER_MANAGER_SERVICE_HANDLE,
                    POWER_MANAGER_CLR_WAKEUP_SOURCE,
                    in_vec, IOVEC_LEN(in_vec),
                    out_vec, IOVEC_LEN(out_vec));
}

psa_status_t power_manager_get_wakeup_src(uint32_t *wakeup_src)
{
    psa_invec in_vec[] = {
        { .base = NULL, .len = 0 }
    };

    psa_outvec out_vec[] = {
        { .base = wakeup_src, .len = sizeof(*wakeup_src) }
    };

    return psa_call(POWER_MANAGER_SERVICE_HANDLE,
                    POWER_MANAGER_GET_WAKEUP_SOURCE,
                    in_vec, IOVEC_LEN(in_vec),
                    out_vec, IOVEC_LEN(out_vec));
}