#include "web_credentials.h"

#include <Arduino.h>
#include <WebServer.h>
#include "credentials.h"

namespace web_credentials
{
  static WebServer *srv = nullptr;

  // Submitted value if present and non-empty, else the fallback (so "blank = keep existing").
  static String argOr(const char *field, const String &fallback)
  {
    if (srv->hasArg(field) && srv->arg(field).length() > 0)
    {
      return srv->arg(field);
    }
    return fallback;
  }

  // Parse "AABBCCDDEEFF" / "AA:BB:..." into 6 bytes. Returns false if not exactly 6 bytes.
  static bool parseMac(const String &in, uint8_t out[6])
  {
    String hex;
    for (size_t i = 0; i < in.length(); i++)
    {
      char c = in[i];
      if (isxdigit(c))
      {
        hex += c;
      }
    }
    if (hex.length() != 12)
    {
      return false;
    }
    for (int i = 0; i < 6; i++)
    {
      out[i] = (uint8_t)strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }
    return true;
  }

  static String buildForm()
  {
    credentials::WifiCreds w = credentials::wifi();
    bool ent = (w.type == credentials::WifiType::Enterprise);

    String html;
    html.reserve(3200);
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ESP-Guard Credentials</title></head>";
    html += "<body style='font-family:Verdana,sans-serif;font-size:14px;'>";
    html += "<div style='max-width:440px;padding:20px;border-radius:10px;border:solid 2px #e0e0e0;margin:20px auto;'>";
    html += "<div style='text-align:center;font-size:18px;font-weight:bold;margin-bottom:12px;'>WiFi &amp; Telegram</div>";
    html += "<p style='color:#888;'>Leave password/token fields blank to keep the current value.</p>";
    html += "<form method='POST' action='/credentials/save'>";

    // WiFi type selector
    html += "<label>WiFi type<br><select name='wifiType' id='wifiType' onchange='toggle()'>";
    html += "<option value='psk'" + String(ent ? "" : " selected") + ">Home (WPA2-PSK)</option>";
    html += "<option value='ent'" + String(ent ? " selected" : "") + ">Enterprise (WPA2-EAP)</option>";
    html += "</select></label><br><br>";

    html += "<label>WiFi SSID<br><input name='ssid' value='" + w.ssid + "'></label><br><br>";

    // PSK section
    html += "<div id='pskBox'>";
    html += "<label>WiFi password<br><input type='password' name='psk' placeholder='(unchanged)'></label><br><br>";
    html += "</div>";

    // Enterprise section
    html += "<div id='entBox'>";
    html += "<label>EAP identity<br><input name='eapId' value='" + w.eapIdentity + "'></label><br><br>";
    html += "<label>EAP username<br><input name='eapUser' value='" + w.eapUsername + "'></label><br><br>";
    html += "<label>EAP password<br><input type='password' name='eapPass' placeholder='(unchanged)'></label><br><br>";
    html += "</div>";

    html += "<hr>";
    html += "<label>Telegram bot token<br><input type='password' name='token' placeholder='(unchanged)'></label><br><br>";
    html += "<label>Telegram chat ID<br><input name='chat' value='" + credentials::chatId() + "'></label><br><br>";

    html += "<hr>";
    html += "<label>Setup-AP SSID<br><input name='apSsid' value='" + credentials::apSsid() + "'></label><br><br>";
    html += "<label>Setup-AP password<br><input type='password' name='apPass' placeholder='(unchanged)'></label><br><br>";
    html += "<label>MAC override (optional, e.g. AA:BB:CC:DD:EE:FF)<br><input name='mac' placeholder='(unchanged)'></label><br><br>";

    html += "<input type='submit' value='Save' style='width:100%;padding:8px;'>";
    html += "</form>";
    html += "<div style='margin-top:14px;text-align:center;'><a href='/'>&larr; Menu</a></div>";
    html += "</div>";

    // Show only the relevant WiFi fields.
    html += "<script>function toggle(){var e=document.getElementById('wifiType').value=='ent';";
    html += "document.getElementById('entBox').style.display=e?'block':'none';";
    html += "document.getElementById('pskBox').style.display=e?'none':'block';}toggle();</script>";
    html += "</body></html>";
    return html;
  }

  static void handleGet()
  {
    srv->send(200, "text/html", buildForm());
  }

  static void handleSave()
  {
    credentials::WifiCreds cur = credentials::wifi();

    credentials::WifiCreds w;
    w.type = (srv->arg("wifiType") == "ent") ? credentials::WifiType::Enterprise
                                             : credentials::WifiType::Psk;
    w.ssid = argOr("ssid", cur.ssid);
    w.password = argOr("psk", cur.password);
    w.eapIdentity = argOr("eapId", cur.eapIdentity);
    w.eapUsername = argOr("eapUser", cur.eapUsername);
    w.eapPassword = argOr("eapPass", cur.eapPassword);
    credentials::setWifi(w);

    credentials::setTelegram(argOr("token", credentials::botToken()),
                             argOr("chat", credentials::chatId()));

    credentials::setAp(argOr("apSsid", credentials::apSsid()),
                       argOr("apPass", credentials::apPassword()));

    // MAC: only touch it if the user typed a valid one (blank = leave unchanged).
    if (srv->hasArg("mac") && srv->arg("mac").length() > 0)
    {
      uint8_t macBytes[6];
      if (parseMac(srv->arg("mac"), macBytes))
      {
        credentials::setMac(macBytes);
      }
    }

    String html;
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Saved</title></head>";
    html += "<body style='font-family:Verdana,sans-serif;font-size:14px;'>";
    html += "<div style='max-width:440px;padding:20px;border-radius:10px;border:solid 2px #e0e0e0;margin:20px auto;text-align:center;'>";
    html += "<div style='font-size:18px;font-weight:bold;margin-bottom:8px;'>Credentials saved</div>";
    html += "<p>They take effect the next time the device connects. You can close this page.</p>";
    html += "<a href='/'>&larr; Menu</a>";
    html += "</div></body></html>";
    srv->send(200, "text/html", html);
  }

  void registerRoutes(WebServer &server)
  {
    srv = &server;
    server.on("/credentials", HTTP_GET, handleGet);
    server.on("/credentials/save", HTTP_POST, handleSave);
  }
}
