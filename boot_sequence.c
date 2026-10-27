/**
 ******************************************************************************
 * @file    boot_sequence.c
 * @brief   Implementation of STM32 Temperature Data Logger Boot & Self-Check.
 ******************************************************************************
 */

#include "boot_sequence.h"
#include <stdio.h>
#include <string.h>

/* Dedicated non-destructive memory test address in reserved Sector 0 */
#define MEM_TEST_ADDR          0x00000000UL
#define MEM_TEST_LEN           4U

BootResult_t BootSequence_Run(LCD_I2C_t* lcd, I2C_HandleTypeDef* hi2c,
                              DHT11_t* dht11,
                              W25Q128_t* flash, SPI_HandleTypeDef* hspi,
                              GPIO_TypeDef* cs_port, uint16_t cs_pin)
{
    BootResult_t result;
    result.lcd_ok = false;
    result.flash_ok = false;
    result.mem_test_ok = false;
    result.dht11_ok = false;
    result.critical_pass = false;

    printf("\r\n==========================================\r\n");
    printf("   STM32 SYSTEM BOOT & SELF-CHECK START   \r\n");
    printf("==========================================\r\n");

    /* ----------------------------------------------------------------------
     * STAGE 1: SYSTEM POWER-UP
     * ---------------------------------------------------------------------- */
    printf("[STAGE 1] System Power-Up & Stabilization...\r\n");
    /* Initialize LCD peripheral with auto-address detection (0x27 / 0x3F) */
    HAL_StatusTypeDef lcd_status = LCD_Init(lcd, hi2c, 0);

    if (lcd_status == HAL_OK && lcd->initialized) {
        result.lcd_ok = true;
        LCD_Clear(lcd);
        LCD_SetCursor(lcd, 0, 0);
        LCD_Print(lcd, "  TEMP LOGGER   ");
        LCD_SetCursor(lcd, 1, 0);
        LCD_Print(lcd, "  INITIALIZING  ");
    } else {
        printf("[BOOT] LCD Initialization FAILED (I2C bus error)!\r\n");
    }
    HAL_Delay(500);

    /* ----------------------------------------------------------------------
     * STAGE 2: SYSTEM CHECK
     * ---------------------------------------------------------------------- */
    printf("[STAGE 2] System Diagnostics Initiated...\r\n");
    if (result.lcd_ok) {
        LCD_Clear(lcd);
        LCD_SetCursor(lcd, 0, 0);
        LCD_Print(lcd, "  SYSTEM CHECK  ");
        LCD_SetCursor(lcd, 1, 0);
        LCD_Print(lcd, "  PLEASE WAIT   ");
    }
    HAL_Delay(400);

    /* ----------------------------------------------------------------------
     * STAGE 3: CHECK DHT11 SENSOR (NON-CRITICAL)
     * ---------------------------------------------------------------------- */
    printf("[STAGE 3] Checking DHT11 Sensor...\r\n");
    if (result.lcd_ok) {
        LCD_Clear(lcd);
        LCD_SetCursor(lcd, 0, 0);
        LCD_Print(lcd, " CHECKING DHT11 ");
        LCD_SetCursor(lcd, 1, 0);
        LCD_Print(lcd, " STATUS: ....   ");
    }
    HAL_Delay(300);

    /* Attempt initial sensor communication */
    DHT11_Status_t dht_res = DHT11_Read(dht11);
    if (dht_res == DHT11_OK) {
        result.dht11_ok = true;
        printf("  [OK]   DHT11 Sensor detected: Temp=%d C, Hum=%d %%\r\n",
               dht11->temperature, dht11->humidity);
        if (result.lcd_ok) {
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, "DHT11        OK ");
        }
        HAL_Delay(400);
    } else {
        result.dht11_ok = false;
        printf("  [WARN] DHT11 communication failed (Code: %d). Continuing boot...\r\n", dht_res);
        if (result.lcd_ok) {
            LCD_Clear(lcd);
            LCD_SetCursor(lcd, 0, 0);
            LCD_Print(lcd, "DHT11       FAIL");
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, "CONTINUING...   ");
        }
        HAL_Delay(700);
    }

    /* ----------------------------------------------------------------------
     * STAGE 4: CHECK I2C DISPLAY (CRITICAL)
     * ---------------------------------------------------------------------- */
    printf("[STAGE 4] Checking I2C Display Communication...\r\n");
    if (result.lcd_ok) {
        LCD_Clear(lcd);
        LCD_SetCursor(lcd, 0, 0);
        LCD_Print(lcd, " CHECK I2C LCD  ");
        LCD_SetCursor(lcd, 1, 0);
        LCD_Print(lcd, "I2C DISPLAY  OK ");
        printf("  [OK]   I2C 16x2 LCD operational at address 0x%02X\r\n", lcd->i2c_addr);
    } else {
        printf("  [FAIL] I2C 16x2 LCD communication failed!\r\n");
    }
    HAL_Delay(400);

    /* ----------------------------------------------------------------------
     * STAGE 5: CHECK W25QXX SPI FLASH (CRITICAL)
     * ---------------------------------------------------------------------- */
    printf("[STAGE 5] Checking W25QXX SPI Flash...\r\n");
    if (result.lcd_ok) {
        LCD_Clear(lcd);
        LCD_SetCursor(lcd, 0, 0);
        LCD_Print(lcd, " CHECK W25QXX   ");
        LCD_SetCursor(lcd, 1, 0);
        LCD_Print(lcd, " FLASH: ....    ");
    }

    HAL_StatusTypeDef flash_init_res = W25Q_Init(flash, hspi, cs_port, cs_pin);
    if (flash_init_res == HAL_OK && flash->initialized) {
        result.flash_ok = true;
        uint32_t size_mb = flash->capacity_bytes / (1024UL * 1024UL);
        printf("  [OK]   W25QXX Flash detected! Capacity: %lu MB\r\n", size_mb);
        if (result.lcd_ok) {
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, " FLASH       OK ");
        }
    } else {
        result.flash_ok = false;
        printf("  [FAIL] W25QXX Flash JEDEC ID check failed!\r\n");
        if (result.lcd_ok) {
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, " FLASH      FAIL");
        }
    }
    HAL_Delay(400);

    /* ----------------------------------------------------------------------
     * STAGE 6: MEMORY TEST (CRITICAL & NON-DESTRUCTIVE)
     * ---------------------------------------------------------------------- */
    printf("[STAGE 6] Performing Flash Memory Verification Test...\r\n");
    if (result.lcd_ok) {
        LCD_Clear(lcd);
        LCD_SetCursor(lcd, 0, 0);
        LCD_Print(lcd, "  MEMORY TEST   ");
    }

    if (result.flash_ok) {
        /*
         * Non-Destructive Verification:
         * 1. Reads test block from Sector 0 (outside log partition).
         * 2. Polls status register to ensure bus lines are stable.
         * 3. Mounts existing logs without overwriting or erasing data.
         */
        uint8_t test_buf[MEM_TEST_LEN] = {0};
        HAL_StatusTypeDef read_res = W25Q_ReadData(flash, MEM_TEST_ADDR, test_buf, MEM_TEST_LEN);
        HAL_StatusTypeDef busy_res = W25Q_WaitForBusy(flash, 50);

        if (read_res == HAL_OK && busy_res == HAL_OK) {
            FlashLog_Mount(flash);
            result.mem_test_ok = true;
            printf("  [OK]   Memory communication verified! Existing logs: %lu records\r\n",
                   flash->total_records);
            if (result.lcd_ok) {
                LCD_SetCursor(lcd, 1, 0);
                LCD_Print(lcd, " PASS           ");
            }
        } else {
            result.mem_test_ok = false;
            printf("  [FAIL] Memory read test or status register check failed!\r\n");
            if (result.lcd_ok) {
                LCD_SetCursor(lcd, 1, 0);
                LCD_Print(lcd, " FAIL           ");
            }
        }
    } else {
        result.mem_test_ok = false;
        if (result.lcd_ok) {
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, " FAIL           ");
        }
    }
    HAL_Delay(400);

    /* ----------------------------------------------------------------------
     * STAGE 7: EVALUATE SYSTEM STATUS
     * ---------------------------------------------------------------------- */
    /* Critical components: LCD and W25QXX Flash */
    if (result.lcd_ok && result.flash_ok && result.mem_test_ok) {
        result.critical_pass = true;
        printf(">>> [BOOT RESULT] All Critical Components PASSED! <<<\r\n");
        if (result.lcd_ok) {
            LCD_Clear(lcd);
            LCD_SetCursor(lcd, 0, 0);
            LCD_Print(lcd, "  SYSTEM READY  ");
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, " STARTING...    ");
        }
        HAL_Delay(600);

        /* ------------------------------------------------------------------
         * STAGE 8: TRANSITION TO NORMAL SCREEN
         * ------------------------------------------------------------------ */
        if (result.lcd_ok) {
            LCD_Clear(lcd);
            LCD_SetCursor(lcd, 0, 0);
            if (result.dht11_ok) {
                char temp_str[17];
                snprintf(temp_str, sizeof(temp_str), " TEMP: %2d%cC    ",
                         dht11->temperature, (char)LCD_CHAR_DEGREE);
                LCD_Print(lcd, temp_str);
            } else {
                LCD_Print(lcd, " TEMP: --.- C   ");
            }
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, " LOGGER: READY  ");
        }
    } else {
        result.critical_pass = false;
        printf(">>> [BOOT ERROR] Critical component failure! Logging halted. <<<\r\n");
        if (result.lcd_ok) {
            LCD_Clear(lcd);
            LCD_SetCursor(lcd, 0, 0);
            LCD_Print(lcd, "   BOOT ERROR   ");
            LCD_SetCursor(lcd, 1, 0);
            LCD_Print(lcd, "  CHECK SYSTEM  ");
        }
    }

    printf("==========================================\r\n\r\n");
    return result;
}
