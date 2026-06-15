#pragma once

// Piezo buzzer driven on buzzerPin.
namespace buzzer
{
  void begin(); // configure the buzzer pin and leave it silent

  void beep(int durationMs = 500, int frequency = 2000);
}
