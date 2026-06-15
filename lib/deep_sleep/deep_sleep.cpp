#include "deep_sleep.h"

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config.h"
#include "alarm.h"
#include "web_portal.h"

namespace deep_sleep
{
  static void handleExtWakeUp()
  {
    uint64_t wakeupPinStatus = esp_sleep_get_ext1_wakeup_status();

    if (wakeupPinStatus & PIN_BITMASK(wakeupGpioDoor))
    {
      if (alarm_system::isActive())
      {
        alarm_system::onDoorOpened();
      }
    }

    if (wakeupPinStatus & PIN_BITMASK(wakeupGpioPortal))
    {
      // Entering the portal mid-wait cancels the alarm (and its pending phase).
      if (alarm_system::hasPendingPhase())
      {
        alarm_system::cancel();
      }
      web_portal::enter();
    }

    if (wakeupPinStatus & PIN_BITMASK(wakeupGpioToggle))
    {
      alarm_system::toggle();
    }

    if (wakeupPinStatus & PIN_BITMASK(wakeupGpioStatus))
    {
      // A pending phase is the "temporary" state -> show the pending blink instead.
      if (alarm_system::hasPendingPhase())
      {
        alarm_system::showPendingStatus();
      }
      else
      {
        alarm_system::showStatus();
      }
    }
  }

  void handleWakeup()
  {
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    switch (wakeupReason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      handleExtWakeUp();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      alarm_system::runDuePhase(); // a timed phase came due
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup not caused by deep sleep: %d\n", wakeupReason);
      break;
    }
  }

  void goToSleep()
  {
    bool phasePending = alarm_system::hasPendingPhase();

    // Always wake on the buttons.
    uint64_t wakeupBitmask = PIN_BITMASK(wakeupGpioToggle) | PIN_BITMASK(wakeupGpioStatus) | PIN_BITMASK(wakeupGpioPortal);

    // Watch the door only when armed and not in a timed phase or temporary disable.
    bool watchDoor = !phasePending && alarm_system::isActive() && !alarm_system::isTemporarilyDisabled();
    if (watchDoor)
    {
      wakeupBitmask |= PIN_BITMASK(wakeupGpioDoor);
    }

    esp_sleep_enable_ext1_wakeup(wakeupBitmask, ESP_EXT1_WAKEUP_ANY_HIGH);

    // Configure pull-ups / pull-downs for the watched pins.
    rtc_gpio_pullup_dis(wakeupGpioToggle);
    rtc_gpio_pulldown_dis(wakeupGpioToggle);
    rtc_gpio_pullup_dis(wakeupGpioStatus);
    rtc_gpio_pulldown_dis(wakeupGpioStatus);
    rtc_gpio_pullup_dis(wakeupGpioPortal);
    rtc_gpio_pulldown_dis(wakeupGpioPortal);
    if (watchDoor)
    {
      rtc_gpio_pullup_dis(wakeupGpioDoor);
      rtc_gpio_pulldown_dis(wakeupGpioDoor);
    }

    // Timed phase -> also wake when it's due; otherwise clear any stale timer.
    if (phasePending)
    {
      esp_sleep_enable_timer_wakeup((uint64_t)alarm_system::phaseRemainingMs() * 1000ULL);
    }
    else
    {
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }
}
