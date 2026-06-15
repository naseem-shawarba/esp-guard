#include <Arduino.h>

#include "config.h"
#include "settings.h"
#include "credentials.h"
#include "rgb_led.h"
#include "buzzer.h"
#include "alarm.h"
#include "web_portal.h"
#include "deep_sleep.h"

// =========================
// Setup and Loop
// =========================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  settings::begin();
  rgb_led::begin();
  buzzer::begin();
  alarm_system::begin();
  web_portal::begin();

  // Startup indicator
  rgb_led::blue();

  if (!credentials::isConfigured())
  {
    // Fresh device / missing credentials -> open the setup portal so it can be provisioned.
    Serial.println("No credentials configured; entering setup portal");
    web_portal::enter();
  }
  else
  {
    deep_sleep::handleWakeup();
  }

  rgb_led::off();
  deep_sleep::goToSleep();
}

void loop()
{
  // Not used; device sleeps and wakes on interrupts
}
