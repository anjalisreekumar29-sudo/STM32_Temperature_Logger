# Sensors & Actuators Runbook (STM32 HAL & Direct Register)

This reference document contains battle-tested driver patterns for LM75 digital temperature sensors, PCF8574 I2C LCD displays, relay modules, and LDR light sensors, learned from the `STM-32-BASICS-DEMO` suite.

---

## 1. LM75 Digital Temperature Sensor (I2C)

The LM75 / LM75A is an industry-standard digital temperature sensor with an I2C interface.

### I2C Addressing:
- Base address: `0x48` (when A0, A1, A2 are grounded or left open).
- Address range: `0x48` to `0x4F` (depending on A0..A2 pin states).
- 8-bit HAL write address: `(0x48 << 1) = 0x90`
- 8-bit HAL read address:  `((0x48 << 1) | 1) = 0x91`

### Register Map:
- `0x00`: Temperature Register (Read-only, 16-bit, MSB first)
- `0x01`: Configuration Register (Read/Write, 8-bit)
- `0x02`: $T_{\text{HYST}}$ Register (Read/Write, 16-bit)
- `0x03`: $T_{\text{OS}}$ Over-temperature Shutdown Register (Read/Write, 16-bit)

### HAL Implementation:
```c
#define LM75_I2C_ADDR_8BIT   (0x48 << 1)
#define LM75_REG_TEMP        0x00

float LM75_ReadTemperature(I2C_HandleTypeDef *hi2c)
{
    uint8_t raw_data[2] = {0};
    
    // Read 2 bytes from temperature register 0x00
    if (HAL_I2C_Mem_Read(hi2c, LM75_I2C_ADDR_8BIT, LM75_REG_TEMP, 
                         I2C_MEMADD_SIZE_8BIT, raw_data, 2, 100) != HAL_OK)
    {
        return -999.0f; // Bus error
    }

    // Combine MSB and LSB
    int16_t temp_raw = (int16_t)((raw_data[0] << 8) | raw_data[1]);
    
    // Most LM75 sensors output 9-bit resolution (bits 15..7) -> 0.5°C step
    // LM75A outputs 11-bit resolution (bits 15..5) -> 0.125°C step
    // Shift arithmetic preserves two's-complement sign for negative temps:
    float temperature_c = (temp_raw >> 7) * 0.5f;
    
    return temperature_c;
}
```

### Auto-Probe LM75 Address:
```c
uint8_t LM75_ProbeAddress(I2C_HandleTypeDef *hi2c)
{
    for (uint8_t addr = 0x48; addr <= 0x4F; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 2, 20) == HAL_OK) {
            return addr;
        }
    }
    return 0; // Not found
}
```

---

## 2. PCF8574 I2C Character LCD (16x2 & 20x4)

Character LCDs (HD44780) driven via a PCF8574 8-bit I2C GPIO expander use a 4-bit nibble protocol.

### PCF8574 Pin Mapping to LCD:
- `Bit 0`: RS (Register Select: 0 = Command, 1 = Data)
- `Bit 1`: RW (Read/Write: 0 = Write, usually tied to 0)
- `Bit 2`: EN (Enable Strobe: active high pulse)
- `Bit 3`: Backlight LED (1 = ON, 0 = OFF)
- `Bits 4..7`: High 4 bits data bus (DB4, DB5, DB6, DB7)

### Common I2C Addresses:
- `0x27` (PCF8574T)
- `0x3F` (PCF8574AT)

