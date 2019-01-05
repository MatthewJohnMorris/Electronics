#ifndef IN_CONNECTIONMAINTENANCE_HPP
#define IN_CONNECTIONMAINTENANCE_HPP

#include <FastLED.h>

#include "WiFi.h"
#include "ESPmDNS.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp.h"

#include "BestConnectionOptions.hpp"
#include "IWebResponder.hpp"
#include "Utilities.hpp"

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
          u8x8_writeRow(1, descForStatusCode(status));
        } // not connected
        else
        {
          if(_prevStatus != WL_CONNECTED)
          {
            Serial.printf("Setting up services upon initial connection\n");
            setUpServicesUponConnection();  

            led_setColour(CRGB::Green);
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

#endif // IN_CONNECTIONMAINTENANCE_HPP
