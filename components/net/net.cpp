#include "net/net.hpp"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "board/board.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "net/softap_config.hpp"

namespace net {
namespace {

constexpr const char* kTag = "softap";
constexpr std::size_t kSsidBufferSize = 33;
constexpr std::size_t kMacSuffixLength = 4;
static_assert(sizeof(kSsidPrefix) + kMacSuffixLength <= kSsidBufferSize);
char ap_ssid[kSsidBufferSize]{};
std::atomic<uint8_t> associated_clients{0};

esp_err_t check_step(const char* step, esp_err_t error) {
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "%s failed: %s", step, esp_err_to_name(error));
    }
    return error;
}

void wifi_event(void*, esp_event_base_t, int32_t event_id, void* event_data) {
    // Only the default event loop writes the counter. Readers need only the count.
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED: {
            const auto& event = *static_cast<const wifi_event_ap_staconnected_t*>(event_data);
            const uint8_t count = associated_clients.load(std::memory_order_relaxed);
            associated_clients.store(count + 1, std::memory_order_relaxed);
            ESP_LOGI(kTag, "Station " MACSTR " connected (AID %u)", MAC2STR(event.mac),
                     static_cast<unsigned>(event.aid));
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            const auto& event = *static_cast<const wifi_event_ap_stadisconnected_t*>(event_data);
            const uint8_t count = associated_clients.load(std::memory_order_relaxed);
            if (count > 0) {
                associated_clients.store(count - 1, std::memory_order_relaxed);
            }
            ESP_LOGI(kTag, "Station " MACSTR " disconnected (AID %u)", MAC2STR(event.mac),
                     static_cast<unsigned>(event.aid));
            break;
        }
        case WIFI_EVENT_AP_START:
            ESP_LOGI(kTag, "AP started");
            break;
        case WIFI_EVENT_AP_STOP:
            associated_clients.store(0, std::memory_order_relaxed);
            ESP_LOGI(kTag, "AP stopped");
            break;
        default:
            break;
    }
}

esp_err_t configure_ap_netif(esp_netif_t* ap_netif) {
    esp_netif_ip_info_t ip_info{};
    if (const auto error =
            check_step("Parse AP address", esp_netif_str_to_ip4(kApIp, &ip_info.ip));
        error != ESP_OK) {
        return error;
    }
    if (const auto error =
            check_step("Parse AP netmask", esp_netif_str_to_ip4(kApNetmask, &ip_info.netmask));
        error != ESP_OK) {
        return error;
    }
    ip_info.gw = ip_info.ip;

    if (const auto error = check_step("esp_netif_dhcps_stop", esp_netif_dhcps_stop(ap_netif));
        error != ESP_OK) {
        return error;
    }
    if (const auto error =
            check_step("esp_netif_set_ip_info", esp_netif_set_ip_info(ap_netif, &ip_info));
        error != ESP_OK) {
        return error;
    }

    // IDF's DHCP server always sends a DNS option: with OFFER_DNS cleared it names the AP's own
    // address (dhcpserver.c, add_offer_options; same in v5.3.1 and v6.0). We run no DNS server, so
    // lwIP answers port 53 with ICMP port-unreachable and lookups fail at once. macOS and Windows
    // then report "no internet" instead of opening a captive sign-in sheet. Keep OFFER_DNS off so
    // a stray DNS address set elsewhere can never be handed out.
    uint8_t offer_dns = 0;
    if (const auto error = check_step(
            "Disable DHCP DNS offer",
            esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                   &offer_dns, sizeof(offer_dns)));
        error != ESP_OK) {
        return error;
    }

    // While the interface is down, IDF schedules DHCP to start when the AP comes up.
    return check_step("esp_netif_dhcps_start", esp_netif_dhcps_start(ap_netif));
}

}  // namespace

esp_err_t start_softap() {
    if (const auto error = check_step("esp_netif_init", esp_netif_init()); error != ESP_OK) {
        return error;
    }
    if (const auto error =
            check_step("esp_event_loop_create_default", esp_event_loop_create_default());
        error != ESP_OK) {
        return error;
    }
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == nullptr) {
        return check_step("esp_netif_create_default_wifi_ap", ESP_ERR_NO_MEM);
    }

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    if (const auto error = check_step("esp_wifi_init", esp_wifi_init(&wifi_init));
        error != ESP_OK) {
        return error;
    }
    // Keep the AP configuration in RAM so the firmware determines the settings on every boot.
    if (const auto error =
            check_step("esp_wifi_set_storage", esp_wifi_set_storage(WIFI_STORAGE_RAM));
        error != ESP_OK) {
        return error;
    }
    // Concurrent station scans could pull the AP away from its fixed channel.
    if (const auto error = check_step("esp_wifi_set_mode", esp_wifi_set_mode(WIFI_MODE_AP));
        error != ESP_OK) {
        return error;
    }
    if (const auto error =
            check_step("esp_wifi_set_country_code", esp_wifi_set_country_code(kCountryCode, true));
        error != ESP_OK) {
        return error;
    }

    uint8_t mac[6]{};
    if (const auto error = check_step("esp_wifi_get_mac", esp_wifi_get_mac(WIFI_IF_AP, mac));
        error != ESP_OK) {
        return error;
    }
    std::snprintf(ap_ssid, sizeof(ap_ssid), "%s%02X%02X", kSsidPrefix,
                  static_cast<unsigned>(mac[4]), static_cast<unsigned>(mac[5]));
    wifi_config_t config{};
    config.ap.ssid_len = sizeof(kSsidPrefix) - 1 + kMacSuffixLength;
    std::memcpy(config.ap.ssid, ap_ssid, config.ap.ssid_len);
    std::memcpy(config.ap.password, kPassword, sizeof(kPassword));
    config.ap.channel = kChannel;
    config.ap.max_connection = kMaxClients;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.beacon_interval = kBeaconIntervalTu;
    config.ap.pmf_cfg.required = false;  // Allow older laptops without PMF support.
    if (const auto error =
            check_step("esp_wifi_set_config", esp_wifi_set_config(WIFI_IF_AP, &config));
        error != ESP_OK) {
        return error;
    }

    if (const auto error = configure_ap_netif(ap_netif); error != ESP_OK) {
        return error;
    }
    if (const auto error = check_step(
            "Register Wi-Fi event handler",
            esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, nullptr));
        error != ESP_OK) {
        return error;
    }
    if (const auto error = check_step("esp_wifi_start", esp_wifi_start()); error != ESP_OK) {
        return error;
    }
    // The AP is up at this point. TX power and power save are tuning: if either call fails, keep
    // the working AP (logged by check_step) rather than failing start-up and rebooting.
    check_step("esp_wifi_set_max_tx_power",
               esp_wifi_set_max_tx_power(board::kBoard.wifi_max_tx_power_qdbm));
    check_step("esp_wifi_set_ps", esp_wifi_set_ps(WIFI_PS_NONE));
    return ESP_OK;
}

const char* ssid() { return ap_ssid; }

uint8_t client_count() { return associated_clients.load(std::memory_order_relaxed); }

}  // namespace net
