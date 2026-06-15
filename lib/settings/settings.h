#pragma once

// Runtime-configurable settings, persisted in the NVS "settings" namespace.
// Defaults come from the DEFAULT_* constants in config.h when NVS is empty.
namespace settings
{
  struct Settings
  {
    bool stealthMode;                             // when true: LED stays off, buzzer silent
    unsigned long alarmDeactivationWindowMs;      // window to disarm before the alarm sounds
    unsigned long alarmDeactivationLedBlinkMs;    // LED blink interval during that window
    unsigned long postAlarmActivationDelayMs;     // wait after arming before checking the door
    unsigned long postAlarmTriggerDelayMs;        // cooldown after a trigger
    unsigned long temporarilyDisableAlarmDelayMs; // how long a temporary disable lasts
    unsigned long portalDurationMs;               // how long the web portal stays open
  };

  void begin();                  // load from NVS (DEFAULT_* fallbacks)
  const Settings &get();         // current in-RAM settings
  bool isStealth();              // convenience: get().stealthMode
  void save(const Settings &s);  // persist to NVS and update the in-RAM copy
}
