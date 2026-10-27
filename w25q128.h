/**
 ******************************************************************************
 * @file    w25q128.h
 * @brief   Header for Winbond W25Q128 128M-bit (16MB) SPI Flash Driver
 *          with robust structured record logging for STM32F1 HAL.
 ******************************************************************************
 */

#ifndef W25Q128_H_
#define W25Q128_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* W25Q128 Specifications */
#define W25Q_PAGE_SIZE            256
#define W25Q_SECTOR_SIZE          4096
#define W25Q_BLOCK_SIZE           65536
#define W25Q_FLASH_SIZE           (16UL * 1024UL * 1024UL) /* 16 MB */

/* W25Q128 SPI Commands */
#define W25Q_CMD_WRITE_ENABLE     0x06
#define W25Q_CMD_WRITE_DISABLE    0x04
#define W25Q_CMD_READ_STATUS_1    0x05
#define W25Q_CMD_READ_DATA        0x03
#define W25Q_CMD_PAGE_PROGRAM     0x02
#define W25Q_CMD_SECTOR_ERASE     0x20
#define W25Q_CMD_BLOCK_ERASE_64K  0xD8
#define W25Q_CMD_CHIP_ERASE       0xC7
#define W25Q_CMD_JEDEC_ID         0x9F

/* Status Register 1 Bits */
#define W25Q_STATUS1_BUSY         0x01
#define W25Q_STATUS1_WEL          0x02

/* Recognized JEDEC Manufacturers & Capacities (W25QXX Series) */
#define W25Q_JEDEC_MFG_WINBOND    0xEF
#define W25Q_JEDEC_MFG_GD         0xC8  /* GigaDevice compatible */
#define W25Q_JEDEC_MEMTYPE_SPI    0x40

#define W25Q16_CAPACITY_ID        0x15  /* 2 MB   (16 Mbit)  */
#define W25Q32_CAPACITY_ID        0x16  /* 4 MB   (32 Mbit)  */
#define W25Q64_CAPACITY_ID        0x17  /* 8 MB   (64 Mbit)  */
#define W25Q128_CAPACITY_ID       0x18  /* 16 MB  (128 Mbit) */

/* Logger Partition Config */
#define LOG_RECORD_MAGIC          0xAA55A55A
#define LOG_START_SECTOR          1                         /* Reserve Sector 0 for system info */
#define LOG_START_ADDR            (LOG_START_SECTOR * W25Q_SECTOR_SIZE) /* 0x00001000 */
#define LOG_MAX_SECTORS           500                       /* ~2 MB partition (128,000 records) */
#define LOG_MAX_ADDR              (LOG_START_ADDR + (LOG_MAX_SECTORS * W25Q_SECTOR_SIZE))

/* 16-Byte Packed Binary Log Record */
typedef struct __attribute__((packed)) {
    uint32_t magic;         /* 0xAA55A55A (marker for valid record) */
    uint32_t timestamp_sec; /* Elapsed seconds since start */
    int16_t  temp_c;        /* Temperature in Celsius */
    int16_t  humidity;      /* Relative Humidity in % */
    uint16_t record_id;     /* Sequential record ID */
    uint16_t checksum;      /* Simple checksum for validation */
} LogRecord_t;

/* W25Q128 / W25QXX Handle */
typedef struct {
    SPI_HandleTypeDef* hspi;
    GPIO_TypeDef*      cs_port;
    uint16_t           cs_pin;
    uint32_t           capacity_bytes;  /* Detected flash capacity in bytes */
    uint32_t           next_write_addr; /* Current write pointer */
    uint32_t           total_records;   /* Number of stored valid records */
    bool               initialized;
    bool               flash_full;
} W25Q128_t;


/* Low-level flash API */
HAL_StatusTypeDef W25Q_Init(W25Q128_t* dev, SPI_HandleTypeDef* hspi, GPIO_TypeDef* cs_port, uint16_t cs_pin);
HAL_StatusTypeDef W25Q_ReadJEDECID(W25Q128_t* dev, uint8_t* mfg, uint8_t* mem_type, uint8_t* capacity);
HAL_StatusTypeDef W25Q_WaitForBusy(W25Q128_t* dev, uint32_t timeout_ms);
HAL_StatusTypeDef W25Q_WriteEnable(W25Q128_t* dev);
HAL_StatusTypeDef W25Q_EraseSector(W25Q128_t* dev, uint32_t sector_addr);
HAL_StatusTypeDef W25Q_ReadData(W25Q128_t* dev, uint32_t addr, uint8_t* buffer, uint32_t size);
HAL_StatusTypeDef W25Q_PageProgram(W25Q128_t* dev, uint32_t page_addr, const uint8_t* data, uint16_t size);

/* High-level persistent logger storage API */
HAL_StatusTypeDef FlashLog_Mount(W25Q128_t* dev);
HAL_StatusTypeDef FlashLog_SaveRecord(W25Q128_t* dev, uint32_t timestamp_sec, int16_t temp_c, int16_t humidity);
void              FlashLog_DumpSerial(W25Q128_t* dev, UART_HandleTypeDef* huart);
HAL_StatusTypeDef FlashLog_EraseAllLogs(W25Q128_t* dev);

#ifdef __cplusplus
}
#endif

#endif /* W25Q128_H_ */
