#pragma once

#include <Arduino.h>

// WiFi (WPA2-Enterprise) connection + Telegram messaging.
// Credentials are read from the NVS "credentials" namespace.
namespace connectivity
{
  void connectWiFi();

  bool sendMessage(const String &message);
}
