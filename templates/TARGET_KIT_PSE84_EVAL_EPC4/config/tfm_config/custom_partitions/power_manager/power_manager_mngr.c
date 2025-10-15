/*
 * Copyright (c) 2025 Cypress Semiconductor Corporation (an Infineon company)
 * or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "psa/service.h"
#include "psa_manifest/power_manager.h"
#include "power_manager_defs.h"

#include <stdio.h>
#include "tfm_hal_interrupt.h"


/* Holds the bitfield value of wake-up sources */
static uint32_t wakeup_src_flag = 0U;


psa_flih_result_t user_btn1_interrupt_flih(void)
{
    /* Update wakeup src bitfield */
    wakeup_src_flag |= WAKEUP_SOURCE_USER_BTN1;

    return PSA_FLIH_NO_SIGNAL;
}

psa_status_t power_manager_init(void)
{
    printf("POWER MANAGER Partition init\r\n");

    /* Enable USE_BTN1 Interrupt */
    psa_irq_enable(USER_BTN1_INTERRUPT_SIGNAL);

    return PSA_SUCCESS;
}

psa_status_t power_manager_service_sfn(const psa_msg_t *msg)
{
    psa_status_t status = PSA_ERROR_GENERIC_ERROR;

    /* Handle messages sent to the POWER_MANAGER */
    switch (msg->type)
    {
        case POWER_MANAGER_GET_WAKEUP_SOURCE:
        {
            if (msg->out_size[0] == sizeof(uint32_t))
            {
                /* Populate the outupt with wake-up source */
                psa_write(msg->handle, 0, &wakeup_src_flag, sizeof(wakeup_src_flag));

                status = PSA_SUCCESS;
            }
            else
            {
                status = PSA_ERROR_INVALID_ARGUMENT;
            }
        }
        break;

        case POWER_MANAGER_CLR_WAKEUP_SOURCE:
        {
            /* CLear the wake-up source variable */
            wakeup_src_flag = 0U;

            status = PSA_SUCCESS;
        }
        break;

        default:
        {
            status = PSA_ERROR_NOT_SUPPORTED;
        }
        break;
    }

    return status;
}