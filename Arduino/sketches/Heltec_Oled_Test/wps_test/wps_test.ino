/*
Example Code To Get ESP32 To Connect To A Router Using WPS
===========================================================
This example code provides both Push Button method and Pin
based WPS entry to get your ESP connected to your WiFi router.
Hardware Requirements
========================
ESP32 and a Router having atleast one WPS functionality
This code is under Public Domain License.
Author:
Pranav Cherukupalli <cherukupallip@gmail.com>
*/

#include "WiFi.h"
#include "esp_wps.h"

#include <U8x8lib.h>

// the OLED used
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

/*
Change the definition of the WPS mode
from WPS_TYPE_PBC to WPS_TYPE_PIN in
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

String wpspin2string(uint8_t a[]){
  char wps_pin[9];
  for(int i=0;i<8;i++){
    wps_pin[i] = a[i];
  }
  wps_pin[8] = '\0';
  return (String)wps_pin;
}

void WiFiEvent(WiFiEvent_t event, system_event_info_t info){
  switch(event){
    case SYSTEM_EVENT_STA_START:
      u8x8.drawString(0, 2, "                ");
      u8x8.drawString(0, 2, "Station Mode");
      Serial.println("Station Mode Started");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      u8x8.drawString(0, 2, "                ");
      u8x8.drawString(0, 2, ("C:" + String(WiFi.SSID())).c_str());
      Serial.println("Connected to :" + String(WiFi.SSID()));
      u8x8.drawString(0, 3, ("@" + WiFi.localIP().toString()).c_str());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      u8x8.drawString(0, 2, "                ");
      u8x8.drawString(0, 2, "Disc: try recon");
      Serial.println("Disconnected from station, attempting reconnection");
      WiFi.reconnect();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      u8x8.drawString(0, 2, "                ");
      u8x8.drawString(0, 2, ("W:" + String(WiFi.SSID())).c_str());
      Serial.println("WPS Successfull, stopping WPS and connecting to: " + String(WiFi.SSID()));
      esp_wifi_wps_disable();
      delay(10);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      u8x8.drawString(0, 2, "                ");
      u8x8.drawString(0, 2, "WPS Fail, retry");
      Serial.println("WPS Failed, retrying");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      u8x8.drawString(0, 2, "                ");
      u8x8.drawString(0, 2, "WPS T/O, retry");
      Serial.println("WPS Timedout, retrying");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      Serial.println("WPS_PIN = " + wpspin2string(info.sta_er_pin.pin_code));
      break;
    default:
      break;
  }
}

void setup(){
  Serial.begin(115200);
  delay(100);

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  u8x8.drawString(0, 0, "Starting");
  Serial.println();

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_STA);

  u8x8.drawString(0, 1, "Starting WPS");
  Serial.println("Starting WPS");

  wpsInitConfig();
  esp_wifi_wps_enable(&config);
  esp_wifi_wps_start(0);
}

void loop(){
  //nothing to do here
}
