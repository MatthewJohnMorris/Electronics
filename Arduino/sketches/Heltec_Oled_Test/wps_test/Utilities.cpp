#include "Utilities.hpp"

#include <FastLED.h>

#include "WiFi.h"
#include "ESPmDNS.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp.h"

#include <U8x8lib.h>


// How many leds in your strip?
#define NUM_LEDS 4

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// ESP32 SPI2 Interface
#define DATA_PIN 12
#define CLOCK_PIN 14

// Define the array of leds
CRGB leds[NUM_LEDS];

// the OLED used
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

void u8x8_init()
{
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
}

void u8x8_writeRow(int row, const char* msg)
{
  u8x8.drawString(0, row, "                ");
  u8x8.drawString(0, row, msg);
}

const char* descForStatusCode(int status)
{
  if(status == WL_CONNECTED) { return "CONNECTED"; } // assigned when connected to a WiFi network;
  if(status == WL_NO_SHIELD) { return "NO_SHIELD"; } // assigned when no WiFi shield is present;
  if(status == WL_IDLE_STATUS) { return "IDLE_STATUS"; } // it is a temporary status assigned when WiFi.begin() is called and remains active until the number of attempts expires (resulting in WL_CONNECT_FAILED) or a connection is established (resulting in WL_CONNECTED);
  if(status == WL_NO_SSID_AVAIL) { return "NO_SSID_AVAIL"; } //  assigned when no SSID are available;
  if(status == WL_SCAN_COMPLETED) { return "SCAN_COMPLETED"; } // assigned when the scan networks is completed;
  if(status == WL_CONNECT_FAILED) { return "CONNECT_FAILED"; } // assigned when the connection fails for all the attempts;  
  if(status == WL_CONNECTION_LOST) { return "CONNECTION_LOST"; } // assigned when the connection is lost;
  if(status == WL_DISCONNECTED) { return "DISCONNECTED"; } // assigned when disconnected from a network;
  return "UNKNOWN";
}

void showConnectionStatus(int status)
{
  if(WL_CONNECTED == status)
  {
    u8x8_writeRow(2, ("YC:" + String(WiFi.SSID())).c_str());
    u8x8_writeRow(3, ("@" + WiFi.localIP().toString()).c_str());
  }
  else
  {
    u8x8_writeRow(2, String(status).c_str());
    u8x8_writeRow(3, descForStatusCode(status));
  }
}

void led_init()
{
    FastLED.addLeds<LPD8806, DATA_PIN, CLOCK_PIN, BRG>(leds, NUM_LEDS);
}

void led_setColour(int colour)
{
  for(int i = 0; i < NUM_LEDS; ++i)
  {
    leds[i] = colour;
  }
  FastLED.show();
}




/*
Change the definition of the WPS mode
from WPS_TYPE_PBC to WPS_TYPEPIN in
the case that you are using pin type
WPS
*/
#define ESP_WPS_MODE      WPS_TYPE_PBC
#define ESP_MANUFACTURER  "ESPRESSIF"
#define ESP_MODEL_NUMBER  "ESP32"
#define ESP_MODEL_NAME    "ESPRESSIF IOT"
#define ESP_DEVICE_NAME   "ESP STATION"

static esp_wps_config_t config;

void wpsInitConfig(){
  config.crypto_funcs = &g_wifi_default_wps_crypto_funcs;
  config.wps_type = ESP_WPS_MODE;
  strcpy(config.factory_info.manufacturer, ESP_MANUFACTURER);
  strcpy(config.factory_info.model_number, ESP_MODEL_NUMBER);
  strcpy(config.factory_info.model_name, ESP_MODEL_NAME);
  strcpy(config.factory_info.device_name, ESP_DEVICE_NAME);
}

void wps_init()
{
  u8x8_writeRow(1, "Starting WPS");

  wpsInitConfig();
  esp_wifi_wps_enable(&config);
  esp_wifi_wps_start(0);
}

void wps_disableAndReEnable()
{
  esp_wifi_wps_disable();
  esp_wifi_wps_enable(&config);
}
