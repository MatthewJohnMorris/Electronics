#ifndef IN_LOGGING_HPP
#define IN_LOGGING_HPP

#include "WiFi.h"
#include "ESPmDNS.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp.h"

const char * system_events[] = {
  "SYSTEM_EVENT_WIFI_READY", "SYSTEM_EVENT_SCAN_DONE", "SYSTEM_EVENT_STA_START", "SYSTEM_EVENT_STA_STOP",
  "SYSTEM_EVENT_STA_CONNECTED", "SYSTEM_EVENT_STA_DISCONNECTED", "SYSTEM_EVENT_STA_AUTHMODE_CHANGE", "SYSTEM_EVENT_STA_GOT_IP",
  "SYSTEM_EVENT_STA_LOST_IP", "SYSTEM_EVENT_STA_WPS_ER_SUCCESS", "SYSTEM_EVENT_STA_WPS_ER_FAILED", "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT",
  "SYSTEM_EVENT_STA_WPS_ER_PIN", "SYSTEM_EVENT_AP_START", "SYSTEM_EVENT_AP_STOP", "SYSTEM_EVENT_AP_STACONNECTED",
  "SYSTEM_EVENT_AP_STADISCONNECTED", "SYSTEM_EVENT_AP_STAIPASSIGNED", "SYSTEM_EVENT_AP_PROBEREQRECVED", "SYSTEM_EVENT_GOT_IP6",
  "SYSTEM_EVENT_ETH_START", "SYSTEM_EVENT_ETH_STOP", "SYSTEM_EVENT_ETH_CONNECTED", "SYSTEM_EVENT_ETH_DISCONNECTED",
  "SYSTEM_EVENT_ETH_GOT_IP" };

const char* GetWiFiEventDesc(int event)
{
  if(event >=0 && event <= 24)
  {
    return system_events[event];
  }
  return "UNKNOWN";
}

// https://github.com/espressif/esp-idf/blob/master/components/esp32/include/esp_wifi_types.h
const char * system_event_reasons[] = { 
  "UNSPECIFIED", "AUTH_EXPIRE", "AUTH_LEAVE", "ASSOC_EXPIRE", // 4
  "ASSOC_TOOMANY", "NOT_AUTHED", "NOT_ASSOCED", "ASSOC_LEAVE", // 8
  "ASSOC_NOT_AUTHED", "DISASSOC_PWRCAP_BAD", "DISASSOC_SUPCHAN_BAD", "UNSPECIFIED", // 12
  "IE_INVALID", "MIC_FAILURE", "4WAY_HANDSHAKE_TIMEOUT", "GROUP_KEY_UPDATE_TIMEOUT", // 16
  "IE_IN_4WAY_DIFFERS", "GROUP_CIPHER_INVALID", "PAIRWISE_CIPHER_INVALID", "AKMP_INVALID", // 20 
  "UNSUPP_RSN_IE_VERSION", "INVALID_RSN_IE_CAP", "802_1X_AUTH_FAILED", "CIPHER_SUITE_REJECTED", //24
  "BEACON_TIMEOUT", // 200 -> 24
  "NO_AP_FOUND", "AUTH_FAIL", "ASSOC_FAIL", "HANDSHAKE_TIMEOUT" // 204 
  };

const char* ReasonDesc(int reason)
{
  if(reason >= 200 && reason <= 204)
  {
    return system_event_reasons[reason-176];
  }
  if(reason > 0 && reason <= 24)
  {
    return system_event_reasons[reason-1];
  }
  return "UNKNOWN";
}

void LogWiFiEvent(WiFiEvent_t event, system_event_info_t info)
{
  Serial.printf("-> WiFiEvent: %i (%s)\n", event, GetWiFiEventDesc(event));
  if(event == SYSTEM_EVENT_STA_DISCONNECTED)
  {
    auto disconnectInfo = info.disconnected;
    Serial.printf("--> Reason: %i (%s)\n", disconnectInfo.reason, ReasonDesc(disconnectInfo.reason));
  }
  else if(event == SYSTEM_EVENT_STA_CONNECTED)
  {
    auto connectInfo = info.connected;
    char ssid[32];
    memset(ssid, 0, 32);
    for(int i = 0; i < connectInfo.ssid_len; ++i)
    {
      ssid[i] = (char)connectInfo.ssid[i];
    }
    Serial.printf("--> SSID: %s\n", ssid);
  }
  else if(event == SYSTEM_EVENT_STA_GOT_IP)
  {
    auto ip = info.got_ip.ip_info.ip;
    Serial.printf("--> IP: %i.%i.%i.%i\n",
      ip4_addr1(&ip),
      ip4_addr2(&ip),
      ip4_addr3(&ip),
      ip4_addr4(&ip));
  }
}

#endif // IN_LOGGING_HPP
