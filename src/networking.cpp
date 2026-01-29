#include "networking.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_config.h"
#include "config_features.h"
#include "config_hw.h"
#include "log.h"
#include "network_client.h"

static bool wifiInitialized = false;
static bool wifiEverBegun = false;

static portMUX_TYPE g_flightMux = portMUX_INITIALIZER_UNLOCKED;
static FlightInfo g_pendingFlight;
static bool g_pendingValid = false;
static uint32_t g_pendingSeq = 0;

static void waitMs(uint32_t durationMs) {
  uint32_t start = millis();
  while ((int32_t)(millis() - start) < (int32_t)durationMs) {
    yield();
  }
}

static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!wifiInitialized) return;
  if (wifiEverBegun) return;
  LOG_INFO("WiFi connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiEverBegun = true;
}

static void fetchTask(void *arg) {
  (void)arg;
  uint32_t lastFetch = 0;
  for (;;) {
    uint32_t now = millis();
    if ((int32_t)(now - lastFetch) >= (int32_t)FETCH_INTERVAL_MS || lastFetch == 0) {
      lastFetch = now;
      FlightInfo fi;
      bool ok = networkClientFetchNearestFlight(fi);
      portENTER_CRITICAL(&g_flightMux);
      g_pendingValid = ok;
      if (ok) {
        g_pendingFlight = fi;
      }
      g_pendingSeq++;
      portEXIT_CRITICAL(&g_flightMux);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void networkingInit() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        LOG_WARN("WiFi disconnected. Reason: %d", info.wifi_sta_disconnected.reason);
        WiFi.setTxPower(WIFI_BOOT_TXPOWER);
        WiFi.setSleep(true);
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        {
          IPAddress ip(info.got_ip.ip_info.ip.addr);
          String ipStr = ip.toString();
          LOG_INFO("WiFi got IP: %s", ipStr.c_str());
        }
        WiFi.setTxPower(WIFI_RUN_TXPOWER);
        WiFi.setSleep(false);
        break;
      default: break;
    }
  });

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_BOOT_TXPOWER);
  WiFi.setSleep(true);
  wifiInitialized = true;

  waitMs(BOOT_POWER_SETTLE_MS);
  connectWiFi();
}

void networkingStartFetchTask() {
  xTaskCreatePinnedToCore(fetchTask, "fetchTask", 12288, nullptr, 1, nullptr, 0);
}

void networkingEnsureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
}

bool networkingGetLatest(FlightInfo &out, bool &outValid, uint32_t &outSeq) {
  portENTER_CRITICAL(&g_flightMux);
  outSeq = g_pendingSeq;
  outValid = g_pendingValid;
  if (outValid) {
    out = g_pendingFlight;
  }
  portEXIT_CRITICAL(&g_flightMux);
  return true;
}
