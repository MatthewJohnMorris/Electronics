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
#include "esp_event.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

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

int isPrgButtonPressed()
{
  return 0 == digitalRead(0);
}

static int assumeConnection = 0;
static int wifiBeginRetryIntervalSecs = 1;

void writeRow(int row, const char* msg)
{
  u8x8.drawString(0, row, "                ");
  u8x8.drawString(0, row, msg);
}


const char* GetWiFiEventDesc(int event)
{
  if(event == SYSTEM_EVENT_WIFI_READY) { return "SYSTEM_EVENT_WIFI_READY"; }
  if(event == SYSTEM_EVENT_SCAN_DONE) { return "SYSTEM_EVENT_SCAN_DONE"; }
  if(event == SYSTEM_EVENT_STA_START) { return "SYSTEM_EVENT_STA_START"; }
  if(event == SYSTEM_EVENT_STA_STOP) { return "SYSTEM_EVENT_STA_STOP"; }
  if(event == SYSTEM_EVENT_STA_CONNECTED) { return "SYSTEM_EVENT_STA_CONNECTED"; }
  if(event == SYSTEM_EVENT_STA_DISCONNECTED) { return "SYSTEM_EVENT_STA_DISCONNECTED"; }
  if(event == SYSTEM_EVENT_STA_AUTHMODE_CHANGE) { return "SYSTEM_EVENT_STA_AUTHMODE_CHANGE"; }
  if(event == SYSTEM_EVENT_STA_WPS_ER_PIN) { return "SYSTEM_EVENT_STA_WPS_ER_PIN"; }
  if(event == SYSTEM_EVENT_STA_GOT_IP) { return "SYSTEM_EVENT_STA_GOT_IP"; }
  if(event == SYSTEM_EVENT_AP_STA_GOT_IP6) { return "SYSTEM_EVENT_AP_STA_GOT_IP6"; }
  if(event == SYSTEM_EVENT_AP_START) { return "SYSTEM_EVENT_AP_START"; }
  if(event == SYSTEM_EVENT_AP_STOP) { return "SYSTEM_EVENT_AP_STOP"; }
  if(event == SYSTEM_EVENT_AP_STACONNECTED) { return "SYSTEM_EVENT_AP_STACONNECTED"; }
  if(event == SYSTEM_EVENT_AP_STADISCONNECTED) { return "SYSTEM_EVENT_AP_STADISCONNECTED"; }
  if(event == SYSTEM_EVENT_AP_PROBEREQRECVED) { return "SYSTEM_EVENT_AP_PROBEREQRECVED"; }
  return "UNKNOWN";
}

const char * system_event_reasons[] = { "UNSPECIFIED", "AUTH_EXPIRE", "AUTH_LEAVE", "ASSOC_EXPIRE", "ASSOC_TOOMANY", "NOT_AUTHED", "NOT_ASSOCED", "ASSOC_LEAVE", "ASSOC_NOT_AUTHED", "DISASSOC_PWRCAP_BAD", "DISASSOC_SUPCHAN_BAD", "UNSPECIFIED", "IE_INVALID", "MIC_FAILURE", "4WAY_HANDSHAKE_TIMEOUT", "GROUP_KEY_UPDATE_TIMEOUT", "IE_IN_4WAY_DIFFERS", "GROUP_CIPHER_INVALID", "PAIRWISE_CIPHER_INVALID", "AKMP_INVALID", "UNSUPP_RSN_IE_VERSION", "INVALID_RSN_IE_CAP", "802_1X_AUTH_FAILED", "CIPHER_SUITE_REJECTED", "BEACON_TIMEOUT", "NO_AP_FOUND", "AUTH_FAIL", "ASSOC_FAIL", "HANDSHAKE_TIMEOUT" };
#define reason2str(r) ((r>176)?system_event_reasons[r-176]:system_event_reasons[r-1])

void LogWiFiEvent(WiFiEvent_t event, system_event_info_t info)
{
  Serial.print("-> WiFiEvent: ");
  Serial.print(event);
  Serial.print(": ");
  Serial.print(GetWiFiEventDesc(event));
  Serial.println("");  
  if(event == SYSTEM_EVENT_STA_DISCONNECTED)
  {
    auto disconnectInfo = info.disconnected;
    Serial.print("--> Reason: ");
    Serial.print(disconnectInfo.reason);
    Serial.print(": ");
    Serial.println(reason2str(disconnectInfo.reason));
  }
  else if(event == SYSTEM_EVENT_STA_CONNECTED)
  {
    auto connectInfo = info.connected;
    Serial.print("--> SSID: ");
    char ssid[32];
    memset(ssid, 0, 32);
    for(int i = 0; i < connectInfo.ssid_len; ++i)
    {
      ssid[i] = (char)connectInfo.ssid[i];
    }
    Serial.println(ssid);
  }
  else if(event == SYSTEM_EVENT_STA_GOT_IP)
  {
    auto ip = info.got_ip.ip_info.ip;
    Serial.print("--> IP: ");
    Serial.print(ip4_addr1(&ip));
    Serial.print(".");
    Serial.print(ip4_addr2(&ip));
    Serial.print(".");
    Serial.print(ip4_addr3(&ip));
    Serial.print(".");
    Serial.print(ip4_addr4(&ip));
    Serial.println("");
  }
}

