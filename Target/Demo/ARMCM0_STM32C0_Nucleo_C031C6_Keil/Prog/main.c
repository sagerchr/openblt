/************************************************************************************//**
* \file         Demo/ARMCM0_STM32C0_Nucleo_C031C6_Keil/Prog/main.c
* \brief        Demo program application source file.
* \ingroup      Prog_ARMCM0_STM32C0_Nucleo_C031C6_Keil
* \internal
*----------------------------------------------------------------------------------------
*                          C O P Y R I G H T
*----------------------------------------------------------------------------------------
*   Copyright (c) 2023  by Feaser    http://www.feaser.com    All rights reserved
*
*----------------------------------------------------------------------------------------
*                            L I C E N S E
*----------------------------------------------------------------------------------------
* This file is part of OpenBLT. OpenBLT is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* OpenBLT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You have received a copy of the GNU General Public License along with OpenBLT. It 
* should be located in ".\Doc\license.html". If not, contact Feaser to obtain a copy.
* 
* \endinternal
****************************************************************************************/

/****************************************************************************************
* Include files
****************************************************************************************/
#include "header.h"                                    /* generic header               */


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static void Init(void);
static void SystemClock_Config(void);
static void VectorBase_Config(void);


/************************************************************************************//**
** \brief     This is the entry point for the bootloader application and is called 
**            by the reset interrupt vector after the C-startup routines executed.
** \return    Program return code.
**
****************************************************************************************/
int main(void)
{
  /* initialize the microcontroller */
  Init();
  /* initialize the bootloader interface */
  BootComInit();

  /* start the infinite program loop */
  while (1)
  {
    /* toggle LED with a fixed frequency */
    LedToggle();
    /* check for bootloader activation request */
    BootComCheckActivationRequest();
  }

  /* program should never get here */
  return 0;
} /*** end of main ***/


/************************************************************************************//**
** \brief     Initializes the microcontroller.
** \return    none.
**
****************************************************************************************/
static void Init(void)
{
  /* configure the vector table base address. */
  VectorBase_Config();
  /* reset of all peripherals, initializes the Flash interface and the Systick. */
  HAL_Init();
  /* configure the system clock. */
  SystemClock_Config();
  /* initialize the timer driver */
  TimerInit();
  /* initialize the led driver */
  LedInit();
} /*** end of Init ***/


/************************************************************************************//**
** \brief     Vector base address configuration. It should no longer be at the start of
**            flash memory but moved forward because the first part of flash is
**            reserved for the bootloader. Note that this is already done by the
**            bootloader before starting this program. Unfortunately, function
**            SystemInit() overwrites this change again. 
** \return    none.
**
****************************************************************************************/
static void VectorBase_Config(void)
{
  /* The constant array with vectors of the vector table is declared externally in the
   * c-startup code.
   */
  extern const unsigned long __Vectors[];

  /* Remap the vector table to where the vector table is located for this program. */
  SCB->VTOR = (unsigned long)&__Vectors[0];
} /*** end of VectorBase_Config ***/


/************************************************************************************//**
** \brief     System Clock Configuration. This code was created by CubeMX and configures
**            the system clock.
** \return    none.
**
****************************************************************************************/
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  
  /* De-initialize the RCC to make sure the RCC_CR_HSEBYP-bit is cleared. Otherwise
   * the HSE cannot be configured, resulting in HAL_RCC_OscConfig() failing.
   */
  HAL_RCC_DeInit();

  /* Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Clock configuration incorrect or hardware failure. Hang the system to prevent
     * damage.
     */
    while(1);
  }

  /* Initializes the CPU, AHB and APB buses clocks. */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    /* Flash latency configuration incorrect or hardware failure. Hang the system to
     * prevent damage.
     */
    while(1);
  }
} /*** end of SystemClock_Config ***/


/************************************************************************************//**
** \brief     Initializes the Global MSP. This function is called from HAL_Init()
**            function to perform system level initialization (GPIOs, clock, DMA,
**            interrupt).
** \return    none.
**
****************************************************************************************/
void HAL_MspInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  /* SYSCFG clock enable. */
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  /* GPIO ports clock enable. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
#if (BOOT_COM_RS232_ENABLE > 0)
  /* Peripheral clock enable. */
  __HAL_RCC_USART2_CLK_ENABLE();
#endif /* BOOT_COM_RS232_ENABLE > 0 */

  /* SVC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
  /* PendSV_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  /* Configure the LED GPIO pin. */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#if (BOOT_COM_RS232_ENABLE > 0)
  /* UART TX and RX GPIO pin configuration. */
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#endif /* BOOT_COM_RS232_ENABLE > 0 */
} /*** end of HAL_MspInit ***/


/************************************************************************************//**
** \brief     Deinitializes the Global MSP. This function is called from HAL_DeInit()
**            function to perform system level Deinitialization (GPIOs, clock, DMA,
**            interrupt).
** \return    none.
**
****************************************************************************************/
void HAL_MspDeInit(void)
{
#if (BOOT_COM_RS232_ENABLE > 0)
  /* Reset UART GPIO pin configuration. */
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
#endif /* BOOT_COM_RS232_ENABLE > 0 */
  /* Deconfigure GPIO pin for the LED. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);

#if (BOOT_COM_RS232_ENABLE > 0)
  /* Peripheral clock disable. */
  __HAL_RCC_USART2_CLK_DISABLE();
#endif /* BOOT_COM_RS232_ENABLE > 0 */
  /* GPIO ports clock disable. */
  __HAL_RCC_GPIOA_CLK_DISABLE();
  /* SYSCFG clock disable. */
  __HAL_RCC_SYSCFG_CLK_DISABLE();
} /*** end of HAL_MspDeInit ***/


/*********************************** end of main.c *************************************/
