#include <ArduinoJson.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <cstdint>
#include <string>

#include "blink_pattern.h"
#include "button.h"
#include "colors.h"

enum class State {
  Initial,
  OpenAccessPoint,
  ConnectingToWiFi,
  ConnectionError,
  ReadyToWork,
  ReadingSensor,
};

State state = State::Initial;

DNSServer dnsServer;
ESP8266WebServer webServer(80);

const char MAIN_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WaterMeter.RX</title>
</head>
<body>
<style type="text/css">
  .main { display: flex; flex-direction: column; align-items: center; }

  .tabs { display: flex; padding: 0; }
  .tab { list-style-type: none; margin: 0 5px; padding: 2px; border: 1px solid gray; cursor: pointer; }
  .tab.active { background: aquamarine; }
  .tab-content { visibility: collapse; display: flex; flex-direction: column; }
  .tab-content.active { visibility: visible; }

  form { display: flex; flex-direction: column; }
  form > input { margin-bottom: 5px; }
  .error { color:red; }
</style>
<script>
  const $ = document.querySelector.bind(document);
  document.addEventListener('DOMContentLoaded', () => {
    function activateTab(tab) {
      const tabId = tab.getAttribute('data-tab');
      if (tabId) {
        document.querySelectorAll('.tab-content, .tab').forEach(el => el.classList.remove('active'));
        document.getElementById(tabId).classList.add('active');
        tab.classList.add('active');
      }
    }

    $('.tabs').addEventListener('click', e => {
      if (e.target instanceof HTMLElement) {
        activateTab(e.target);
      }
    });

    activateTab($('.tab:first-child'));

    async function fetchJson(url, request) {
      const response = await fetch(url, request);
      if (!response.ok) {
        throw new Error(`Request failed: ${response.status} ${response.statusText}`);
      }
      return await response.json();
    }

    setInterval(async () => {
      try {
        const data = await fetchJson('/sensor');
        $('#sensor-value').innerText = data.value;
        $('#sensor-error').innerText = '';
      } catch (e) {
        $('#sensor-error').innerText = e.message;
      }
    }, 500);

    (async function () {
      try {
        const settings = await fetchJson('/connect-settings');
        $('#connect-ssid').value = settings.ssid;
        $('#connect-pass').value = settings.password;
        $('#connect-error').innerText = '';
      } catch (e) {
        $('#connect-error').innerText = e.message;
      }
    })();
  });
</script>
<div class="main">
  <h2>WaterMeter.RX</h2>

  <ul class="tabs">
    <li class="tab" data-tab="sensor-tab">Sensor</li>
    <li class="tab" data-tab="connect-tab">Connect</li>
    <li class="tab" data-tab="calibrate-tab">Calibrate</li>
  </ul>

  <section id="sensor-tab" class="tab-content">
    <h3>Sensor</h3>
    <div>Raw value: <span id='sensor-value'>&mdash;</span></div>
    <div class='error' id='sensor-error'></div>
  </section>

  <section id="connect-tab" class="tab-content">
    <h3>Connect</h3>
    <form method="POST" action="/connect" autocomplete="off">
      <label for="connect-ssid">SSID:</label>
      <input type="text" id="connect-ssid" name="ssid">
      <label for="connect-pass">Password:</label>
      <input type="text" id="connect-pass" name="password">
      <input type="submit" value="Connect">
    </form>
    <div class='error' id='connect-error'></div>
  </section>

  <section id="calibrate-tab" class="tab-content">
    <h3>Calibrate</h3>
  </section>
</div>
</body></html>)rawliteral";

const uint8_t R_PIN = 4; // D1
const uint8_t G_PIN = 5; // D2
const uint8_t B_PIN = 14; // D5
const uint8_t BUTTON_PIN = 12; // D6

Button button(BUTTON_PIN);

enum class ConnectMode { Default, ForceAP };
const uint16_t DISCONNECT_TIMEOUT = 5000;
wl_status_t lastWiFiStatus;
uint32_t lastSensorReadTime = 0;
uint32_t lastActivityTime = 0;

BlinkPattern blinking;
RgbColor ledColor = { 0, 0, 0 };