void WiFiEventWps(WiFiEvent_t event, system_event_info_t info){
  if(assumeConnection == 1)
  {
    WiFiEventConnection(event, info);
    return;
  }
  LogWiFiEvent(event, info);
  switch(event){
    case SYSTEM_EVENT_STA_START:
      writeRow(2, "Station Mode");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      writeRow(2, ("C:" + String(WiFi.SSID())).c_str());
      writeRow(3, ("@" + WiFi.localIP().toString()).c_str());
      assumeConnection = 1;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      writeRow(2, "Disc: try recon");
      WiFi.reconnect();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      writeRow(2, ("W:" + String(WiFi.SSID())).c_str());
      esp_wifi_wps_disable();
      delay(10);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      writeRow(2, "WPS Fail, retry");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      writeRow(2, "WPS T/O, retry");
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

int lastConnectionEvent = -1;
void WiFiEventConnection(WiFiEvent_t event, system_event_info_t info)
{
  lastConnectionEvent = event;
  LogWiFiEvent(event, info);
}

void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(100);

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  writeRow(0, "Hit PRG for WPS");
  delay(2000);
  
  Serial.println();

  if(isPrgButtonPressed())
  {
    writeRow(0, "Trying WPS");
    initWps();
  }
  else
  {
    writeRow(0, "Trying Existing");
    int beginRetCode = WiFi.begin();
    Serial.println("Called WiFi.begin() on entry");
    writeRow(1, descForStatusCode(beginRetCode));
    assumeConnection = 1;
    showConnectionStatus(beginRetCode);
    WiFi.onEvent(WiFiEventConnection);
  }
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
    writeRow(2, ("YC:" + String(WiFi.SSID())).c_str());
    writeRow(3, ("@" + WiFi.localIP().toString()).c_str());
  }
  else
  {
    writeRow(2, String(status).c_str());
    writeRow(3, descForStatusCode(status));
  }
}

void initWps()
{
  WiFi.mode(WIFI_MODE_STA);

  writeRow(1, "Starting WPS");

  wpsInitConfig();
  esp_wifi_wps_enable(&config);
  esp_wifi_wps_start(0);
}

void hard_restart() {
  esp_task_wdt_init(1,true);
  esp_task_wdt_add(NULL);
  while(true);
}

/*
 * Failure modes so far
 * 
 * 1) No events received at all
 * 2) Repeated events of
 ** WiFiEvent: 5: SYSTEM_EVENT_STA_DISCONNECTED
 ** Reason: 6: NOT_AUTHED
 */

int connectCount = 0;
int noStatusUpdateCount = 0;
int prev_status = -1;

void enterLockupIfRunningForTooLong()
{
  const int MAX_LENGTH_BEFORE_LOCKUP = 30000;

  if(millis() > MAX_LENGTH_BEFORE_LOCKUP)
  {
    Serial.printf("Have been running past limit of %i: holding so output can be examined\n", MAX_LENGTH_BEFORE_LOCKUP);
    for(;;) {}
  }
}

void maintainConnection()
{
  enterLockupIfRunningForTooLong();
  
  delay(1000 * wifiBeginRetryIntervalSecs);
  int status = WiFi.status();
  if(status != WL_CONNECTED || status != prev_status)
  {
    showConnectionStatus(status);
  }
  prev_status = status;
  if(assumeConnection == 1)
  {
    if(status != WL_CONNECTED)
    {
      Serial.printf("Status: %i (%s)\n", status, descForStatusCode(status));

      if(lastConnectionEvent != -1)
      {
        noStatusUpdateCount = 0;
        Serial.printf("New lastConnectionEvent: %i (%s)\n", lastConnectionEvent, GetWiFiEventDesc(lastConnectionEvent));
        if(lastConnectionEvent != SYSTEM_EVENT_STA_CONNECTED && lastConnectionEvent != SYSTEM_EVENT_STA_GOT_IP)
        {
          int reconnectStatus = WiFi.reconnect();          
          Serial.printf("Called WiFi.reconnect(), got %i\n", reconnectStatus);
        }
        lastConnectionEvent = -1;
      }
      else
      {
        noStatusUpdateCount++;
        if(noStatusUpdateCount == 5)
        {
          Serial.printf("No new event for %i seconds - calling WiFi.reconnect()\n", noStatusUpdateCount);
          noStatusUpdateCount = 0;
          int reconnectStatus = WiFi.reconnect();          
          Serial.printf("Called WiFi.reconnect(), got %i\n", reconnectStatus);
        }
      }
      writeRow(1, descForStatusCode(status));
    }
    else
    {
      Serial.print(".");
      wifiBeginRetryIntervalSecs = 1;
      connectCount++;
      if(connectCount == 3)
      {
        Serial.print("We have a stable connection after ");
        Serial.print(millis());
        Serial.println("ms: trying again");
        hard_restart();
      }
    }
  }  
}

void loop(){
  maintainConnection();
}
