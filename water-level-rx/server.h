#ifndef _SERVER_H
#define _SERVER_H

#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "server_assets.h"

class RestServer {
  DNSServer dnsServer;

protected:
  AssetInfo assets;
  const AssetEntry *indexHtml;

public:
  ESP8266WebServer webServer;

  RestServer();

  wl_status_t wiFiStatus();

  void startAccessPoint(
    const char* ssid,
    const char* password
  );
  void connectToNetwork(
    const char* ssid,
    const char* password
  );
  void update();
  void stop();

protected:
  virtual void setupHandlers();
  virtual void handleMain();

private:
  void registerAssets();
};

#endif