const uint32_t EEPROM_MAGIC = 0xABCD4321;
const uint32_t EEPROM_VERSION = 1;
const size_t MAX_SSID_LENGTH = 32;
const size_t MAX_PASSWORD_LENGTH = 64;

struct EepromData {
  uint32_t magic;
  uint32_t version;
  char ssid[MAX_SSID_LENGTH];
  char password[MAX_PASSWORD_LENGTH];
};

EepromData storedData;

bool isValidEepromData(const EepromData &data) {
  return data.magic == EEPROM_MAGIC && data.version == EEPROM_VERSION;
}

void setup() {
  Serial.begin(9600);

  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  EEPROM.begin(sizeof(EepromData));
  EEPROM.get(0, storedData);
  if (isValidEepromData(storedData)) {
    Serial.print("Found valid EEPROM data, version = ");
    Serial.println(storedData.version);
  } else {
    Serial.println("Not found valid EEPROM data");
    memset(&storedData, 0, sizeof(storedData));
    storedData.magic = EEPROM_MAGIC;
    storedData.version = EEPROM_VERSION;
  }

  transitionToState(State::Initial);
  serverStart(ConnectMode::Default);
}

void loop() {
  auto time = millis();
  serverTick();

  button.update(time);
  if (button.isPressed()) {
    updateActivityTime();
  }

  if (blinking.update(time)) {
    if (blinking.isOn()) {
      writeLedColor(ledColor.r, ledColor.g, ledColor.b);
    } else {
      writeLedColor(0, 0, 0);
    }
  }

  if (WiFi.status() != lastWiFiStatus) {
    Serial.print("WiFi has changed status: ");
    Serial.print(lastWiFiStatus);
    Serial.print(" -> ");
    Serial.println(WiFi.status());
    lastWiFiStatus = WiFi.status();
  }

  switch (state) {
    case State::ConnectingToWiFi:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected: ");
        Serial.println(WiFi.status());
        transitionToState(State::ReadyToWork);
      } else if (WiFi.status() == WL_WRONG_PASSWORD) {
        Serial.print("Wrong WiFi password for SSID = ");
        Serial.println(storedData.ssid);
        transitionToState(State::ConnectionError);
      }
      break;
    case State::ReadyToWork:
      if (WiFi.getMode() == WIFI_AP) {
        if (time >= (lastActivityTime + DISCONNECT_TIMEOUT)) {
          Serial.println("Disconnected by timeout in AP mode");
          transitionToState(State::OpenAccessPoint);
        }
      } else if (WiFi.getMode() == WIFI_STA) {
        if (WiFi.status() != WL_CONNECTED) {
          Serial.print("WiFi disconnected: ");
          Serial.println(WiFi.status());
          transitionToState(State::ConnectingToWiFi);
        }
      }
      break;
    case State::ReadingSensor:
      if (time >= (lastSensorReadTime + 300)) {
        transitionToState(State::ReadyToWork);
      }
      break;
  }

  if (
    state == State::ConnectingToWiFi ||
    state == State::ConnectionError ||
    state == State::ReadyToWork ||
    state == State::ReadingSensor
  ) {
    if (button.isPressedFor(time, 3000)) {
      Serial.println("Force-restart WiFi in AP mode");
      transitionToState(State::Initial);
      serverStop();
      serverStart(ConnectMode::ForceAP);
    }
  }
}

void transitionToState(State nextState) {
  switch (nextState) {
    case State::Initial:
      ledColor = { 255, 0, 0 };
      blinking.setPattern(500, 500);
    case State::OpenAccessPoint:
      ledColor = { 0, 255, 0 };
      blinking.setPattern(500, 500);
      break;
    case State::ConnectingToWiFi:
      ledColor = { 0, 255, 0 };
      blinking.setPattern(100, 100);
      break;
    case State::ConnectionError:
      ledColor = { 255, 0, 0 };
      blinking.setPattern(1000, 0);
      break;
    case State::ReadyToWork:
      ledColor = { 0, 255, 0 };
      blinking.setPattern(1000, 0);
      break;
    case State::ReadingSensor:
      lastSensorReadTime = millis();
      ledColor = { 0, 0, 255 };
      blinking.setPattern(500, 0);
      break;
  }
  state = nextState;
}

