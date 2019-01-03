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

void writeRow(int row, const char* msg)
{
  u8x8.drawString(0, row, "                ");
  u8x8.drawString(0, row, msg);
}


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

class ConnectionEventProcessor
{
private:
  int _lastEvent; 
  bool _hasDisconnectEvent;

public:
  ConnectionEventProcessor()
  {
    _lastEvent = -1;
    _hasDisconnectEvent = false;
  }
  void Register(WiFiEvent_t value)
  {
    if(value == SYSTEM_EVENT_STA_DISCONNECTED)
    {
      _hasDisconnectEvent = true;
    }
    _lastEvent = value;
  }
  int GetAndClearLastEvent()
  {
    auto ret = _lastEvent;
    _lastEvent = -1;
    return ret;    
  }
  bool GetAndClearHasDisconnectEvent()
  {
    auto ret = _hasDisconnectEvent;
    _hasDisconnectEvent = false;
    return ret;
  }
};

ConnectionEventProcessor theConnectionEventProcessor;

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

class Restarter
{
public:
  static bool shouldHaveRestarted;
  static void HardRestart()
  {
    // Let our code know we should be dead
    shouldHaveRestarted = true;

    // set up watchdog to kill us after a second
    esp_task_wdt_init(1,true);
    esp_task_wdt_add(NULL);

    // attempt a clean restart
    ESP.restart();

    // Loop - the watchdog will take care of things
    // This may be a less clean restart than the other option: in particular, code
    // may still run in loop() - noticed around 1/500 restarts/
    while(true);
  }
  static void RestartIfWeShouldAlreadyHaveDoneSo()
  {
    if(shouldHaveRestarted)
    {
      Serial.println("****************************************************");
      Serial.println("EXTRA RESTART: earlier hard reset failed!!!");
      Serial.println("****************************************************");
      HardRestart();
    }
  }
};
bool Restarter::shouldHaveRestarted = false;

class IWebResponder
{
  public:
    virtual String Respond(const String& request) = 0;
    virtual ~IWebResponder() {}
};

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

class ConnectionMaintenance
{
  private:
    bool _assumeConnection = false;
    int _prevStatus = -1;
    int _connectCount = 0;
    int _noStatusUpdateCount = 0;
    int _connectMillis = -1;
    bool _servicesSetUp = false;
    std::shared_ptr<BestConnectionOptions> _pBestConnectionOptions;
    std::shared_ptr<IWebResponder> _pWebResponder;
    ConnectionEventProcessor& _connectionEventProcessor;
    std::shared_ptr<WiFiServer> _pWiFiServer;

    const int SERVER_PORT = 80;

  public:
    ConnectionMaintenance(
      std::shared_ptr<BestConnectionOptions> pBestConnectionOptions, 
      std::shared_ptr<IWebResponder> pWebResponder,
      ConnectionEventProcessor& connectionEventProcessor)
    : _pBestConnectionOptions(pBestConnectionOptions),
      _pWebResponder(pWebResponder),
      _connectionEventProcessor(connectionEventProcessor)
    {
    }
    
    void SetAssumeConnection(bool assumeConnection)
    {
      _assumeConnection = assumeConnection;
    }
  
    bool GetAssumeConnection()
    {
      return _assumeConnection;
    }

  private:
  
    void setUpServicesUponConnection()
    { 
      // MAC is 48 bits = 12 hex
      uint64_t chipId = ESP.getEfuseMac(); 
      uint16_t chipIdHi16 = (uint16_t)(chipId >> 32);
      uint32_t chipIdLo32 = (uint32_t)chipId;
      
      // Set up mDNS responder with name "esp32-{MAC}"
      char chipIdStr[32];
      snprintf(chipIdStr, 32, "esp32-%04X%08X", chipIdHi16, chipIdLo32);
      Serial.printf("mDNS name = '%s'\n", chipIdStr);
      if (!MDNS.begin(chipIdStr)) {
          Serial.println("Error setting up mDNS responder!");
          while(1) {
              delay(1000);
          }
      }
      Serial.println("mDNS responder started");

      // Only (re)create WiFiServer once WiFi is up because otherwise
      // it'll have issues - in particular we aren't supposed to keep sockets around
      // WiFi connections
      _pWiFiServer = std::shared_ptr<WiFiServer>(new WiFiServer(SERVER_PORT));
      _pWiFiServer->begin();
      Serial.printf("TCP server started\n");

      // Add service to MDNS-SD - this will make us come up on mDNS-aware browsers
      MDNS.addService("http", "tcp", 80);
      Serial.printf("Added MDNS-SD server http:tcp:80\n");

      _servicesSetUp = true;
    }

