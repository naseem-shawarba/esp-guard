#include "alarm.h"

#include <Arduino.h>
#include <Preferences.h>
#include <sys/time.h>
#include "config.h"
#include "settings.h"
#include "rgb_led.h"
#include "connectivity.h"
#include "buzzer.h"

namespace alarm_system
{
  static bool isAlarmActive = false; // restored from NVS in begin()

  // Persisted across deep sleep in RTC memory.
  RTC_DATA_ATTR static bool temporarilyDisableAlarm = false;
  RTC_DATA_ATTR static int alarmTriggerCount = 0;

  // Deep-sleep phase machine (the timed "armed" waits).
  enum class Phase : uint8_t
  {
    None = 0, // value on cold power-on
    Grace,        // after arming, before reporting door state
    Cooldown,     // after a trigger
    TempDisabled, // auto-disabled after too many triggers
  };
  RTC_DATA_ATTR static Phase pendingPhase = Phase::None;
  RTC_DATA_ATTR static uint64_t phaseDeadlineMs = 0;

  // Wall-clock milliseconds; ESP-IDF keeps this advancing across deep sleep.
  static uint64_t nowMs()
  {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
  }

  static void schedulePhase(Phase phase, unsigned long durationMs)
  {
    pendingPhase = phase;
    phaseDeadlineMs = nowMs() + durationMs;
  }

  static void cancelPhase()
  {
    pendingPhase = Phase::None;
  }

  static void save()
  {
    Preferences preferences;
    preferences.begin("settings", false);
    preferences.putBool("isAlarmActive", isAlarmActive);
    preferences.end();
  }

  bool isActive() { return isAlarmActive; }
  bool isTemporarilyDisabled() { return temporarilyDisableAlarm; }
  bool hasPendingPhase() { return pendingPhase != Phase::None; }

  unsigned long phaseRemainingMs()
  {
    uint64_t now = nowMs();
    return phaseDeadlineMs > now ? (unsigned long)(phaseDeadlineMs - now) : 0;
  }

  void showStatus()
  {
    Serial.println(isAlarmActive ? "Alarm is active" : "Alarm is not active");
    if (isAlarmActive)
    {
      rgb_led::red();
    }
    else
    {
      rgb_led::green();
    }

    delay(4000);
    rgb_led::blue();
  }

  void showPendingStatus()
  {
    // Temporary state (mid timed-wait): alternate the status color with blue so it's
    // visually distinct from the settled showStatus().
    Serial.println(isAlarmActive ? "Alarm pending (active)" : "Alarm pending (inactive)");

    const int cycles = 5;
    const int blinkMs = 400; // ~5 * (400 + 400) = 4s, matching showStatus duration
    for (int i = 0; i < cycles; i++)
    {
      if (isAlarmActive)
      {
        rgb_led::red();
      }
      else
      {
        rgb_led::green();
      }
      delay(blinkMs);
      rgb_led::blue();
      delay(blinkMs);
    }
  }

  // Awake, blinking countdown for the short disarm window. Returns true if the user
  // disarmed before it elapsed.
  static bool waitForDeactivation()
  {
    const unsigned long windowMs = settings::get().alarmDeactivationWindowMs;
    const unsigned long blinkMs = settings::get().alarmDeactivationLedBlinkMs;

    int elapsed = 0;
    bool ledState = false;

    Serial.println("You have a few seconds to deactivate the alarm...");

    while (elapsed < windowMs)
    {
      ledState = !ledState;
      if (ledState)
      {
        rgb_led::red();
      }
      else
      {
        rgb_led::off();
      }

      if (digitalRead(wakeupGpioToggle) == HIGH)
      {
        toggle(); // disarm
        rgb_led::off();
        Serial.println("Alarm deactivated before it could sound!");
        return true;
      }

      delay(blinkMs);
      elapsed += blinkMs;
      Serial.println(max(0UL, (windowMs - elapsed) / 1000));
    }

    rgb_led::blue();
    return false; // still armed
  }

  
  static String formatDuration(unsigned long ms)
  {
    if (ms >= 60000UL)
    {
      return String(ms / 60000UL) + " min";
    }
    return String(ms / 1000UL) + " sec";
  }

  void toggle()
  {
    isAlarmActive = !isAlarmActive;
    save();

    Serial.println(isAlarmActive ? "Activated Alarm" : "Deactivated Alarm");
    showStatus();

    if (isAlarmActive)
    {

      unsigned long bufferMs = settings::get().postAlarmActivationDelayMs;
      String msg = "Alarm activated.";
      if (bufferMs > 0)
      {
        msg += " Arming in " + formatDuration(bufferMs) + ".";
      }

      
      msg += digitalRead(doorPin) ? "Door is open" : "Door is closed";

      connectivity::connectWiFi();
      connectivity::sendMessage(msg);

      // Grace period after arming; the door state is reported when it elapses.
      schedulePhase(Phase::Grace, bufferMs);
    }
    else
    {
      cancelPhase();
    }
  }

  void onDoorOpened()
  {
    Serial.println("Door sensor triggered");

    if (waitForDeactivation())
    {
      return; // disarmed within the window
    }

    // Still armed -> notify, sound the buzzer, then cooldown (in deep sleep).
    alarmTriggerCount += 1;
    connectivity::connectWiFi();
    connectivity::sendMessage("The Door has been opened");
    buzzer::beep(5000, 2500);
    schedulePhase(Phase::Cooldown, settings::get().postAlarmTriggerDelayMs);
  }

  void cancel()
  {
    isAlarmActive = false;
    save();
    cancelPhase();
    Serial.println("Alarm cancelled");
  }

  void runDuePhase()
  {
    switch (pendingPhase)
    {
    case Phase::Grace:
    {
      bool doorOpen = digitalRead(doorPin);
      connectivity::connectWiFi();
      connectivity::sendMessage(doorOpen ? "Door is open" : "Door is closed");
      cancelPhase();
      break;
    }

    case Phase::Cooldown:
      if (alarmTriggerCount >= maxAlarmTriggers)
      {
        connectivity::connectWiFi();
        connectivity::sendMessage("The alarm has been temporarily deactivated");
        temporarilyDisableAlarm = true;
        schedulePhase(Phase::TempDisabled, settings::get().temporarilyDisableAlarmDelayMs);
      }
      else
      {
        cancelPhase();
      }
      break;

    case Phase::TempDisabled:
      temporarilyDisableAlarm = false;
      alarmTriggerCount = 0;
      connectivity::connectWiFi();
      connectivity::sendMessage("The alarm has been re-activated after temporal deactivation");
      cancelPhase();
      break;

    case Phase::None:
    default:
      break;
    }
  }

  void begin()
  {
    pinMode(doorPin, INPUT);

    Preferences preferences;
    preferences.begin("settings", true); // read-only
    isAlarmActive = preferences.getBool("isAlarmActive", false);
    preferences.end();
  }
}
