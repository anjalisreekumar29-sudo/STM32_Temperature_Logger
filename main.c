/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : STM32 Temperature & Humidity Data Logger using DHT11,
 *                   16x2 I2C LCD, and Winbond W25Q128 SPI Flash.
 ******************************************************************************
 * Hardware Pinout (STM32F103C8T6 Blue Pill):
 *
 * 1. DHT11 Sensor:
 *    - VCC   -> 3.3V (Recommended for PA1 voltage safety)
 *    - GND   -> GND
 *    - DATA  -> PA1 (Pull-up resistor 4.7k to 3.3V)
 *
 * 2. 16x2 LCD (PCF8574 I2C Backpack):
 *    - VCC   -> 5V (Ensures clear contrast for HD44780 glass)
 *    - GND   -> GND
 *    - SCL   -> PB6 (I2C1_SCL, 5V-tolerant pin)
 *    - SDA   -> PB7 (I2C1_SDA, 5V-tolerant pin)
 *
 * 3. W25Q128 SPI Flash (16 MB):
 *    - VCC   -> 3.3V ONLY (Never connect to 5V!)
 *    - GND   -> GND
 *    - CS    -> PA4 (GPIO Output Push-Pull)
 *    - SCK   -> PA5 (SPI1_SCK)
 *    - MISO  -> PA6 (SPI1_MISO)
 *    - MOSI  -> PA7 (SPI1_MOSI)
 *
 * 4. Serial Debug / PC Data Dump (USART1, 115200 Baud):
 *    - TX    -> PA9  (STM32 TX -> PC USB-TTL RX)
 *    - RX    -> PA10 (STM32 RX <- PC USB-TTL TX)
 *    - GND   -> GND
 *
 * 5. Heartbeat Status LED:
 *    - PC13 (Active-Low onboard Blue Pill LED)
 ******************************************************************************
 */

#include "main.h"
#include "boot_sequence.h"
#include "dht11.h"
#include "i2c_lcd.h"
#include "w25q128.h"
#include <stdio.h>
#include <string.h>

/* Timing Intervals */
#define SENSOR_READ_INTERVAL_MS   3000U              /* Refresh LCD every 3 seconds */
#define FLASH_LOG_INTERVAL_MS    (5U * 60U * 1000U)  /* Log to Flash every 5 minutes (300,000 ms) */

/* Pin Definitions */
#define DHT11_PORT               GPIOA
#define DHT11_PIN                GPIO_PIN_1

#define FLASH_CS_PORT            GPIOA
#define FLASH_CS_PIN             GPIO_PIN_4

#define LED_STATUS_PORT          GPIOC
#define LED_STATUS_PIN           GPIO_PIN_13

/* Peripheral Handles */
I2C_HandleTypeDef  hi2c1;
SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart1;

/* Device Instances */
DHT11_t   dht11;
LCD_I2C_t lcd;
W25Q128_t flash;

/* UART Interactive Command Buffer */
uint8_t uart_rx_char;

/* Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void ProcessUARTCommand(char cmd);
static void PrintWelcomeBanner(void);

/* Retarget printf for STM32CubeIDE / GCC */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, 10);
    return ch;
}

/**
  * @brief  Application Entry Point
  */
