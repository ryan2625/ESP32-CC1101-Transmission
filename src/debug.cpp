#include "c1101.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstdint>
#include <string>

// This file abstracts logging the reg values. Function definitions
// and byte constants could be moved to the header file to prevent
// code duplication, but I wanted main.cpp to be more self contained.
constexpr uint8_t CC1101_STROBE_SRES      = 0x30;
constexpr uint8_t CC1101_STROBE_STX       = 0x35;
constexpr uint8_t CC1101_STROBE_SIDLE     = 0x36;
constexpr uint8_t CC1101_STROBE_SFTX      = 0x3B;
constexpr uint8_t CC1101_CONFIG_IOCFG0    = 0x02; 
constexpr uint8_t CC1101_CONFIG_FIFOTHR   = 0x03;
constexpr uint8_t CC1101_CONFIG_SYNC1     = 0x04;
constexpr uint8_t CC1101_CONFIG_SYNC0     = 0x05;
constexpr uint8_t CC1101_CONFIG_PKTLEN    = 0x06;
constexpr uint8_t CC1101_CONFIG_PKTCTRL0  = 0x08; 
constexpr uint8_t CC1101_CONFIG_FREQ2     = 0x0D;
constexpr uint8_t CC1101_CONFIG_MDMCFG4   = 0x10;
constexpr uint8_t CC1101_CONFIG_MDMCFG3   = 0x11; 
constexpr uint8_t CC1101_CONFIG_MDMCFG2   = 0x12; 
constexpr uint8_t CC1101_CONFIG_MDMCFG1   = 0x13;
constexpr uint8_t CC1101_CONFIG_DEVIATN   = 0x15; 
constexpr uint8_t CC1101_CONFIG_MCSM1     = 0x17; 
constexpr uint8_t CC1101_CONFIG_PATABLE   = 0x3E;
constexpr uint8_t CC1101_STATUS_TXBYTES   = 0x3A;
constexpr uint8_t CC1101_STATUS_MARCSTATE = 0x35;
constexpr uint8_t CC1101_REG_FIFO         = 0x3F;
constexpr uint8_t CC1101_VALUE_FREQ2      = 0x0C;
constexpr uint8_t CC1101_VALUE_FREQ1      = 0x1D;
constexpr uint8_t CC1101_VALUE_FREQ0      = 0x8A;
constexpr uint8_t CC1101_VALUE_DEVIATN    = 0x40;
constexpr uint8_t CC1101_VALUE_MDMCFG2    = 0x03;
constexpr uint8_t CC1101_VALUE_MDMCFG4    = 0x89;
constexpr uint8_t CC1101_VALUE_MDMCFG3    = 0xF8; 
constexpr uint8_t CC1101_VALUE_MDMCFG1    = 0xF8; 
constexpr uint8_t CC1101_VALUE_SYNC1      = 0xF8; 
constexpr uint8_t CC1101_VALUE_SYNC0      = 0xF8; 
constexpr uint8_t CC1101_VALUE_PKTCTRL0   = 0xF8; 
constexpr uint8_t CC1101_VALUE_TXFIFO     = 0xF8; 
constexpr uint8_t CC1101_VALUE_FIFOTHR    = 0xF8;
constexpr uint8_t CC1101_VALUE_IOCFG0     = 0x02;
constexpr uint8_t CC1101_VALUE_PATABLE    = 0x51;
constexpr uint8_t CC1101_DUMMY_BYTE       = 0x00;
constexpr uint8_t CC1101_VALUE_MCSM0      = 0x14; 
constexpr uint8_t CC1101_CONFIG_MCSM0     = 0x18;

uint8_t calculate__header_byte(uint8_t address, bool read, bool burst) {
    return address | (read ? 0x80 : 0x00) | (burst ? 0x40 : 0x00);
}

void transmit__data(spi_device_handle_t cc1101, const uint8_t* data, size_t len,  const std::string& operation) {
    spi_transaction_t t = {};
    uint8_t rx[len];
    t.tx_buffer = data;
    t.rx_buffer = rx;
    t.length = len * 8;
    ESP_ERROR_CHECK(spi_device_polling_transmit(cc1101, &t));
    char buffer[256] = {0};
    int offset = 0;
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                    "Operation: %s | ", operation.c_str());
    for (size_t i = 0; i < len; ++i) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                        "0x%02X ", rx[i]);
    }
    ESP_LOGI("CC1101", "%s", buffer);
};

void log_reg_values(spi_device_handle_t cc1101) {
    ESP_LOGI("CC1101", "========== ALL CONFIG VALUES ==========");
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_MCSM0, true, false),
            CC1101_DUMMY_BYTE,
        },
        2,
        "READ AUTOCAL"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_FREQ2, true, true),
            CC1101_DUMMY_BYTE,
            CC1101_DUMMY_BYTE,
            CC1101_DUMMY_BYTE,
        },
        4,
        "READ FREQUENCY"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_MDMCFG2, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ MOD FORMAT / SYNC MODE"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_DEVIATN, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ DEVIATION"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_MDMCFG4, true, true),
            CC1101_DUMMY_BYTE,
            CC1101_DUMMY_BYTE
        },
        3,
        "READ DATA RATE"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_PATABLE, true, true),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ POWER"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_SYNC1, true, true),
            CC1101_DUMMY_BYTE,
            CC1101_DUMMY_BYTE
        },
        3,
        "READ SYNC1 SYNC2"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_MDMCFG1, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ PREAMBLE"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_PKTCTRL0, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ PKTCTRL0"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_PKTLEN, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ PKTLEN"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_IOCFG0, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ IOCFG0"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_FIFOTHR, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ FIFOTHR"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_CONFIG_MCSM1, true, false),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ MCSM1"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_STATUS_MARCSTATE, true, true),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ MARCSTATE"
    );
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_STATUS_TXBYTES, true, true),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ TXBYTES"
    );
    ESP_LOGI("CC1101", "GDO0 level: %d", gpio_get_level(GPIO_NUM_4));
}



void log_after_tx(spi_device_handle_t cc1101, int bytes) {
    ESP_LOGI("CC1101", "============ AFTER %d BYTES ============", bytes);
    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_STATUS_MARCSTATE, true, true),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ MARCSTATE"
    );

    transmit__data(
        cc1101,
        (uint8_t[]){
            calculate__header_byte(CC1101_STATUS_TXBYTES, true, true),
            CC1101_DUMMY_BYTE
        },
        2,
        "READ TXBYTES"
    );
    ESP_LOGI("CC1101", "GDO0 level: %d", gpio_get_level(GPIO_NUM_4));
}
