/*****************************************************************************
* File Name        : main.c
*
* Description      : This source file contains the main routine for non-secure
*                    application in the CM33 CPU
*
* Related Document : See README.md
*
*******************************************************************************
# \copyright
# (c) 2024-2025, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG.  SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include <stdio.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "tfm_ns_interface.h"
#include "os_wrapper/common.h"
#include "ifx_platform_api.h"

#include "cy_time.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"

#include "power_manager_defs.h"
#include "power_manager_api.h"

/*******************************************************************************
* Macros
*******************************************************************************/

/* The timeout value in microseconds used to wait for CM55 core to be booted */
#define CM55_BOOT_WAIT_TIME_USEC (10U)

/* App boot address for CM55 project */
#define CM55_APP_BOOT_ADDR (CYMEM_CM33_0_m55_nvm_START + \
                            CYBSP_MCUBOOT_HEADER_SIZE)

#define TASK_DELAY_MSEC (1000U)
#define TASK_STACK_SIZE (4096)
#define TASK_PRIORITY (3)

/* Enabling or disabling a MCWDT requires a wait time of upto 2 CLK_LF cycles
 * to come into effect. This wait time value will depend on the actual CLK_LF
 * frequency set by the BSP.*/
#define LPTIMER_0_WAIT_TIME_USEC (62U)

/* App State Timeouts */
#define APP_STATE_ACTIVE_TIME_MS (20000)

/* Heart Beat freqyency */
#define HEART_BEAT_FREQ_MS (500)


/* Logging */
#define LOG_BUFFER_SIZE (256)
#define LOG(fmt, ...) ifx_platform_log_msg((const uint8_t *)log_buffer, snprintf(log_buffer, LOG_BUFFER_SIZE, (fmt), ##__VA_ARGS__))
#define LOG_WAIT_FOR_TX_COMPLETE() Cy_SysLib_Delay(100U);

/*******************************************************************************
* Typedefs
*******************************************************************************/

/* App States */
typedef enum
{
    APP_STATE_ACTIVE = 1U,
    APP_STATE_IDLE = 2U
} en_app_state_t;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

cy_en_syspm_status_t deepsleep_callback(cy_stc_syspm_callback_params_t *callbackParams,
                                        cy_en_syspm_callback_mode_t mode);

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* LPTimer HAL object */
static mtb_hal_lptimer_t lptimer_obj;

/* RTC HAL object */
static mtb_hal_rtc_t rtc_obj;

/* Log buffer */
static char log_buffer[LOG_BUFFER_SIZE];

/* Tasks Handle */
static TaskHandle_t vTaskHandelHeartBeat;
static TaskHandle_t vTaskHandelAppStateManager;

/* Holds the current App State */
en_app_state_t app_state;

/* Deep Sleep Callback config structures*/
cy_stc_syspm_callback_params_t cback_params =
{
    .base = NULL,
    .context = NULL
};
cy_stc_syspm_callback_t sys_ds_cback =
{
    .callback = deepsleep_callback,
    .type = CY_SYSPM_DEEPSLEEP,
    .skipMode = ~(CY_SYSPM_BEFORE_TRANSITION | CY_SYSPM_AFTER_TRANSITION),
    .callbackParams = &cback_params,
    .prevItm = NULL,
    .nextItm = NULL,
    .order = 0
};

static uint32_t wakeup_src = 0U;

/*******************************************************************************
* Function Name: handle_app_error
********************************************************************************
* Summary:
* User defined error handling function
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void handle_app_error(void)
{
    /* Disable all interrupts. */
    __disable_irq();

    CY_ASSERT(0);

    /* Infinite loop */
    while(true);
}

/*******************************************************************************
* Function Name: setup_clib_support
********************************************************************************
* Summary:
*    1. This function configures and initializes the Real-Time Clock (RTC).
*    2. It then initializes the RTC HAL object to enable CLIB support library 
*       to work with the provided Real-Time Clock (RTC) module.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_clib_support(void)
{
    /* RTC Initialization */
    Cy_RTC_Init(&CYBSP_RTC_config);
    Cy_RTC_SetDateAndTime(&CYBSP_RTC_config);

    /* Initialize the ModusToolbox CLIB support library */
    mtb_clib_support_init(&rtc_obj);
}

