[Click here](../README.md) to view the README.

## Design and implementation

The design of this application is minimalistic to get started with Power management on Infineon's PSOC&trade; Edge MCU devices. This code example demonstrates how to use Trusted Firmware-M (TF-M) with FreeRTOS and configure FreeRTOS to enter into DeepSleep mode when idle on the PSOC&trade; Edge MCU. It has a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE). A project folder consists of various subfolders, each denoting a specific aspect of the project. The three project folders are as follows:

**Table 1. Application projects**

Project | Description
--------|------------------------
*proj_cm33_s* | Project for CM33 SPE; implemented by TF-M
*proj_cm33_ns* | Project for CM33 NSPE
*proj_cm55* | Project for CM55 NSPE

<br>

Trusted Firmware-M (TF-M) implements the SPE for Arm&reg;v8-M and Arm&reg;v8.1-M architectures (e.g., the Cortex&reg;-M33, Cortex&reg;-M23, Cortex&reg;-M55, and Cortex&reg;-M85 processors) and dual-core platforms. It is a Platform Security Architecture (PSA) reference implementation aligning with PSA-certified guidelines, enabling chips, real-time operating systems, and devices to become PSA-certified. For more details, see the [Trusted Firmware-M Documentation](https://trustedfirmware-m.readthedocs.io/en/latest/index.html).

The extended boot launches the Edge Protect Bootloader (EPB) from RRAM. The EPB authenticates the CM33 secure, CM33 non-secure, and CM55 projects, which are placed in the external flash and launches the CM33 secure application from the external flash. The CM33 secure project contains the TF-M. TF-M is available in source code format as a library in the *mtb_shared* directory. The CM33 secure application does not have any source files and instead includes this TF-M library from the *mtb-shared* for building TF-M firmware. For more information on TF-M library and services, see AN240096 – Getting started with Trusted Firmware-M (TF-M) on PSOC&trade; Edge.

The TF-M's secure partition manager (SPM) creates the secure SPE and NSPE using TrustZone&reg;, memory protection unit (MPU), memory protection controller (MPC), and peripheral protection controller (PPC) protection units. TF-M offers several services, which can be used by the non-secure application. These services are placed in independent partitions. The following partitions are initialized by the SPM: 
- Internal trusted storage (ITS)
- Protected storage (PS)
- Crypto
- Initial attestation
- Platform
- Power Manager (a custom partition used in this code example)

**Power Manager** is an Application ROT TF-M custom partition implemented using Secure Function (SFN) model, which enables GPIO secure-interrupt to detect the **USER BTN1** button press, remembers the wakeup source in case of GPIO interrupt and provides following service APIs to NSPE

**Table 2. Power Manager partition service APIs**

API | Description
--------|------------------------
`power_manager_clr_wakeup_src` | Clears the wakeup source
`power_manager_get_wakeup_src` | Returns the wakeup source


**Table 3. Power Manager partition files**

File | Description
--------|------------------------
*power_manager_mngr.json* | Manifest file - Defines the format, services, and interrupts of the partition
*power_manager_mngr.c* | Core file of the partition implements everything needed on SPE
*power_manager_api.c* <br> *power_manager_api.h* | Provides secure aware APIs to NSPE
*power_manager_defs.h* | Provides required definitions, used by both SPE and NSPE
*power_manager_interrupts* | Provides init and handlers for interrupts owned by the partition This file will be part of TFM SPM and not the Power Manager partition itself
custom_partitions.cmake <br> custom_top_level_manifest.yaml | Common CMake and manifest files for all custom partitions. Currently includes only Power Manager Partition


After initializing the partitions, TF-M launches the M33 NSPE project from the external flash, which initializes the M33 NSPE <-> M55 NSPE interface using secure request framework (SRF). It then enables the CM55 core using the `Cy_SysEnableCM55()` function and the CM55 core is subsequently put into Deep Sleep mode as it is not used in this code example. 

The CM33 NS project makes calls to TF-M via the PSA APIs. In this code example, usage of logging and DeepSleep operations/APIs offered by the platform TF-M partition; operations offered by custom TF-M partition **Power Manager** are demonstrated. CM33 non-secure application creates the following two tasks.

**Table 4. Application tasks**

Task | Description
--------|------------------------
App State Manager | Manages the Application state <br> *APP_STATE_ACTIVE* - Resumes all Tasks for 20 seconds <br> *APP_STATE_IDLE* - Suspends all tasks to simulate FreeRTOS idle scenario
Heart Beat | Blinks LED1 at 1 kHz in *APP_STATE_ACTIVE*


Tickless idle functionality of FreeRTOS is configured to make the device enter into DeepSleep mode when idle. When all the tasks are suspended in *APP_STATE_IDLE*, device automatically enters into DeepSleep mode. This is indicated by LED2. If required, wakeup the device manually by pressing the **USER_BTN1** button and transition to *APP_STATE_ACTIVE*.

On Edge Protect Category 4 (EPC4) MCUs, the NSPE interrupts are masked when the device is in SPE. The device is configured to enter into DeepSleep mode inside SPE. As a result, only secure-interrupts can wake up the device from DeepSleep mode. Therefore, in this code example, the USER BTN1 (GPIO) interrupt is configured as a secure-interrupt and managed in SPE by Power Manager partition.

<br>