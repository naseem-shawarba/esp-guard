#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

namespace settings
{
  // NVS namespace shared with the alarm module (distinct keys). Keys must be <= 15 chars.
  static const char *NVS_NAMESPACE = "settings";
  static const char *KEY_STEALTH = "stealth";
  static const char *KEY_DEACT_WIN = "deactWin";
  static const char *KEY_BLINK = "blink";
  static const char *KEY_ACT_DELAY = "actDelay";
  static const char *KEY_TRIG_DELAY = "trigDelay";
  static const char *KEY_TMP_DIS = "tmpDis";
  static const char *KEY_PORTAL_DUR = "portalDur"; // NVS key string kept for compatibility

  static Settings current;

  void begin()
  {
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE, true); // read-only
    current.stealthMode = preferences.getBool(KEY_STEALTH, DEFAULT_stealthMode);
    current.alarmDeactivationWindowMs = preferences.getULong(KEY_DEACT_WIN, DEFAULT_alarmDeactivationWindowMs);
    current.alarmDeactivationLedBlinkMs = preferences.getULong(KEY_BLINK, DEFAULT_alarmDeactivationLedBlinkMs);
    current.postAlarmActivationDelayMs = preferences.getULong(KEY_ACT_DELAY, DEFAULT_postAlarmActivationDelayMs);
    current.postAlarmTriggerDelayMs = preferences.getULong(KEY_TRIG_DELAY, DEFAULT_postAlarmTriggerDelayMs);
    current.temporarilyDisableAlarmDelayMs = preferences.getULong(KEY_TMP_DIS, DEFAULT_temporarilyDisableAlarmDelayMs);
    current.portalDurationMs = preferences.getULong(KEY_PORTAL_DUR, DEFAULT_portalDurationMs);
    preferences.end();
  }

  const Settings &get()
  {
    return current;
  }

  bool isStealth()
  {
    return current.stealthMode;
  }

  void save(const Settings &s)
  {
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE, false); // read-write
    preferences.putBool(KEY_STEALTH, s.stealthMode);
    preferences.putULong(KEY_DEACT_WIN, s.alarmDeactivationWindowMs);
    preferences.putULong(KEY_BLINK, s.alarmDeactivationLedBlinkMs);
    preferences.putULong(KEY_ACT_DELAY, s.postAlarmActivationDelayMs);
    preferences.putULong(KEY_TRIG_DELAY, s.postAlarmTriggerDelayMs);
    preferences.putULong(KEY_TMP_DIS, s.temporarilyDisableAlarmDelayMs);
    preferences.putULong(KEY_PORTAL_DUR, s.portalDurationMs);
    preferences.end();

    current = s; // takes effect immediately
  }
}
