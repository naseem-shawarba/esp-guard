#include "web_portal.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP2SOTA.h>
#include "config.h"
#include "settings.h"
#include "credentials.h"
#include "rgb_led.h"
#include "connectivity.h"
#include "web_config.h"
#include "web_credentials.h"

namespace web_portal
{
  static WebServer server(80);
  static DNSServer dnsServer;

  void begin()
  {
    pinMode(portalPin, INPUT);
  }

  static String menuHtml()
  {
    String html;
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ESP-Guard</title></head>";
    html += "<body style='font-family:Verdana,sans-serif;font-size:15px;'>";
    html += "<div style='max-width:420px;padding:20px;border-radius:10px;border:solid 2px #e0e0e0;margin:20px auto;'>";
    html += "<div style='text-align:center;font-size:20px;font-weight:bold;margin-bottom:16px;'>ESP-Guard</div>";
    html += "<p><a href='/credentials'>&#128272; Configure WiFi &amp; Telegram</a></p>";
    html += "<p><a href='/settings'>&#9881; Settings (stealth &amp; timings)</a></p>";
    html += "<p><a href='/update'>&#11014; Firmware update</a></p>";
    html += "</div></body></html>";
    return html;
  }

  static void handleMenu()
  {
    server.send(200, "text/html", menuHtml());
  }

  // Captive-portal: send any unknown URL to the menu so the OS sign-in page pops up.
  static void handleNotFound()
  {
    server.sendHeader("Location", "http://10.10.10.1/", true);
    server.send(302, "text/plain", "");
  }

  void enter()
  {
    delay(1000);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(credentials::apSsid().c_str(), credentials::apPassword().c_str());
    delay(1000);
    IPAddress IP = IPAddress(10, 10, 10, 1);
    IPAddress NMask = IPAddress(255, 255, 255, 0);
    WiFi.softAPConfig(IP, IP, NMask);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Captive portal: resolve every hostname to us so the menu auto-opens on connect.
    dnsServer.start(53, "*", IP);

    // Routes: menu, credentials form, settings form, and ESP2SOTA's /update.
    ESP2SOTA.begin(&server);
    server.on("/", HTTP_GET, handleMenu);
    web_credentials::registerRoutes(server);
    web_config::registerRoutes(server);
    server.onNotFound(handleNotFound);
    server.begin();

    delay(1000);
    unsigned long startTime = millis();
    const unsigned long durationMs = settings::get().portalDurationMs;

    while (millis() - startTime < durationMs)
    {
      if ((millis() % 1500 > 750))
      {
        rgb_led::blue();
      }
      else
      {
        rgb_led::off();
      }
      if (digitalRead(portalPin))
      {
        connectivity::connectWiFi();
        connectivity::sendMessage("Exiting portal mode");
        break;
      }
      dnsServer.processNextRequest();
      server.handleClient();
      delay(5);
    }

    dnsServer.stop();
    rgb_led::blue();
    Serial.println("Portal window ended");
  }
}
