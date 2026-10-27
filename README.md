# STM32 Temperature Data Logger

A reliable, compact embedded temperature data logging system built with STM32 HAL for the **STM32F-series development board (CBT6 / Blue Pill)**. The system measures ambient temperature with a **DHT11 sensor**, displays live readings continuously on a **16×2 I2C LCD**, periodically records data every **5 minutes** into non-volatile **W25QXX SPI Flash memory**, and supports retrieving stored historical records across power cycles.

---

## 1. System Architecture

```
                    +--------------------------------+
                    |    STM32F-Series Microcontroller|
                    |           (CBT6 Package)       |
                    +--------------------------------+
                                |    |    |
           Single-Wire (PA1)    |    |    |   I2C1 (PB6/PB7)
            + 4.7k Pull-up      |    |    |   Address: 0x27 / 0x3F
         +----------------------+    |    +------------------------+
         |                           |                             |
         v                           v                             v
+------------------+        +------------------+         +-------------------+
|   DHT11 Sensor   |        |  Status LED PC13 |         |  16x2 HD44780 LCD |
| (Digital Sensor) |        |  (Heartbeat)     |         | (PCF8574 I2C Mod) |
+------------------+        +------------------+         +-------------------+
                                     |
                                     | SPI1 (PA4..PA7)
                                     v
                            +------------------+
                            | W25QXX SPI Flash |
                            | Non-Volatile Mem |
                            +------------------+
                                     |
                              USART1 (PA9/PA10)
                              115200 Baud 8-N-1
                                     |
                                     v
                            +------------------+
                            |  PC Serial / CSV |
                            |  Record Dump     |
                            +------------------+
```

---

## 2. Hardware Components & Pinout

### A. STM32F CBT6 Pin Mapping Table

| Peripheral | Signal / Pin | STM32 Pin | Function / Notes |
|:---|:---|:---|:---|
| **DHT11 Sensor** | DATA | **PA1** | Single-wire bidir line. **Requires external 4.7 kΩ pull-up to 3.3V!** |
| | VCC | **3.3V** | Power supply (3.3V safe for PA1) |
| | GND | **GND** | System ground |
| **16×2 I2C LCD** | SCL | **PB6** | I2C1 Clock (5V-tolerant pin) |
| | SDA | **PB7** | I2C1 Data (5V-tolerant pin) |
| | VCC | **5V** | 5V ensures clear contrast for HD44780 glass |
| | GND | **GND** | System ground |
| **W25QXX SPI Flash** | CS / NSS | **PA4** | SPI1 Chip Select (GPIO Output Push-Pull) |
| | SCK | **PA5** | SPI1 Serial Clock |
| | MISO | **PA6** | SPI1 Master-In-Slave-Out |
| | MOSI | **PA7** | SPI1 Master-Out-Slave-In |
| | VCC | **3.3V** | **3.3V ONLY** (Never connect 5V to W25QXX!) |
| | GND | **GND** | System ground |
| **ST-LINK V2** | SWCLK | **PA14** | Serial Wire Clock |
| | SWDIO | **PA13** | Serial Wire Data Input/Output |
| | GND | **GND** | Debugger ground |
| | 3.3V | **3.3V** | Target power sense / supply |
| **PC UART Monitor** | TXD | **PA9** | USART1_TX (Connect to USB-TTL RX) |
| | RXD | **PA10** | USART1_RX (Connect to USB-TTL TX) |
| | GND | **GND** | Common serial ground |
| **Heartbeat LED** | LED | **PC13** | Onboard status indicator (Active-LOW) |

---

## 3. Professional Startup & System Self-Check Sequence

On power-up or reset, the system does not jump immediately into the logger screen. Instead, `BootSequence_Run()` executes a self-diagnostic sequence on the 16×2 LCD lasting **~3 seconds**:

```
HAL_Init() -> SystemClock_Config() -> Peripherals Init -> BootSequence_Run() -> Normal Superloop
```

