#pragma once

#include <WebServer.h>

// Serves the credentials form (WiFi + Telegram + setup-AP + optional MAC) on the portal.
// Register after ESP2SOTA.begin() and before server.begin().
namespace web_credentials
{
  void registerRoutes(WebServer &server); // GET /credentials, POST /credentials/save
}
