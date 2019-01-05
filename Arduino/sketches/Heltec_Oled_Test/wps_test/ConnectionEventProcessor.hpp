#ifndef IN_CONNECTIONEVENTPROCESSOR_HPP
#define IN_CONNECTIONEVENTPROCESSOR_HPP

#include "WiFi.h"

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

#endif 
