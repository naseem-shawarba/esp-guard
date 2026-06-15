#pragma once

// Wakeup dispatch and deep-sleep configuration. Decides which wakeup source fired
// (door, toggle, status, portal, timer) and routes it; then arms EXT1 wakeup and sleeps.
namespace deep_sleep
{
  void handleWakeup(); // act on the cause of this boot

  void goToSleep(); // configure wakeup sources and enter deep sleep
}
