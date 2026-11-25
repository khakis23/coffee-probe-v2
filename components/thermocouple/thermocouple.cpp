#include "thermocouple/thermocouple.hpp"

#include <cstdint>

#include "board/board.hpp"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace thermocouple {
namespace {

constexpr const char* kTag = "thermocouple";
constexpr int kSpiHz = 4'000'000;  // Below the MAX6675's 4.3 MHz limit.
// A frame takes about 8 us at 4 MHz. A tick-based wait can end up to one tick early, and
// Wi-Fi interrupts can delay the task, so the result wait is generous. Waits per read: at most
// ~70 ms of SPI plus the read-interval wait below, which only runs after a late read.
constexpr TickType_t kQueueWait = pdMS_TO_TICKS(20);
constexpr TickType_t kResultWait = pdMS_TO_TICKS(50);
constexpr TickType_t kRetryInterval = pdMS_TO_TICKS(5000);
constexpr TickType_t kWarnInterval = pdMS_TO_TICKS(10000);
constexpr core::TcReading kBusError{{0}, std::nullopt, core::TcFault::BusError};

// A transfer that never completes cannot be cancelled or removed from the driver, so the
// sensor would report BusError forever. After this long it is a hang, and R3 turns a hang into
// panic -> coredump -> reboot (esp_system_abort, not esp_restart, so the crash is recorded).
constexpr int64_t kStuckRestartUs = 5'000'000;

// Pulling CS low aborts a conversion in progress, so consecutive reads must be at least one
// worst-case conversion apart (datasheets: MAX6675 0.22 s, MAX31855 0.1 s), plus 10 ms margin.
// The sensor task's 250 ms period normally guarantees this; a late read followed by an
// on-time one does not, so the driver enforces it.
constexpr int64_t kMax6675ReadIntervalUs = 230'000;
constexpr int64_t kMax31855ReadIntervalUs = 110'000;
constexpr int kMaxFrameBytes = 4;

// Set by configure() before the sensor task starts; the build options are the factory default.
#if CONFIG_PROBE_TC_MAX6675
core::TcChip configured_chip = core::TcChip::Max6675;
#else
core::TcChip configured_chip = core::TcChip::Max31855;
#endif
#if CONFIG_PROBE_TC_GROUNDED
bool configured_grounded = true;
#else
bool configured_grounded = false;
#endif

int frame_bits() { return configured_chip == core::TcChip::Max6675 ? 16 : 32; }
int64_t min_read_interval_us() {
    return configured_chip == core::TcChip::Max6675 ? kMax6675ReadIntervalUs
                                                    : kMax31855ReadIntervalUs;
}

// The sensor task is the only caller. The SPI driver may retain the descriptor
// and its inline RX buffer after a result timeout.
spi_transaction_t transaction{};
bool in_flight = false;
int64_t in_flight_since_us = 0;
int64_t last_frame_us = 0;  // When the last transfer ended (CS high: a conversion starts).
bool have_frame = false;
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

void configure(core::TcChip chip, bool grounded_probe) {
    configured_chip = chip;
    configured_grounded = grounded_probe;
}

core::TcChip chip() { return configured_chip; }

bool grounded_probe() { return configured_grounded; }

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
        bus_config.max_transfer_sz = kMaxFrameBytes;  // Either chip.
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
            if (esp_timer_get_time() - in_flight_since_us >= kStuckRestartUs) {
                esp_system_abort("thermocouple SPI transfer stuck for 5 s");
            }
            return spi_error("previous SPI transaction pending", error);
        }
        in_flight = false;
        // It completed some time before now (CS went high, a conversion started). Counting from
        // now is conservative: the read below waits a full conversion time.
        last_frame_us = esp_timer_get_time();
        have_frame = true;
    }

    if (have_frame) {
        const int64_t remaining_us =
            min_read_interval_us() - (esp_timer_get_time() - last_frame_us);
        if (remaining_us > 0) {
            // A tick delay can end up to one tick early, so add one. At most ~240 ms.
            vTaskDelay(pdMS_TO_TICKS(remaining_us / 1000) + 1);
        }
    }

    transaction = {};
    transaction.flags = SPI_TRANS_USE_RXDATA;
    transaction.length = frame_bits();
    transaction.rxlength = frame_bits();
    esp_err_t error = spi_device_queue_trans(device, &transaction, kQueueWait);
    if (error != ESP_OK) {
        return spi_error("SPI queue", error);
    }
    in_flight = true;
    in_flight_since_us = esp_timer_get_time();
    error = spi_device_get_trans_result(device, &completed, kResultWait);
    if (error != ESP_OK) {
        return spi_error("SPI result", error);
    }
    in_flight = false;
    last_frame_us = esp_timer_get_time();
    have_frame = true;

    // The most significant byte arrives first on both converters.
    const auto& rx = completed->rx_data;
    if (configured_chip == core::TcChip::Max6675) {
        const auto frame = static_cast<uint16_t>((static_cast<uint16_t>(rx[0]) << 8) | rx[1]);
        return core::decode_max6675(frame);
    }
    const uint32_t frame = (static_cast<uint32_t>(rx[0]) << 24) |
                           (static_cast<uint32_t>(rx[1]) << 16) |
                           (static_cast<uint32_t>(rx[2]) << 8) | rx[3];
    return core::decode_max31855(frame, configured_grounded);
}

const char* chip_name() {
    return configured_chip == core::TcChip::Max6675 ? "MAX6675" : "MAX31855";
}

}  // namespace thermocouple
