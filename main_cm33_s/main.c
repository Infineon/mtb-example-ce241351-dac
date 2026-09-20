/*******************************************************************************
* File Name:   main.c
*
* Description: This is the main file for the main CPU non safe application of
* the code example. It initializes the peripherals, PPCA CPU cores and starts
* it. Then it reads the data shared by the PPCA CPUs and prints.
*
* Related Document: See README.md
*
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cy_pdl.h"
#include "cycfg.h"
#include "cybsp.h"
#include <stdio.h>
#include "cy_retarget_io.h"

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Debug UART variables */
static cy_stc_scb_uart_context_t    UART_context; /* UART context */
static mtb_hal_uart_t               UART_hal_obj; /* Debug UART HAL object */
uint32_t counter = 0 ;

/******************************************************************************
* Macros
*******************************************************************************/
/* These are the addresses where the core0 and core1 images are located and its size*/
#define CORE0_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca0_nvm_C_S_START
#define CORE1_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca1_nvm_C_S_START
#define PPCA0_IMAGE_SIZE       CYMEM_CM33_0_S_ppca0_code_SIZE
#define PPCA1_IMAGE_SIZE       CYMEM_CM33_0_S_ppca1_code_SIZE

/* Shared memory addresses for inter-core communication */
/* PPCA cores write to these locations, main core reads from them */
/* Variables located in M4 shared memory space (16KB at 0x20040000-0x20043FFF from PPCA view) */
/* Main core accesses PPCA memory through PPCA peripheral base with memory windows: */
/* M1 (CPU0 data): 0x53020000, M3 (CPU1 data): 0x53040000, M4 (shared): 0x53050000 */
#define PPCA_CPU0_M4_VAR_ADDRESS   0x53050400  /* Written by PPCA Core 0 */
#define PPCA_CPU1_M4_VAR_ADDRESS   0x53050800  /* Written by PPCA Core 1 */


/* State in the state machine */
enum states {OUT_INVALID, OUT_SINE, OUT_SAWTOOTH, OUT_FIXED};

#define SINE_COUNTER_MAX 99

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
* This is the main function for the non safe project for the main core. It
* performs the initialization of the peripherals, initialization and starting
* of the PPCA CPU cores, and send the data received from PPCA CPU Core 0 through
* UART.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    /* Variable for result */
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);
    Cy_SCB_UART_Enable(UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config,
                                &UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&UART_hal_obj);

    /* retarget IO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: DAC\r\n");
    printf("************************************************************\r\n\n");

    /* Variables located in the shared memory. */
    int32_t *read_mode    = (int32_t *)PPCA_CPU0_M4_VAR_ADDRESS;
    int32_t *read_dac_val = (int32_t *)read_mode + 1;

    /* Enable the PPCA */
    Cy_PPCA_Enable(PPCA_CNFG);

    /* Initializing the TCPWM unit */
    Cy_TCPWM_PWM_Init(PWM_HW, PWM_NUM, &PWM_config);
    Cy_TCPWM_PWM_Enable(PWM_HW, PWM_NUM);

     /* enable interrupts */
     __enable_irq();

     /* Initializing and enabling ATOP Analog reference */
     Cy_PPCA_AREF_Init(AREF_HW, &AREF_config);
     Cy_PPCA_AREF_Enable(AREF_HW);

     /* Initializing and enabling the DAC */
     Cy_PPCA_DAC_Init(DAC_HW, &DAC_config);
     Cy_PPCA_DACBUF_Enable(DAC_HW);

     /* Enable DAC buffer */
     Cy_PPCA_DAC_Enable(DAC_HW);

     /* Software start to TCPWM unit */
     Cy_TCPWM_TriggerStart_Single(PWM_HW, PWM_NUM);

    /* Initializing and starting PPCA CPU Core 0. */
    Cy_System_Init_CPU0((void*)CORE0_IMAGE_ADDRESS, PPCA0_IMAGE_SIZE);

    /* Initializing and starting PPCA CPU Core 1. Use the below line to start PPCA Core 1 */
    /*Cy_System_Init_CPU1((void*)CORE1_IMAGE_ADDRESS, PPCA1_IMAGE_SIZE);*/

    /* Initialize the states */
    *read_mode = OUT_INVALID;

    for (;;)
    {
        /* Print CE Menu and read user selection. */
        printf("\r\n DAC Output select: Please select one option from below.");
        printf("\r\n 1: Sine wave.");
        printf("\r\n 2: Sawtooth wave.");
        printf("\r\n 3: Fixed output.\r\n");
        scanf("%d", (int *)read_mode);

        switch(*read_mode)
        {
        /* Sine wave is selected. */
        case OUT_SINE:
        {
            printf("\r\n Please check sine wave on the pin AIN0 for next 20 seconds.\r\n");
            Cy_SysLib_Delay(20000);
            break;
        }
        /* Sawtooth wave is selected. */
        case OUT_SAWTOOTH:
        {
            printf("\r\n Please check sawtooth wave on the pin AIN0 for next 20 seconds.\r\n");
            Cy_SysLib_Delay(20000);
            break;
        }
        /* Fixed output is selected. */
        case OUT_FIXED:
        {
            printf("\r\n Please enter the required DAC value.\r\n");
            scanf("%d", (int *)read_dac_val);
            printf("\r\n Please check the set voltage on the pin AIN0 for next 20 seconds..\r\n");
            Cy_SysLib_Delay(20000);
            break;
        }
        default:
        {
            /* Wrong option is selected. */
            printf("\r\n Please select a valid option.\r\n");
            break;
        }
        }
        Cy_SysLib_Delay(250);
        *read_mode = OUT_INVALID;
    }
}

