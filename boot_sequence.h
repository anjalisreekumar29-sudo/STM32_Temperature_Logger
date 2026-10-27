/**
 ******************************************************************************
 * @file    boot_sequence.h
 * @brief   Header for STM32 Temperature Data Logger Boot & Self-Check Engine.
 ******************************************************************************
 */

#ifndef BOOT_SEQUENCE_H_
#define BOOT_SEQUENCE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "dht11.h"
#include "i2c_lcd.h"
#include "w25q128.h"
#include <stdbool.h>

/**
 * @brief Boot check results structure
 */
typedef struct {
    bool lcd_ok;         /* Critical: I2C LCD detected and operational */
    bool flash_ok;       /* Critical: W25QXX JEDEC detected */
    bool mem_test_ok;    /* Critical: SPI bus & status register operational */
    bool dht11_ok;       /* Non-Critical: DHT11 initial response received */
    bool critical_pass;  /* True if all critical subsystems passed */
} BootResult_t;

/**
 * @brief  Executes professional system self-check and boot sequence on LCD.
 * @param  lcd       Pointer to LCD handle
 * @param  hi2c      Pointer to I2C peripheral handle
 * @param  dht11     Pointer to DHT11 handle
 * @param  flash     Pointer to W25QXX handle
 * @param  hspi      Pointer to SPI peripheral handle
 * @param  cs_port   GPIO port for Flash CS pin
 * @param  cs_pin    GPIO pin for Flash CS pin
 * @return BootResult_t structure detailing component check status
 */
BootResult_t BootSequence_Run(LCD_I2C_t* lcd, I2C_HandleTypeDef* hi2c,
                              DHT11_t* dht11,
                              W25Q128_t* flash, SPI_HandleTypeDef* hspi,
                              GPIO_TypeDef* cs_port, uint16_t cs_pin);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_SEQUENCE_H_ */
