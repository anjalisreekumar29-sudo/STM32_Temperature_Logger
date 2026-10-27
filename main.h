/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 ******************************************************************************
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STM32F103xB) || defined(STM32F103xE) || defined(STM32F1)
#include "stm32f1xx_hal.h"
#elif defined(STM32F401xC) || defined(STM32F411xE) || defined(STM32F4)
#include "stm32f4xx_hal.h"
#else
#include "stm32f1xx_hal.h"
#endif

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
