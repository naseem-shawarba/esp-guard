#include "web_config.h"

#include <Arduino.h>
#include <WebServer.h>
#include "settings.h"

namespace web_config
{
  static WebServer *srv = nullptr;

  // Unit conversions.
  static const unsigned long MS_PER_SEC = 1000UL;
  static const unsigned long MS_PER_MIN = 60UL * 1000UL;
  static const unsigned long MS_PER_HOUR = 60UL * 60UL * 1000UL;

  // Parse a positive integer field; fall back to the current value if missing/invalid.
  static unsigned long readScaled(const char *field, unsigned long scale, unsigned long fallbackMs)
  {
    if (!srv->hasArg(field))
    {
      return fallbackMs;
    }
    long value = srv->arg(field).toInt();
    if (value <= 0)
    {
      return fallbackMs;
    }
    return (unsigned long)value * scale;
  }

  static String buildForm()
  {
    const settings::Settings &s = settings::get();

    String html;
    html.reserve(2400);
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ESP-Guard Config</title></head>";
    html += "<body style='font-family:Verdana,sans-serif;font-size:14px;'>";
    html += "<div style='max-width:420px;padding:20px;border-radius:10px;border:solid 2px #e0e0e0;margin:20px auto;'>";
    html += "<div style='text-align:center;font-size:18px;font-weight:bold;margin-bottom:12px;'>Settings</div>";
    html += "<form method='POST' action='/settings/save'>";

    html += "<label><input type='checkbox' name='stealth' value='1'";
    if (s.stealthMode)
      html += " checked";
    html += "> Stealth mode (LED off, buzzer silent)</label><br><br>";

    html += "<label>Deactivation window (seconds)<br><input type='number' min='1' name='window' value='" +
            String(s.alarmDeactivationWindowMs / MS_PER_SEC) + "'></label><br><br>";
    html += "<label>LED blink interval (ms)<br><input type='number' min='1' name='blink' value='" +
            String(s.alarmDeactivationLedBlinkMs) + "'></label><br><br>";
    html += "<label>Post-activation delay (minutes)<br><input type='number' min='1' name='actDelay' value='" +
            String(s.postAlarmActivationDelayMs / MS_PER_MIN) + "'></label><br><br>";
    html += "<label>Post-trigger delay (minutes)<br><input type='number' min='1' name='trigDelay' value='" +
            String(s.postAlarmTriggerDelayMs / MS_PER_MIN) + "'></label><br><br>";
    html += "<label>Temporary-disable period (hours)<br><input type='number' min='1' name='tmpDis' value='" +
            String(s.temporarilyDisableAlarmDelayMs / MS_PER_HOUR) + "'></label><br><br>";
    html += "<label>Portal window (minutes)<br><input type='number' min='1' name='portalDur' value='" +
            String(s.portalDurationMs / MS_PER_MIN) + "'></label><br><br>";

    html += "<input type='submit' value='Save' style='width:100%;padding:8px;'>";
    html += "</form>";
    html += "<div style='margin-top:14px;text-align:center;'><a href='/'>&larr; Menu</a></div>";
    html += "</div></body></html>";
    return html;
  }

  static void handleGet()
  {
    srv->send(200, "text/html", buildForm());
  }

  static void handleSave()
  {
    settings::Settings s = settings::get(); // start from current, override below

    s.stealthMode = srv->hasArg("stealth");
    s.alarmDeactivationWindowMs = readScaled("window", MS_PER_SEC, s.alarmDeactivationWindowMs);
    s.alarmDeactivationLedBlinkMs = readScaled("blink", 1UL, s.alarmDeactivationLedBlinkMs);
    s.postAlarmActivationDelayMs = readScaled("actDelay", MS_PER_MIN, s.postAlarmActivationDelayMs);
    s.postAlarmTriggerDelayMs = readScaled("trigDelay", MS_PER_MIN, s.postAlarmTriggerDelayMs);
    s.temporarilyDisableAlarmDelayMs = readScaled("tmpDis", MS_PER_HOUR, s.temporarilyDisableAlarmDelayMs);
    s.portalDurationMs = readScaled("portalDur", MS_PER_MIN, s.portalDurationMs);

    settings::save(s);

    String html;
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Saved</title></head>";
    html += "<body style='font-family:Verdana,sans-serif;font-size:14px;'>";
    html += "<div style='max-width:420px;padding:20px;border-radius:10px;border:solid 2px #e0e0e0;margin:20px auto;text-align:center;'>";
    html += "<div style='font-size:18px;font-weight:bold;margin-bottom:8px;'>Settings saved</div>";
    html += "<p>Stealth and timing changes apply immediately. The portal window length applies to the <b>next</b> portal session.</p>";
    html += "<a href='/'>&larr; Menu</a>";
    html += "</div></body></html>";
    srv->send(200, "text/html", html);
  }

  void registerRoutes(WebServer &server)
  {
    srv = &server;
    server.on("/settings", HTTP_GET, handleGet);
    server.on("/settings/save", HTTP_POST, handleSave);
  }
}
