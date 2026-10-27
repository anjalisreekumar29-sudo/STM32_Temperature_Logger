# Flashing and Debugging STM32 Devices

## 1. STM32CubeProgrammer CLI Commands

The official ST-Link CLI utility is `STM32_Programmer_CLI.exe` (installed with STM32CubeIDE or STM32CubeProgrammer):

### Probe Connected Target:
```powershell
STM32_Programmer_CLI.exe -c port=SWD
```

### Full Chip Erase:
```powershell
STM32_Programmer_CLI.exe -c port=SWD -e all
```

### Flash Binary / HEX file:
```powershell
# Flash HEX:
STM32_Programmer_CLI.exe -c port=SWD -w firmware.hex -v -rst

# Flash BIN to standard Flash base (0x08000000):
STM32_Programmer_CLI.exe -c port=SWD -w firmware.bin 0x08000000 -v -rst

# Hotplug Mode (prevents target reset lockup when NRST is floating):
STM32_Programmer_CLI.exe -c port=SWD freq=4000 mode=HOTPLUG -e all -w firmware.bin 0x08000000 -v -rst

# Automated Script:
Use the helper script in the skill:
`.\.agents\skills\stm32\scripts\flash_firmware.ps1 -FirmwarePath <path_to_bin_or_elf>`
```


---

## 2. OpenOCD Commands (Alternative open-source tool)

### Flash with OpenOCD (STM32F1x):
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program firmware.elf verify reset exit"
```

### Flash with OpenOCD (STM32F4x):
```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program firmware.elf verify reset exit"
```

---

## 3. SWD Lockout Recovery (ST-Link "No target connected")

If your ST-Link reports "Cannot connect to target" or "No target found":

1. **Hardware Reset under Connect**:
   In STM32CubeProgrammer or STM32CubeIDE Debug Configuration:
   - Change `Reset Mode` from **Software system reset** to **Hardware reset** or **Connect Under Reset**.
2. **BOOT0 Pin Override**:
   - Move the **BOOT0** jumper from `0` to `1` (or hold the BOOT button).
   - Press the hardware **RESET** button. This boots the MCU into internal system memory (ROM bootloader) and frees the SWD debug pins.
   - Flash the new code.
   - Move **BOOT0** back to `0` and press **RESET**.
