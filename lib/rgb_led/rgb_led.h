#pragma once

// RGB LED control. Single common-anode/cathode RGB LED wired to redPin/greenPin/bluePin.
namespace rgb_led
{
  void begin();
  
  void setColor(int red, int green, int blue);

  void off();
  void red();
  void green();
  void blue();
}