### Driver Pattern (HAL):
```c
#define LCD_ADDR_DEFAULT (0x27 << 1)
#define LCD_BACKLIGHT    0x08
#define LCD_ENABLE       0x04

static void LCD_WriteNibble(I2C_HandleTypeDef *hi2c, uint8_t addr, uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT | (rs & 0x01);
    uint8_t buf[2];
    
    // Pulse EN HIGH then LOW
    buf[0] = data | LCD_ENABLE;
    buf[1] = data;
    HAL_I2C_Master_Transmit(hi2c, addr, &buf[0], 1, 10);
    for (volatile int i = 0; i < 200; i++) __asm__("nop");
    HAL_I2C_Master_Transmit(hi2c, addr, &buf[1], 1, 10);
    for (volatile int i = 0; i < 200; i++) __asm__("nop");
}

void LCD_SendCmd(I2C_HandleTypeDef *hi2c, uint8_t addr, uint8_t cmd)
{
    LCD_WriteNibble(hi2c, addr, cmd & 0xF0, 0);
    LCD_WriteNibble(hi2c, addr, (cmd << 4) & 0xF0, 0);
    if (cmd == 0x01 || cmd == 0x02) {
        HAL_Delay(2); // Clear and Home commands require >1.52 ms
    }
}

void LCD_SendData(I2C_HandleTypeDef *hi2c, uint8_t addr, uint8_t data)
{
    LCD_WriteNibble(hi2c, addr, data & 0xF0, 1);
    LCD_WriteNibble(hi2c, addr, (data << 4) & 0xF0, 1);
}

void LCD_Init(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    HAL_Delay(50);
    // 4-bit mode initialization sequence:
    LCD_WriteNibble(hi2c, addr, 0x30, 0);
    HAL_Delay(5);
    LCD_WriteNibble(hi2c, addr, 0x30, 0);
    HAL_Delay(1);
    LCD_WriteNibble(hi2c, addr, 0x30, 0);
    HAL_Delay(1);
    LCD_WriteNibble(hi2c, addr, 0x20, 0); // Switch to 4-bit mode
    HAL_Delay(1);

    LCD_SendCmd(hi2c, addr, 0x28); // 2 lines, 5x8 font
    LCD_SendCmd(hi2c, addr, 0x0C); // Display ON, Cursor OFF
    LCD_SendCmd(hi2c, addr, 0x06); // Increment cursor
    LCD_SendCmd(hi2c, addr, 0x01); // Clear display
    HAL_Delay(2);
}

void LCD_SetCursor(I2C_HandleTypeDef *hi2c, uint8_t addr, uint8_t row, uint8_t col)
{
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    LCD_SendCmd(hi2c, addr, 0x80 | (col + row_offsets[row]));
}

void LCD_Print(I2C_HandleTypeDef *hi2c, uint8_t addr, const char *str)
{
    while (*str) {
        LCD_SendData(hi2c, addr, (uint8_t)(*str++));
    }
}
```

---

## 3. Relay Module Control (Actuators)

Relays provide electrical isolation between STM32 logic and high-power/mains loads.

### Critical Safety & Architecture Rules:
1. **Active-LOW Logic (Optocoupler Inputs)**:
   - Most standard 1-channel / 4-channel relay modules use **Active-LOW** triggers:
     - Output **LOW (`0`)** energizes coil $\rightarrow$ Relay ON (COM connected to NO).
     - Output **HIGH (`1`)** de-energizes coil $\rightarrow$ Relay OFF (COM connected to NC).
   - *Boot-up glitch prevention*: Microcontroller pins default to floating/input on reset. Ensure internal or external pull-up keeps pin HIGH during MCU boot to prevent unintended relay pulses.
2. **Flyback Diode & Power Supply**:
   - Never power multiple relay coils directly from STM32 3.3V VDD rail (coils consume 70–100mA each). Power relay VCC from dedicated 5V rail.
3. **Switching Frequency**:
   - Mechanical relay cycles must have minimum delay of $\ge 500\text{ms}$ between switches to avoid mechanical contact bounce and premature wear.

```c
#define RELAY_PORT GPIOA
#define RELAY_PIN  GPIO_PIN_0

static inline void Relay_On(void)  { HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET); }
static inline void Relay_Off(void) { HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET); }
```

---

## 4. HW-072 LDR Comparator Module (Light Sensing)

The HW-072 module pairs an LDR photoresistor with an LM393 voltage comparator and an onboard potentiometer.

### Digital Pin Operation:
- **Output (DO)**:
  - When light falls below threshold (Dark) $\rightarrow$ Comparator DO goes **HIGH**.
  - When light exceeds threshold (Bright) $\rightarrow$ Comparator DO goes **LOW**.
- **Software Debouncing / Hysteresis**:
  Avoid rapid toggling when light level hovers near threshold:
  ```c
  #define LDR_PORT GPIOA
  #define LDR_PIN  GPIO_PIN_1

  bool LDR_IsDarkDebounced(void)
  {
      if (HAL_GPIO_ReadPin(LDR_PORT, LDR_PIN) == GPIO_PIN_SET) {
          HAL_Delay(50); // 50 ms stability window
          if (HAL_GPIO_ReadPin(LDR_PORT, LDR_PIN) == GPIO_PIN_SET) {
              return true;
          }
      }
      return false;
  }
  ```
