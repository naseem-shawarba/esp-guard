#include "credentials.h"

#include <Preferences.h>
#include "config.h"

namespace credentials
{
  static const char *NVS_NAMESPACE = "credentials";

  // Keys (all <= 15 chars). Several are kept verbatim for compatibility with pre-existing data.
  static const char *KEY_WIFI_TYPE = "wifiType";
  static const char *KEY_WIFI_SSID = "WIFI_SSID";
  static const char *KEY_WIFI_PASS = "WIFI_PASSWORD";
  static const char *KEY_EAP_IDENTITY = "EAP_IDENTITY";
  static const char *KEY_EAP_USERNAME = "EAP_USERNAME";
  static const char *KEY_EAP_PASSWORD = "EAP_PASSWORD";
  static const char *KEY_BOT_TOKEN = "BOT_TOKEN";
  static const char *KEY_CHAT_ID = "CHAT_ID";
  static const char *KEY_AP_SSID = "OTA_SSID";
  static const char *KEY_AP_PASS = "OTA_PASSWORD";
  static const char *KEY_MAC = "newMACAddress";

  WifiCreds wifi()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    WifiCreds c;
    c.type = (WifiType)prefs.getUChar(KEY_WIFI_TYPE, (uint8_t)WifiType::Psk);
    c.ssid = prefs.getString(KEY_WIFI_SSID, "");
    c.password = prefs.getString(KEY_WIFI_PASS, "");
    c.eapIdentity = prefs.getString(KEY_EAP_IDENTITY, "");
    c.eapUsername = prefs.getString(KEY_EAP_USERNAME, "");
    c.eapPassword = prefs.getString(KEY_EAP_PASSWORD, "");
    prefs.end();
    return c;
  }

  String botToken()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    String v = prefs.getString(KEY_BOT_TOKEN, "");
    prefs.end();
    return v;
  }

  String chatId()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    String v = prefs.getString(KEY_CHAT_ID, "");
    prefs.end();
    return v;
  }

  bool hasMac()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    bool present = prefs.getBytesLength(KEY_MAC) == 6;
    prefs.end();
    return present;
  }

  void mac(uint8_t out[6])
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    prefs.getBytes(KEY_MAC, out, 6);
    prefs.end();
  }

  String apSsid()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    String v = prefs.getString(KEY_AP_SSID, "");
    prefs.end();
    return v.length() ? v : String(DEFAULT_AP_SSID);
  }

  String apPassword()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    String v = prefs.getString(KEY_AP_PASS, "");
    prefs.end();
    return v.length() ? v : String(DEFAULT_AP_PASSWORD);
  }

  bool isConfigured()
  {
    WifiCreds c = wifi();
    if (c.ssid.length() == 0)
    {
      return false;
    }
    if (c.type == WifiType::Psk && c.password.length() == 0)
    {
      return false;
    }
    if (c.type == WifiType::Enterprise && (c.eapUsername.length() == 0 || c.eapPassword.length() == 0))
    {
      return false;
    }
    return botToken().length() > 0 && chatId().length() > 0;
  }

  void setWifi(const WifiCreds &creds)
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(KEY_WIFI_TYPE, (uint8_t)creds.type);
    prefs.putString(KEY_WIFI_SSID, creds.ssid);
    prefs.putString(KEY_WIFI_PASS, creds.password);
    prefs.putString(KEY_EAP_IDENTITY, creds.eapIdentity);
    prefs.putString(KEY_EAP_USERNAME, creds.eapUsername);
    prefs.putString(KEY_EAP_PASSWORD, creds.eapPassword);
    prefs.end();
  }

  void setTelegram(const String &token, const String &chat)
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(KEY_BOT_TOKEN, token);
    prefs.putString(KEY_CHAT_ID, chat);
    prefs.end();
  }

  void setAp(const String &ssid, const String &password)
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(KEY_AP_SSID, ssid);
    prefs.putString(KEY_AP_PASS, password);
    prefs.end();
  }

  void setMac(const uint8_t macBytes[6])
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes(KEY_MAC, macBytes, 6);
    prefs.end();
  }

  void clearMac()
  {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(KEY_MAC);
    prefs.end();
  }
}