void serverStart(ConnectMode mode) {
  if (mode == ConnectMode::Default && strlen(storedData.ssid) > 0) {
    Serial.print("Connecting to WiFi, SSID = ");
    Serial.println(storedData.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(storedData.ssid, storedData.password);

    webServer.on("/", HTTP_GET, webServer_handleMain);
    webServer.on("/sensor", HTTP_GET, webServer_handleSensor);
    webServer.on("/connect-settings", HTTP_GET, webServer_handleConnectSettings);
    webServer.on("/connect", HTTP_POST, webServer_handleConnect);
    webServer.begin();
  
    transitionToState(State::ConnectingToWiFi);
  } else {
    Serial.println("Starting WiFi AP");

    IPAddress apIP(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, subnet);
    WiFi.softAP("ZF_Water_Meter", "J4242what");
    dnsServer.start(53, "*", apIP);

    webServer.onNotFound(webServer_handleMain);
    webServer.on("/sensor", HTTP_GET, webServer_handleSensor);
    webServer.on("/connect-settings", HTTP_GET, webServer_handleConnectSettings);
    webServer.on("/connect", HTTP_POST, webServer_handleConnect);
    webServer.begin();
  
    transitionToState(State::OpenAccessPoint);
  }

  lastWiFiStatus = WiFi.status();
}

void serverStop() {
  dnsServer.stop();
  webServer.stop();
  WiFi.disconnect();
}

void serverTick() {
  if (state != State::Initial) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
}

void webServer_handleMain() {
  updateActivityTime();
  transitionToState(State::ReadyToWork);
  webServer.send(200, "text/html", MAIN_PAGE_HTML);
}

void webServer_handleSensor() {
  updateActivityTime();
  if (state == State::ReadyToWork) {
    transitionToState(State::ReadingSensor);
  }

  int value = analogRead(A0);

  JsonDocument doc;
  doc["value"] = value;

  std::string response;
  serializeJsonPretty(doc, response);
  webServer.send(200, "application/json", response.c_str());
}

void webServer_handleConnectSettings() {
  JsonDocument doc;
  doc["ssid"] = storedData.ssid;
  doc["password"] = "(hidden)";

  std::string response;
  serializeJsonPretty(doc, response);
  webServer.send(200, "application/json", response.c_str());
}

void webServer_handleConnect() {
  Serial.println("Received request to change connection settings");
  if (!webServer.hasArg("ssid")) {
    webServer.send(400, "text/plain", "Missing 'ssid' argument");
    return;
  } else if (!webServer.hasArg("password")) {
    webServer.send(400, "text/plain", "Missing 'password' argument");
    return;
  }

  std::string ssid = webServer.arg("ssid").c_str();
  std::string password = webServer.arg("password").c_str();

  if (ssid.size() >= MAX_SSID_LENGTH) {
    webServer.send(422, "text/plain", "Too long 'ssid' string");
    return;
  } else if (password.size() >= MAX_PASSWORD_LENGTH) {
    webServer.send(422, "text/plain", "Too long 'password' string");
    return;
  }

  snprintf(&storedData.ssid[0], MAX_SSID_LENGTH, "%s", ssid.c_str());
  snprintf(&storedData.password[0], MAX_PASSWORD_LENGTH, "%s", password.c_str());
  EEPROM.put(0, storedData);
  EEPROM.commit();

  Serial.print("Changed connection settings: SSID = ");
  Serial.print(storedData.ssid);
  Serial.print(", password = ");
  Serial.println(storedData.password);

  Serial.println("Restarting WiFi after settings change");
  transitionToState(State::Initial);
  serverStop();
  serverStart(ConnectMode::Default);
}

void updateActivityTime() {
  lastActivityTime = millis();
}

void writeLedColor(byte r, byte g, byte b) {
  analogWrite(R_PIN, r);
  analogWrite(G_PIN, g);
  analogWrite(B_PIN, b);
}
