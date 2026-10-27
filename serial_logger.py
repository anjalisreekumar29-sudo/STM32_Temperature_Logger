#!/usr/bin/env python3
"""
=============================================================================
STM32 Temperature Logger - PC Companion & Data Logger
=============================================================================
This script reads live temperature data streamed over UART from the STM32
microcontroller, logs data into a timestamped CSV file, and prints real-time
telemetry to the console.

Requirements:
    pip install pyserial

Optional (for live plotting):
    pip install matplotlib
=============================================================================
"""

import sys
import os
import time
import datetime
import serial
import serial.tools.list_ports

DEFAULT_BAUDRATE = 115200

def list_serial_ports():
    """Lists available COM ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("[!] No COM ports detected. Please connect your STM32 / USB-TTL adapter.")
        return []
    print("\n--- Available Serial Ports ---")
    for i, port in enumerate(ports):
        print(f"[{i+1}] {port.device} - {port.description}")
    return ports

def choose_port(ports):
    """Prompts user to select a COM port."""
    if len(ports) == 1:
        print(f"\n[*] Automatically selecting the only available port: {ports[0].device}")
        return ports[0].device

    while True:
        try:
            choice = input("\nSelect COM port number (or press Enter for 1): ").strip()
            if not choice:
                return ports[0].device
            idx = int(choice) - 1
            if 0 <= idx < len(ports):
                return ports[idx].device
            print("Invalid index. Try again.")
        except ValueError:
            print("Please enter a valid number.")

def start_logger(port_name, baudrate=DEFAULT_BAUDRATE):
    now_str = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_filename = f"temperature_log_{now_str}.csv"

    print(f"\n[*] Connecting to {port_name} at {baudrate} baud...")
    try:
        ser = serial.Serial(port=port_name, baudrate=baudrate, timeout=1.0)
    except Exception as e:
        print(f"[!] Error opening serial port {port_name}: {e}")
        return

    print(f"[+] Connected successfully!")
    print(f"[+] Logging temperature data to: {os.path.abspath(csv_filename)}")
    print("[*] Press Ctrl+C at any time to stop logging.\n")

    # Prepare CSV file
    with open(csv_filename, mode="w", newline="", encoding="utf-8") as csv_file:
        csv_file.write("PC_Timestamp,MCU_Time_s,Temp_C,Temp_F,Min_C,Max_C,Status\n")
        csv_file.flush()

        try:
            while True:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue

                pc_now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

                # If the line is CSV formatted: Time_s,Temp_C,Temp_F,Min_C,Max_C,Status
                if "," in line and not line.startswith("Time_s") and not line.startswith("==="):
                    parts = line.split(",")
                    csv_line = f"{pc_now},{line}\n"
                    csv_file.write(csv_line)
                    csv_file.flush()
                    print(f"[{pc_now}] {line}")
                else:
                    # Formatted text line from STM32
                    print(line)
        except KeyboardInterrupt:
            print("\n[*] Logging stopped by user.")
        finally:
            ser.close()
            print(f"[+] Log saved to {csv_filename}")

if __name__ == "__main__":
    available = list_serial_ports()
    if not available:
        sys.exit(1)
    selected_port = choose_port(available)
    start_logger(selected_port)
