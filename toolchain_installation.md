# Installing STM32 Development Tools on Windows

## 1. STM32CubeIDE (Recommended All-in-One IDE)

STM32CubeIDE includes the Eclipse-based IDE, STM32CubeMX graphical pin/clock configurator, GCC ARM compiler toolchain, and GDB debugger.

### Installing from Local Downloads:
An installer zip is already downloaded in your Downloads folder:
- **File**: `C:\Users\User\Downloads\stm32cubeide_2.2.0-Win-x86_64.exe.zip`

### Steps to Install:
1. Extract `stm32cubeide_2.2.0-Win-x86_64.exe.zip`.
2. Run the extracted `st-stm32cubeide_2.2.0_x86_64.exe`.
3. Follow the wizard prompts and accept the default installation directory (`C:\ST\STM32CubeIDE_2.2.0`).
4. Ensure the checkbox for **ST-Link Drivers** is checked during setup.

---

## 2. ST-Link USB Drivers

If Windows Device Manager shows an exclamation mark under "STMicroelectronics STLink Virtual COM Port" or "ST-Link Debug":
1. Download official driver package **STSW-LINK009** from STMicroelectronics or use the bundled driver in `C:\ST\STM32CubeIDE_2.2.0\plugins\com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.win32_...\tools\driver`.
2. Run `dpinst_amd64.exe`.

---

## 3. Alternative Command-Line Toolchain (Winget / Scoop)

If you prefer lightweight command line builds (Makefile / CMake):

```powershell
# Install ARM GNU Toolchain (arm-none-eabi-gcc):
winget install Arm.GnuArmEmbeddedToolchain

# Install Make / CMake / Ninja:
winget install Kitware.CMake
winget install Ninja-build.Ninja

# Install OpenOCD:
winget install OpenOCD.OpenOCD
```
