#include "connectivity.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <esp_wifi.h>
#include <esp_wpa2.h>
#include "credentials.h"

namespace connectivity
{
  // One secure client shared across messages.
  static WiFiClientSecure client;

  static const unsigned long CONNECT_TIMEOUT_MS = 20000;

  void connectWiFi()
  {
    // Fully restart the WiFi stack.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);

    // Optional MAC override (only when one has been provisioned).
    if (credentials::hasMac())
    {
      uint8_t macBytes[6];
      credentials::mac(macBytes);
      esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, macBytes);
      Serial.println(err == ESP_OK ? "Success changing MAC Address" : "Failed to set MAC");
    }

    credentials::WifiCreds w = credentials::wifi();
    if (w.type == credentials::WifiType::Enterprise)
    {
      Serial.println("Connecting to WPA2-Enterprise WiFi...");
      WiFi.begin(w.ssid.c_str());
      esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)w.eapIdentity.c_str(), w.eapIdentity.length());
      esp_wifi_sta_wpa2_ent_set_username((uint8_t *)w.eapUsername.c_str(), w.eapUsername.length());
      esp_wifi_sta_wpa2_ent_set_password((uint8_t *)w.eapPassword.c_str(), w.eapPassword.length());
      esp_wifi_sta_wpa2_ent_enable();
    }
    else
    {
      Serial.println("Connecting to WiFi (WPA2-PSK)...");
      WiFi.begin(w.ssid.c_str(), w.password.c_str());
    }

    // Bounded wait so a misconfigured device doesn't hang forever.
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS)
    {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nWiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.println("\nWiFi connect timed out");
    }

    // Required for the HTTPS Telegram connection.
    client.setInsecure();
  }

  bool sendMessage(const String &message)
  {
    delay(1000);
    String token = credentials::botToken();
    String chat = credentials::chatId();
    delay(500);

    UniversalTelegramBot bot(token.c_str(), client);
    Serial.print("Sending message: ");
    Serial.println(message);

    bool sent = bot.sendMessage(chat, message, "");
    Serial.println(sent ? "Message sent successfully" : "Message failed");
    delay(1000);
    return sent;
  }
}
