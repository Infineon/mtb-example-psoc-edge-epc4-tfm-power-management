// tfm_interrupts.c (compiled into TF-M core/SPM target)
#include <stdint.h>

#include "config_tfm.h"
#include "cmsis.h"
#include "cy_ipc_drv.h"
#include "ifx_interrupt_defs.h"
#include "interrupt.h"
#include "platform_multicore.h"
#include "spm.h"
#include "tfm_multi_core.h"
#include "tfm_peripherals_def.h"
#include "load/interrupt_defs.h"
#include "static_checks.h"


/* Debounce delay */
#define BTN_DEBOUNCE_DELAY_MS (200)

/* User BTN1 IRQ info */
static struct irq_t user_btn1_irq_info = {0};

void IFX_IRQ_NAME_TO_HANDLER(CYBSP_USER_BTN1_IRQ)(void)
{
    /* Delay to handle de-bouncing  */
    Cy_SysLib_Delay(BTN_DEBOUNCE_DELAY_MS);

    /* Clear CYBSP_USER_BTN1 interrupt source */
    if(1UL == Cy_GPIO_GetInterruptStatus(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN))
    {
        Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
        NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);

        spm_handle_interrupt(user_btn1_irq_info.p_pt, user_btn1_irq_info.p_ildi);
    }

    /* CYBSP_USER_BTN1 (SW2) and CYBSP_USER_BTN2 (SW4) share the same port in
     * the PSOC™ Edge E84 evaluation kit and hence they share the same NVIC IRQ
     * line. Since both the buttons are configured for falling edge interrupt in
     * the BSP, pressing any button will trigger the execution of this ISR. Therefore,
     * we must clear the interrupt flag of the user button (CYBSP_USER_BTN2) to avoid
     * issues in case if user presses BTN2 by mistake */
#if defined(CYBSP_USER_BTN2_ENABLED)
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN2_IRQ);
#endif
}

enum tfm_hal_status_t cybsp_user_btn1_irq_init(void *p_pt, const struct irq_load_info_t *p_ildi)
{
    user_btn1_irq_info.p_pt   = p_pt;
    user_btn1_irq_info.p_ildi = p_ildi;

    /* Ensure the line targets Secure state */
    NVIC_ClearTargetState(CYBSP_USER_BTN1_IRQ);

    /* Configure priority within (0, N/2) */
    NVIC_SetPriority(CYBSP_USER_BTN1_IRQ, DEFAULT_IRQ_PRIORITY);

    /* Make sure nothing is pending at boot */
    /* CYBSP_USER_BTN1 (SW2) and CYBSP_USER_BTN2 (SW4) share the same port in the
     * PSOC™ Edge E84 evaluation kit and hence they share the same NVIC IRQ line.
     * Since both are configured in the BSP via the Device Configurator, the
     * interrupt flags for both the buttons are set right after they get initialized
     * through the call to cybsp_init(). The flags must be cleared before initializing
     * the interrupt, otherwise the interrupt line will be constantly asserted */
#if defined(CYBSP_USER_BTN2_ENABLED)
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN2_IRQ);
#endif
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);

    return TFM_HAL_SUCCESS;
}
