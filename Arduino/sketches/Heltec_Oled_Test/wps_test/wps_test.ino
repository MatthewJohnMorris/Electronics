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
#include "ESPmDNS.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp.h"

#include "BestConnectionOptions.hpp"
#include "ConnectionEventProcessor.hpp"
#include "ConnectionMaintenance.hpp"
#include "IWebResponder.hpp"
#include "Logging.hpp"
#include "Utilities.hpp"

#include <FastLED.h>

String wpspin2string(uint8_t a[]){
  char wps_pin[9];
  for(int i=0;i<8;i++){
    wps_pin[i] = a[i];
  }
  wps_pin[8] = '\0';
  return (String)wps_pin;
}

int isPrgButtonPressed()
{
  return 0 == digitalRead(0);
}



ConnectionEventProcessor theConnectionEventProcessor;


class Esp32WebResponder : public IWebResponder
{
  private:
    String Respond(const String& request)
    {
      if (request == "/")
      {
          IPAddress ip = WiFi.localIP();
          String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
          String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
          s += "Hello from ESP32 at ";
          s += ipStr;
          s += "<br><br>";
          s += "<form action=\"\">";
          s += "<input type=\"text\" name=\"colour\" value=\"red\">";
          s += "<br><br>";
          s += "<input type=\"text\" name=\"brightness\" value=\"10\">";
          s += "<br><br>";
          s += "<input type=\"submit\" value=\"Submit\"></form>";
          s += "</html>\r\n\r\n";
          return s;
      }
      else
      {
          String s = "HTTP/1.1 404 Not Found\r\n\r\n";
          return s;
      }
    }
};

std::shared_ptr<ConnectionMaintenance> theConnectionMaintenance;

bool wps_got_ip = false;
void WiFiEventWps(WiFiEvent_t event, system_event_info_t info)
{
  if(theConnectionMaintenance.get() != 0 && theConnectionMaintenance->GetAssumeConnection())
  {
    WiFiEventConnection(event, info);
    return;
  }
  LogWiFiEvent(event, info);
  switch(event){
    case SYSTEM_EVENT_STA_START:
      u8x8_writeRow(2, "Station Mode");
      Serial.printf("Wps: SYSTEM_EVENT_STA_START\n");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      u8x8_writeRow(2, ("C:" + String(WiFi.SSID())).c_str());
      u8x8_writeRow(3, ("@" + WiFi.localIP().toString()).c_str());
      Serial.printf("Wps: SYSTEM_EVENT_STA_GOT_IP: %s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      wps_got_ip = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      u8x8_writeRow(2, "Disc: try recon");
      Serial.printf("Wps: SYSTEM_EVENT_STA_DISCONNECTED\n");
      WiFi.reconnect();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      u8x8_writeRow(2, ("W:" + String(WiFi.SSID())).c_str());
      Serial.printf("Wps: SYSTEM_EVENT_STA_WPS_ER_SUCCESS\n");
      esp_wifi_wps_disable();
      delay(10);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      u8x8_writeRow(2, "WPS Fail, retry");
      Serial.printf("Wps: SYSTEM_EVENT_STA_WPS_ER_FAILED\n");
      wps_disableAndReEnable();
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      u8x8_writeRow(2, "WPS T/O, retry");
      Serial.printf("Wps: SYSTEM_EVENT_STA_WPS_ER_TIMEOUT\n");
      wps_disableAndReEnable();
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      Serial.printf("WPS_PIN: %s\n", wpspin2string(info.sta_er_pin.pin_code));
      break;
    default:
      break;
  }
}

void WiFiEventConnection(WiFiEvent_t event, system_event_info_t info)
{
  theConnectionEventProcessor.Register(event);
  LogWiFiEvent(event, info);
}

void setup(){
  Serial.begin(115200);

  led_init();
  led_setColour(CRGB::Black);
  led_setColour(CRGB::DarkRed);
  
  Serial.setDebugOutput(true);
  delay(100);

  led_setColour(CRGB::Red);
  
  u8x8_init();

  Serial.printf("Setting Debug logging for wifi stack on\n");
  esp_log_level_set("wifi", ESP_LOG_DEBUG);               

  led_setColour(CRGB::Purple);
  
  u8x8_writeRow(0, "Hit PRG for WPS");
  delay(2000);

  if(isPrgButtonPressed())
  {
    led_setColour(CRGB::Blue);
    WiFi.onEvent(WiFiEventWps);
    u8x8_writeRow(0, "Trying WPS");
    auto modeRet = WiFi.mode(WIFI_MODE_STA);
    Serial.printf("Initialised WIFI_MODE_STA, returned %i\n", modeRet);
    Serial.printf("Called WiFi.enableSTA(false), got %i\n", WiFi.enableSTA(false));
    Serial.printf("Called WiFi.enableSTA(true), got %i\n", WiFi.enableSTA(true));
    wps_init();
    while(! wps_got_ip)
    {
      delay(1000);
    }
    WiFi.disconnect();
  }
  else
  {
    led_setColour(CRGB::Yellow);
    u8x8_writeRow(0, "Trying to Connect");
    Serial.printf("WiFi.getAutoReconnect() returned %i\n", WiFi.getAutoReconnect());
    WiFi.onEvent(WiFiEventConnection);
    // Serial.printf("Initialised WIFI_OFF, returned %i\n", WiFi.mode(WIFI_OFF));
    Serial.printf("Initialised WIFI_MODE_STA, returned %i\n", WiFi.mode(WIFI_MODE_STA));
  }

  std::shared_ptr<BestConnectionOptions> pBestConnectionOptions(new BestConnectionOptions());
  std::shared_ptr<IWebResponder> pWebResponder(new Esp32WebResponder());
  theConnectionMaintenance.reset(
    new ConnectionMaintenance(
      pBestConnectionOptions, 
      pWebResponder,
      theConnectionEventProcessor));

  int beginRetCode = pBestConnectionOptions->WiFiBegin();
  Serial.printf("WiFi.begin() returned %i (%s)\n", beginRetCode, descForStatusCode(beginRetCode));
  u8x8_writeRow(1, descForStatusCode(beginRetCode));
  theConnectionMaintenance->SetAssumeConnection(true);
  showConnectionStatus(beginRetCode);
}

void loop()
{  
  if(theConnectionMaintenance.get() != 0)
  {
    if(! theConnectionMaintenance->isConnected())
    {
      led_setColour(CRGB::Orange);
      delay(1000);
    }

    theConnectionMaintenance->acquireAndMaintainConnection();
  }
}
