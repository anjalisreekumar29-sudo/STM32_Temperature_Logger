---
name: stm32-development
description: Comprehensive expert runbook for STM32 firmware development using STM32CubeIDE, CubeMX, HAL/LL drivers, and bare-metal Cortex-M techniques. Use when designing, coding, configuring, building, flashing, or debugging STM32 microcontrollers (STM32F1, F4, G4, L4, H7, etc.), configuring peripherals (GPIO, I2C, SPI, UART, ADC, PWM, Timers, DMA, FreeRTOS, LM75, LCD, Keypad, 7-Segment displays, Relays), or troubleshooting embedded firmware issues.
---

# STM32 Firmware Development Skill

This skill provides practical runbooks, API patterns, hardware interface rules, and troubleshooting procedures for developing robust embedded software on STM32 microcontrollers (including F1, F4, G4, L4, and H7 series).

---

## 1. Golden Rules of STM32 Development

1. **Protect User Code Blocks**:
   When working with STM32CubeMX or STM32CubeIDE code generation, **always** write application code strictly inside the generated comment markers:
   ```c
   /* USER CODE BEGIN 2 */
   // Safe from being wiped when regenerating CubeMX code!
   /* USER CODE END 2 */
   ```
   *Any code written outside these tags will be permanently deleted when CubeMX regenerates the project.*

2. **Open-Drain for Shared Busses**:
   - 1-Wire (DS18B20) and I2C lines (`SCL`, `SDA`) require **Open-Drain** mode (`GPIO_MODE_OUTPUT_OD`) with external pull-up resistors (typically 4.7 kΩ).
   - Never drive a shared single-wire bus in Push-Pull mode; you risk short-circuiting when the slave pulls the bus LOW.

3. **Microsecond Timing via DWT (Not HAL_Delay)**:
   - `HAL_Delay()` uses SysTick and only has **millisecond** resolution.
   - For microsecond timing (e.g. 1-Wire, bit-banging, DHT11/DS18B20, ultrasonic sensors), use the ARM Cortex-M **DWT Cycle Counter**:
     ```c
     CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
     DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
     // Wait us:
     uint32_t ticks = us * (SystemCoreClock / 1000000);
     uint32_t start = DWT->CYCCNT;
     while ((DWT->CYCCNT - start) < ticks);
     ```

4. **FreeRTOS Timebase Rule**:
   - When enabling FreeRTOS in CubeMX, change the HAL Timebase Source in **SYS -> Timebase Source** from `SysTick` to a hardware timer (e.g. `TIM1` or `TIM4`).
   - FreeRTOS takes over `SysTick` exclusively; having both HAL and RTOS share SysTick can cause deadlock or priority inversion.

5. **Keypad Row Settling & Debounce Filter**:
   - In 4x4 matrix scanning, driving a row LOW charges parasitic line capacitance. Insert a small settling pause (40 NOPs / $\sim 1\text{--}2\mu\text{s}$) before reading column inputs to avoid ghost keys.
   - Require a 15ms stable debounce filter and wait for key release before registering input.

6. **Relay Active-LOW Safety & Boot Glitch Prevention**:
   - Standard optocoupler relay modules trigger on **Active-LOW** (`GPIO_PIN_RESET` = ON, `GPIO_PIN_SET` = OFF).
   - Always configure the relay control pin with a Pull-Up (`GPIO_PULLUP`) or initialize it HIGH immediately during `MX_GPIO_Init` to prevent brief relay chattering during boot.

7. **Cortex-M4/M7 FPU Enable in Startup**:
   - Cortex-M4F/M7 microcontrollers will trigger a `UsageFault` (`NOCP`) or `HardFault` if floating-point instructions execute while the FPU coprocessor is disabled.
   - Enable CP10 & CP11 in `SCB->CPACR` before executing application code:
     ```c
     *((volatile uint32_t *)0xE000ED88) |= (0xF << 20);
     ```

8. **Display Polarity Auto-Detection**:
   - 7-Segment displays can be Common Cathode (CC, active-HIGH segments) or Common Anode (CA, active-LOW segments).
   - Check a config pin (e.g. `PB0` with pull-up) or configuration flag to conditionally invert the segment bit pattern (`~pattern`) and run a startup lamp test (`8.`) for 1 second.

---

## 2. Standard HAL API Quick Reference

### GPIO
```c
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  // Set LOW
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    // Set HIGH
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);                 // Toggle
GPIO_PinState state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0); // Read
```

