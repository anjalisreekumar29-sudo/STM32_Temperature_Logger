# Bare-Metal Startup, Linker Scripts & Flashing Automation

This guide captures low-level techniques from the `STM-32-BASICS-DEMO` suite, including lightweight Cortex-M startup routines, linker scripts, and automated ST-Link flashing scripts.

---

## 1. Lightweight Cortex-M4 Startup Routine (`startup.c`)

When building without STM32CubeIDE full runtime or in standalone toolchains, a minimalist `startup.c` initialises memory, vectors, and FPU before branching to `main()`.

### Key Elements of `startup.c`:
1. **Vector Table (`.isr_vector`)**:
   Top of stack (`_estack`), followed by `Reset_Handler`, fault handlers, and `SysTick_Handler`.
2. **Flash to RAM Data Copy**:
   Copies initialized variables from LMA (Flash `_sidata`) to VMA (SRAM `_sdata` to `_edata`).
3. **BSS Zeroing**:
   Zeros uninitialized variables (`_sbss` to `_ebss`).
4. **VTOR Configuration**:
   Sets `SCB->VTOR = 0x08000000` to anchor interrupt handling to flash.
5. **Cortex-M4 FPU Coprocessor Enable (CP10 & CP11)**:
   Crucial step: Cortex-M4F chips (STM32F4, G4, L4, etc.) trigger a `UsageFault (NOCP)` or `HardFault` if floating-point instructions execute while the FPU is disabled:
   ```c
   volatile uint32_t *SCB_CPACR = (volatile uint32_t *)0xE000ED88;
   *SCB_CPACR |= (0xF << 20); // Full access to CP10 and CP11
   ```

### Startup Code Template:
```c
#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void);
void Default_Handler(void) { while (1); }
void SysTick_Handler(void);
int main(void);

__attribute__((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    Default_Handler, /* NMI */
    Default_Handler, /* HardFault */
    Default_Handler, /* MemManage */
    Default_Handler, /* BusFault */
    Default_Handler, /* UsageFault */
    0, 0, 0, 0,
    Default_Handler, /* SVCall */
    Default_Handler, /* DebugMon */
    0,
    Default_Handler, /* PendSV */
    SysTick_Handler, /* SysTick */
};

void Reset_Handler(void)
{
    // 1. Copy data section
    uint32_t *pSrc = &_sidata;
    uint32_t *pDst = &_sdata;
    while (pDst < &_edata) *pDst++ = *pSrc++;

    // 2. Clear bss section
    pDst = &_sbss;
    while (pDst < &_ebss) *pDst++ = 0;

    // 3. Set Vector Table Offset Register
    *((volatile uint32_t *)0xE000ED08) = 0x08000000;

    // 4. Enable FPU (Full Access to CP10/CP11)
    *((volatile uint32_t *)0xE000ED88) |= (0xF << 20);

    // 5. Call main
    main();

    while (1);
}
```

---

## 2. Minimal Linker Script (`linker.ld`)

```ld
ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM
}
```

---

## 3. Automated Command-Line Flashing via ST-LINK

The `STM32_Programmer_CLI` allows one-command programming without opening graphical IDEs.

### Hotplug SWD Programming Syntax:
```powershell
# Flash raw binary at 0x08000000 and reset:
STM32_Programmer_CLI -c port=SWD freq=4000 mode=HOTPLUG -e all -w "firmware.bin" 0x08000000 -v -rst

# Flash ELF directly (preserves address sections):
STM32_Programmer_CLI -c port=SWD freq=4000 mode=HOTPLUG -d "firmware.elf" -v -rst
```

### Parameter Breakdown:
- `-c port=SWD freq=4000`: Connect over Serial Wire Debug at 4.0 MHz clock.
- `mode=HOTPLUG`: Connects without asserting hardware NRST line, preventing target lockups when reset pin is unconnected or floating.
- `-e all`: Erases all flash sectors before write.
- `-w <file> 0x08000000`: Writes binary to base flash address.
- `-v`: Verifies programmed flash memory against binary.
- `-rst`: Issues software system reset so the target begins running immediately.

---

## 4. ST-Link USB Driver Installation

If ST-Link hardware is connected via USB but not recognized (Device Manager code 28 / yellow exclamation):

1. **Driver Source**:
   Use driver package **STSW-LINK009** (bundled in the demo repository at `DRIVER/stsw-link009.zip` or installed with STM32CubeIDE).
2. **Installation Command (Administrator)**:
   ```powershell
   # Extract stsw-link009.zip
   Expand-Archive -Path "DRIVER\stsw-link009.zip" -DestinationPath "$env:TEMP\stlink_driver" -Force
   
   # Run 64-bit driver installer silently or interactively:
   Start-Process -FilePath "$env:TEMP\stlink_driver\dpinst_amd64.exe" -Wait
   ```
