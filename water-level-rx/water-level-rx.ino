#include <ArduinoJson.h>
#include <EEPROM.h>

#include <cstdint>
#include <memory>
#include <string>

#include "blink_pattern.h"
#include "button.h"
#include "colors.h"
#include "server.h"

enum class State {
  Initial,
  OpenAccessPoint,
  ConnectingToWiFi,
  ConnectionError,
  ReadyToWork,
  ReadingSensor,
};

State state = State::Initial;

const uint8_t R_PIN = 4; // D2
const uint8_t G_PIN = 5; // D1
const uint8_t B_PIN = 14; // D5
const uint8_t BUTTON_PIN = 12; // D6

Button button(BUTTON_PIN);

BlinkPattern blinking;
RgbColor ledColor = { 0, 0, 0 };

const uint16_t SENSOR_READ_INTERVAL = 1000;
const size_t SENSOR_VALUES_COUNT = 10;
uint32_t lastSensorRead = 0;
size_t nextSensorValueIndex = 0;
int sensorValues[SENSOR_VALUES_COUNT];

const uint16_t ASSUME_INACTIVE_TIMEOUT = 5000;

const uint32_t EEPROM_MAGIC = 0xABCD4321;
const uint32_t EEPROM_VERSION = 2;
const size_t MAX_SSID_LENGTH = 32;
const size_t MAX_PASSWORD_LENGTH = 64;

struct EepromData {
  uint32_t magic;
  uint32_t version;
  char ssid[MAX_SSID_LENGTH];
  char password[MAX_PASSWORD_LENGTH];
  float rawPerDepth;
  float depthOffset;
};

EepromData storedData;

bool isValidEepromData(const EepromData &data) {
  return data.magic == EEPROM_MAGIC && data.version == EEPROM_VERSION;
}

class WlrxServer : public RestServer {
private:
  wl_status_t lastWiFiStatus;
  uint32_t lastActivityTime;
  uint32_t lastSensorRequestTime;

public:
  enum class ConnectMode { Default, ForceAP };

  WlrxServer()
    : lastWiFiStatus(WL_DISCONNECTED),
    lastActivityTime(0),
    lastSensorRequestTime(0)
  {}

  wl_status_t updateWiFiStatus() {
    auto status = wiFiStatus();
    if (status != lastWiFiStatus) {
      Serial.print("WiFi has changed status: ");
      Serial.print(lastWiFiStatus);
      Serial.print(" -> ");
      Serial.println(status);
      lastWiFiStatus = status;
    }
    return status;
  }

  void updateActivityTime() {
    lastActivityTime = millis();
  }

  bool isInactiveFor(uint32_t time, uint32_t span) {
    return time >= (lastActivityTime + span);
  }

  bool hasNotRequestedSensorFor(uint32_t time, uint32_t span) {
    return time >= (lastSensorRequestTime + span);
  }

  void start(ConnectMode mode) {
    if (mode == ConnectMode::Default && strlen(storedData.ssid) > 0) {
      Serial.print("Connecting to WiFi, SSID = ");
      Serial.println(storedData.ssid);
      connectToNetwork(storedData.ssid, storedData.password);
      transitionToState(State::ConnectingToWiFi);
    } else {
      Serial.println("Starting WiFi AP");
      startAccessPoint("ZF_Water_Meter", "J4242what");
      transitionToState(State::OpenAccessPoint);
    }

    updateWiFiStatus();
  }

protected:
  bool inReadonlyMode() {
    return WiFi.getMode() != WIFI_AP;
  }

  void setupHandlers() override {
    webServer.on("/sensor", HTTP_GET, [=]{ this->handleSensor(); });
    webServer.on("/connect-settings", HTTP_GET, [=]{ this->handleConnectSettings(); });
    webServer.on("/connect", HTTP_POST, [=]{ this->handleConnect(); });
    webServer.on("/calibration", HTTP_GET, [=]{ this->handleGetCalibration(); });
    webServer.on("/calibration", HTTP_POST, [=]{ this->handleSetCalibration(); });
  }

  void handleMain() override {
    updateActivityTime();
    transitionToState(State::ReadyToWork);
    RestServer::handleMain();
  }

  void handleSensor() {
    updateActivityTime();
  
    if (state == State::ReadyToWork) {
      transitionToState(State::ReadingSensor);
      lastSensorRequestTime = millis();
    }

    float average = 0;
    for (auto value : sensorValues) {
      average += value;
    }
    average /= SENSOR_VALUES_COUNT;

    float varience = 0;
    for (auto value : sensorValues) {
      float difference = (value - average);
      varience += difference * difference;
    }

    JsonDocument doc;
    doc["value"] = (average / storedData.rawPerDepth) - storedData.depthOffset;
    doc["standardDeviation"] = sqrt(varience) / storedData.rawPerDepth;
    doc["rawValue"] = average;
    doc["unit"] = "m";

    std::string response;
    serializeJsonPretty(doc, response);
    webServer.send(200, "application/json", response.c_str());
  }

  void handleConnectSettings() {
    JsonDocument doc;
    doc["ssid"] = storedData.ssid;
    doc["password"] = "(hidden)";

    std::string response;
    serializeJsonPretty(doc, response);
    webServer.send(200, "application/json", response.c_str());
  }