int main(void)
{
    /* Reset peripherals, initialize Flash interface and Systick */
    HAL_Init();

    /* Configure System Clock (72 MHz via HSE 8MHz PLL) */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();

    /* Turn off onboard LED initially (PC13 is Active-Low) */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_SET);

    /* Start non-blocking UART interrupt reception for user commands */
    HAL_UART_Receive_IT(&huart1, &uart_rx_char, 1);

    /* Print serial greeting & menu */
    PrintWelcomeBanner();

    /* Initialize DHT11 GPIO and DWT cycle timer */
    DHT11_Init(&dht11, DHT11_PORT, DHT11_PIN);

    /* ----------------------------------------------------------------------
     * Execute Professional System Startup & Self-Check Sequence
     * ---------------------------------------------------------------------- */
    BootResult_t boot = BootSequence_Run(&lcd, &hi2c1, &dht11, &flash,
                                         &hspi1, FLASH_CS_PORT, FLASH_CS_PIN);

    /* If critical component failed (I2C LCD or W25QXX), halt temperature logging */
    if (!boot.critical_pass) {
        printf("[FATAL] Boot self-check failed critical tests. Main logging loop suspended.\r\n");
        while (1) {
            /* Blink onboard LED at 2Hz to signal hardware error condition */
            HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
            HAL_Delay(250);
        }
    }

    /* Timing trackers */
    uint32_t last_sensor_tick = 0;
    uint32_t last_log_tick = HAL_GetTick();

    /* Immediate first log if DHT11 was already verified during boot */
    bool initial_log_done = false;
    if (boot.dht11_ok && flash.initialized) {
        initial_log_done = true;
        FlashLog_SaveRecord(&flash, HAL_GetTick() / 1000U, dht11.temperature, dht11.humidity);
        printf("[FLASH] Initial boot record #%lu logged successfully.\r\n", flash.total_records);
    }


    /* ----------------------------------------------------------------------
     * Main Superloop
     * ---------------------------------------------------------------------- */
    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* Task 1: Read DHT11 and Refresh LCD (Every 3 seconds) */
        if ((now - last_sensor_tick) >= SENSOR_READ_INTERVAL_MS) {
            last_sensor_tick = now;

            /* Toggle status LED as a visual heartbeat */
            HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);

            /* Read Sensor */
            DHT11_Status_t status = DHT11_Read(&dht11);

            if (status == DHT11_OK) {
                /* Display on LCD */
                if (lcd.initialized) {
                    char line0[17];
                    char line1[17];

                    /* Line 0: "T: 25°C  H: 54%  " (uses custom icon 0 for °) */
                    snprintf(line0, sizeof(line0), "T: %2d%cC  H: %2d%%  ",
                             dht11.temperature, (char)LCD_CHAR_DEGREE, dht11.humidity);
                    LCD_SetCursor(&lcd, 0, 0);
                    LCD_Print(&lcd, line0);

                    /* Line 1: "Rec:#0042 [OK]   " or "Flash Full!     " */
                    if (flash.flash_full) {
                        snprintf(line1, sizeof(line1), "R:#%04lu FLASHFUL", flash.total_records);
                    } else {
                        snprintf(line1, sizeof(line1), "R:#%04lu   [OK]  ", flash.total_records);
                    }
                    LCD_SetCursor(&lcd, 1, 0);
                    LCD_Print(&lcd, line1);
                }

                /* Print telemetry to serial monitor */
                printf("[T+%04lus] DHT11 -> Temp: %d C | Humidity: %d %% | Flash Records: %lu\r\n",
                       now / 1000U, dht11.temperature, dht11.humidity, flash.total_records);

                /* Write the first record on startup */
                if (!initial_log_done && flash.initialized) {
                    initial_log_done = true;
                    FlashLog_SaveRecord(&flash, now / 1000U, dht11.temperature, dht11.humidity);
                    printf("[FLASH] Initial Record #%lu written to Flash memory.\r\n", flash.total_records);
                }

            } else {
                /* Sensor read failure */
                printf("[WARNING] DHT11 Read Error (Code: %d)\r\n", status);
                if (lcd.initialized) {
                    LCD_SetCursor(&lcd, 0, 0);
                    LCD_Print(&lcd, "DHT11 READ ERROR");
                    LCD_SetCursor(&lcd, 1, 0);
                    LCD_Print(&lcd, "Check Pin PA1   ");
                }
            }
        }

        /* Task 2: Save Reading to W25Q128 Flash (Every 5 minutes) */
        if (initial_log_done && ((now - last_log_tick) >= FLASH_LOG_INTERVAL_MS)) {
            last_log_tick = now;

            if (flash.initialized && !flash.flash_full) {
                HAL_StatusTypeDef save_res = FlashLog_SaveRecord(&flash, now / 1000U, dht11.temperature, dht11.humidity);
                if (save_res == HAL_OK) {
                    printf(">>> [5-MIN LOG] Saved Record #%lu at Address 0x%08lX (Temp: %d C, Hum: %d %%) <<<\r\n",
                           flash.total_records, flash.next_write_addr - sizeof(LogRecord_t),
                           dht11.temperature, dht11.humidity);
                } else {
                    printf("[ERROR] Failed to write record to W25Q128 Flash!\r\n");
                }
            } else if (flash.flash_full) {
                printf("[ALERT] Flash memory partition is FULL! Dump records with 'D' and erase with 'E'.\r\n");
            }
        }
    }
}

