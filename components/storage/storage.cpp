#include "storage/storage.hpp"

#include <array>
#include <span>

#include "esp_log.h"
#include "nvs.h"

namespace storage {
namespace {

constexpr const char* kTag = "storage";
constexpr const char* kNamespace = "probe";
constexpr const char* kKey = "settings";
constexpr std::size_t kLoadBufferSize = 64;

}  // namespace

core::SettingsLoad load() {
    nvs_handle_t handle{};
    esp_err_t error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error == ESP_OK) {
        // The boot task is the sole caller and owns this scratch buffer.
        static std::array<uint8_t, kLoadBufferSize> buffer{};
        std::size_t length = buffer.size();
        error = nvs_get_blob(handle, kKey, buffer.data(), &length);
        nvs_close(handle);
        if (error == ESP_OK) {
            // An entry that exists but is empty is a damaged write, not a fresh board.
            if (length == 0) {
                return {.status = core::SettingsStatus::Corrupt};
            }
            return core::decode_settings(std::span{buffer}.first(length));
        }
        if (error == ESP_ERR_NVS_INVALID_LENGTH) {
            return {.status = core::SettingsStatus::Corrupt};
        }
    }
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return {};
    }
    ESP_LOGW(kTag, "NVS settings read failed: %s", esp_err_to_name(error));
    return {.status = core::SettingsStatus::Corrupt};
}

esp_err_t save(const core::Settings& settings) {
    // Never store a value the next boot would have to repair: refuse it instead.
    uint8_t out_of_range = 0;
    (void)core::repair_settings(settings, &out_of_range);
    if (out_of_range > 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const auto blob = core::encode_settings(settings);
    nvs_handle_t handle{};
    esp_err_t error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        return error;
    }

    error = nvs_set_blob(handle, kKey, blob.data(), blob.size());
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    if (error == ESP_OK) {
        std::array<uint8_t, core::kSettingsBlobSize> readback{};
        std::size_t length = readback.size();
        error = nvs_get_blob(handle, kKey, readback.data(), &length);
        if (error == ESP_ERR_NVS_INVALID_LENGTH ||
            (error == ESP_OK && (length != blob.size() || readback != blob))) {
            error = ESP_FAIL;
        }
    }
    nvs_close(handle);
    return error;
}

}  // namespace storage
