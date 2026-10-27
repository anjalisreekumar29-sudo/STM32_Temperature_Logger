/**
 ******************************************************************************
 * @file    i2c_lcd.h
 * @brief   Header file for HD44780 16x2 LCD with PCF8574 I2C expander backpack.
 ******************************************************************************
 */

#ifndef I2C_LCD_H_
#define I2C_LCD_H_

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

#include <stdint.h>
#include <stdbool.h>

/* Default I2C Addresses (7-bit address shifted left by 1 for STM32 HAL) */
#define LCD_DEFAULT_ADDR_1      (0x27 << 1)   /* PCF8574T  -> 0x4E */
#define LCD_DEFAULT_ADDR_2      (0x3F << 1)   /* PCF8574AT -> 0x7E */

/* PCF8574 Pin Mapping to LCD:
 * P0 -> RS (Register Select: 0=Cmd, 1=Data)
 * P1 -> RW (Read/Write: 0=Write)
 * P2 -> EN (Enable pulse)
 * P3 -> Backlight (1=On, 0=Off)
 * P4 -> D4
 * P5 -> D5
 * P6 -> D6
 * P7 -> D7
 */
#define LCD_PIN_RS              (1U << 0)
#define LCD_PIN_RW              (1U << 1)
#define LCD_PIN_EN              (1U << 2)
#define LCD_PIN_BL              (1U << 3)

/* LCD Commands */
#define LCD_CMD_CLEAR_DISPLAY   0x01
#define LCD_CMD_RETURN_HOME     0x02
#define LCD_CMD_ENTRY_MODE      0x04
#define LCD_CMD_DISPLAY_CTRL    0x08
#define LCD_CMD_CURSOR_SHIFT    0x10
#define LCD_CMD_FUNCTION_SET    0x20
#define LCD_CMD_SET_CGRAM_ADDR  0x40
#define LCD_CMD_SET_DDRAM_ADDR  0x80

/* LCD Structure */
typedef struct {
    I2C_HandleTypeDef* hi2c;
    uint8_t            i2c_addr;
    uint8_t            backlight;
    bool               initialized;
} LCD_I2C_t;

/* Public API */
HAL_StatusTypeDef LCD_Init(LCD_I2C_t* lcd, I2C_HandleTypeDef* hi2c, uint8_t i2c_addr);
void LCD_Clear(LCD_I2C_t* lcd);
void LCD_SetCursor(LCD_I2C_t* lcd, uint8_t row, uint8_t col);
void LCD_SendChar(LCD_I2C_t* lcd, char ch);
void LCD_Print(LCD_I2C_t* lcd, const char* str);
void LCD_Printf(LCD_I2C_t* lcd, const char* format, ...);
void LCD_SetBacklight(LCD_I2C_t* lcd, bool enable);
void LCD_CreateCustomChar(LCD_I2C_t* lcd, uint8_t location, const uint8_t charmap[8]);

/* Convenience custom character indexes */
#define LCD_CHAR_DEGREE         0
#define LCD_CHAR_THERMOMETER    1

void LCD_LoadDefaultIcons(LCD_I2C_t* lcd);

#ifdef __cplusplus
}
#endif

#endif /* I2C_LCD_H_ */
