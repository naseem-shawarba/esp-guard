#pragma once

// Alarm state, control, and the deep-sleep "phase" machine for its timed waits
// (post-arm grace period, post-trigger cooldown, temporary auto-disable). State is
// persisted in NVS ("settings") and RTC memory (survives deep sleep).
namespace alarm_system
{
  void begin(); // configure the door pin and restore state from NVS

  void toggle();            // flip alarm on/off; arming schedules the grace phase
  void showStatus();        // settled state on the RGB LED (solid red/green)
  void showPendingStatus(); // temporary state: status color alternating with blue
  void onDoorOpened();      // door-trigger response: disarm window, notify, buzzer, cooldown
  void cancel();            // disarm + clear any pending phase (e.g. when entering the portal)

  bool isActive();
  bool isTemporarilyDisabled();

  // Deep-sleep phase support (used by the sleep/wake dispatcher).
  bool hasPendingPhase();            // a timed wait is in progress
  unsigned long phaseRemainingMs();  // ms until the pending phase is due (0 if due/none)
  void runDuePhase();                // run the due phase's follow-up (called on timer wake)
}
