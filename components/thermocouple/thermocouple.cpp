#include "thermocouple/thermocouple.hpp"

#include <cstdint>

#include "board/board.hpp"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace thermocouple {
namespace {

constexpr const char* kTag = "thermocouple";
constexpr int kSpiHz = 4'000'000;  // Below the MAX6675's 4.3 MHz limit.
// A frame takes about 8 us at 4 MHz. A tick-based wait can end up to one tick early, and
// Wi-Fi interrupts can delay the task, so the result wait is generous; both waits together
// (at most ~70 ms) still fit well inside the 250 ms sample period.
constexpr TickType_t kQueueWait = pdMS_TO_TICKS(20);
constexpr TickType_t kResultWait = pdMS_TO_TICKS(50);
constexpr TickType_t kRetryInterval = pdMS_TO_TICKS(5000);
constexpr TickType_t kWarnInterval = pdMS_TO_TICKS(10000);
constexpr core::TcReading kBusError{{0}, std::nullopt, core::TcFault::BusError};

#if CONFIG_PROBE_TC_MAX6675
constexpr int kFrameBits = 16;
#else
constexpr int kFrameBits = 32;
constexpr bool kGroundedProbe =
#if CONFIG_PROBE_TC_GROUNDED
    true;
#else
    false;
#endif
#endif

// The sensor task is the only caller. The SPI driver may retain the descriptor
// and its inline RX buffer after a result timeout.
spi_transaction_t transaction{};
bool in_flight = false;
bool bus_ready = false;
spi_device_handle_t device = nullptr;
TickType_t last_init_attempt = 0;
TickType_t last_warning = 0;
bool warned = false;

core::TcReading spi_error(const char* operation, esp_err_t error) {
    const TickType_t now = xTaskGetTickCount();
    if (!warned || now - last_warning >= kWarnInterval) {
        ESP_LOGW(kTag, "%s: %s", operation, esp_err_to_name(error));
        last_warning = now;
        warned = true;
    }
    return kBusError;
}

}  // namespace

esp_err_t init() {
    if (device != nullptr) {
        return ESP_OK;
    }
    last_init_attempt = xTaskGetTickCount();

    const auto& pins = board::kBoard.pins;
    if (!bus_ready) {
        spi_bus_config_t bus_config{};
        bus_config.mosi_io_num = -1;  // The converters only send data.
        bus_config.miso_io_num = pins.tc_miso;
        bus_config.sclk_io_num = pins.tc_sck;
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.max_transfer_sz = kFrameBits / 8;
        const esp_err_t error = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_DISABLED);
        if (error != ESP_OK) {
            return error;
        }
        bus_ready = true;
    }

    // Keep the initialized bus if adding the device fails, so a retry can resume here.
    spi_device_interface_config_t device_config{};
    device_config.mode = 0;  // Both converters change SO on falling edges.
    device_config.clock_speed_hz = kSpiHz;
    device_config.spics_io_num = pins.tc_cs;
    device_config.queue_size = 1;
    return spi_bus_add_device(SPI2_HOST, &device_config, &device);
}

core::TcReading read() {
    if (device == nullptr) {
        if (xTaskGetTickCount() - last_init_attempt >= kRetryInterval) {
            const esp_err_t error = init();
            if (error != ESP_OK) {
                return spi_error("SPI initialization retry", error);
            }
        }
        return kBusError;
    }

    spi_transaction_t* completed = nullptr;
    if (in_flight) {
        // Drain without waiting so the new queue and result calls keep their full time budget.
        const esp_err_t error = spi_device_get_trans_result(device, &completed, 0);
        if (error != ESP_OK) {
            return spi_error("previous SPI transaction pending", error);
        }
        in_flight = false;
    }

    transaction = {};
    transaction.flags = SPI_TRANS_USE_RXDATA;
    transaction.length = kFrameBits;
    transaction.rxlength = kFrameBits;
    esp_err_t error = spi_device_queue_trans(device, &transaction, kQueueWait);
    if (error != ESP_OK) {
        return spi_error("SPI queue", error);
    }
    in_flight = true;
    error = spi_device_get_trans_result(device, &completed, kResultWait);
    if (error != ESP_OK) {
        return spi_error("SPI result", error);
    }
    in_flight = false;

    // The most significant byte arrives first on both converters.
    const auto& rx = completed->rx_data;
#if CONFIG_PROBE_TC_MAX6675
    const auto frame = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8) | rx[1]);
    return core::decode_max6675(frame);
#else
    const uint32_t frame = (static_cast<uint32_t>(rx[0]) << 24) |
                           (static_cast<uint32_t>(rx[1]) << 16) |
                           (static_cast<uint32_t>(rx[2]) << 8) | rx[3];
    return core::decode_max31855(frame, kGroundedProbe);
#endif
}

const char* chip_name() {
#if CONFIG_PROBE_TC_MAX6675
    return "MAX6675";
#else
    return "MAX31855";
#endif
}

}  // namespace thermocouple