  void handleConnect() {
    Serial.println("Received request to change connection settings");
    if (!webServer.hasArg("ssid")) {
      webServer.send(400, "text/plain", "Missing 'ssid' argument");
      return;
    } else if (!webServer.hasArg("password")) {
      webServer.send(400, "text/plain", "Missing 'password' argument");
      return;
    } else if (inReadonlyMode()) {
      webServer.send(403, "text/plain", "Connections settings can only be modified in AP mode");
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

    stop();
    start(ConnectMode::Default);
  }

  void handleGetCalibration() {
    JsonDocument doc;
    doc["rawPerDepth"] = storedData.rawPerDepth;
    doc["depthOffset"] = storedData.depthOffset;

    std::string response;
    serializeJsonPretty(doc, response);
    webServer.send(200, "application/json", response.c_str());
  }

  void handleSetCalibration() {
    Serial.println("Received request to change calibration data");
    if (!webServer.hasArg("rawPerDepth")) {
      webServer.send(400, "text/plain", "Missing 'rawPerDepth' argument");
      return;
    } else if (!webServer.hasArg("depthOffset")) {
      webServer.send(400, "text/plain", "Missing 'depthOffset' argument");
      return;
    } else if (inReadonlyMode()) {
      webServer.send(403, "text/plain", "Calibration data can only be modified in AP mode");
      return;
    }

    float rawPerDepth;
    if (!tryParseFloat(webServer.arg("rawPerDepth").c_str(), rawPerDepth)) {
      webServer.send(400, "text/plain", "Invalid 'rawPerDepth' argument");
    }

    float depthOffset;
    if (!tryParseFloat(webServer.arg("depthOffset").c_str(), depthOffset)) {
      webServer.send(400, "text/plain", "Invalid 'depthOffset' argument");
    }

    storedData.rawPerDepth = rawPerDepth;
    storedData.depthOffset = depthOffset;
    EEPROM.put(0, storedData);
    EEPROM.commit();
  }
} server;

bool tryParseFloat(const char *str, float &result) {
  char* parseEnd = nullptr;
  errno = 0;
  result = std::strtof(str, &parseEnd);
  return errno == 0 && parseEnd != str && parseEnd != nullptr && *parseEnd != 0;
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
    storedData.rawPerDepth = 120;
    storedData.depthOffset = 0;
  }

  int initial_sensor_value = analogRead(A0);
  for (size_t i = 0; i < SENSOR_VALUES_COUNT; i++) {
    sensorValues[i] = initial_sensor_value;
  }
  lastSensorRead = millis();

  transitionToState(State::Initial);
  server.start(WlrxServer::ConnectMode::Default);
}

void loop() {
  auto time = millis();

  if (time >= lastSensorRead + SENSOR_READ_INTERVAL) {
    sensorValues[nextSensorValueIndex] = analogRead(A0);
    nextSensorValueIndex++;
    if (nextSensorValueIndex >= SENSOR_VALUES_COUNT) {
      nextSensorValueIndex = 0;
    }
    lastSensorRead = time;
  }
  
  if (state != State::Initial) {
    server.update();
  }

  button.update(time);
  if (button.isPressed()) {
    server.updateActivityTime();
  }

  if (blinking.update(time)) {
    if (blinking.isOn()) {
      writeLedColor(ledColor.r, ledColor.g, ledColor.b);
    } else {
      writeLedColor(0, 0, 0);
    }
  }

  auto wiFiStatus = server.updateWiFiStatus();

  switch (state) {
    case State::ConnectingToWiFi:
      if (wiFiStatus == WL_CONNECTED) {
        Serial.print("WiFi connected: ");
        Serial.println(wiFiStatus);
        transitionToState(State::ReadyToWork);
      } else if (wiFiStatus == WL_WRONG_PASSWORD) {
        Serial.print("Wrong WiFi password for SSID = ");
        Serial.println(storedData.ssid);
        transitionToState(State::ConnectionError);
      } else if (
        wiFiStatus == WL_CONNECT_FAILED ||
        wiFiStatus == WL_CONNECTION_LOST ||
        wiFiStatus == WL_DISCONNECTED
      ) {
        Serial.print("Other connection error: ");
        Serial.println(wiFiStatus);
        transitionToState(State::ConnectionError);
      }
      break;
    case State::ConnectionError:
      if (wiFiStatus == WL_CONNECTED) {
        Serial.print("WiFi re-connected: ");
        Serial.println(wiFiStatus);
        transitionToState(State::ReadyToWork);
      }
      break;
    case State::ReadyToWork:
      if (WiFi.getMode() == WIFI_AP) {
        if (server.isInactiveFor(time, ASSUME_INACTIVE_TIMEOUT)) {
          Serial.println("Assumed inactive by timeout in AP mode");
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
      if (server.hasNotRequestedSensorFor(time, 300)) {
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
      server.stop();
      server.start(WlrxServer::ConnectMode::ForceAP);
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
      ledColor = { 0, 0, 255 };
      blinking.setPattern(500, 0);
      break;
  }
  state = nextState;
}

void writeLedColor(byte r, byte g, byte b) {
  analogWrite(R_PIN, r);
  analogWrite(G_PIN, g);
  analogWrite(B_PIN, b);
}
