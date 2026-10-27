# STM32CubeMX Configuration Guidelines

## 1. Clock Configuration Presets

### STM32F103 (Blue Pill) - 72 MHz Max
- **Crystal Oscillator (HSE)**: 8.0 MHz
- **PLL Source**: HSE
- **PLL Multiplier**: `x9` -> 72 MHz
- **AHB Prescaler**: `/1` (HCLK = 72 MHz)
- **APB1 Prescaler**: `/2` (PCLK1 = 36 MHz, APB1 Timers = 72 MHz)
- **APB2 Prescaler**: `/1` (PCLK2 = 72 MHz)
- **Flash Latency**: 2 Wait States (WS)

### STM32F401 (Black Pill) - 84 MHz Max
- **HSE**: 25.0 MHz (most Black Pills have 25MHz crystal)
- **PLLM**: `/25` (1 MHz input to PLL)
- **PLLN**: `x168` (VCO = 168 MHz)
- **PLLP**: `/2` (SYSCLK = 84 MHz)
- **AHB**: `/1` (HCLK = 84 MHz)
- **APB1**: `/2` (PCLK1 = 42 MHz)
- **APB2**: `/1` (PCLK2 = 84 MHz)

### STM32F411 (Black Pill) - 100 MHz Max
- **HSE**: 25.0 MHz
- **PLLM**: `/25`
- **PLLN**: `x200`
- **PLLP**: `/2` (SYSCLK = 100 MHz)

---

## 2. SYS & Debug Configuration
Always ensure in **Pinout & Configuration -> System Core -> SYS**:
- **Debug**: Set to **Serial Wire** (`PA13 = SWDIO`, `PA14 = SWCLK`).
- *Warning*: If Debug is left as "No Debug", programming the board once disables SWD pins, locking you out from flashing again unless you hold `BOOT0` high and press `Reset`!

---

## 3. Peripheral Mode Guidelines

| Peripheral | Recommended Pin Mode | Pull-Up / Pull-Down | Notes |
|---|---|---|---|
| **I2C SCL/SDA** | Open-Drain | Pull-up (External 4.7k preferred) | Required for bus contention arbitration |
| **1-Wire (DS18B20)** | Output Open-Drain | Internal Pull-up + **External 4.7k** | Fast rise time needed for 1-Wire slots |
| **SPI MOSI/SCK** | Push-Pull Alternative | No-Pull | Driven by master |
| **SPI MISO** | Input Floating / Pull-Up | Pull-up | Driven by slave |
| **UART TX** | Alternate Function PP | No-Pull | Idle state is HIGH |
| **UART RX** | Input Floating | Pull-up | Prevents noise when cable disconnected |
| **Status LED** | Output Push-Pull | No-Pull | Fast/Low speed |