/**
  * @brief  UART Rx Interrupt Callback: handles interactive user keypresses
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        ProcessUARTCommand((char)uart_rx_char);
        /* Re-enable UART interrupt for next character */
        HAL_UART_Receive_IT(&huart1, &uart_rx_char, 1);
    }
}

/**
  * @brief  Command interpreter for interactive debugging and data extraction
  */
static void ProcessUARTCommand(char cmd) {
    switch (cmd) {
        case 'D':
        case 'd':
            printf("\r\n[*] Command 'D' received: Starting CSV dump...\r\n");
            FlashLog_DumpSerial(&flash, &huart1);
            break;

        case 'S':
        case 's':
            printf("\r\n--- SYSTEM STATUS ---\r\n");
            printf("Uptime           : %lu seconds\r\n", HAL_GetTick() / 1000U);
            printf("Last Temperature : %d C\r\n", dht11.temperature);
            printf("Last Humidity    : %d %%\r\n", dht11.humidity);
            printf("Total Flash Logs : %lu records\r\n", flash.total_records);
            printf("Next Flash Addr  : 0x%08lX\r\n", flash.next_write_addr);
            printf("Flash Full       : %s\r\n", flash.flash_full ? "YES" : "NO");
            printf("---------------------\r\n\r\n");
            break;

        case 'L':
        case 'l':
            printf("\r\n[*] Command 'L': Forcing immediate flash log write...\r\n");
            if (flash.initialized && !flash.flash_full) {
                FlashLog_SaveRecord(&flash, HAL_GetTick() / 1000U, dht11.temperature, dht11.humidity);
                printf("[+] Saved Record #%lu (Temp: %d C, Hum: %d %%)\r\n",
                       flash.total_records, dht11.temperature, dht11.humidity);
            }
            break;

        case 'E':
        case 'e':
            printf("\r\n[!] CAUTION: Erasing all logged records from W25Q128 Flash...\r\n");
            if (FlashLog_EraseAllLogs(&flash) == HAL_OK) {
                printf("[+] Flash partition erased successfully! Record count reset to 0.\r\n");
            } else {
                printf("[ERROR] Failed to erase flash partition!\r\n");
            }
            break;

        case '?':
        case 'h':
        case 'H':
            PrintWelcomeBanner();
            break;

        default:
            break;
    }
}

static void PrintWelcomeBanner(void) {
    printf("\r\n============================================================\r\n");
    printf("   STM32 DHT11 + I2C LCD + W25Q128 FLASH DATA LOGGER        \r\n");
    printf("============================================================\r\n");
    printf(" Commands available in Serial Monitor (115200 Baud):\r\n");
    printf("   'D' -> Dump all stored flash records in CSV format\r\n");
    printf("   'S' -> Print system & memory status\r\n");
    printf("   'L' -> Force an immediate test log to Flash\r\n");
    printf("   'E' -> Erase all log records from Flash\r\n");
    printf("   '?' -> Show this help menu\r\n");
    printf("============================================================\r\n\r\n");
}

/**
  * @brief System Clock Configuration (72 MHz via HSE 8MHz PLL)
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while(1);
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        while(1);
    }
}

/**
  * @brief I2C1 Initialization Function (PB6=SCL, PB7=SDA, 100 kHz)
  */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        while(1);
    }
}

/**
  * @brief SPI1 Initialization Function (PA5=SCK, PA6=MISO, PA7=MOSI)
  */
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;   /* Mode 0: CPOL=0 */
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;       /* Mode 0: CPHA=0 */
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 72 MHz / 8 = 9 MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        while(1);
    }
}

/**
  * @brief USART1 Initialization Function (PA9=TX, PA10=RX, 115200 Baud)
  */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        while(1);
    }

    /* Enable USART1 Global Interrupt in NVIC */
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO Clocks */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure PC13 (Heartbeat LED) */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = LED_STATUS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_STATUS_PORT, &GPIO_InitStruct);

    /* Configure PA4 (Flash CS Pin) */
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET); /* CS Idle HIGH */
    GPIO_InitStruct.Pin = FLASH_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FLASH_CS_PORT, &GPIO_InitStruct);

    /* Configure PA1 (DHT11 Data Pin) initially as Input Pull-Up */
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
