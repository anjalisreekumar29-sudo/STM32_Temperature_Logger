import os
import sys
from reportlab.lib.pagesizes import letter
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether, HRFlowable
)
from reportlab.pdfgen import canvas

class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super(NumberedCanvas, self).__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_number(num_pages)
            super(NumberedCanvas, self).showPage()
        super(NumberedCanvas, self).save()

    def draw_page_number(self, page_count):
        self.saveState()
        self.setFont("Helvetica", 9)
        self.setFillColor(colors.HexColor("#718096"))
        
        # Header (pages > 1)
        if self._pageNumber > 1:
            self.drawString(54, 750, "STM32 Temperature Data Logger — Technical Codebase Guide")
            self.setStrokeColor(colors.HexColor("#CBD5E1"))
            self.setLineWidth(0.5)
            self.line(54, 742, 558, 742)

        # Footer
        footer_text = f"Page {self._pageNumber} of {page_count}"
        self.drawRightString(558, 36, footer_text)
        self.drawString(54, 36, "Confidential & Engineering Reference — STM32F CBT6 Architecture")
        self.setStrokeColor(colors.HexColor("#CBD5E1"))
        self.setLineWidth(0.5)
        self.line(54, 48, 558, 48)
        self.restoreState()

def create_guide_pdf(filename):
    doc = SimpleDocTemplate(
        filename,
        pagesize=letter,
        leftMargin=54,
        rightMargin=54,
        topMargin=54,
        bottomMargin=54
    )

    styles = getSampleStyleSheet()
    
    # Custom styles
    primary_color = colors.HexColor("#1E3A8A")   # Deep navy blue
    secondary_color = colors.HexColor("#0284C7") # Teal blue
    dark_neutral = colors.HexColor("#0F172A")    # Slate 900
    body_color = colors.HexColor("#334155")      # Slate 700
    accent_bg = colors.HexColor("#F8FAFC")       # Slate 50
    border_color = colors.HexColor("#E2E8F0")

    title_style = ParagraphStyle(
        'DocTitle',
        parent=styles['Heading1'],
        fontName='Helvetica-Bold',
        fontSize=24,
        leading=28,
        textColor=primary_color,
        spaceAfter=6
    )

    subtitle_style = ParagraphStyle(
        'DocSubTitle',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=12,
        leading=16,
        textColor=secondary_color,
        spaceAfter=15
    )

    h1_style = ParagraphStyle(
        'SectionH1',
        parent=styles['Heading1'],
        fontName='Helvetica-Bold',
        fontSize=15,
        leading=19,
        textColor=primary_color,
        spaceBefore=14,
        spaceAfter=8,
        keepWithNext=True
    )

    h2_style = ParagraphStyle(
        'SectionH2',
        parent=styles['Heading2'],
        fontName='Helvetica-Bold',
        fontSize=12,
        leading=16,
        textColor=secondary_color,
        spaceBefore=10,
        spaceAfter=6,
        keepWithNext=True
    )

    body_style = ParagraphStyle(
        'BodyDark',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9.5,
        leading=13.5,
        textColor=body_color,
        spaceAfter=6
    )

    bullet_style = ParagraphStyle(
        'BulletDark',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9,
        leading=13,
        textColor=body_color,
        leftIndent=15,
        firstLineIndent=-10,
        spaceAfter=4
    )

    code_style = ParagraphStyle(
        'CodeStyle',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=8,
        leading=11,
        textColor=colors.HexColor("#0F172A")
    )

    table_header_style = ParagraphStyle(
        'TableHeader',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=9,
        leading=11,
        textColor=colors.white
    )

    table_body_style = ParagraphStyle(
        'TableBody',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=11,
        textColor=dark_neutral
    )

    story = []

    # Title Block
    story.append(Spacer(1, 10))
    story.append(Paragraph("STM32 Temperature Data Logger", title_style))
    story.append(Paragraph("Comprehensive Codebase, Hardware & Architecture Guide", subtitle_style))
    story.append(HRFlowable(width="100%", thickness=1.5, color=primary_color, spaceAfter=15))

    # Metadata Box
    meta_data = [
        [Paragraph("<b>Target Hardware:</b> STM32F-Series (CBT6, e.g. STM32F103CBT6)", table_body_style),
         Paragraph("<b>Sensor:</b> DHT11 Digital Sensor (PA1)", table_body_style)],
        [Paragraph("<b>Display:</b> 16x2 HD44780 LCD via PCF8574 I2C (PB6/PB7)", table_body_style),
         Paragraph("<b>Storage:</b> W25QXX SPI NOR Flash (PA4..PA7)", table_body_style)],
        [Paragraph("<b>Clock Frequency:</b> 72 MHz (HSE 8MHz + PLL x9)", table_body_style),
         Paragraph("<b>Telemetry:</b> USART1 (115200 Baud 8-N-1)", table_body_style)]
    ]
    t_meta = Table(meta_data, colWidths=[250, 254])
    t_meta.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor("#F1F5F9")),
        ('BOX', (0,0), (-1,-1), 1, border_color),
        ('INNERGRID', (0,0), (-1,-1), 0.5, border_color),
        ('TOPPADDING', (0,0), (-1,-1), 5),
        ('BOTTOMPADDING', (0,0), (-1,-1), 5),
        ('LEFTPADDING', (0,0), (-1,-1), 8),
        ('RIGHTPADDING', (0,0), (-1,-1), 8),
    ]))
    story.append(t_meta)
    story.append(Spacer(1, 14))

    # 1. Executive Summary & Working Flow
    story.append(Paragraph("1. System Overview & Working Principle", h1_style))
    story.append(Paragraph(
        "The STM32 Temperature Data Logger is an autonomous embedded monitoring instrument designed around "
        "the STM32F-series microcontroller (CBT6 package). It performs three core operations continuously: "
        "accurate ambient sensing, real-time visual feedback, and persistent non-volatile data logging.",
        body_style
    ))
    story.append(Paragraph("<b>Operational Sequence:</b>", body_style))
    story.append(Paragraph("• <b>Live Sampling (Every 3 seconds):</b> The microcontroller requests a fresh temperature and relative humidity reading from the DHT11 sensor via a single-wire digital protocol. The reading is validated through an 8-bit checksum and immediately refreshed on the 16x2 I2C LCD along with heartbeat status indicator.", bullet_style))
    story.append(Paragraph("• <b>Periodic Logging (Every 5 minutes):</b> Every 300 seconds (300,000 ms), the reading is formatted into a 16-byte packed binary structure with header magic and verification checksum, then written to the external Winbond W25QXX SPI Flash memory.", bullet_style))
    story.append(Paragraph("• <b>Non-Volatile Retention:</b> Recorded history is preserved across complete power shutdowns. Upon system reboot, the storage engine mounts existing records without erasing historical data.", bullet_style))
    story.append(Paragraph("• <b>On-Demand Extraction:</b> Historical records can be extracted at any time through the USART1 serial port in comma-separated values (CSV) format using interactive terminal commands or the provided automated PowerShell/Python tools.", bullet_style))

    story.append(Spacer(1, 10))

    # 2. Hardware Interfacing Matrix
    story.append(Paragraph("2. Hardware Pinout & Interfacing Matrix", h1_style))
    story.append(Paragraph(
        "The table below details all physical pin assignments, electrical characteristics, and interfacing notes for the STM32F CBT6 microcontroller:",
        body_style
    ))

    pin_table_data = [
        [Paragraph("Peripheral", table_header_style), Paragraph("Pin", table_header_style), Paragraph("STM32 Port", table_header_style), Paragraph("Mode / Electrical Specification", table_header_style)],
        [Paragraph("<b>DHT11 Sensor</b>", table_body_style), Paragraph("DATA", table_body_style), Paragraph("<b>PA1</b>", table_body_style), Paragraph("Single-wire bidir. <b>Requires 4.7k pull-up to 3.3V</b>", table_body_style)],
        [Paragraph("", table_body_style), Paragraph("VCC / GND", table_body_style), Paragraph("3.3V / GND", table_body_style), Paragraph("3.3V supply ensures safe logic level for PA1", table_body_style)],
        [Paragraph("<b>16x2 I2C LCD</b>", table_body_style), Paragraph("SCL", table_body_style), Paragraph("<b>PB6</b>", table_body_style), Paragraph("I2C1_SCL (100 kHz standard mode, 5V-tolerant)", table_body_style)],
        [Paragraph("", table_body_style), Paragraph("SDA", table_body_style), Paragraph("<b>PB7</b>", table_body_style), Paragraph("I2C1_SDA (PCF8574 backpack, 5V-tolerant)", table_body_style)],
        [Paragraph("", table_body_style), Paragraph("VCC / GND", table_body_style), Paragraph("5V / GND", table_body_style), Paragraph("5V ensures high contrast on HD44780 glass", table_body_style)],
        [Paragraph("<b>W25QXX Flash</b>", table_body_style), Paragraph("CS / NSS", table_body_style), Paragraph("<b>PA4</b>", table_body_style), Paragraph("GPIO Output Push-Pull (Chip Select)", table_body_style)],
        [Paragraph("", table_body_style), Paragraph("SCK / MISO / MOSI", table_body_style), Paragraph("<b>PA5 / PA6 / PA7</b>", table_body_style), Paragraph("SPI1 full-duplex master (Clock, MISO, MOSI)", table_body_style)],
        [Paragraph("", table_body_style), Paragraph("VCC / GND", table_body_style), Paragraph("3.3V / GND", table_body_style), Paragraph("<b>3.3V ONLY.</b> Never connect 5V to W25QXX chips!", table_body_style)],
        [Paragraph("<b>ST-LINK V2</b>", table_body_style), Paragraph("SWCLK / SWDIO", table_body_style), Paragraph("<b>PA14 / PA13</b>", table_body_style), Paragraph("Serial Wire Debug programming & debug port", table_body_style)],
        [Paragraph("<b>UART Serial</b>", table_body_style), Paragraph("TX / RX", table_body_style), Paragraph("<b>PA9 / PA10</b>", table_body_style), Paragraph("USART1 (115200 Baud, 8-N-1) for serial telemetry & CSV", table_body_style)],
        [Paragraph("<b>Status LED</b>", table_body_style), Paragraph("LED", table_body_style), Paragraph("<b>PC13</b>", table_body_style), Paragraph("Onboard heartbeat LED (Active-LOW)", table_body_style)]
    ]
    t_pin = Table(pin_table_data, colWidths=[90, 80, 80, 254])
    t_pin.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), primary_color),
        ('ALIGN', (0,0), (-1,-1), 'LEFT'),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('BOX', (0,0), (-1,-1), 1, border_color),
        ('INNERGRID', (0,0), (-1,-1), 0.5, border_color),
        ('ROWBACKGROUNDS', (0,1), (-1,-1), [colors.white, accent_bg]),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t_pin)

    story.append(PageBreak())

    # 3. Directory Layout & Codebase Structure
    story.append(Paragraph("3. Codebase Architecture & Modular Layout", h1_style))
    story.append(Paragraph(
        "The project has been refactored into a modular, decoupled hierarchy with separate header and source layers:",
        body_style
    ))

    code_layout = """
STM32_Temperature_Logger/
├── library/                 <-- Public Headers & Peripheral API Declarations
│   ├── boot_sequence.h      # Boot sequence API & subsystem status structure
│   ├── dht11.h              # DHT11 driver structures & timings
│   ├── i2c_lcd.h            # PCF8574 LCD commands, addresses & glyphs
│   ├── main.h               # Core system definitions & HAL headers
│   ├── stm32f1xx_hal_conf.h # Hardware abstraction layer configuration
│   └── w25q128.h            # W25QXX SPI Flash commands & log record format
│
├── codes/                   <-- Driver & Application Implementations
│   ├── boot_sequence.c      # 8-stage professional boot & self-check engine
│   ├── dht11.c              # Cycle-accurate single-wire bitbanging via DWT
│   ├── i2c_lcd.c            # 4-bit nibble I2C expander protocol
│   ├── main.c               # System initialization, dual superloop tasks & UART CLI
│   └── w25q128.c            # JEDEC detection, flash read/write/erase & partition manager
│
├── check_environment.ps1    # Host PC diagnostic script for toolchain & COM ports
├── flash_firmware.ps1       # Automated ST-LINK SWD programmer (OpenOCD auto-detect)
├── upload.ps1               # One-click build & flash automation script
├── install_stlink_driver.ps1# Automatic ST-LINK USB driver installer (dpinst_amd64)
├── dump_logs.ps1            # Command-line utility to download flash records to CSV
├── README.md                # System documentation & hardware pinout
└── python_logger/           # Python serial companion & continuous logger
"""
    t_code = Table([[Paragraph(f"<pre>{code_layout.strip()}</pre>", code_style)]], colWidths=[504])
    t_code.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor("#0F172A")),
        ('TEXTCOLOR', (0,0), (-1,-1), colors.HexColor("#38BDF8")),
        ('BOX', (0,0), (-1,-1), 1, colors.HexColor("#1E293B")),
        ('TOPPADDING', (0,0), (-1,-1), 8),
        ('BOTTOMPADDING', (0,0), (-1,-1), 8),
        ('LEFTPADDING', (0,0), (-1,-1), 10),
        ('RIGHTPADDING', (0,0), (-1,-1), 10),
    ]))
    story.append(t_code)
    story.append(Spacer(1, 12))

    # 4. Detailed Module Analysis
    story.append(Paragraph("4. Detailed Module Implementation Analysis", h1_style))

    # A. main.c
    story.append(Paragraph("A. Main Application Superloop (codes/main.c)", h2_style))
    story.append(Paragraph(
        "<b>Role:</b> Manages system initialization, invokes the boot sequence engine, periodic multi-rate execution, non-blocking timing, and UART user command interpretation.<br/>"
        "• <b>Boot Sequence Integration:</b> Calls <code>BootSequence_Run()</code> before starting normal operations. If any critical subsystem fails, execution halts in an error loop with a 2 Hz LED alert.<br/>"
        "• <b>Dual-Rate Timing:</b> Uses <code>HAL_GetTick()</code> state tracking rather than blocking delays. Task 1 refreshes sensor data on LCD every 3,000 ms; Task 2 appends records to W25QXX Flash every 300,000 ms (5 minutes).<br/>"
        "• <b>Interactive UART CLI:</b> Listens via <code>HAL_UART_Receive_IT()</code> on USART1. Supports single-character interactive control: <code>'D'</code> (dump CSV), <code>'S'</code> (status report), <code>'L'</code> (instant log), and <code>'E'</code> (partition erase).",
        body_style
    ))

    # B. boot_sequence.c
    story.append(Paragraph("B. Professional Boot & Hardware Self-Check Engine (codes/boot_sequence.c & library/boot_sequence.h)", h2_style))
    story.append(Paragraph(
        "<b>Role:</b> Executes an 8-stage self-diagnostic sequence on startup within 2–5 seconds with visual feedback on the 16x2 LCD.<br/>"
        "• <b>Stage 1: Power-Up:</b> Displays <code>TEMP LOGGER / INITIALIZING</code> during system voltage stabilization (500 ms).<br/>"
        "• <b>Stage 2: System Check:</b> Displays <code>SYSTEM CHECK / PLEASE WAIT</code> as diagnostic routines begin (400 ms).<br/>"
        "• <b>Stage 3: DHT11 Sensor Test:</b> Pings DHT11 over PA1. Displays <code>DHT11 OK</code> on success. If failed (non-critical), displays <code>DHT11 FAIL / CONTINUING...</code> and boots gracefully.<br/>"
        "• <b>Stage 4: I2C LCD Handshake:</b> Validates I2C1 communication with PCF8574 backpack. Displays <code>CHECK I2C LCD / I2C DISPLAY OK</code> (critical).<br/>"
        "• <b>Stage 5: W25QXX Detection:</b> Reads 3-byte JEDEC ID over SPI1 (PA4..PA7). Displays <code>CHECK W25QXX / FLASH OK</code> (critical).<br/>"
        "• <b>Stage 6: Non-Destructive Memory Test:</b> Reads test signature from reserved Sector 0 (outside log sectors) and validates status register busy flag without erasing existing logs. Displays <code>MEMORY TEST / PASS</code>.<br/>"
        "• <b>Stage 7: System Ready:</b> Displays <code>SYSTEM READY / STARTING...</code> if all critical checks pass, or <code>BOOT ERROR / CHECK SYSTEM</code> if critical components fail.<br/>"
        "• <b>Stage 8: Normal Screen:</b> Seamlessly transitions to temperature monitoring screen: <code>TEMP: 27°C / LOGGER: READY</code>.",
        body_style
    ))

    # C. dht11.c
    story.append(Paragraph("C. DHT11 Sensor Driver (codes/dht11.c & library/dht11.h)", h2_style))
    story.append(Paragraph(
        "<b>Role:</b> Executes single-wire bidirectional digital timing for temperature and humidity measurements.<br/>"
        "• <b>DWT Cycle Counter Precision:</b> Uses ARM Cortex-M <code>DWT->CYCCNT</code> at 72 MHz (13.88 ns per tick) to measure microsecond-level pulse widths accurately without triggering timer interrupt overhead.<br/>"
        "• <b>Protocol Stages:</b> 18 ms host LOW start pulse $\\rightarrow$ 30 $\\mu$s HIGH release $\\rightarrow$ sensor presence handshake (80 $\\mu$s LOW + 80 $\\mu$s HIGH) $\\rightarrow$ 40-bit data stream parsing.<br/>"
        "• <b>Bit Discrimination:</b> 50 $\\mu$s LOW followed by 26–28 $\\mu$s HIGH is a binary <code>'0'</code>; 70 $\\mu$s HIGH is a binary <code>'1'</code>. An 8-bit checksum confirms data integrity.",
        body_style
    ))

    story.append(PageBreak())

    # D. i2c_lcd.c
    story.append(Paragraph("D. I2C 16x2 Character LCD Driver (codes/i2c_lcd.c & library/i2c_lcd.h)", h2_style))
    story.append(Paragraph(
        "<b>Role:</b> Drives standard HD44780 LCD panels over I2C using a PCF8574 8-bit GPIO expander.<br/>"
        "• <b>4-Bit Nibble Protocol:</b> Sends bytes as two sequential 4-bit nibbles with Enable (EN) pulse strobing.<br/>"
        "• <b>Auto-Address Probing:</b> Automatically tests address <code>0x27</code> (0x4E 8-bit) and <code>0x3F</code> (0x7E 8-bit) on boot, automatically adapting to different PCF8574 chip variants.<br/>"
        "• <b>Custom CGRAM Glyphs:</b> Defines a custom 5x8 degree symbol (<code>°</code>) in LCD CGRAM address 0 for clean thermal formatting.",
        body_style
    ))

    # E. w25q128.c
    story.append(Paragraph("E. W25QXX Universal SPI Flash Storage (codes/w25q128.c & library/w25q128.h)", h2_style))
    story.append(Paragraph(
        "<b>Role:</b> High-capacity non-volatile persistent storage for temperature logs across power resets.<br/>"
        "• <b>Universal W25QXX JEDEC Detection:</b> Reads 3-byte JEDEC ID (`0x9F`). Accepts Winbond (`0xEF`) and compatible chips (`0xC8`), dynamically calculating total capacity: <code>capacity_bytes = 1UL << cap</code> (W25Q16 = 2MB, W25Q32 = 4MB, W25Q64 = 8MB, W25Q128 = 16MB).<br/>"
        "• <b>Structured 16-Byte Packed Binary Record:</b>",
        body_style
    ))

    record_table_data = [
        [Paragraph("Offset", table_header_style), Paragraph("Field", table_header_style), Paragraph("Type", table_header_style), Paragraph("Description", table_header_style)],
        [Paragraph("0..3", table_body_style), Paragraph("<code>magic</code>", table_body_style), Paragraph("uint32_t", table_body_style), Paragraph("Header synchronization marker (<code>0xAA55A55A</code>)", table_body_style)],
        [Paragraph("4..7", table_body_style), Paragraph("<code>timestamp_sec</code>", table_body_style), Paragraph("uint32_t", table_body_style), Paragraph("System uptime seconds at time of logging", table_body_style)],
        [Paragraph("8..9", table_body_style), Paragraph("<code>temp_c</code>", table_body_style), Paragraph("int16_t", table_body_style), Paragraph("Temperature in whole degrees Celsius", table_body_style)],
        [Paragraph("10..11", table_body_style), Paragraph("<code>humidity</code>", table_body_style), Paragraph("int16_t", table_body_style), Paragraph("Relative humidity in percentage (%)", table_body_style)],
        [Paragraph("12..13", table_body_style), Paragraph("<code>record_id</code>", table_body_style), Paragraph("uint16_t", table_body_style), Paragraph("Monotonically increasing sequential index", table_body_style)],
        [Paragraph("14..15", table_body_style), Paragraph("<code>checksum</code>", table_body_style), Paragraph("uint16_t", table_body_style), Paragraph("XOR checksum verifying record validity", table_body_style)]
    ]
    t_rec = Table(record_table_data, colWidths=[60, 100, 80, 264])
    t_rec.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), primary_color),
        ('ALIGN', (0,0), (-1,-1), 'LEFT'),
        ('BOX', (0,0), (-1,-1), 1, border_color),
        ('INNERGRID', (0,0), (-1,-1), 0.5, border_color),
        ('ROWBACKGROUNDS', (0,1), (-1,-1), [colors.white, accent_bg]),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t_rec)
    story.append(Spacer(1, 12))

    # 5. Automation & PowerShell Tooling
    story.append(Paragraph("5. Automation Scripts & Development Tooling", h1_style))
    story.append(Paragraph(
        "Five specialized PowerShell scripts located in the project root provide automated hardware diagnostics, compilation, ST-LINK programming, driver setup, and data extraction:",
        body_style
    ))

    scripts_data = [
        [Paragraph("Script", table_header_style), Paragraph("Command Line", table_header_style), Paragraph("Function & Capabilities", table_header_style)],
        [Paragraph("<b>check_environment.ps1</b>", table_body_style), Paragraph("<code>.\\check_environment.ps1</code>", table_body_style), Paragraph("Scans host PC for STM32CubeIDE, GCC ARM compiler, programmer CLI, active COM ports, and ST-Link drivers.", table_body_style)],
        [Paragraph("<b>flash_firmware.ps1</b>", table_body_style), Paragraph("<code>.\\flash_firmware.ps1</code>", table_body_style), Paragraph("Locates <code>STM32_Programmer_CLI</code> and flashes target in SWD Hotplug mode without NRST lockup.", table_body_style)],
        [Paragraph("<b>upload.ps1</b>", table_body_style), Paragraph("<code>.\\upload.ps1</code>", table_body_style), Paragraph("One-click script: builds source code from <code>codes/</code> with headers in <code>library/</code> and flashes via ST-LINK.", table_body_style)],
        [Paragraph("<b>install_stlink_driver.ps1</b>", table_body_style), Paragraph("<code>.\\install_stlink_driver.ps1</code>", table_body_style), Paragraph("Extracts bundled <code>stsw-link009.zip</code> and launches 64-bit installer (<code>dpinst_amd64.exe</code>).", table_body_style)],
        [Paragraph("<b>dump_logs.ps1</b>", table_body_style), Paragraph("<code>.\\dump_logs.ps1 -Port COM3</code>", table_body_style), Paragraph("Connects over UART, sends <code>'D'</code> command to download all Flash records, and saves them to a timestamped CSV file.", table_body_style)]
    ]
    t_scripts = Table(scripts_data, colWidths=[130, 140, 234])
    t_scripts.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), primary_color),
        ('ALIGN', (0,0), (-1,-1), 'LEFT'),
        ('BOX', (0,0), (-1,-1), 1, border_color),
        ('INNERGRID', (0,0), (-1,-1), 0.5, border_color),
        ('ROWBACKGROUNDS', (0,1), (-1,-1), [colors.white, accent_bg]),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t_scripts)

    story.append(Spacer(1, 14))

    # 6. Troubleshooting
    story.append(Paragraph("6. Hardware Verification & Troubleshooting Runbook", h1_style))
    story.append(Paragraph("• <b>DHT11 Returns Error (Code 1 or 2):</b> Check that the external 4.7 kΩ pull-up resistor is firmly connected between PA1 (DATA) and 3.3V. Ensure wire length is under 1 meter to avoid line capacitance degradation.", bullet_style))
    story.append(Paragraph("• <b>LCD Displays Black / White Solid Blocks:</b> Rotate the small blue contrast trimmer potentiometer on the rear of the PCF8574 I2C backpack with a small screwdriver until text is crisp.", bullet_style))
    story.append(Paragraph("• <b>W25QXX Flash Initialization Error:</b> Verify SPI CS pin is PA4 and VCC is strictly 3.3V (5V will damage or disable NOR Flash memory). Check that MISO (PA6) and MOSI (PA7) are not swapped.", bullet_style))
    story.append(Paragraph("• <b>ST-LINK Not Recognized:</b> Run <code>.\\install_stlink_driver.ps1</code> to install the official STSW-LINK009 USB driver package.", bullet_style))

    doc.build(story, canvasmaker=NumberedCanvas)
    print(f"Successfully generated: {filename}")

if __name__ == "__main__":
    out_file = "STM32_Temperature_Logger_Codebase_Guide.pdf"
    if len(sys.argv) > 1:
        out_file = sys.argv[1]
    create_guide_pdf(out_file)
