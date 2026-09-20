/*******************************************************************************
* File Name:   main.c
*
* Description: This is the main file for the PPCA CPU core 0. This file contains
* the main function for the core.
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
#include <stdio.h>

/******************************************************************************
* Macros
*******************************************************************************/
/* Shared memory addresses in M4 shared memory space (0x20040000-0x20043FFF) */
/* All cores can access M4 shared memory for inter-core communication */
#define PPCA_CPU0_M4_VAR_ADDRESS 0x20040400  /* Used by this core (CPU0) */


/* number of elements in the sine wave lookup table. */
#define SINE_COUNTER_MAX 99

/* Maximum value for the counter used for sawtooth. */
#define SAWTOOTH_COUNTER_MAX 0xfff

/* State in the state machine */
enum states {OUT_INVALID, OUT_SINE, OUT_SAWTOOTH, OUT_FIXED};

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Lookup table for a sine wave in unsigned format. */
uint32_t sinewave_pattern[] = {2062,2198,2300,2434,2565,2662,2788,2880,2998,3112,
           3193,3296,3369,3461,3545,3603,3674,3721,3777,3824,
           3853,3883,3900,3915,3920,3917,3905,3890,3861,3824,
           3789,3736,3690,3622,3545,3483,3393,3321,3219,3112,
           3027,2910,2819,2694,2565,2467,2333,2232,2096,1960,
           1857,1721,1619,1485,1354,1257,1131,1039,921,807,
           726,623,550,458,374,316,245,198,142,95,
           66,36,19,4,0,2,14,29,58,95,
           130,183,229,297,374,436,526,598,700,807,
           892,1009,1100,1225,1354,1452,1586,1687,1823,1959};

/*counters for waveform generation*/
uint32_t sine_counter = 0;
uint32_t sawtooth_counter = 0;

/* TCPWM ISR for sine and sawtooth value update */
cy_stc_sysint_t pwm_intr_config =
{
    .intrSrc = PWM_IRQ,
    .intrPriority = 1U,
};

int32_t *read_mode    = (int32_t *)PPCA_CPU0_M4_VAR_ADDRESS;
int32_t *read_dac_val = (int32_t *)PPCA_CPU0_M4_VAR_ADDRESS + 1;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* ISR of PWM */
void pwm_isr();

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
* This is the main function for PPCA CPU 0. It performs the initialization of the
* variables used in the code.
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

    /* Initializing and enabling the interrupt */
     Cy_SysInt_Init(&pwm_intr_config, &pwm_isr);
     NVIC_EnableIRQ(pwm_intr_config.intrSrc);

     /* enable interrupts */
     __enable_irq();

     for(;;)
     {
     }
}

/*******************************************************************************
* Function Name: pwm_isr
*********************************************************************************
* Summary:
* This ISR generates sine wave using the provided lookup table and sawtooth
* using a counter. It writes the data from the lookup table/counter to the DAC
* output register and creates sine wave and sawtooth wave. Also it generates
* fixed value output based on the user selection. Selection of the wave type is
* done based on the user input value kept on the shared memory.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
void pwm_isr()
{
    /* Clearing the interrupt */
    Cy_TCPWM_ClearInterrupt(PWM_HW, PWM_NUM, CY_TCPWM_INT_ON_CC0_OR_TC);

    /* Sine wave is selected. */
    if(OUT_SINE == *read_mode)
    {
        /* Updating the DAC output */
        Cy_PPCA_DAC_Set_DACOut(DAC_HW, sinewave_pattern[sine_counter]);

        /* Counter to iterate through the lookup table */
        if(sine_counter < SINE_COUNTER_MAX)
        {
            sine_counter+=1;
        }
        else
        {
            sine_counter = 0;
        }
    }
    /* Sawtooth wave is selected. */
    else if(OUT_SAWTOOTH == *read_mode)
    {
        /* Updating the DAC output */
        Cy_PPCA_DAC_Set_DACOut(DAC_HW, sawtooth_counter);

        /* Counter to generate the sawtooth wave*/
        if(sawtooth_counter < SAWTOOTH_COUNTER_MAX)
        {
            sawtooth_counter+=10;
        }
        else
        {
            sawtooth_counter = 0;
        }
    }
    /* Fixed output is selected. */
    else if(OUT_FIXED == *read_mode)
    {
        /* Updating the DAC output */
        Cy_PPCA_DAC_Set_DACOut(DAC_HW, *read_dac_val);
    }
    else
    {
        /* Wrong value is selected. */
        *read_mode = OUT_INVALID;
    }
}
