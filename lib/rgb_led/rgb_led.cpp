#include "rgb_led.h"

#include <Arduino.h>
#include "config.h"

namespace rgb_led
{
  void setColor(int red, int green, int blue)
  {
    analogWrite(redPin, red);
    analogWrite(greenPin, green);
    analogWrite(bluePin, blue);
  }

  void off()
  {
    setColor(0, 0, 0);
  }

  void red()
  {
    setColor(255, 0, 0);
  }

  void green()
  {
    setColor(0, 255, 0);
  }

  void blue()
  {
    setColor(0, 0, 255);
  }

  void begin()
  {
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
  }
}
