#include "buzzer.h"

#include <Arduino.h>
#include "config.h"
#include "settings.h"

namespace buzzer
{
  void begin()
  {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
  }

  void beep(int durationMs, int frequency)
  {
    if (settings::isStealth())
    {
      return; // stealth mode: stay silent
    }

    tone(buzzerPin, frequency, durationMs);
    delay(durationMs);
  }
}