/*******************************************************************************
* Function Name: setup_tickless_idle_timer
********************************************************************************
* Summary:
*    1. This function first configures and initializes an interrupt for LPTimer.
*    2. Then it initializes the LPTimer HAL object to be used in the RTOS 
*       tickless idle mode implementation to allow the device enter deep sleep 
*       when idle task runs. LPTIMER_0 instance is configured for CM33 CPU.
*    3. It then passes the LPTimer object to abstraction RTOS library that 
*       implements tickless idle mode
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_tickless_idle_timer(void)
{
    /* Initialize the MCWDT block */
    cy_en_mcwdt_status_t mcwdt_init_status = 
                                    Cy_MCWDT_Init(CYBSP_CM33_LPTIMER_0_HW, 
                                                &CYBSP_CM33_LPTIMER_0_config);

    /* MCWDT initialization failed. Stop program execution. */
    if(CY_MCWDT_SUCCESS != mcwdt_init_status)
    {
        handle_app_error();
    }
  
    /* Enable MCWDT instance */
    Cy_MCWDT_Enable(CYBSP_CM33_LPTIMER_0_HW,
                    CY_MCWDT_CTR_Msk, 
                    LPTIMER_0_WAIT_TIME_USEC);

    /* Setup LPTimer using the HAL object and desired configuration as defined
     * in the device configurator. */
    cy_rslt_t result = mtb_hal_lptimer_setup(&lptimer_obj, 
                                            &CYBSP_CM33_LPTIMER_0_hal_config);
    
    /* LPTimer setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Pass the LPTimer object to abstraction RTOS library that implements 
     * tickless idle mode 
     */
    cyabs_rtos_set_lptimer(&lptimer_obj);
}

/*******************************************************************************
* Function Name: deepsleep_callback
********************************************************************************
* Summary:
*  Controls USER LED2 to indicate that DeepSleep entry and exit in Idle state .
*
* Parameter:
*  void
*
* Return:
*  void
*
*******************************************************************************/
cy_en_syspm_status_t deepsleep_callback(cy_stc_syspm_callback_params_t *callbackParams,
                                        cy_en_syspm_callback_mode_t mode)
{
    CY_UNUSED_PARAMETER(callbackParams);

    switch (mode)
    {
        case CY_SYSPM_BEFORE_TRANSITION:
            /* Clear the wake-up source */
            power_manager_clr_wakeup_src();
            /* Turn On LED to indicate Deep Sleep Entry */
            Cy_GPIO_Set(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN);
            break;
        case CY_SYSPM_AFTER_TRANSITION:
            /* Turn Off LED to indicate Deep Sleep Exit */
            Cy_GPIO_Clr(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN);
            /* Read the wake-up source */
            power_manager_get_wakeup_src(&wakeup_src);
            /* Unblock AppStateManager Task */
            xTaskNotifyGive(vTaskHandelAppStateManager);
            break;
        default:
            break;
    }
    
    return CY_SYSPM_SUCCESS;
}

/********************************************************************************
 * Function Name: vHeartBeatTask
 ********************************************************************************
 * Summary:
 *  Heart Beat Task. Blinks LED1 according to HeartBeat Frequency
 *
 * Parameters:
 *  pvParameters - Task Arguments.
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void vHeartBeatTask(void* pvParameters)
{
    LOG(" Heart Beat Task        - Running\r\n");

    for (;;)
    {
        /* Toggle LED1 according to HeartBeat Frequency */
        Cy_GPIO_Inv(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN);
        vTaskDelay(HEART_BEAT_FREQ_MS / portTICK_PERIOD_MS);
    }
}

