# Keypad & Display Interfacing Runbook (STM32 HAL & Direct Register)

This reference document details the hardware wiring, timing, and scanning algorithms for 4x4 Matrix Keypads and 7-Segment Displays, derived from the `STM-32-BASICS-DEMO` suite.

---

## 1. 4x4 Matrix Keypad Scanning

A 4×4 keypad controls 16 individual buttons using only 8 GPIO pins (4 Rows + 4 Columns) through matrix scanning.

### Hardware Configuration:
- **Rows (4 pins)**: Configured as **Outputs** (Push-Pull or Open-Drain). Normally held **HIGH**.
- **Columns (4 pins)**: Configured as **Inputs with Internal PULL-UP resistors**. Normally read **HIGH**.

### Scanning Algorithm:
1. Drive Row $N$ **LOW** (0V), keeping all other rows **HIGH** ($V_{\text{DD}}$).
2. Small stabilization delay (1–2 $\mu\text{s}$) to allow bus capacitance to settle.
3. Read Columns 1..4:
   - If Column $M$ reads **LOW**, the key at intersection `(Row N, Col M)` is pressed.
4. Drive Row $N$ back **HIGH** and repeat for next row.

```
Keypad Matrix Layout:
       Col 1   Col 2   Col 3   Col 4
Row 1: [ 1 ]   [ 2 ]   [ 3 ]   [ A ]
Row 2: [ 4 ]   [ 5 ]   [ 6 ]   [ B ]
Row 3: [ 7 ]   [ 8 ]   [ 9 ]   [ C ]
Row 4: [ * ]   [ 0 ]   [ # ]   [ D ]
```

### Complete HAL Implementation with Debouncing:

```c
#include "stm32g4xx_hal.h"
#include <stdbool.h>

const char KEY_MAP[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} KeypadPin_t;

// Pin mapping (configure in MX_GPIO_Init)
static const KeypadPin_t ROW_PINS[4] = {
    {GPIOC, GPIO_PIN_13},
    {GPIOC, GPIO_PIN_15},
    {GPIOF, GPIO_PIN_1},
    {GPIOA, GPIO_PIN_1}
};

static const KeypadPin_t COL_PINS[4] = {
    {GPIOA, GPIO_PIN_3},
    {GPIOA, GPIO_PIN_5},
    {GPIOA, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_1}
};

void Keypad_Init(void)
{
    // Set all rows HIGH initially
    for (int r = 0; r < 4; r++) {
        HAL_GPIO_WritePin(ROW_PINS[r].port, ROW_PINS[r].pin, GPIO_PIN_SET);
    }
}

char Keypad_Scan(void)
{
    for (int r = 0; r < 4; r++) {
        // Drive current row LOW
        HAL_GPIO_WritePin(ROW_PINS[r].port, ROW_PINS[r].pin, GPIO_PIN_RESET);
        
        // Settle time for parasitic capacitance on lines
        for (volatile int d = 0; d < 50; d++) __asm__("nop");

        for (int c = 0; c < 4; c++) {
            if (HAL_GPIO_ReadPin(COL_PINS[c].port, COL_PINS[c].pin) == GPIO_PIN_RESET) {
                // Debounce filter (15ms)
                HAL_Delay(15);
                if (HAL_GPIO_ReadPin(COL_PINS[c].port, COL_PINS[c].pin) == GPIO_PIN_RESET) {
                    char pressed_key = KEY_MAP[r][c];
                    
                    // Wait for key release to prevent repeated triggers
                    while (HAL_GPIO_ReadPin(COL_PINS[c].port, COL_PINS[c].pin) == GPIO_PIN_RESET) {
                        HAL_Delay(5);
                    }
                    HAL_Delay(15); // Release debounce

                    // Restore row HIGH
                    HAL_GPIO_WritePin(ROW_PINS[r].port, ROW_PINS[r].pin, GPIO_PIN_SET);
                    return pressed_key;
                }
            }
        }
        // Restore row HIGH
        HAL_GPIO_WritePin(ROW_PINS[r].port, ROW_PINS[r].pin, GPIO_PIN_SET);
    }

    return '\0'; // No key pressed
}
```

---

## 2. 7-Segment Display Control

A 7-segment display contains 8 LEDs (segments `a`, `b`, `c`, `d`, `e`, `f`, `g`, and decimal point `dp`).

```
       -- a --
      |       |
      f       b
      |       |
       -- g --
      |       |
      e       c
      |       |
       -- d --   (dp)
```

### Segment Lookup Table (LUT):
Bits ordered: `[ DP, g, f, e, d, c, b, a ]` (LSB = segment a, MSB = DP):
```c
const uint8_t DIGIT_LUT[10] = {
    0x3F, // 0: a, b, c, d, e, f
    0x06, // 1: b, c
    0x5B, // 2: a, b, d, e, g
    0x4F, // 3: a, b, c, d, g
    0x66, // 4: b, c, f, g
    0x6D, // 5: a, c, d, f, g
    0x7D, // 6: a, c, d, e, f, g
    0x07, // 7: a, b, c
    0x7F, // 8: a, b, c, d, e, f, g
    0x6F  // 9: a, b, c, d, f, g
};
```

### Common Cathode vs. Common Anode Polarity:
- **Common Cathode (CC)**:
  - Common COM pin tied to **GND**.
  - Output **HIGH** lights segment.
  - Pattern used directly: `pattern = DIGIT_LUT[digit]`.
- **Common Anode (CA)**:
  - Common COM pin tied to **$V_{\text{DD}}$ (3.3V)**.
  - Output **LOW** lights segment.
  - Inverted pattern: `pattern = ~DIGIT_LUT[digit]`.

### Hardware Auto-Detection Jumper:
Configure a dedicated detection pin (e.g. `PB0`) with `GPIO_PULLUP`:
- If `PB0` is left open/floating (reads HIGH) $\rightarrow$ **Common Cathode**.
- If `PB0` is jumpered to GND (reads LOW) $\rightarrow$ **Common Anode**.

```c
void Display_Digit(uint8_t digit, bool show_dp, bool is_common_anode)
{
    if (digit > 9) digit = 0;
    
    uint8_t pattern = DIGIT_LUT[digit];
    if (show_dp) {
        pattern |= (1 << 7); // Set DP bit
    }

    if (is_common_anode) {
        pattern = ~pattern; // Invert for Common Anode
    }

    // Write to PA0..PA7 in parallel using BSRR or HAL:
    for (int i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(GPIOA, (1 << i), 
                          (pattern & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
```

### Startup Diagnostics & Heartbeat:
1. **Startup Lamp Test**: Display `8.` (`0xFF`) for 1000ms upon boot to ensure all 8 LED segments are functional.
2. **Heartbeat Indication**: Invert `dp_state = !dp_state` every loop cycle (1Hz) to give a visual watchdog signal that firmware is running.
