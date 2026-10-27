/**
 ******************************************************************************
 * @file    w25q128.c
 * @brief   Driver implementation for Winbond W25Q128 SPI Flash on STM32F1 HAL.
 ******************************************************************************
 */

#include "w25q128.h"
#include <stdio.h>
#include <string.h>

#define SPI_TIMEOUT_MS 200

/* Helper macros for Chip Select */
static inline void CS_Select(W25Q128_t* dev) {
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static inline void CS_Unselect(W25Q128_t* dev) {
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/* Calculate 16-bit record integrity checksum */
static uint16_t CalculateChecksum(const LogRecord_t* rec) {
    return (uint16_t)(rec->magic ^ rec->timestamp_sec ^ (uint32_t)rec->temp_c ^ 
                      ((uint32_t)rec->humidity << 8) ^ rec->record_id);
}

/* --------------------------------------------------------------------------
 * Low-Level SPI Operations
 * -------------------------------------------------------------------------- */

HAL_StatusTypeDef W25Q_Init(W25Q128_t* dev, SPI_HandleTypeDef* hspi, GPIO_TypeDef* cs_port, uint16_t cs_pin) {
    dev->hspi = hspi;
    dev->cs_port = cs_port;
    dev->cs_pin = cs_pin;
    dev->next_write_addr = LOG_START_ADDR;
    dev->total_records = 0;
    dev->initialized = false;
    dev->flash_full = false;

    /* Ensure CS pin is initially deasserted (HIGH) */
    CS_Unselect(dev);
    HAL_Delay(5);

    /* Verify communication by reading JEDEC ID */
    uint8_t mfg, mem_type, cap;
    if (W25Q_ReadJEDECID(dev, &mfg, &mem_type, &cap) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Check manufacturer and capacity bounds (W25Q16: 0x15, W25Q32: 0x16, W25Q64: 0x17, W25Q128: 0x18) */
    if ((mfg != W25Q_JEDEC_MFG_WINBOND && mfg != W25Q_JEDEC_MFG_GD) || (cap < 0x14 || cap > 0x19)) {
        return HAL_ERROR; /* Not a recognized W25QXX device */
    }

    dev->capacity_bytes = 1UL << cap;
    dev->initialized = true;
    return HAL_OK;
}


HAL_StatusTypeDef W25Q_ReadJEDECID(W25Q128_t* dev, uint8_t* mfg, uint8_t* mem_type, uint8_t* capacity) {
    uint8_t cmd = W25Q_CMD_JEDEC_ID;
    uint8_t id[3] = {0, 0, 0};

    CS_Select(dev);
    if (HAL_SPI_Transmit(dev->hspi, &cmd, 1, SPI_TIMEOUT_MS) != HAL_OK) {
        CS_Unselect(dev);
        return HAL_ERROR;
    }
    if (HAL_SPI_Receive(dev->hspi, id, 3, SPI_TIMEOUT_MS) != HAL_OK) {
        CS_Unselect(dev);
        return HAL_ERROR;
    }
    CS_Unselect(dev);

    if (mfg) *mfg = id[0];
    if (mem_type) *mem_type = id[1];
    if (capacity) *capacity = id[2];

    return HAL_OK;
}

HAL_StatusTypeDef W25Q_WaitForBusy(W25Q128_t* dev, uint32_t timeout_ms) {
    uint8_t cmd = W25Q_CMD_READ_STATUS_1;
    uint8_t status = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        CS_Select(dev);
        HAL_SPI_Transmit(dev->hspi, &cmd, 1, SPI_TIMEOUT_MS);
        HAL_SPI_Receive(dev->hspi, &status, 1, SPI_TIMEOUT_MS);
        CS_Unselect(dev);

        if ((status & W25Q_STATUS1_BUSY) == 0) {
            return HAL_OK;
        }
    }
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef W25Q_WriteEnable(W25Q128_t* dev) {
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    CS_Select(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, &cmd, 1, SPI_TIMEOUT_MS);
    CS_Unselect(dev);
    return status;
}

HAL_StatusTypeDef W25Q_EraseSector(W25Q128_t* dev, uint32_t sector_addr) {
    if (W25Q_WriteEnable(dev) != HAL_OK) return HAL_ERROR;

    uint8_t tx_buf[4];
    tx_buf[0] = W25Q_CMD_SECTOR_ERASE;
    tx_buf[1] = (uint8_t)((sector_addr >> 16) & 0xFF);
    tx_buf[2] = (uint8_t)((sector_addr >> 8) & 0xFF);
    tx_buf[3] = (uint8_t)(sector_addr & 0xFF);

    CS_Select(dev);
    HAL_SPI_Transmit(dev->hspi, tx_buf, 4, SPI_TIMEOUT_MS);
    CS_Unselect(dev);

    /* Sector erase typically takes ~45ms; wait with 400ms timeout */
    return W25Q_WaitForBusy(dev, 400);
}

HAL_StatusTypeDef W25Q_ReadData(W25Q128_t* dev, uint32_t addr, uint8_t* buffer, uint32_t size) {
    uint8_t header[4];
    header[0] = W25Q_CMD_READ_DATA;
    header[1] = (uint8_t)((addr >> 16) & 0xFF);
    header[2] = (uint8_t)((addr >> 8) & 0xFF);
    header[3] = (uint8_t)(addr & 0xFF);

    CS_Select(dev);
    if (HAL_SPI_Transmit(dev->hspi, header, 4, SPI_TIMEOUT_MS) != HAL_OK) {
        CS_Unselect(dev);
        return HAL_ERROR;
    }
    HAL_StatusTypeDef status = HAL_SPI_Receive(dev->hspi, buffer, size, SPI_TIMEOUT_MS);
    CS_Unselect(dev);
    return status;
}

HAL_StatusTypeDef W25Q_PageProgram(W25Q128_t* dev, uint32_t page_addr, const uint8_t* data, uint16_t size) {
    if (size > W25Q_PAGE_SIZE) return HAL_ERROR;
    if (W25Q_WriteEnable(dev) != HAL_OK) return HAL_ERROR;

    uint8_t header[4];
    header[0] = W25Q_CMD_PAGE_PROGRAM;
    header[1] = (uint8_t)((page_addr >> 16) & 0xFF);
    header[2] = (uint8_t)((page_addr >> 8) & 0xFF);
    header[3] = (uint8_t)(page_addr & 0xFF);

    CS_Select(dev);
    HAL_SPI_Transmit(dev->hspi, header, 4, SPI_TIMEOUT_MS);
    HAL_SPI_Transmit(dev->hspi, (uint8_t*)data, size, SPI_TIMEOUT_MS);
    CS_Unselect(dev);

    /* Page program takes ~0.7ms - 3ms */
    return W25Q_WaitForBusy(dev, 50);
}

/* --------------------------------------------------------------------------
 * High-Level Persistent Record Logger
 * -------------------------------------------------------------------------- */

HAL_StatusTypeDef FlashLog_Mount(W25Q128_t* dev) {
    if (!dev->initialized) return HAL_ERROR;

    dev->total_records = 0;
    dev->next_write_addr = LOG_START_ADDR;
    dev->flash_full = false;

    LogRecord_t record;
    uint32_t addr = LOG_START_ADDR;

    /* Scan memory sequentially to find the head of the log */
    while (addr < LOG_MAX_ADDR) {
        if (W25Q_ReadData(dev, addr, (uint8_t*)&record, sizeof(LogRecord_t)) != HAL_OK) {
            return HAL_ERROR;
        }

        /* 0xFFFFFFFF means unwritten/erased flash: we found the next free slot! */
        if (record.magic == 0xFFFFFFFF) {
            dev->next_write_addr = addr;
            return HAL_OK;
        }

        /* If record matches magic and checksum, it's a valid historical record */
        if (record.magic == LOG_RECORD_MAGIC) {
            if (record.checksum == CalculateChecksum(&record)) {
                dev->total_records = record.record_id;
            }
        }

        addr += sizeof(LogRecord_t);
    }

    /* If we looped through all reserved sectors without finding empty space */
    dev->next_write_addr = LOG_MAX_ADDR;
    dev->flash_full = true;
    return HAL_OK;
}

HAL_StatusTypeDef FlashLog_SaveRecord(W25Q128_t* dev, uint32_t timestamp_sec, int16_t temp_c, int16_t humidity) {
    if (!dev->initialized) return HAL_ERROR;
    if (dev->flash_full || dev->next_write_addr >= LOG_MAX_ADDR) {
        dev->flash_full = true;
        return HAL_ERROR;
    }

    /* When entering a new 4096-byte sector, erase it before programming */
    if ((dev->next_write_addr % W25Q_SECTOR_SIZE) == 0) {
        if (W25Q_EraseSector(dev, dev->next_write_addr) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    /* Prepare the 16-byte record */
    LogRecord_t record;
    record.magic = LOG_RECORD_MAGIC;
    record.timestamp_sec = timestamp_sec;
    record.temp_c = temp_c;
    record.humidity = humidity;
    record.record_id = (uint16_t)(dev->total_records + 1);
    record.checksum = CalculateChecksum(&record);

    /* Write record to flash */
    if (W25Q_PageProgram(dev, dev->next_write_addr, (const uint8_t*)&record, sizeof(LogRecord_t)) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Advance pointers */
    dev->total_records++;
    dev->next_write_addr += sizeof(LogRecord_t);

    if (dev->next_write_addr >= LOG_MAX_ADDR) {
        dev->flash_full = true;
    }

    return HAL_OK;
}

void FlashLog_DumpSerial(W25Q128_t* dev, UART_HandleTypeDef* huart) {
    if (!dev->initialized || !huart) return;

    char msg[128];
    snprintf(msg, sizeof(msg),
             "\r\n============================================================\r\n"
             "           W25Q128 LOGGED DATA DUMP (CSV FORMAT)            \r\n"
             " Total Records: %lu | Partition Size: %lu KB               \r\n"
             "============================================================\r\n"
             "Index,Elapsed_Time_s,Temperature_C,Humidity_Pct,Status\r\n",
             dev->total_records, (LOG_MAX_SECTORS * W25Q_SECTOR_SIZE) / 1024);
    HAL_UART_Transmit(huart, (uint8_t*)msg, strlen(msg), 500);

    LogRecord_t record;
    uint32_t addr = LOG_START_ADDR;

    while (addr < dev->next_write_addr) {
        if (W25Q_ReadData(dev, addr, (uint8_t*)&record, sizeof(LogRecord_t)) == HAL_OK) {
            if (record.magic == LOG_RECORD_MAGIC) {
                bool valid = (record.checksum == CalculateChecksum(&record));
                snprintf(msg, sizeof(msg), "%u,%lu,%d,%d,%s\r\n",
                         record.record_id, record.timestamp_sec,
                         record.temp_c, record.humidity,
                         valid ? "VALID" : "CHECKSUM_ERROR");
                HAL_UART_Transmit(huart, (uint8_t*)msg, strlen(msg), 200);
            }
        }
        addr += sizeof(LogRecord_t);
    }

    snprintf(msg, sizeof(msg), "========================= END OF DUMP ======================\r\n\r\n");
    HAL_UART_Transmit(huart, (uint8_t*)msg, strlen(msg), 200);
}

HAL_StatusTypeDef FlashLog_EraseAllLogs(W25Q128_t* dev) {
    if (!dev->initialized) return HAL_ERROR;

    for (uint32_t sector = LOG_START_SECTOR; sector < (LOG_START_SECTOR + LOG_MAX_SECTORS); sector++) {
        uint32_t sector_addr = sector * W25Q_SECTOR_SIZE;
        if (W25Q_EraseSector(dev, sector_addr) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    dev->total_records = 0;
    dev->next_write_addr = LOG_START_ADDR;
    dev->flash_full = false;
    return HAL_OK;
}
