/**
 ******************************************************************************
 * @file    dht11.h
 * @brief   Header for DHT11 Temperature and Humidity Sensor Driver (STM32F1 HAL).
 *          Uses ARM Cortex-M DWT Cycle Counter for precise microsecond delays.
 ******************************************************************************
 */

#ifndef DHT11_H_
#define DHT11_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* DHT11 Status Return Codes */
typedef enum {
    DHT11_OK = 0,
    DHT11_ERR_NO_RESPONSE,  /* Sensor did not pull line low after start signal */
    DHT11_ERR_TIMEOUT,      /* Bus stayed high or low longer than allowed */
    DHT11_ERR_CHECKSUM      /* Data corrupted in transit */
} DHT11_Status_t;

/* DHT11 Device Structure */
typedef struct {
    GPIO_TypeDef* GPIOx;        /* e.g., GPIOA */
    uint16_t      GPIO_Pin;     /* e.g., GPIO_PIN_1 */
    uint8_t       temperature;  /* Temperature in degrees Celsius (0 to 50 C) */
    uint8_t       humidity;     /* Relative humidity in percent (20 to 90 %) */
    uint32_t      last_read_ms; /* Timestamp of last reading */
} DHT11_t;

/* Public API */
void           DHT11_Init(DHT11_t* dev, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
DHT11_Status_t DHT11_Read(DHT11_t* dev);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H_ */
