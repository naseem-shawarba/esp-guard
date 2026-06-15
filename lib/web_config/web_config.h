#pragma once

#include <WebServer.h>

// Serves the device configuration form on the web portal's web server.
// Register the routes after ESP2SOTA.begin() and before server.begin().
namespace web_config
{
  void registerRoutes(WebServer &server); // adds GET "/" (form) and POST "/save"
}