### LM75 Digital Temperature Reading (I2C)
```c
uint8_t raw[2];
if (HAL_I2C_Mem_Read(&hi2c1, (0x48 << 1), 0x00, I2C_MEMADD_SIZE_8BIT, raw, 2, 100) == HAL_OK) {
    int16_t temp_raw = (int16_t)((raw[0] << 8) | raw[1]);
    float temp_c = (temp_raw >> 7) * 0.5f; // 0.5°C step for 9-bit LM75
}
```

### UART
```c
// Blocking Transmit:
HAL_UART_Transmit(&huart1, (uint8_t*)"Hello\r\n", 7, 100);

// Retarget printf to UART in GCC/CubeIDE:
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, 10);
    return ch;
}

// Non-blocking Interrupt Receive:
HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
```

### I2C Master Transmit & Memory Operations
```c
// Check if device is ready on bus:
if (HAL_I2C_IsDeviceReady(&hi2c1, dev_addr, 2, 50) == HAL_OK) { /* found */ }

// Read/Write raw bytes:
HAL_I2C_Master_Transmit(&hi2c1, dev_addr, tx_data, len, 100);
HAL_I2C_Master_Receive(&hi2c1, dev_addr, rx_data, len, 100);

// Read/Write peripheral internal registers (e.g., EEPROM, sensors, RTC):
HAL_I2C_Mem_Write(&hi2c1, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 100);
HAL_I2C_Mem_Read(&hi2c1, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 100);
```

---

## 3. Peripheral Troubleshooting & Fault Analysis

### A. I2C Bus Hangs / `HAL_I2C_STATE_BUSY`
- **Cause**: Slave device was interrupted mid-read and is holding `SDA` low, permanently blocking the STM32 hardware I2C state machine.
- **Fix**: Reconfigure `SCL` and `SDA` pins as GPIO outputs, toggle `SCL` 9 times to send 9 clock pulses until slave releases `SDA`, issue a STOP condition, and re-initialize I2C.

### B. DS18B20 / 1-Wire Returns -127°C or 85°C
- **-127°C (or No Response)**: Missing 4.7k pull-up resistor or disconnected sensor wire.
- **+85°C**: Power-on reset value. The sensor has not been given enough time to complete temperature conversion (takes 750ms for 12-bit). Issue `0x44` (Convert) and wait $\ge 750$ ms before reading scratchpad.

### C. HardFault Handler Crashes
When execution lands in `HardFault_Handler`:
1. Check `SCB->HFSR` (HardFault Status Register) and `SCB->CFSR` (Configurable Fault Status Register).
2. Look for `NOCP` (UsageFault: coprocessor disabled - often FPU used in F4/G4 without calling `SCB->CPACR |= (0xF << 20)`).
3. Check `BFARVALID` and inspect `SCB->BFAR` for invalid pointer dereferencing or unaligned memory access.

---

## 4. Deep-Dive References

For detailed code samples, CLI flash scripts, and peripheral configuration cheat sheets:
- [Sensors & Actuators Runbook](./references/sensors_and_actuators.md): LM75 temperature, PCF8574 LCD, Relays, HW-072 LDR.
- [Keypad & Displays Runbook](./references/keypad_and_displays.md): 4x4 Matrix Keypad scanning, Debouncing, 7-Segment Display LUTs & Polarity.
- [Bare-Metal Startup & Linker Scripts](./references/baremetal_and_scripts.md): Minimal `startup.c`, FPU enable, `linker.ld`, ST-Link Hotplug flashing.
- [HAL Drivers Cheat Sheet](./references/hal_cheat_sheet.md)
- [CubeMX Setup & Clock Tree Guide](./references/cubemx_rules.md)
- [Flashing & Debugging (ST-Link & OpenOCD)](./references/debugging_and_flashing.md)
- [PC Toolchain & Driver Installation](./references/toolchain_installation.md)

---

## 5. Helper Scripts & Bundled Drivers

- [Automated Firmware Flasher](./scripts/flash_firmware.ps1): Locates `STM32_Programmer_CLI` and flashes target via ST-LINK (SWD, Hotplug, Full Erase, Verify, Reset).
- [ST-Link Driver Installer](./scripts/install_stlink_driver.ps1): Extracts and runs the official STSW-LINK009 USB driver installer.
- [Check Environment Script](./scripts/check_environment.ps1): Diagnostics for IDE, toolchain, downloaded installers, and CLI tools.
- [Bundled ST-Link USB Driver](./drivers/stsw-link009.zip): Offline driver package for ST-LINK V2/V2-1/V3.
