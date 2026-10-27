/**
 ******************************************************************************
 * @file    dht11.c
 * @brief   Driver implementation for DHT11 sensor on STM32F103 (STM32F1 HAL).
 *          Precise timings handled with Cortex-M DWT Cycle Counter.
 ******************************************************************************
 */

#include "dht11.h"

/* Ensure DWT Cycle Counter is running */
static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void DWT_Delay_us(uint32_t us) {
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < ticks) {
        /* Busy wait */
    }
}

/* Helper to reconfigure pin mode on the fly */
static void DHT11_SetPinMode(DHT11_t* dev, uint32_t mode, uint32_t pull) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = dev->GPIO_Pin;
    GPIO_InitStruct.Mode = mode;
    GPIO_InitStruct.Pull = pull;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(dev->GPIOx, &GPIO_InitStruct);
}

void DHT11_Init(DHT11_t* dev, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    dev->GPIOx = GPIOx;
    dev->GPIO_Pin = GPIO_Pin;
    dev->temperature = 0;
    dev->humidity = 0;
    dev->last_read_ms = 0;

    DWT_Init();

    /* Set as input with pull-up in idle state */
    DHT11_SetPinMode(dev, GPIO_MODE_INPUT, GPIO_PULLUP);
}

DHT11_Status_t DHT11_Read(DHT11_t* dev) {
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint32_t start_tick;

    /* ----------------------------------------------------------------------
     * 1. Send Host Start Signal
     * ---------------------------------------------------------------------- */
    /* Drive pin LOW for at least 18ms */
    DHT11_SetPinMode(dev, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    HAL_GPIO_WritePin(dev->GPIOx, dev->GPIO_Pin, GPIO_PIN_RESET);
    HAL_Delay(18);

    /* Pull line HIGH for 30us */
    HAL_GPIO_WritePin(dev->GPIOx, dev->GPIO_Pin, GPIO_PIN_SET);
    DWT_Delay_us(30);

    /* Switch to Input with Pull-up to listen for DHT11 response */
    DHT11_SetPinMode(dev, GPIO_MODE_INPUT, GPIO_PULLUP);

    /* ----------------------------------------------------------------------
     * 2. Wait for DHT11 Response (80us LOW followed by 80us HIGH)
     * ---------------------------------------------------------------------- */
    /* Wait for DHT11 to pull pin LOW (timeout 100us) */
    start_tick = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(dev->GPIOx, dev->GPIO_Pin) == GPIO_PIN_SET) {
        if ((DWT->CYCCNT - start_tick) > (100U * (SystemCoreClock / 1000000U))) {
            return DHT11_ERR_NO_RESPONSE;
        }
    }

    /* Wait for DHT11 to release pin HIGH (timeout 100us) */
    start_tick = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(dev->GPIOx, dev->GPIO_Pin) == GPIO_PIN_RESET) {
        if ((DWT->CYCCNT - start_tick) > (100U * (SystemCoreClock / 1000000U))) {
            return DHT11_ERR_TIMEOUT;
        }
    }

    /* Wait for DHT11 to pull LOW again to begin first data bit (timeout 100us) */
    start_tick = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(dev->GPIOx, dev->GPIO_Pin) == GPIO_PIN_SET) {
        if ((DWT->CYCCNT - start_tick) > (100U * (SystemCoreClock / 1000000U))) {
            return DHT11_ERR_TIMEOUT;
        }
    }

    /* ----------------------------------------------------------------------
     * 3. Read 40 Data Bits (5 Bytes)
     * ---------------------------------------------------------------------- */
    for (int byte_idx = 0; byte_idx < 5; byte_idx++) {
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            /* Each bit starts with a 50us LOW pulse: wait until line goes HIGH */
            start_tick = DWT->CYCCNT;
            while (HAL_GPIO_ReadPin(dev->GPIOx, dev->GPIO_Pin) == GPIO_PIN_RESET) {
                if ((DWT->CYCCNT - start_tick) > (80U * (SystemCoreClock / 1000000U))) {
                    return DHT11_ERR_TIMEOUT;
                }
            }

            /* Measure width of HIGH pulse:
             * 26-28us HIGH = '0'
             * 70us HIGH    = '1'
             */
            uint32_t high_start = DWT->CYCCNT;
            while (HAL_GPIO_ReadPin(dev->GPIOx, dev->GPIO_Pin) == GPIO_PIN_SET) {
                if ((DWT->CYCCNT - high_start) > (100U * (SystemCoreClock / 1000000U))) {
                    return DHT11_ERR_TIMEOUT;
                }
            }
            uint32_t high_duration_us = (DWT->CYCCNT - high_start) / (SystemCoreClock / 1000000U);

            if (high_duration_us > 40) {
                data[byte_idx] |= (1U << bit_idx);
            }
        }
    }

    /* ----------------------------------------------------------------------
     * 4. Verify Checksum & Restore Pin to Idle
     * ---------------------------------------------------------------------- */
    DHT11_SetPinMode(dev, GPIO_MODE_INPUT, GPIO_PULLUP);

    uint8_t expected_checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (data[4] != expected_checksum) {
        return DHT11_ERR_CHECKSUM;
    }

    /* Data parsed successfully */
    dev->humidity = data[0];     /* Humidity integer byte */
    dev->temperature = data[2];  /* Temperature integer byte */
    dev->last_read_ms = HAL_GetTick();

    return DHT11_OK;
}
