#pragma once

#include <Arduino.h>

// Single source of truth for the NVS "credentials" namespace: WiFi (PSK or
// WPA2-Enterprise), Telegram, the setup-AP credentials, and an optional MAC override.
// Provisioned at runtime via the web portal; nothing is hard-coded in the firmware.
namespace credentials
{
  enum class WifiType
  {
    Psk = 0,    // normal home WiFi (SSID + password)
    Enterprise, // WPA2-Enterprise (EAP identity/username/password)
  };

  struct WifiCreds
  {
    WifiType type;
    String ssid;
    String password;     // PSK
    String eapIdentity;  // Enterprise
    String eapUsername;  // Enterprise
    String eapPassword;  // Enterprise
  };

  // Reads
  WifiCreds wifi();
  String botToken();
  String chatId();
  bool hasMac();
  void mac(uint8_t out[6]);
  String apSsid();     // OTA_SSID, or DEFAULT_AP_SSID when unset
  String apPassword(); // OTA_PASSWORD, or DEFAULT_AP_PASSWORD when unset

  // True when there's enough to operate (WiFi + Telegram configured).
  bool isConfigured();

  // Writes (used by the credentials form)
  void setWifi(const WifiCreds &creds);
  void setTelegram(const String &token, const String &chatId);
  void setAp(const String &ssid, const String &password);
  void setMac(const uint8_t mac[6]);
  void clearMac();
}
