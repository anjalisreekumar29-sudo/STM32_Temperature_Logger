# STM32 HAL Drivers Cheat Sheet

## 1. GPIO
```c
/* Pin Write / Toggle */
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* Pull low (active-low LED ON) */
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* Pull high (LED OFF) */
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

/* Pin Read */
GPIO_PinState state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
if (state == GPIO_PIN_SET) {
    /* Pin is HIGH */
}

/* External Interrupt Callback */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) {
        /* Handle button press / EXTI line 0 */
    }
}
```

---

## 2. UART / Serial Communication

### Retargeting `printf` (GCC & STM32CubeIDE)
Include `<stdio.h>` and implement:
```c
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, 10);
    return ch;
}
```

### Non-blocking Interrupt Reception (1 byte at a time or ring buffer)
```c
uint8_t rx_data;

/* Start receiving */
HAL_UART_Receive_IT(&huart1, &rx_data, 1);

/* Callback invoked automatically upon receive */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        /* Store rx_data in your ring buffer or parse */
        RingBuffer_Put(&rx_ring, rx_data);

        /* Re-enable interrupt for next byte */
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}
```

### DMA Reception with IDLE Line Interrupt (High-throughput packets)
```c
uint8_t dma_rx_buf[256];

/* Start DMA reception */
HAL_UART_Receive_DMA(&huart1, dma_rx_buf, sizeof(dma_rx_buf));
__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

/* In stm32f1xx_it.c USART1_IRQHandler(): */
void USART1_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        uint16_t received_len = sizeof(dma_rx_buf) - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
        ProcessPacket(dma_rx_buf, received_len);
    }
    HAL_UART_IRQHandler(&huart1);
}
```

---

## 3. I2C (Inter-Integrated Circuit)

```c
/* Bus Probe / Ping */
HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, (DEVICE_7BIT_ADDR << 1), 2, 50);
if (status == HAL_OK) {
    /* Device is present and ACKed */
}

/* Raw Transmit and Receive */
HAL_I2C_Master_Transmit(&hi2c1, (DEVICE_ADDR << 1), tx_buf, tx_len, 100);
HAL_I2C_Master_Receive(&hi2c1, (DEVICE_ADDR << 1), rx_buf, rx_len, 100);

/* Memory / Register Read and Write (Sensors, EEPROMs) */
/* Write 1 byte to register 0x1A: */
uint8_t reg_val = 0x55;
HAL_I2C_Mem_Write(&hi2c1, (DEV_ADDR << 1), 0x1A, I2C_MEMADD_SIZE_8BIT, &reg_val, 1, 100);

/* Read 6 bytes starting from register 0x3B: */
uint8_t data[6];
HAL_I2C_Mem_Read(&hi2c1, (DEV_ADDR << 1), 0x3B, I2C_MEMADD_SIZE_8BIT, data, 6, 100);
```

---

## 4. SPI (Serial Peripheral Interface)

```c
/* Drive Chip Select (CS / SS) manually via GPIO */
#define CS_LOW()  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET)

/* Full Duplex SPI Transfer */
CS_LOW();
HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, length, 100);
CS_HIGH();
```

---

## 5. Analog-to-Digital Converter (ADC)

### Polling Mode (Single Channel)
```c
HAL_ADC_Start(&hadc1);
if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
    uint32_t raw_adc = HAL_ADC_GetValue(&hadc1);
    float voltage = ((float)raw_adc * 3.3f) / 4095.0f; /* 12-bit ADC */
}
HAL_ADC_Stop(&hadc1);
```

### DMA Continuous Mode (Multi-Channel Scan)
```c
uint32_t adc_buffer[4]; /* 4 channels */
HAL_ADC_Start_DMA(&hadc1, adc_buffer, 4);
/* adc_buffer updates continuously in background without CPU load */
```

---

## 6. LM75 Digital Temperature Sensor
```c
/* Read LM75 2-byte temperature register (0x00) with two's-complement decoding */
float LM75_Read(I2C_HandleTypeDef *hi2c, uint8_t addr_7bit) {
    uint8_t buf[2];
    if (HAL_I2C_Mem_Read(hi2c, addr_7bit << 1, 0x00, I2C_MEMADD_SIZE_8BIT, buf, 2, 50) != HAL_OK)
        return -999.0f;
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    return (raw >> 7) * 0.5f; /* 0.5°C step for 9-bit LM75 */
}
```

---

## 7. 4x4 Matrix Keypad Scanning
```c
/* Rows: Outputs (normally HIGH); Cols: Inputs with Pull-Up (normally HIGH) */
char Keypad_Scan(GPIO_TypeDef* r_port[], uint16_t r_pin[], GPIO_TypeDef* c_port[], uint16_t c_pin[], const char map[4][4]) {
    for (int r = 0; r < 4; r++) {
        HAL_GPIO_WritePin(r_port[r], r_pin[r], GPIO_PIN_RESET);
        for (volatile int d = 0; d < 40; d++) __asm__("nop");
        for (int c = 0; c < 4; c++) {
            if (HAL_GPIO_ReadPin(c_port[c], c_pin[c]) == GPIO_PIN_RESET) {
                HAL_Delay(15); /* Debounce */
                if (HAL_GPIO_ReadPin(c_port[c], c_pin[c]) == GPIO_PIN_RESET) {
                    char key = map[r][c];
                    while (HAL_GPIO_ReadPin(c_port[c], c_pin[c]) == GPIO_PIN_RESET) HAL_Delay(5);
                    HAL_Delay(15);
                    HAL_GPIO_WritePin(r_port[r], r_pin[r], GPIO_PIN_SET);
                    return key;
                }
            }
        }
        HAL_GPIO_WritePin(r_port[r], r_pin[r], GPIO_PIN_SET);
    }
    return '\0';
}
```

---

## 8. 7-Segment Display (Common Cathode / Anode)
```c
/* Lookup table for segments a..g + dp */
static const uint8_t DIGIT_LUT[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void Display_Digit(GPIO_TypeDef *port, uint8_t digit, bool dp, bool common_anode) {
    uint8_t pat = (digit <= 9) ? DIGIT_LUT[digit] : 0x00;
    if (dp) pat |= (1 << 7);
    if (common_anode) pat = ~pat;
    for (int i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(port, (1 << i), (pat & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
```

