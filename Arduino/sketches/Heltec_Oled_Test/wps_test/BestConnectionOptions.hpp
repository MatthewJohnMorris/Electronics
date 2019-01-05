#ifndef IN_BESTCONNECTIONOPTIONS_HPP
#define IN_BESTCONNECTIONOPTIONS_HPP

#include "WiFi.h"
#include "ESPmDNS.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp.h"

class BestConnectionOptions
{
private:
  int32_t _bestRssi;
  const uint8_t * _bestBssid;
  int _bestChannel;
public:
  BestConnectionOptions()
  {
    _bestRssi = -1024;
    _bestBssid = 0;
    _bestChannel = -1;

    auto modeRet = WiFi.mode(WIFI_MODE_STA);

    Serial.println("Fetching config");
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    Serial.println("Procesing config");
    auto configSsid = String(reinterpret_cast<char*>(conf.sta.ssid));
    Serial.printf("Config SSID: %s\n", configSsid.c_str());      
    auto configPassword = String(reinterpret_cast<char*>(conf.sta.password));
    Serial.printf("Config Password: %s\n", configPassword.c_str());
    Serial.printf("Config setbssid: %i\n", conf.sta.bssid_set);

    Serial.println("scan started");
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } 
    else 
    {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        String ssid;
        uint8_t encType;
        int32_t rssi;
        uint8_t* bssid;
        int32_t channel;
        WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel);
        if(0 == strcmp(ssid.c_str(), configSsid.c_str()))
        {
          Serial.printf("Ssid %s found: rssi=%i, bssid=%s channel=%i\n", ssid.c_str(), rssi, String(reinterpret_cast<char*>(bssid)).c_str(), channel);
          if(rssi > _bestRssi)
          {
            Serial.printf("Adopting as best\n");
            _bestRssi = rssi;
            _bestBssid = bssid;
            _bestChannel = channel;
          }
        }
      }
    }
    
  }
  
  int WiFiBegin()
  {
    if(_bestRssi == -1024)
    {
      Serial.println("WifiBeginWithBestOption: No SSID found, entering lockup");
      for(;;)
      {
      }
    }
    wifi_config_t conf;   esp_wifi_get_config(WIFI_IF_STA, &conf);
    auto configSsid = reinterpret_cast<char*>(conf.sta.ssid);
    auto configPassword = reinterpret_cast<char*>(conf.sta.password);
    auto usingBssid = reinterpret_cast<const char*>(_bestBssid);
    Serial.printf("Calling Wifi.Begin with SSID '%s', Password '%s', Channel %i Bssid '%s'\n",
      configSsid, configPassword, _bestChannel, usingBssid);
    return WiFi.begin(configSsid, configPassword, _bestChannel, _bestBssid, true);
  }

};

#endif // IN_BESTCONNECTIONOPTIONS_HPP