| Stage | LCD Display Line 1 | LCD Display Line 2 | Subsystem / Operation | Critical? |
|:---|:---|:---|:---|:---|
| **1. Power-Up** | `  TEMP LOGGER   ` | `  INITIALIZING  ` | Stabilization delay (500 ms) | Yes |
| **2. Diagnostics** | `  SYSTEM CHECK  ` | `  PLEASE WAIT   ` | Hardware validation begins (400 ms) | Yes |
| **3. DHT11 Check** | ` CHECKING DHT11 ` | ` STATUS: ....   ` $\rightarrow$ `DHT11        OK ` | Single-wire ping on PA1 | **No** (Retries in background if fail) |
| **4. I2C LCD Check**| ` CHECK I2C LCD  ` | `I2C DISPLAY  OK ` | PCF8574 I2C1 communication check | **Yes** |
| **5. Flash Check** | ` CHECK W25QXX   ` | ` FLASH       OK ` | Read W25QXX JEDEC/Device ID | **Yes** |
| **6. Memory Test** | `  MEMORY TEST   ` | ` PASS           ` | Non-destructive read & status test | **Yes** |
| **7. System Status**| `  SYSTEM READY  ` | ` STARTING...    ` | Evaluates subsystem flags | - |
| **8. Normal Screen**| ` TEMP: 27°C     ` | ` LOGGER: READY  ` | Enters temperature superloop | - |

> [!NOTE]
> **Fault Tolerance:** If DHT11 fails during boot, the LCD indicates `DHT11 FAIL / CONTINUING...` and proceeds with the boot. The sensor is periodically polled during normal operation.
>
> If a **critical component** (I2C LCD or W25QXX Flash) fails, the LCD displays `BOOT ERROR / CHECK SYSTEM`, logging is halted, and the onboard LED blinks rapidly at 2 Hz.

---

## 4. Working Mechanism

1. **Measurement**:
   The DHT11 sensor measures temperature and humidity. The STM32 uses the ARM Cortex-M **DWT Cycle Counter** for sub-microsecond timing resolution, ensuring jitter-free pulse reading without taking up a hardware timer.
2. **Live Display**:
   Every **3 seconds**, the live temperature and humidity readings are refreshed on the 16×2 I2C LCD:
   ```
   +----------------+
   | TEMP: 27°C     |
   | LOGGER: READY  |
   +----------------+
   ```
3. **Periodic Persistent Logging**:
   Every **5 minutes (300 seconds)**, the STM32 packages the measurement into a structured 16-byte record (`magic`, `timestamp`, `temp_c`, `humidity`, `record_id`, `checksum`) and saves it to the W25QXX SPI Flash memory.
4. **Non-Volatile Retention**:
   Even if the system is completely powered down, all historical records are preserved in the SPI Flash. Upon restart, `FlashLog_Mount()` scans the partition and resumes logging at the next available sector address without overwriting previous data.
5. **Data Extraction & Monitoring**:
   Connect a serial terminal or the Python companion script at **115200 baud**:
   - Send `'D'` or `'d'` $\rightarrow$ Dumps all stored historical records in clean CSV format.
   - Send `'S'` or `'s'` $\rightarrow$ Displays live system health, total records, and memory status.
   - Send `'L'` or `'l'` $\rightarrow$ Forces an immediate record save.
   - Send `'E'` or `'e'` $\rightarrow$ Erases all historical records.

---

## 5. Building & Flashing

### One-Click Build & Auto-Flash:
```powershell
.\upload.ps1
```
This automatically compiles `codes/*.c` using the ARM GCC toolchain and invokes `flash_firmware.ps1`.

### Standalone ST-LINK Flasher:
```powershell
.\flash_firmware.ps1
```
Automatically detects `build/firmware.bin` or `build/firmware.elf` and programs via OpenOCD / ST-LINK V2.

### STM32CubeIDE Setup:
1. Open or import the project into **STM32CubeIDE**.
2. Select target: `STM32F103CBT6` (or `STM32F103C8T6`).
3. Build Configuration: **Release** or **Debug**.
4. Click **Run -> Debug** or **Run -> Run** to flash via ST-LINK V2.

---

## 5. Applications

- **Room & Ambient Temperature Monitoring**: Continuous tracking for offices, classrooms, and living spaces.
- **Storage & Warehouse Monitoring**: Verifying temperature thresholds for sensitive goods, perishables, and pharmaceuticals.
- **Laboratory & Educational Experiments**: Hands-on demonstrations of embedded protocols (I2C, SPI, 1-Wire/single-wire, UART).
- **Long-term Environmental Profiling**: Capturing diurnal and seasonal temperature variations over weeks and months.
