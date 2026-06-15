#pragma once

// Temporary web portal: brings up a WiFi SoftAP + web server for a bounded window,
// hosting the settings form (web_config) and the firmware updater (ESP2SOTA /update).
// SoftAP credentials are read from the NVS "credentials" namespace.
namespace web_portal
{
  void begin(); // configure the portal-exit pin

  void enter(); // open the AP + web server until the window elapses or the button is pressed
}