/********************************************************************************
 * Function Name: vAppStateManagerTask
 ********************************************************************************
 * Summary:
 *  Controls the Applicaton state
 *
 * Parameters:
 *  pvParameters - Task Arguments.
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void vAppStateManagerTask(void* pvParameters)
{
    en_app_state_t app_state_next = APP_STATE_ACTIVE;
    bool tasks_suspended = false;
    uint32_t timeout_cnt = 0;

    LOG(" App State Manager Task - Running\r\n");
    vTaskDelay(1U / portTICK_PERIOD_MS);

    for (;;)
    {
        LOG("\r\n=======================================================\r\n");

        switch (app_state_next)
        {
            case APP_STATE_ACTIVE:
            {
                /* Active State Set-up*/
                if (tasks_suspended == true)
                {
                    vTaskResume(vTaskHandelHeartBeat);
                    tasks_suspended = false;
                }

                /* In Active State */
                app_state = APP_STATE_ACTIVE;
                LOG(" Current App State: APP_STATE_ACTIVE\r\n");
                LOG(" -----------------------------------\r\n");
                timeout_cnt = 0;
                while(timeout_cnt != APP_STATE_ACTIVE_TIME_MS)
                {
                    vTaskDelay(1U / portTICK_PERIOD_MS);
                    timeout_cnt++;
                }

                /* Time to move to next state */
                LOG(" App State Switch: APP_STATE_ACTIVE -> APP_STATE_IDLE\r\n");
                LOG(" Reason          : Active State Timeout\r\n");
                app_state_next = APP_STATE_IDLE;
            }
            break;

            default:
            {
                /* Idle State Set-up*/
                vTaskSuspend(vTaskHandelHeartBeat);
                tasks_suspended = true;

                /* In Idle State */
                app_state = APP_STATE_IDLE;
                LOG(" Current App State: APP_STATE_IDLE\r\n");
                LOG(" ---------------------------------\r\n");
                LOG_WAIT_FOR_TX_COMPLETE();
                Cy_GPIO_Clr(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                /* Time to move to next state */
                LOG(" App State Switch: APP_STATE_IDLE -> APP_STATE_ACTIVE\r\n");
                LOG(" Reason          : %s\r\n", wakeup_src ? "User Button-1 Interrupt" : "Unkown Interrupt");
                app_state_next = APP_STATE_ACTIVE;
            }
            break;
        }

        LOG("=======================================================\r\n");
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function of the CM33 non-secure application. 
*
* It initializes the TF-M NS interface to communicate with TF-M FW. Creates
* required Tasks and starts the freertos schedular
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    uint32_t rslt;
    BaseType_t status;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        handle_app_error();
    }

    /* Setup CLIB support library. */
    setup_clib_support();

    /* Setup the LPTimer instance for CM33 CPU. */
    setup_tickless_idle_timer();

    /* Register Deepsleep entry/exit callback */
    Cy_SysPm_RegisterCallback(&sys_ds_cback);

    /* Enable CM55. */
    Cy_SysEnableCM55(MXCM55, CM55_APP_BOOT_ADDR, CM55_BOOT_WAIT_TIME_USEC);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize TF-M interface */
    rslt = tfm_ns_interface_init();
    if(rslt != OS_WRAPPER_SUCCESS)
    {
        handle_app_error();
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    LOG("\x1b[2J\x1b[;H");

    LOG("**** PSOC Edge MCU: Secure Power Management (using TF_M) ****\r\n\n");
 
    /* Create Tasks */
    status = xTaskCreate(vHeartBeatTask, "HeartBeat", TASK_STACK_SIZE,
                         NULL, TASK_PRIORITY, &vTaskHandelHeartBeat);
    if (pdPASS != status)
    {
        handle_app_error();
    }
    status = xTaskCreate(vAppStateManagerTask, "AppState", TASK_STACK_SIZE,
                         NULL, TASK_PRIORITY, &vTaskHandelAppStateManager);
    if (pdPASS != status)
    {
        handle_app_error();
    }

    /* Start the Scheduler */
    vTaskStartScheduler();

    LOG("Failed to start Scheduler\r\n");

    handle_app_error();

}

/* [] END OF FILE */