  private:
    void disconnectAndBegin()
    {
      // int disconnectResult = WiFi.disconnect(); // WiFi.disconnect(wifioff: true, eraseap: false);
      // Serial.printf("Called WiFi.disconnect(), got %i\n", disconnectResult);
      
      // Serial.printf("Initialised WIFI_OFF, returned %i\n", WiFi.mode(WIFI_OFF));
      // Serial.printf("Initialised WIFI_MODE_STA, returned %i\n", WiFi.mode(WIFI_MODE_STA));
      // Serial.printf("Called WiFi.enableSTA(true), got %i\n", WiFi.enableSTA(true));

      if(_servicesSetUp)
      {
        if(_pWiFiServer.get())
        {
          _pWiFiServer->end();
          _pWiFiServer = 0;
          Serial.printf("TCP server stopped\n");
        }
      
        MDNS.end();
        Serial.printf("MDNS server stopped\n");

        _servicesSetUp = false;
      }
      
      int beginStatus = _pBestConnectionOptions->WiFiBegin();
      Serial.printf("Called WiFi.begin(), got %i (%s)\n", beginStatus, descForStatusCode(beginStatus));
    }
  
    static void enterLockupIfRunningForTooLong()
    {
      const int MAX_LENGTH_BEFORE_LOCKUP = 60000;
  /*
      if(millis() > MAX_LENGTH_BEFORE_LOCKUP)
      {
        Serial.printf("Have been running past limit of %i: holding so output can be examined\n", MAX_LENGTH_BEFORE_LOCKUP);
        for(;;) {}
      }
    */
    }

    int _serviceClientRequestCalls = 0;
    
public:
    void serviceClientRequests(void)
    {
      _serviceClientRequestCalls++;
      if(_serviceClientRequestCalls % 1000000 == 0)
      {
        Serial.printf("Service calls: %i\n", _serviceClientRequestCalls);
      }
      
      // Check if a client has connected
      if(! _pWiFiServer.get())
      {
        return;
      }
      WiFiClient client = _pWiFiServer->available();
      if (!client) {
          return;
      }
      Serial.printf("\nNew client\n");
  
      // Wait for data from client to become available
      while(client.connected() && !client.available()){
          delay(1);
      }
  
      // Read the first line of HTTP request
      String req = client.readStringUntil('\r');
  
      // First line of HTTP request looks like "GET /path HTTP/1.1"
      // Retrieve the "/path" part by finding the spaces
      int addr_start = req.indexOf(' ');
      int addr_end = req.indexOf(' ', addr_start + 1);
      if (addr_start == -1 || addr_end == -1) {
          Serial.printf("Invalid request: '%s'", req.c_str());
          return;
      }
      Serial.printf("Full Request: '%s'\n", req.c_str());
      req = req.substring(addr_start + 1, addr_end);
      Serial.printf("Parsed Request: '%s'\n", req.c_str());

      String response = _pWebResponder->Respond(req);
      
      int written = client.print(response);
      Serial.printf("Wrote %i bytes to client\n", written);
  
      // Flush kills the socket so don't call until we are done
      Serial.printf("before flush client.connected = %i\n", client.connected());
      client.flush();
      Serial.printf("after flush client.connected = %i\n", client.connected());
  
      Serial.println("Done with client");
    }

  public:
    void acquireAndMaintainConnection()
    {
      enterLockupIfRunningForTooLong();
      
      int status = WiFi.status();
      if(status != WL_CONNECTED || status != _prevStatus)
      {
        showConnectionStatus(status);
      }
      
      if(_assumeConnection)
      {
        if(status != WL_CONNECTED)
        {
          Serial.printf("[%i] Not connected, status: %i (%s)\n", _noStatusUpdateCount, status, descForStatusCode(status));

          auto lce = _connectionEventProcessor.GetAndClearLastEvent();
          if(lce != -1 && lce != SYSTEM_EVENT_STA_CONNECTED && lce != SYSTEM_EVENT_STA_GOT_IP)
          {
            _noStatusUpdateCount = 0;
            auto hasde = _connectionEventProcessor.GetAndClearHasDisconnectEvent();
            if(hasde)
            {
              Serial.printf("New disconnect event detected: will attempt reconnection\n");
              disconnectAndBegin();          
            }
          }
          else
          {
            _noStatusUpdateCount++;
            if(_noStatusUpdateCount == 5)
            {
              Serial.printf("No new event for %i seconds - calling WiFi.reconnect()\n", _noStatusUpdateCount);
              _noStatusUpdateCount = 0;
              disconnectAndBegin();          
            }
          }
          writeRow(1, descForStatusCode(status));
        } // not connected
        else
        {
          if(_prevStatus != WL_CONNECTED)
          {
            Serial.printf("Setting up services upon initial connection\n");
            setUpServicesUponConnection();  
          }

          // Serial.printf("r");
          serviceClientRequests();
          
          if(_connectMillis == -1)
          {
            _connectMillis = millis();
          }
          // Serial.printf("c");
          _connectCount++;
          /*
          if(_connectCount == 2)
          {
            Serial.println("");
            Serial.println("********************************************************************");
            Serial.printf("Got connection after %i ms, reboot after %i ms\n", _connectMillis, millis());
            Serial.println("********************************************************************");
            Restarter::HardRestart();
          }
          */
        } // connected
        
      } // _assumeConnection
      _prevStatus = status;
      
    }

