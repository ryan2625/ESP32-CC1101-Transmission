/******************************************************************************
 * Author: Ryan Freas
 * Project: Interfacing your ESP32 and CC1101
 *
 * Purpose:
 * A program for transmitting a 315 MHz signal with the CC1101.
 * 
 ******************************************************************************/

#include "esp_log.h"
#include <cstdint>
#include <string>

extern "C" {
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

// Register values from the CC1101 datasheet
// ================= STROBES ================ //
constexpr uint8_t CC1101_STROBE_SRES      = 0x30;
constexpr uint8_t CC1101_STROBE_STX       = 0x35;
constexpr uint8_t CC1101_STROBE_SIDLE     = 0x36;
constexpr uint8_t CC1101_STROBE_SFTX      = 0x3B;
// ========================================== //

// ============ CONFIG REGISTERS ============ //
constexpr uint8_t CC1101_CONFIG_IOCFG0    = 0x02; // GDO0_CFG
constexpr uint8_t CC1101_CONFIG_FIFOTHR   = 0x03; // FIFO_THR

constexpr uint8_t CC1101_CONFIG_SYNC1     = 0x04;
constexpr uint8_t CC1101_CONFIG_SYNC0     = 0x05;
constexpr uint8_t CC1101_CONFIG_PKTLEN    = 0x06;
constexpr uint8_t CC1101_CONFIG_PKTCTRL0  = 0x08; // LENGTH_CONFIG

constexpr uint8_t CC1101_CONFIG_FREQ2     = 0x0D;

constexpr uint8_t CC1101_CONFIG_MDMCFG4   = 0x10; // DRATE_E
constexpr uint8_t CC1101_CONFIG_MDMCFG3   = 0x11; // DRATE_M
constexpr uint8_t CC1101_CONFIG_MDMCFG2   = 0x12; // SYNC_MODE, MOD_FORMAT
constexpr uint8_t CC1101_CONFIG_MDMCFG1   = 0x13; // NUM_PREAMBLE

constexpr uint8_t CC1101_CONFIG_DEVIATN   = 0x15; // DEVIATION_E, DEVIATION_M
constexpr uint8_t CC1101_CONFIG_MCSM1     = 0x17; // TXOFF_MODE
constexpr uint8_t CC1101_CONFIG_PATABLE   = 0x3E;
// =========================================== //

// ============= STATUS REGISTERS ============ //
constexpr uint8_t CC1101_STATUS_TXBYTES   = 0x3A;
constexpr uint8_t CC1101_STATUS_MARCSTATE = 0x35;
// =========================================== //

// =================== FIFO ================= //
constexpr uint8_t CC1101_REG_FIFO         = 0x3F;
// =========================================== //

// ================== VALUES ================= //
// Frequency (315 MHz)
constexpr uint8_t CC1101_VALUE_FREQ2      = 0x0C;
constexpr uint8_t CC1101_VALUE_FREQ1      = 0x1D;
constexpr uint8_t CC1101_VALUE_FREQ0      = 0x8A;
// 2-FSK Modulation, Sync Mode
constexpr uint8_t CC1101_VALUE_MDMCFG2    = 0x03;
// Deviation Mantissa, Exponent
constexpr uint8_t CC1101_VALUE_DEVIATN    = 0x40;
// Data rate (25 kBaud)
constexpr uint8_t CC1101_VALUE_MDMCFG4    = 0x89; // DRATE_E
constexpr uint8_t CC1101_VALUE_MDMCFG3    = 0xF8; // DRATE_M
// FIFO threshold config
constexpr uint8_t CC1101_VALUE_IOCFG0     = 0x02;
// Transmit power
constexpr uint8_t CC1101_VALUE_PATABLE    = 0x51;
constexpr uint8_t CC1101_DUMMY_BYTE       = 0x00;
// =========================================== //


uint8_t calculate_header_byte(uint8_t address, bool read, bool burst) {
    /*
    The header byte is comprised of a R/W bit at position 7, burst bit at position 6, 
    and address bits at positions 5-0. We can simply OR the address bits by these
    values to get the resulting header byte. See Assets/bit_shift.png for more info.

    Example: TX FIFO address is 0011 1111 (0x3F) in binary. If we want burst access at this 
    address, we can OR it by a burst bit at position 6 which is 0100 0000 (0x40). 
    The resulting byte is 0111 1111 (0x7F).
    */
    return address | (read ? 0x80 : 0x00) | (burst ? 0x40 : 0x00);
}

void transmit_data(spi_device_handle_t cc1101, const uint8_t* data, size_t len,  const std::string& operation) {
    spi_transaction_t t = {};
    uint8_t rx[len];
    t.tx_buffer = data;
    t.rx_buffer = rx; // rx[0] will always be the Chip Status Byte (section 10.1 of the datasheet)
    t.length = len * 8;
    ESP_ERROR_CHECK(spi_device_polling_transmit(cc1101, &t));
    ESP_LOGI("CC1101", "Operation: %s", operation.c_str());
    for (size_t i = 0; i < len; ++i) {
        ESP_LOGI("CC1101_RX", "rx[%zu] = 0x%02X", i, rx[i]);
    }
};

void initialize_device(spi_device_handle_t cc1101) {
    // Reset chip
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STROBE_SRES},
        1,
        "SRES"
        );
    // Put CC1101 in idle mode
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STROBE_SIDLE},
        1,
        "SIDLE"
        );
    // Flush transmit buffer: in order to send the SFTX strobe, CC1101 must be in certain states (like idle mode)
    transmit_data(
        cc1101,
        (uint8_t[]){CC1101_STROBE_SFTX},
        1,
        "SFTX"
        );
};
// To write c++ in the ESP-IDF, we have to include extern "C"
extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI("MAIN", "Hello World...?");

    // ===================== CONFIGURE BUS SECTION ========================= //
    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = GPIO_NUM_23;
    busConfig.miso_io_num = GPIO_NUM_19;
    busConfig.sclk_io_num = GPIO_NUM_18;
    busConfig.quadwp_io_num = -1; 
    busConfig.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &busConfig, SPI_DMA_DISABLED));
    // ===================== END CONFIGURE BUS SECTION ===================== //

    // ===================== CONFIGURE DEVICE SECTION ====================== //
    spi_device_interface_config_t deviceConfig = {};
    spi_device_handle_t cc1101; 
    deviceConfig.command_bits = 0; 
    deviceConfig.address_bits = 0;
    deviceConfig.dummy_bits = 0;
    deviceConfig.mode = 0;
    deviceConfig.clock_speed_hz = 1000000; // 1 MHz
    deviceConfig.spics_io_num = GPIO_NUM_5; // Chip Select Pin
    deviceConfig.queue_size = 1; // Program doesn't queue anything, only synchronous methods used
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &deviceConfig, &cc1101));
    // =================== END CONFIGURE DEVICE SECTION ===================== //

    // =================== CONFIGURE PARAMETERS SECTION ===================== //
    initialize_device(cc1101);
    // FREQUENCY
    transmit_data(
        cc1101,
        (uint8_t[]){
            calculate_header_byte(CC1101_CONFIG_FREQ2, false, true),
            CC1101_VALUE_FREQ2,
            CC1101_VALUE_FREQ1,
            CC1101_VALUE_FREQ0,
        },
        4,
        "FREQUENCY"
    );
    // MODULATION
    transmit_data(
        cc1101,
        (uint8_t[]){
            calculate_header_byte(CC1101_CONFIG_MDMCFG2, false, false),
            CC1101_VALUE_MDMCFG2
        },
        2,
        "MOD FORMAT"
    );
    transmit_data(
        cc1101,
        (uint8_t[]){
            calculate_header_byte(CC1101_CONFIG_DEVIATN, false, false),
            CC1101_VALUE_DEVIATN 
        },
        2,
        "MOD DEVIATION"
    );
    // DATA RATE
    transmit_data(
        cc1101,
        (uint8_t[]){
            calculate_header_byte(CC1101_CONFIG_MDMCFG4, false, true),
            CC1101_VALUE_MDMCFG4,
            CC1101_VALUE_MDMCFG3
        },
        3,
        "DATA RATE"
    );
    // POWER
    transmit_data(
        cc1101,
        (uint8_t[]){
            calculate_header_byte(CC1101_CONFIG_PATABLE, false, false),
            CC1101_VALUE_PATABLE
        },
        2,
        "POWER"
    );
    // ================= END CONFIGURE PARAMETERS SECTION =================== //

    // ====================== CONFIGURE FIFO SECTION ======================== //

    // ==================== END CONFIGURE FIFO SECTION ====================== //

}