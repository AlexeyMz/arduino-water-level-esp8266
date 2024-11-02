#include <string>

#include "server.h"

RestServer::RestServer()
  : webServer(80),
  indexHtml(nullptr)
{
  indexHtml = assets.find("index.html");
}

wl_status_t RestServer::wiFiStatus() {
  return WiFi.status();
}

void RestServer::startAccessPoint(
  const char* ssid,
  const char* password
) {
  IPAddress apIP(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(ssid, password);
  dnsServer.start(53, "*", apIP);

  webServer.onNotFound([=]{ this->handleMain(); });
  registerAssets();
  setupHandlers();
  webServer.begin();
}

void RestServer::connectToNetwork(
  const char* ssid,
  const char* password
) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  webServer.on("/", HTTP_GET, [=]{ this->handleMain(); });
  registerAssets();
  setupHandlers();
  webServer.begin();
}

void RestServer::registerAssets() {
  for (const AssetEntry &asset : assets) {
    std::string path = "/";
    path += asset.name;
    webServer.on(path.c_str(), HTTP_GET, [=, &asset]{
      webServer.send(200, asset.contentType, asset.content);
    });
  }
}

void RestServer::update() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}

void RestServer::stop() {
  dnsServer.stop();
  webServer.stop();
  WiFi.disconnect();
}

void RestServer::setupHandlers() {}

void RestServer::handleMain() {
  webServer.send(200, indexHtml->contentType, indexHtml->content);
}