    bool isConnected()
    {
      return _prevStatus == WL_CONNECTED;
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
      writeRow(2, "Station Mode");
      Serial.printf("Wps: SYSTEM_EVENT_STA_START\n");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      writeRow(2, ("C:" + String(WiFi.SSID())).c_str());
      writeRow(3, ("@" + WiFi.localIP().toString()).c_str());
      Serial.printf("Wps: SYSTEM_EVENT_STA_GOT_IP: %s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      wps_got_ip = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      writeRow(2, "Disc: try recon");
      Serial.printf("Wps: SYSTEM_EVENT_STA_DISCONNECTED\n");
      WiFi.reconnect();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      writeRow(2, ("W:" + String(WiFi.SSID())).c_str());
      Serial.printf("Wps: SYSTEM_EVENT_STA_WPS_ER_SUCCESS\n");
      esp_wifi_wps_disable();
      delay(10);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      writeRow(2, "WPS Fail, retry");
      Serial.printf("Wps: SYSTEM_EVENT_STA_WPS_ER_FAILED\n");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      writeRow(2, "WPS T/O, retry");
      Serial.printf("Wps: SYSTEM_EVENT_STA_WPS_ER_TIMEOUT\n");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
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

void set_time()
{
  struct tm tm;
  tm.tm_year = 2018 - 1900;
  tm.tm_mon = 12;
  tm.tm_mday = 1;
  tm.tm_hour = 23;
  tm.tm_min = 25;
  tm.tm_sec = 00;
  time_t t = mktime(&tm);
  printf("Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL); 
}


void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(100);

  // set_time();
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  Serial.printf("Setting Debug logging for wifi stack on\n");
  esp_log_level_set("wifi", ESP_LOG_DEBUG);               
  
  writeRow(0, "Hit PRG for WPS");
  delay(2000);

  if(isPrgButtonPressed())
  {
    WiFi.onEvent(WiFiEventWps);
    writeRow(0, "Trying WPS");
    auto modeRet = WiFi.mode(WIFI_MODE_STA);
    Serial.printf("Initialised WIFI_MODE_STA, returned %i\n", modeRet);
    Serial.printf("Called WiFi.enableSTA(false), got %i\n", WiFi.enableSTA(false));
    Serial.printf("Called WiFi.enableSTA(true), got %i\n", WiFi.enableSTA(true));
    initWps();
    while(! wps_got_ip)
    {
      delay(1000);
    }
    WiFi.disconnect();
  }
  else
  {
    writeRow(0, "Trying to Connect");
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

  // Serial.printf("Called WiFi.enableSTA(false), got %i\n", WiFi.enableSTA(false));
  // Serial.printf("Called WiFi.enableSTA(true), got %i\n", WiFi.enableSTA(true));
  int beginRetCode = pBestConnectionOptions->WiFiBegin();
  Serial.printf("WiFi.begin() returned %i (%s)\n", beginRetCode, descForStatusCode(beginRetCode));
  writeRow(1, descForStatusCode(beginRetCode));
  theConnectionMaintenance->SetAssumeConnection(true);
  showConnectionStatus(beginRetCode);
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
  writeRow(1, "Starting WPS");

  wpsInitConfig();
  esp_wifi_wps_enable(&config);
  esp_wifi_wps_start(0);
}

/*
 * Failure modes so far
 * 
 * 1) No events received at all
 * 2) Repeated events of
 ** WiFiEvent: 5: SYSTEM_EVENT_STA_DISCONNECTED
 ** Reason: 6: NOT_AUTHED
 */

void loop()
{
  Restarter::RestartIfWeShouldAlreadyHaveDoneSo();
  
  if(theConnectionMaintenance.get() != 0)
  {
    if(! theConnectionMaintenance->isConnected())
    {
      delay(1000);
    }

    theConnectionMaintenance->acquireAndMaintainConnection();
  }
}
