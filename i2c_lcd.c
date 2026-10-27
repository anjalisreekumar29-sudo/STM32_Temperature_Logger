/**
 ******************************************************************************
 * @file    i2c_lcd.c
 * @brief   Driver implementation for HD44780 16x2 LCD via PCF8574 I2C expander.
 ******************************************************************************
 */

#include "i2c_lcd.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define LCD_TIMEOUT_MS  100

static const uint8_t icon_degree[8] = {
    0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t icon_thermometer[8] = {
    0x04, 0x0A, 0x0A, 0x0E, 0x0E, 0x1F, 0x1F, 0x0E
};

/* Internal helper: write byte directly to PCF8574 over I2C */
static HAL_StatusTypeDef LCD_WriteI2C(LCD_I2C_t* lcd, uint8_t data) {
    return HAL_I2C_Master_Transmit(lcd->hi2c, lcd->i2c_addr, &data, 1, LCD_TIMEOUT_MS);
}

/* Internal helper: latch 4-bit nibble into LCD via EN pin toggle */
static void LCD_PulseEnable(LCD_I2C_t* lcd, uint8_t nibble) {
    uint8_t bl = lcd->backlight ? LCD_PIN_BL : 0x00;
    
    /* EN high */
    LCD_WriteI2C(lcd, nibble | LCD_PIN_EN | bl);
    HAL_Delay(1);
    
    /* EN low */
    LCD_WriteI2C(lcd, (nibble & ~LCD_PIN_EN) | bl);
    HAL_Delay(1);
}

/* Internal helper: send 4 bits (nibble) */
static void LCD_SendNibble(LCD_I2C_t* lcd, uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | (mode ? LCD_PIN_RS : 0x00);
    LCD_PulseEnable(lcd, data);
}

/* Internal helper: send full 8-bit command or data as two 4-bit nibbles */
static void LCD_Send(LCD_I2C_t* lcd, uint8_t value, uint8_t mode) {
    uint8_t high_nibble = value & 0xF0;
    uint8_t low_nibble = (value << 4) & 0xF0;

    LCD_SendNibble(lcd, high_nibble, mode);
    LCD_SendNibble(lcd, low_nibble, mode);
}

static void LCD_SendCmd(LCD_I2C_t* lcd, uint8_t cmd) {
    LCD_Send(lcd, cmd, 0);
}

void LCD_SendChar(LCD_I2C_t* lcd, char ch) {
    LCD_Send(lcd, (uint8_t)ch, 1);
}

/* --------------------------------------------------------------------------
 * Public Functions
 * -------------------------------------------------------------------------- */

HAL_StatusTypeDef LCD_Init(LCD_I2C_t* lcd, I2C_HandleTypeDef* hi2c, uint8_t i2c_addr) {
    if (!lcd || !hi2c) {
        return HAL_ERROR;
    }

    lcd->hi2c = hi2c;
    lcd->backlight = 1;
    lcd->initialized = false;

    /* Auto-detect address if 0 is passed */
    if (i2c_addr == 0) {
        if (HAL_I2C_IsDeviceReady(hi2c, LCD_DEFAULT_ADDR_1, 2, 50) == HAL_OK) {
            lcd->i2c_addr = LCD_DEFAULT_ADDR_1;
        } else if (HAL_I2C_IsDeviceReady(hi2c, LCD_DEFAULT_ADDR_2, 2, 50) == HAL_OK) {
            lcd->i2c_addr = LCD_DEFAULT_ADDR_2;
        } else {
            /* Fallback to default */
            lcd->i2c_addr = LCD_DEFAULT_ADDR_1;
        }
    } else {
        lcd->i2c_addr = i2c_addr;
    }

    /* Wait > 40ms after VDD rises to 4.5V */
    HAL_Delay(50);

    /* Initialize in 4-bit mode per HD44780 datasheet */
    LCD_SendNibble(lcd, 0x30, 0);
    HAL_Delay(5);

    LCD_SendNibble(lcd, 0x30, 0);
    HAL_Delay(1);

    LCD_SendNibble(lcd, 0x30, 0);
    HAL_Delay(1);

    /* Switch to 4-bit interface */
    LCD_SendNibble(lcd, 0x20, 0);
    HAL_Delay(1);

    /* Function set: 4-bit mode, 2-line display, 5x8 dot format */
    LCD_SendCmd(lcd, 0x28);
    HAL_Delay(1);

    /* Display control: Display OFF, cursor OFF, blink OFF */
    LCD_SendCmd(lcd, 0x08);
    HAL_Delay(1);

    /* Clear display */
    LCD_Clear(lcd);
    HAL_Delay(2);

    /* Entry mode set: Auto-increment cursor, no display shift */
    LCD_SendCmd(lcd, 0x06);
    HAL_Delay(1);

    /* Display control: Display ON, cursor OFF, blink OFF */
    LCD_SendCmd(lcd, 0x0C);
    HAL_Delay(1);

    lcd->initialized = true;

    /* Load custom icons */
    LCD_LoadDefaultIcons(lcd);

    return HAL_OK;
}

void LCD_Clear(LCD_I2C_t* lcd) {
    LCD_SendCmd(lcd, LCD_CMD_CLEAR_DISPLAY);
    HAL_Delay(2);
}

void LCD_SetCursor(LCD_I2C_t* lcd, uint8_t row, uint8_t col) {
    /* 16x2 row addresses: Row 0 -> 0x00, Row 1 -> 0x40 */
    uint8_t row_offsets[] = {0x00, 0x40};
    if (row > 1) row = 1;
    if (col > 15) col = 15;

    LCD_SendCmd(lcd, LCD_CMD_SET_DDRAM_ADDR | (col + row_offsets[row]));
}

void LCD_Print(LCD_I2C_t* lcd, const char* str) {
    if (!str) return;
    while (*str) {
        LCD_SendChar(lcd, *str++);
    }
}

void LCD_Printf(LCD_I2C_t* lcd, const char* format, ...) {
    char buffer[32];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    LCD_Print(lcd, buffer);
}

void LCD_SetBacklight(LCD_I2C_t* lcd, bool enable) {
    lcd->backlight = enable ? 1 : 0;
    uint8_t bl = lcd->backlight ? LCD_PIN_BL : 0x00;
    LCD_WriteI2C(lcd, bl);
}

void LCD_CreateCustomChar(LCD_I2C_t* lcd, uint8_t location, const uint8_t charmap[8]) {
    location &= 0x07; /* Only 8 locations (0-7) */
    LCD_SendCmd(lcd, LCD_CMD_SET_CGRAM_ADDR | (location << 3));
    for (uint8_t i = 0; i < 8; i++) {
        LCD_Send(lcd, charmap[i], 1);
    }
    /* Return back to DDRAM address space */
    LCD_SendCmd(lcd, LCD_CMD_SET_DDRAM_ADDR);
}

void LCD_LoadDefaultIcons(LCD_I2C_t* lcd) {
    LCD_CreateCustomChar(lcd, LCD_CHAR_DEGREE, icon_degree);
    LCD_CreateCustomChar(lcd, LCD_CHAR_THERMOMETER, icon_thermometer);
}
