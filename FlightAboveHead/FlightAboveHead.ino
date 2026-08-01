#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Fonts/TomThumb.h>
#include <time.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "api_config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("../secrets.h")
#include "../secrets.h"
#else
constexpr const char *WIFI_SSID = "";
constexpr const char *WIFI_PASSWORD = "";
#endif

// =========================
// HW-678 / ESP32-S3-N16R8 hardware pinout
// =========================
// Safe HUB75E mapping for the photographed board. It deliberately avoids:
// - GPIO0/3/45/46: boot-strapping pins
// - GPIO19/20: native USB D-/D+
// - GPIO33..37: connected to the N16R8 module's Octal PSRAM
// - GPIO43/44: UART0 programming/debug output
// - GPIO48: onboard addressable RGB LED on this board family
//
// Keep this block synchronized with docs/hardware-pinout.md.
constexpr int8_t HUB75_R1_PIN = 4;
constexpr int8_t HUB75_G1_PIN = 5;
constexpr int8_t HUB75_B1_PIN = 6;
constexpr int8_t HUB75_R2_PIN = 7;
constexpr int8_t HUB75_G2_PIN = 8;
constexpr int8_t HUB75_B2_PIN = 9;
constexpr int8_t HUB75_A_PIN = 10;
constexpr int8_t HUB75_B_PIN = 11;
constexpr int8_t HUB75_C_PIN = 12;
constexpr int8_t HUB75_D_PIN = 13;
constexpr int8_t HUB75_E_PIN = 14;
constexpr int8_t HUB75_LAT_PIN = 15;
constexpr int8_t HUB75_OE_PIN = 16;
constexpr int8_t HUB75_CLK_PIN = 17;

// Low-speed peripherals.
constexpr int8_t BH1750_SDA_PIN = 1;
constexpr int8_t BH1750_SCL_PIN = 2;
constexpr int8_t TOUCH_BUTTON_PIN = 18;

// =========================
// Configuration
// =========================
constexpr uint16_t PANEL_RES_X = 128;
constexpr uint16_t PANEL_RES_Y = 64;
constexpr uint16_t PANEL_CHAIN = 1;
constexpr uint32_t PANEL_POWER_SETTLE_MS = 1500;
constexpr uint32_t DISPLAY_INIT_RETRY_MS = 2000;

constexpr const char *FLIGHTS_URL = FLIGHTS_API_URL;
constexpr const char *HISTORY_URL = HISTORY_API_URL;
constexpr const char *ALERTS_URL = ALERTS_API_URL;
constexpr const char *NTP_SERVER_1 = "pool.ntp.org";
constexpr const char *NTP_SERVER_2 = "time.google.com";
constexpr const char *ISRAEL_TZ = "IST-2IDT,M3.4.4/26,M10.5.0";

constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t ALERT_POLL_MS = 3000;
constexpr uint32_t FLIGHT_POLL_MS = 5000;
constexpr uint32_t HISTORY_REFRESH_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t HISTORY_RETRY_MS = 30UL * 1000UL;
constexpr uint32_t WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t WEATHER_RETRY_MS = 60UL * 1000UL;
constexpr uint32_t SOLAR_REFRESH_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t SOLAR_RETRY_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t NETWORK_STATUS_MS = 1000;
constexpr uint32_t DIAGNOSTICS_MS = 5000;
constexpr uint32_t FLIGHT_TTL_MS = 45000;
constexpr uint32_t ALERT_TTL_MS = 15000;
constexpr uint16_t HTTP_TIMEOUT_MS = 2500;
constexpr uint16_t HISTORY_HTTP_TIMEOUT_MS = 10000;
constexpr uint16_t EXTERNAL_HTTP_TIMEOUT_MS = 9000;
constexpr uint8_t STATUS_FAILURE_THRESHOLD = 3;
constexpr uint16_t FALLBACK_SUNRISE_MIN = 6 * 60;
constexpr uint16_t FALLBACK_SUNSET_MIN = 19 * 60;
constexpr float SLEEP_ENTER_LUX = 1.0f;
constexpr float SLEEP_EXIT_LUX = 3.0f;
constexpr uint32_t SLEEP_ENTER_MS = 60UL * 1000UL;
constexpr uint32_t SLEEP_EXIT_MS = 30UL * 1000UL;

constexpr uint8_t BH1750_ADDRESS_LOW = 0x23;
constexpr uint8_t BH1750_ADDRESS_HIGH = 0x5C;
constexpr uint32_t BH1750_READ_MS = 1000;
constexpr uint32_t BH1750_RECOVER_MS = 5000;
constexpr uint32_t I2C_CLOCK_HZ = 100000;

// TTP223-style digital touch module: OUT is normally LOW and goes HIGH while
// touched. INPUT_PULLDOWN also keeps GPIO18 quiet when the module is unplugged.
constexpr bool TOUCH_BUTTON_ACTIVE_HIGH = true;
constexpr uint32_t BUTTON_POLL_MS = 1;
constexpr uint32_t BUTTON_PRESS_DEBOUNCE_MS = 2;
constexpr uint32_t BUTTON_RELEASE_DEBOUNCE_MS = 5;
constexpr uint32_t BUTTON_FAST_TAP_MIN_MS = 1;
// A deliberate double tap is compact. A wider window made two quick saver
// changes look like the Last Aircraft gesture.
constexpr uint32_t BUTTON_DOUBLE_TAP_MS = 280;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 800;
constexpr uint32_t HARDWARE_TEST_SCREEN_MS = 8000;
constexpr uint32_t IDLE_BREATH_FRAME_MS = 40;
constexpr uint32_t SCREENSAVER_FRAME_MS = 25;
constexpr uint8_t SCREENSAVER_COUNT = 12;
constexpr uint32_t BOOT_INTRO_DURATION_MS = 2400;
constexpr uint32_t AIRCRAFT_TRANSITION_DURATION_MS = 820;
constexpr uint32_t AIRCRAFT_TRANSITION_FLYBY_MS = 320;

// Fallback brightness when the sensor is absent.
constexpr uint8_t IDLE_BRIGHTNESS = 24;
constexpr uint8_t BUTTON_MANUAL_BRIGHTNESS = 200;
constexpr uint8_t AIRCRAFT_BRIGHTNESS = 140;
constexpr uint8_t ALERT_BRIGHTNESS = 255;
// Automatic ranges. Alerts remain prominent even in a dark room.
constexpr uint8_t IDLE_BRIGHTNESS_MIN = 1;
constexpr uint8_t IDLE_BRIGHTNESS_MAX = 52;
constexpr uint8_t AIRCRAFT_BRIGHTNESS_MIN = 3;
constexpr uint8_t AIRCRAFT_BRIGHTNESS_MAX = 160;
constexpr uint8_t ALERT_BRIGHTNESS_MIN = 150;
constexpr uint8_t ALERT_BRIGHTNESS_MAX = 255;
// One global DMA brightness profile is shared by every screensaver effect.
constexpr uint8_t SCREENSAVER_BRIGHTNESS = 140;
constexpr uint8_t SCREENSAVER_BRIGHTNESS_MIN = 3;
constexpr uint8_t SCREENSAVER_BRIGHTNESS_MAX = 160;
constexpr uint8_t NIGHT_IDLE_MIN = 1;
constexpr uint8_t NIGHT_IDLE_MAX = 20;
constexpr uint8_t NIGHT_SCREENSAVER_MIN = 1;
constexpr uint8_t NIGHT_SCREENSAVER_MAX = 24;
constexpr uint8_t NIGHT_AIRCRAFT_MIN = 1;
constexpr uint8_t NIGHT_AIRCRAFT_MAX = 24;
constexpr uint8_t SLEEP_IDLE_BRIGHTNESS = 1;
constexpr uint8_t SLEEP_SCREENSAVER_BRIGHTNESS = 1;
constexpr uint8_t SLEEP_AIRCRAFT_BRIGHTNESS = 1;
constexpr float AUTO_BRIGHTNESS_LUX_MAX = 350.0f;
constexpr float AUTO_BRIGHTNESS_CURVE = 1.6f;
constexpr uint8_t PANEL_COLOR_DEPTH_BITS = 6;
constexpr uint16_t PANEL_MIN_REFRESH_HZ = 120;
enum class PanelColorOrder : uint8_t {
  RGB,
  BGR,
  BRG,
  GRB,
  GBR,
  RBG
};
// Measured mapping on this panel:
// driver R -> visible B, driver B -> visible G, driver G -> visible R.
constexpr PanelColorOrder PANEL_COLOR_ORDER = PanelColorOrder::BRG;

constexpr uint32_t NETWORK_TASK_STACK = 16 * 1024;
constexpr uint32_t RENDER_TASK_STACK = 8 * 1024;
constexpr UBaseType_t NETWORK_TASK_PRIORITY = 1;
constexpr UBaseType_t RENDER_TASK_PRIORITY = 2;
constexpr BaseType_t NETWORK_TASK_CORE = 0;
constexpr BaseType_t RENDER_TASK_CORE = 1;

// =========================
// Data model
// =========================
enum class AppState : uint8_t {
  IDLE,
  LAST_AIRCRAFT,
  AIRCRAFT,
  HARDWARE_TEST,
  SCREENSAVER,
  ALERT
};

enum class VisualMode : uint8_t {
  DAY,
  NIGHT,
  SLEEP
};

enum class NightOverride : uint8_t {
  AUTO,
  FORCE_DAY,
  FORCE_NIGHT
};

enum class ScreenOverride : uint8_t {
  AUTO,
  IDLE,
  LAST_AIRCRAFT,
  AIRCRAFT,
  HARDWARE_TEST,
  SCREENSAVER,
  ALERT
};

struct FlightData {
  bool active = false;
  bool fresh = false;
  uint8_t count = 0;
  char id[16] = "";
  char callsign[16] = "";
  char airlineIcao[8] = "";
  char aircraft[12] = "";
  char registration[16] = "";
  char origin[8] = "";
  char destination[8] = "";
  int altitudeFt = -1;
  int speedKts = -1;
  int headingDeg = -1;
  int verticalSpeed = 0;
  char updatedAt[40] = "";
  uint32_t receivedMs = 0;
};

struct AlertData {
  bool active = false;
  bool fresh = false;
  char title[48] = "";
  char category[12] = "";
  char firstArea[64] = "";
  uint8_t areaCount = 0;
  uint32_t receivedMs = 0;
};

struct ApiStatus {
  bool wifiOk = false;
  bool timeOk = false;
  bool flightOk = false;
  bool alertOk = false;
  bool historyOk = false;
  bool weatherOk = false;
  uint8_t wifiFailures = 0;
  uint8_t timeFailures = 0;
  uint8_t flightFailures = 0;
  uint8_t alertFailures = 0;
  uint8_t historyFailures = 0;
  uint8_t weatherFailures = 0;
  int32_t wifiRssi = -127;
  char ipAddress[16] = "0.0.0.0";
  char lastError[48] = "";
};

struct WeatherData {
  bool valid = false;
  bool lastFetchOk = false;
  float temperatureC = NAN;
  float feelsLikeC = NAN;
  char observedAt[24] = "";
  uint32_t receivedMs = 0;
};

struct SolarStatus {
  bool valid = false;
  bool lastFetchOk = false;
  uint16_t sunriseMin = FALLBACK_SUNRISE_MIN;
  uint16_t sunsetMin = FALLBACK_SUNSET_MIN;
  char date[11] = "";
  uint32_t receivedMs = 0;
};

struct PeripheralStatus {
  bool lightOk = false;
  bool autoBrightness = true;
  // Keep HUB75 electrically blank after power-up. The first button press (or
  // an explicit `panel on` command) arms deferred DMA initialization.
  bool panelEnabled = false;
  bool manualBrightnessEnabled = false;
  uint8_t manualBrightness = IDLE_BRIGHTNESS;
  float ambientLux = -1.0f;
  uint8_t bh1750Address = 0;
  uint32_t buttonPresses = 0;
  uint32_t hardwareTestUntilMs = 0;
  bool screensaverActive = false;
  uint8_t screensaverIndex = 0;
  bool lastAircraftView = false;
  VisualMode visualMode = VisualMode::DAY;
  NightOverride nightOverride = NightOverride::AUTO;
  ScreenOverride screenOverride = ScreenOverride::AUTO;
};

struct RuntimeMetrics {
  uint32_t alertRequests = 0;
  uint32_t flightRequests = 0;
  uint32_t requestFailures = 0;
  uint32_t solarRequests = 0;
  uint32_t solarFailures = 0;
  uint32_t historyRequests = 0;
  uint32_t historyFailures = 0;
  uint32_t weatherRequests = 0;
  uint32_t weatherFailures = 0;
  uint32_t lastAlertDurationMs = 0;
  uint32_t lastFlightDurationMs = 0;
  uint32_t lastSolarDurationMs = 0;
  uint32_t lastHistoryDurationMs = 0;
  uint32_t lastWeatherDurationMs = 0;
  int lastAlertHttpCode = 0;
  int lastFlightHttpCode = 0;
  int lastSolarHttpCode = 0;
  int lastHistoryHttpCode = 0;
  int lastWeatherHttpCode = 0;
  uint32_t renderedFrames = 0;
  uint32_t lastRenderDurationUs = 0;
  uint32_t lastRenderSignature = 0;
  uint8_t appliedBrightness = IDLE_BRIGHTNESS;
};

struct SharedState {
  FlightData flight;
  FlightData lastAircraft;
  AlertData alert;
  ApiStatus status;
  SolarStatus solar;
  WeatherData weather;
  PeripheralStatus peripheral;
  RuntimeMetrics metrics;
};

SharedState sharedState;
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t networkTaskHandle = nullptr;
TaskHandle_t renderTaskHandle = nullptr;

MatrixPanel_I2S_DMA *dmaDisplay = nullptr;
volatile bool displayReady = false;
VisualMode activeVisualMode = VisualMode::DAY;

uint8_t bh1750Address = BH1750_ADDRESS_LOW;
bool bh1750Configured = false;
uint32_t lastLightReadMs = 0;
uint32_t lastLightRecoverMs = 0;

bool buttonRawState = false;
bool buttonStableState = false;
bool buttonLastRawState = false;
bool buttonLongPressHandled = false;
uint32_t buttonLastChangeMs = 0;
uint32_t buttonPressStartedMs = 0;
uint8_t buttonTapCount = 0;
uint32_t buttonLastReleaseMs = 0;
bool buttonSecondTapCandidate = false;
bool buttonPulseLatched = false;
uint32_t buttonPulseStartedMs = 0;
bool buttonOptimisticStateValid = false;
bool buttonWakeConsumesPress = false;
bool buttonBeforeScreensaverActive = false;
bool buttonBeforeLastAircraftView = false;
uint8_t buttonBeforeScreensaverIndex = 0;
Preferences uiPreferences;
bool uiPreferencesReady = false;
uint8_t persistedUiMode = 0xFF;
uint8_t persistedScreensaverIndex = 0xFF;
uint32_t bootIntroStartedMs = 0;

uint16_t C_BLACK;
uint16_t C_WHITE;
uint16_t C_DIM;
uint16_t C_GREEN;
uint16_t C_BLUE;
uint16_t C_CYAN;
uint16_t C_YELLOW;
uint16_t C_RED;

// =========================
// Shared-state helpers
// =========================
void copyText(char *dest, size_t destSize, const char *src) {
  if (destSize == 0) return;
  if (src == nullptr) src = "";
  strlcpy(dest, src, destSize);
}

// The built-in GFX font contains printable ASCII only. API placeholders such
// as the UTF-8 em dash otherwise become three unrelated glyphs on the panel.
void copyPanelText(char *dest, size_t destSize, const char *src) {
  if (destSize == 0) return;
  if (src == nullptr) src = "";

  size_t output = 0;
  bool previousSpace = true;
  for (size_t input = 0; src[input] != '\0' && output + 1 < destSize;) {
    const uint8_t value = static_cast<uint8_t>(src[input]);
    if (value < 0x80U) {
      char next = static_cast<char>(value);
      ++input;
      if (next == '\t') next = ' ';
      if (next < 0x20 || next > 0x7E) continue;
      if (next == ' ') {
        if (previousSpace) continue;
        previousSpace = true;
      } else {
        previousSpace = false;
      }
      dest[output++] = next;
      continue;
    }

    // Preserve common punctuation semantically; skip unsupported code points.
    char replacement = '\0';
    if (src[input + 1] != '\0' &&
        src[input + 2] != '\0' &&
        value == 0xE2U &&
        static_cast<uint8_t>(src[input + 1]) == 0x80U &&
        (static_cast<uint8_t>(src[input + 2]) == 0x93U ||
         static_cast<uint8_t>(src[input + 2]) == 0x94U)) {
      replacement = '-';
    } else if (
        src[input + 1] != '\0' &&
        src[input + 2] != '\0' &&
        value == 0xE2U &&
        static_cast<uint8_t>(src[input + 1]) == 0x86U &&
        static_cast<uint8_t>(src[input + 2]) == 0x92U) {
      replacement = '>';
    }

    ++input;
    while ((static_cast<uint8_t>(src[input]) & 0xC0U) == 0x80U) {
      ++input;
    }
    if (replacement != '\0') {
      dest[output++] = replacement;
      previousSpace = false;
    }
  }

  while (output > 0 && dest[output - 1] == ' ') --output;
  dest[output] = '\0';
}

void lockState() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
}

void unlockState() {
  xSemaphoreGive(stateMutex);
}

SharedState copyState() {
  SharedState snapshot;
  lockState();
  snapshot = sharedState;
  unlockState();
  return snapshot;
}

void setError(const char *message) {
  lockState();
  copyText(sharedState.status.lastError, sizeof(sharedState.status.lastError), message);
  unlockState();
  Serial.printf("[network] %s\n", message);
}

void clearErrorIfHealthy() {
  lockState();
  if (sharedState.status.wifiOk &&
      sharedState.status.alertOk &&
      sharedState.status.flightOk) {
    sharedState.status.lastError[0] = '\0';
  }
  unlockState();
}

void recordHealthSample(bool ok, bool &publishedOk, uint8_t &failures) {
  publishedOk = ok;
  if (ok) {
    failures = 0;
  } else if (failures < UINT8_MAX) {
    ++failures;
  }
}

void setApiStatus(bool isAlert, bool ok) {
  lockState();
  if (isAlert) {
    recordHealthSample(
        ok,
        sharedState.status.alertOk,
        sharedState.status.alertFailures);
  } else {
    recordHealthSample(
        ok,
        sharedState.status.flightOk,
        sharedState.status.flightFailures);
  }
  unlockState();
}

void setAuxStatus(bool isWeather, bool ok) {
  lockState();
  if (isWeather) {
    recordHealthSample(
        ok,
        sharedState.status.weatherOk,
        sharedState.status.weatherFailures);
  } else {
    recordHealthSample(
        ok,
        sharedState.status.historyOk,
        sharedState.status.historyFailures);
  }
  unlockState();
}

void recordRequest(bool isAlert, bool success, uint32_t durationMs, int httpCode) {
  lockState();
  if (isAlert) {
    ++sharedState.metrics.alertRequests;
    sharedState.metrics.lastAlertDurationMs = durationMs;
    sharedState.metrics.lastAlertHttpCode = httpCode;
  } else {
    ++sharedState.metrics.flightRequests;
    sharedState.metrics.lastFlightDurationMs = durationMs;
    sharedState.metrics.lastFlightHttpCode = httpCode;
  }
  if (!success) {
    ++sharedState.metrics.requestFailures;
  }
  unlockState();
}

// =========================
// BH1750 and touch button
// =========================
bool i2cProbe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool bh1750Command(uint8_t command) {
  Wire.beginTransmission(bh1750Address);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

bool configureBH1750() {
  const uint8_t candidates[] = {
      BH1750_ADDRESS_LOW,
      BH1750_ADDRESS_HIGH,
  };
  bool found = false;
  for (const uint8_t candidate : candidates) {
    if (i2cProbe(candidate)) {
      bh1750Address = candidate;
      found = true;
      break;
    }
  }
  if (!found) return false;

  // Power on, reset data register, continuous high-resolution (1 lx) mode.
  bool ok = bh1750Command(0x01);
  delay(10);
  ok = bh1750Command(0x07) && ok;
  delay(10);
  ok = bh1750Command(0x10) && ok;
  delay(180);
  return ok;
}

void publishLightStatus(bool ok, float lux) {
  lockState();
  sharedState.peripheral.lightOk = ok;
  sharedState.peripheral.bh1750Address = ok ? bh1750Address : 0;
  if (isfinite(lux) && lux >= 0.0f) {
    sharedState.peripheral.ambientLux = lux;
  }
  unlockState();
}

void setupLightSensor() {
  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  bh1750Configured = configureBH1750();
  publishLightStatus(bh1750Configured, -1.0f);

  Serial.printf(
      "[sensor] BH1750=%s address=0x%02X SDA=%d SCL=%d clock=%luHz\n",
      bh1750Configured ? "ok" : "not-found",
      bh1750Address,
      BH1750_SDA_PIN,
      BH1750_SCL_PIN,
      static_cast<unsigned long>(I2C_CLOCK_HZ));
}

void updateLightSensor(uint32_t nowMs) {
  if (!bh1750Configured) {
    if (lastLightRecoverMs == 0 ||
        nowMs - lastLightRecoverMs >= BH1750_RECOVER_MS) {
      lastLightRecoverMs = nowMs;
      bh1750Configured = configureBH1750();
      publishLightStatus(bh1750Configured, -1.0f);
      Serial.printf(
          "[sensor] BH1750 recovery=%s address=0x%02X\n",
          bh1750Configured ? "ok" : "not-found",
          bh1750Address);
    }
    return;
  }

  if (lastLightReadMs != 0 && nowMs - lastLightReadMs < BH1750_READ_MS) {
    return;
  }
  lastLightReadMs = nowMs;

  const uint8_t received = Wire.requestFrom(bh1750Address, static_cast<uint8_t>(2));
  if (received != 2 || Wire.available() < 2) {
    bh1750Configured = false;
    publishLightStatus(false, -1.0f);
    Serial.println("[sensor] BH1750 read failed");
    return;
  }

  const uint16_t raw =
      (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  const float measuredLux = raw / 1.2f;

  lockState();
  const float previousLux = sharedState.peripheral.ambientLux;
  sharedState.peripheral.ambientLux =
      isfinite(previousLux) && previousLux >= 0.0f
          ? previousLux * 0.8f + measuredLux * 0.2f
          : measuredLux;
  sharedState.peripheral.lightOk = true;
  sharedState.peripheral.bh1750Address = bh1750Address;
  unlockState();
}

bool readTouchButton() {
  const bool high = digitalRead(TOUCH_BUTTON_PIN) == HIGH;
  return TOUCH_BUTTON_ACTIVE_HIGH ? high : !high;
}

void showHardwareTest(uint32_t nowMs) {
  lockState();
  sharedState.peripheral.hardwareTestUntilMs =
      nowMs + HARDWARE_TEST_SCREEN_MS;
  unlockState();
}

void persistUiSelection(uint8_t mode, uint8_t screensaverIndex) {
  if (!uiPreferencesReady) return;
  if (persistedUiMode != mode) {
    uiPreferences.putUChar("mode", mode);
    persistedUiMode = mode;
  }
  if (persistedScreensaverIndex != screensaverIndex) {
    uiPreferences.putUChar("saver", screensaverIndex);
    persistedScreensaverIndex = screensaverIndex;
  }
}

void restoreUiSelection() {
  uiPreferencesReady = uiPreferences.begin("flight-ui", false);
  if (!uiPreferencesReady) {
    Serial.println("[boot] UI preferences unavailable; starting on IDLE");
    return;
  }
  persistedUiMode = uiPreferences.getUChar("mode", 0);
  persistedScreensaverIndex =
      uiPreferences.getUChar("saver", 0) % SCREENSAVER_COUNT;
  lockState();
  sharedState.peripheral.screensaverIndex =
      persistedScreensaverIndex;
  sharedState.peripheral.screensaverActive =
      persistedUiMode == 1;
  sharedState.peripheral.lastAircraftView =
      persistedUiMode == 2;
  unlockState();
  Serial.printf(
      "[boot] restored UI mode=%u saver=%u/%u\n",
      static_cast<unsigned>(persistedUiMode),
      static_cast<unsigned>(persistedScreensaverIndex + 1),
      static_cast<unsigned>(SCREENSAVER_COUNT));
}

void toggleLastAircraftView() {
  lockState();
  sharedState.peripheral.lastAircraftView =
      !sharedState.peripheral.lastAircraftView;
  sharedState.peripheral.screensaverActive = false;
  sharedState.peripheral.hardwareTestUntilMs = 0;
  const bool showingLast = sharedState.peripheral.lastAircraftView;
  const uint8_t index = sharedState.peripheral.screensaverIndex;
  unlockState();
  persistUiSelection(showingLast ? 2 : 0, index);
  Serial.printf(
      "[button] double tap: last-aircraft=%s\n",
      showingLast ? "on" : "off");
}

void handleSingleTap(uint32_t nowMs) {
  (void)nowMs;
  lockState();
  if (sharedState.peripheral.screensaverActive) {
    sharedState.peripheral.screensaverIndex =
        (sharedState.peripheral.screensaverIndex + 1) % SCREENSAVER_COUNT;
    const uint8_t index = sharedState.peripheral.screensaverIndex;
    unlockState();
    persistUiSelection(1, index);
    Serial.printf(
        "[button] screensaver pattern=%u/%u\n",
        static_cast<unsigned>(index + 1),
        static_cast<unsigned>(SCREENSAVER_COUNT));
    return;
  }
  sharedState.peripheral.screensaverActive = true;
  sharedState.peripheral.lastAircraftView = false;
  sharedState.peripheral.hardwareTestUntilMs = 0;
  const uint8_t index = sharedState.peripheral.screensaverIndex;
  unlockState();
  persistUiSelection(1, index);
  Serial.printf(
      "[button] short press: screensaver=on pattern=%u/%u\n",
      static_cast<unsigned>(index + 1),
      static_cast<unsigned>(SCREENSAVER_COUNT));
}

void undoOptimisticSingleTap() {
  if (!buttonOptimisticStateValid) return;
  lockState();
  sharedState.peripheral.screensaverActive =
      buttonBeforeScreensaverActive;
  sharedState.peripheral.lastAircraftView =
      buttonBeforeLastAircraftView;
  sharedState.peripheral.screensaverIndex =
      buttonBeforeScreensaverIndex;
  unlockState();
  buttonOptimisticStateValid = false;
}

bool wakePanelFromButton() {
  lockState();
  const bool wasOff = !sharedState.peripheral.panelEnabled;
  if (wasOff) {
    sharedState.peripheral.panelEnabled = true;
  }
  unlockState();

  if (!wasOff) return false;

  // Waking the panel is a complete action of its own. Do not interpret the
  // same physical touch as a page change, double tap, or brightness toggle.
  buttonTapCount = 0;
  buttonSecondTapCandidate = false;
  buttonOptimisticStateValid = false;
  Serial.println("[button] panel wake requested");
  return true;
}

void registerShortTap(uint32_t nowMs) {
  if (buttonTapCount == 1 && buttonSecondTapCandidate) {
    // The first tap is applied immediately. A confirmed second tap rolls it
    // back to the exact prior page before opening the retained aircraft.
    undoOptimisticSingleTap();
    buttonTapCount = 0;
    buttonSecondTapCandidate = false;
    Serial.println("[button] tap 2");
    toggleLastAircraftView();
  } else {
    buttonTapCount = 1;
    buttonSecondTapCandidate = false;
    buttonLastReleaseMs = nowMs;
    lockState();
    buttonBeforeScreensaverActive =
        sharedState.peripheral.screensaverActive;
    buttonBeforeLastAircraftView =
        sharedState.peripheral.lastAircraftView;
    buttonBeforeScreensaverIndex =
        sharedState.peripheral.screensaverIndex;
    unlockState();
    buttonOptimisticStateValid = true;
    Serial.println("[button] tap 1");
    handleSingleTap(nowMs);
  }
}

void handleTouchButton(uint32_t nowMs) {
  buttonRawState = readTouchButton();
  if (buttonRawState != buttonLastRawState) {
    if (buttonRawState) {
      buttonPulseLatched = true;
      buttonPulseStartedMs = nowMs;
    } else if (
        buttonPulseLatched &&
        !buttonStableState &&
        nowMs - buttonPulseStartedMs >= BUTTON_FAST_TAP_MIN_MS) {
      // A fast touch can end before the normal stable-press transition. Latch
      // that pulse explicitly instead of losing a genuine fingertip tap.
      buttonPulseLatched = false;
      buttonSecondTapCandidate =
          buttonTapCount == 1 &&
          buttonPulseStartedMs - buttonLastReleaseMs <=
              BUTTON_DOUBLE_TAP_MS;
      lockState();
      ++sharedState.peripheral.buttonPresses;
      unlockState();
      Serial.println("[button] fast tap");
      if (!wakePanelFromButton()) {
        registerShortTap(nowMs);
      }
    }
    buttonLastRawState = buttonRawState;
    buttonLastChangeMs = nowMs;
  }

  const uint32_t debounceMs =
      buttonRawState
          ? BUTTON_PRESS_DEBOUNCE_MS
          : BUTTON_RELEASE_DEBOUNCE_MS;
  if (nowMs - buttonLastChangeMs >= debounceMs &&
      buttonRawState != buttonStableState) {
    buttonStableState = buttonRawState;
    if (buttonStableState) {
      buttonPulseLatched = false;
      buttonSecondTapCandidate =
          buttonTapCount == 1 &&
          nowMs - buttonLastReleaseMs <= BUTTON_DOUBLE_TAP_MS;
      buttonPressStartedMs = nowMs;
      buttonLongPressHandled = false;
      lockState();
      ++sharedState.peripheral.buttonPresses;
      unlockState();
      buttonWakeConsumesPress = wakePanelFromButton();
      if (!buttonWakeConsumesPress) {
        Serial.println("[button] down");
      }
    } else {
      if (buttonWakeConsumesPress) {
        buttonWakeConsumesPress = false;
        buttonLongPressHandled = false;
        Serial.println("[button] panel wake complete");
      } else if (buttonLongPressHandled) {
        Serial.println("[button] up after long press");
      } else {
        registerShortTap(nowMs);
      }
    }
  }

  if (buttonStableState && !buttonWakeConsumesPress &&
      !buttonLongPressHandled &&
      nowMs - buttonPressStartedMs >= BUTTON_LONG_PRESS_MS) {
    buttonLongPressHandled = true;
    buttonTapCount = 0;
    buttonSecondTapCandidate = false;
    buttonOptimisticStateValid = false;
    lockState();
    const bool enableAuto = !sharedState.peripheral.autoBrightness;
    sharedState.peripheral.autoBrightness = enableAuto;
    sharedState.peripheral.manualBrightnessEnabled = !enableAuto;
    if (!enableAuto) {
      sharedState.peripheral.manualBrightness = BUTTON_MANUAL_BRIGHTNESS;
    }
    unlockState();
    showHardwareTest(nowMs);
    if (enableAuto) {
      Serial.println("[button] long press: brightness=auto");
    } else {
      Serial.printf(
          "[button] long press: brightness=manual %u/255\n",
          BUTTON_MANUAL_BRIGHTNESS);
    }
  }

  if (!buttonRawState && !buttonStableState && buttonTapCount == 1 &&
      nowMs - buttonLastReleaseMs >= BUTTON_DOUBLE_TAP_MS) {
    buttonTapCount = 0;
    buttonSecondTapCandidate = false;
    buttonOptimisticStateValid = false;
  }
}

// =========================
// Display
// =========================
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  if (activeVisualMode != VisualMode::DAY) {
    uint16_t warmR = r + g * 35U / 100U + b * 20U / 100U;
    if (warmR > 255U) warmR = 255U;
    const uint16_t warmG = g * 65U / 100U + b * 12U / 100U;
    const uint16_t warmB = b * 18U / 100U;
    r = static_cast<uint8_t>(warmR);
    g = static_cast<uint8_t>(warmG);
    b = static_cast<uint8_t>(warmB);
  }
  switch (PANEL_COLOR_ORDER) {
    case PanelColorOrder::BRG:
      // Feed desired B,R,G into the driver's R,G,B inputs.
      return dmaDisplay->color565(b, r, g);
    case PanelColorOrder::RGB:
    default:
      return dmaDisplay->color565(r, g, b);
  }
}

bool hasPanelText(const char *text) {
  if (text == nullptr) return false;
  for (const char *cursor = text; *cursor != '\0'; ++cursor) {
    if ((*cursor >= '0' && *cursor <= '9') ||
        (*cursor >= 'A' && *cursor <= 'Z') ||
        (*cursor >= 'a' && *cursor <= 'z')) {
      return true;
    }
  }
  return false;
}

const char *safeText(const char *text, const char *fallback) {
  return hasPanelText(text) ? text : fallback;
}

void initColors() {
  C_BLACK = color565(0, 0, 0);
  C_WHITE = color565(255, 255, 255);
  C_DIM = color565(42, 42, 42);
  C_GREEN = color565(0, 255, 90);
  C_BLUE = color565(0, 85, 255);
  C_CYAN = color565(0, 220, 255);
  C_YELLOW = color565(255, 210, 0);
  C_RED = color565(255, 0, 0);
}

void printFixed(int16_t x, int16_t y, const char *text, uint16_t color) {
  dmaDisplay->setCursor(x, y);
  dmaDisplay->setTextColor(color);
  dmaDisplay->print(text);
}

void printFixed(int16_t x, int16_t y, int value, uint16_t color) {
  dmaDisplay->setCursor(x, y);
  dmaDisplay->setTextColor(color);
  dmaDisplay->print(value);
}

void printCentered(int16_t y, const char *text, uint16_t color) {
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  dmaDisplay->getTextBounds(text, 0, y, &boundsX, &boundsY, &width, &height);
  const int16_t x =
      width < PANEL_RES_X ? (PANEL_RES_X - static_cast<int16_t>(width)) / 2 : 0;
  printFixed(x, y, text, color);
}

void printCenteredClipped(int16_t y, const char *text, uint16_t color) {
  constexpr size_t MAX_VISIBLE_CHARS = 20;
  char clipped[MAX_VISIBLE_CHARS + 1];
  copyText(clipped, sizeof(clipped), text);
  if (strlen(text) > MAX_VISIBLE_CHARS) {
    clipped[MAX_VISIBLE_CHARS - 3] = '.';
    clipped[MAX_VISIBLE_CHARS - 2] = '.';
    clipped[MAX_VISIBLE_CHARS - 1] = '.';
    clipped[MAX_VISIBLE_CHARS] = '\0';
  }
  printCentered(y, clipped, color);
}

void drawBoldClock(const char *timeText, int16_t y, uint32_t nowMs) {
  char hours[3] = {timeText[0], timeText[1], '\0'};
  char minutes[3] = {timeText[3], timeText[4], '\0'};

  // Default GFX font at size 2: HH:MM is 60 px wide. The extra pixel is the
  // subtle second pass that gives the digits more visual weight.
  constexpr int16_t clockWidth = 61;
  const int16_t x = (PANEL_RES_X - clockWidth) / 2;

  const float phase =
      (nowMs % 1000UL) * (2.0f * PI / 1000.0f);
  const float breath = 0.5f - 0.5f * cosf(phase);
  const uint8_t colonLevel =
      activeVisualMode == VisualMode::DAY
          ? static_cast<uint8_t>(roundf(42.0f + breath * 58.0f))
          : 70U;
  const uint16_t colonColor =
      activeVisualMode == VisualMode::DAY
          ? color565(colonLevel, colonLevel, colonLevel)
          : color565(70, 28, 0);

  dmaDisplay->setTextSize(2);
  printFixed(x, y, hours, C_WHITE);
  printFixed(x + 1, y, hours, C_WHITE);
  printFixed(x + 24, y, ":", colonColor);
  printFixed(x + 36, y, minutes, C_WHITE);
  printFixed(x + 37, y, minutes, C_WHITE);
  dmaDisplay->setTextSize(1);
}

void holdPanelBlankDuringPowerUp() {
  // OE is active LOW. Holding it HIGH while both power rails settle prevents
  // the panel from sampling random clock/latch transitions during a cold boot.
  digitalWrite(HUB75_OE_PIN, HIGH);
  pinMode(HUB75_OE_PIN, OUTPUT);
  digitalWrite(HUB75_LAT_PIN, LOW);
  pinMode(HUB75_LAT_PIN, OUTPUT);
  digitalWrite(HUB75_CLK_PIN, LOW);
  pinMode(HUB75_CLK_PIN, OUTPUT);
}

bool setupDisplay() {
  if (displayReady && dmaDisplay != nullptr) return true;

  if (!psramFound()) {
    Serial.println("[display] OPI PSRAM not found. Select Tools > PSRAM > OPI PSRAM.");
    return false;
  }

  // A failed partial initialization is recoverable: release its DMA bus and
  // allocate a fresh driver object on the next attempt.
  if (dmaDisplay != nullptr) {
    delete dmaDisplay;
    dmaDisplay = nullptr;
  }
  digitalWrite(HUB75_OE_PIN, HIGH);
  delay(20);

  HUB75_I2S_CFG::i2s_pins pins = {
      HUB75_R1_PIN, HUB75_G1_PIN, HUB75_B1_PIN,
      HUB75_R2_PIN, HUB75_G2_PIN, HUB75_B2_PIN,
      HUB75_A_PIN, HUB75_B_PIN, HUB75_C_PIN, HUB75_D_PIN, HUB75_E_PIN,
      HUB75_LAT_PIN, HUB75_OE_PIN, HUB75_CLK_PIN};

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
  mxconfig.double_buff = true;
  mxconfig.clkphase = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.latch_blanking = 2;
  mxconfig.min_refresh_rate = PANEL_MIN_REFRESH_HZ;
  mxconfig.setPixelColorDepthBits(PANEL_COLOR_DEPTH_BITS);

  dmaDisplay = new MatrixPanel_I2S_DMA(mxconfig);
  if (dmaDisplay == nullptr || !dmaDisplay->begin()) {
    Serial.println("[display] DMA allocation failed");
    if (dmaDisplay != nullptr) {
      delete dmaDisplay;
      dmaDisplay = nullptr;
    }
    return false;
  }

  // Initialize both hidden and visible buffers and explicitly cycle OE once.
  // This also recovers panels whose shift registers became ready slightly
  // later than the ESP32-S3 during a shared-supply power ramp.
  dmaDisplay->setBrightness8(0);
  dmaDisplay->setTextWrap(false);
  dmaDisplay->setTextSize(1);
  dmaDisplay->setFont();
  dmaDisplay->clearScreen();
  dmaDisplay->flipDMABuffer();
  dmaDisplay->clearScreen();
  dmaDisplay->flipDMABuffer();
  delay(30);
  dmaDisplay->setBrightness8(IDLE_BRIGHTNESS);
  initColors();
  displayReady = true;

  Serial.printf(
      "[display] ready, %u-bit color, double buffer, refresh=%u Hz, brightness=%u/255\n",
      PANEL_COLOR_DEPTH_BITS,
      dmaDisplay->calculated_refresh_rate,
      IDLE_BRIGHTNESS);
  return true;
}

void drawStatusBar(const ApiStatus &status) {
  // Healthy services stay invisible. Only unavailable dependencies appear:
  // N = network, F = flight source, A = alert source, H = history,
  // W = weather, T = time sync. Transient failures stay hidden.
  int16_t x = PANEL_RES_X - 6;
  if (!status.timeOk &&
      status.timeFailures >= STATUS_FAILURE_THRESHOLD) {
    printFixed(x, 0, "T", C_YELLOW);
    x -= 6;
  }
  if (!status.alertOk &&
      status.alertFailures >= STATUS_FAILURE_THRESHOLD) {
    printFixed(x, 0, "A", C_YELLOW);
    x -= 6;
  }
  if (!status.weatherOk &&
      status.weatherFailures >= STATUS_FAILURE_THRESHOLD) {
    printFixed(x, 0, "W", C_YELLOW);
    x -= 6;
  }
  if (!status.historyOk &&
      status.historyFailures >= STATUS_FAILURE_THRESHOLD) {
    printFixed(x, 0, "H", C_YELLOW);
    x -= 6;
  }
  if (!status.flightOk &&
      status.flightFailures >= STATUS_FAILURE_THRESHOLD) {
    printFixed(x, 0, "F", C_YELLOW);
    x -= 6;
  }
  if (!status.wifiOk &&
      status.wifiFailures >= STATUS_FAILURE_THRESHOLD) {
    printFixed(x, 0, "N", C_RED);
  }
}

void drawIdle(
    const SharedState &snapshot,
    const char *timeText,
    const char *dateText,
    uint32_t nowMs) {
  dmaDisplay->fillScreen(C_BLACK);
  drawBoldClock(timeText, 14, nowMs);
  printCentered(41, dateText, color565(70, 70, 70));
  drawStatusBar(snapshot.status);
}

void drawSpeedHeadingLine(
    const FlightData &flight,
    int16_t y,
    uint16_t color) {
  char speedText[8];
  char headingText[8];
  char line[32];
  if (flight.speedKts >= 0) {
    snprintf(speedText, sizeof(speedText), "%d", flight.speedKts);
  } else {
    copyText(speedText, sizeof(speedText), "---");
  }
  if (flight.headingDeg >= 0) {
    snprintf(
        headingText,
        sizeof(headingText),
        "%03d",
        flight.headingDeg % 360);
  } else {
    copyText(headingText, sizeof(headingText), "---");
  }
  snprintf(
      line,
      sizeof(line),
      "%sKT HDG %s",
      speedText,
      headingText);

  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  dmaDisplay->getTextBounds(
      line,
      0,
      y,
      &boundsX,
      &boundsY,
      &width,
      &height);
  const bool showDegree = flight.headingDeg >= 0;
  const int16_t totalWidth =
      static_cast<int16_t>(width) + (showDegree ? 4 : 0);
  const int16_t lineX = max(0, (PANEL_RES_X - totalWidth) / 2);
  printFixed(lineX, y, line, color);
  if (showDegree) {
    // Draw the mark ourselves: the built-in GFX font does not decode UTF-8.
    dmaDisplay->drawCircle(lineX + width + 1, y + 1, 1, color);
  }
}

void drawAircraft(const SharedState &snapshot) {
  const FlightData &flight = snapshot.flight;

  dmaDisplay->fillScreen(C_BLACK);
  dmaDisplay->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, C_BLUE);

  char line[48];
  if (flight.count > 1) {
    snprintf(
        line,
        sizeof(line),
        "%s  +%u",
        safeText(flight.callsign, "UNKNOWN"),
        static_cast<unsigned>(flight.count - 1));
  } else {
    copyText(line, sizeof(line), safeText(flight.callsign, "UNKNOWN"));
  }
  printCenteredClipped(4, line, C_WHITE);

  // Keep the live and historical cards consistent: registration is omitted.
  printCenteredClipped(
      16,
      safeText(flight.aircraft, "AIRCRAFT"),
      C_CYAN);

  snprintf(
      line,
      sizeof(line),
      "%s > %s",
      safeText(flight.origin, "---"),
      safeText(flight.destination, "---"));
  printCenteredClipped(28, line, C_GREEN);

  if (flight.altitudeFt >= 0) {
    snprintf(line, sizeof(line), "ALT %dFT", flight.altitudeFt);
  } else {
    copyText(line, sizeof(line), "ALT ---");
  }
  printCenteredClipped(40, line, C_WHITE);
  drawSpeedHeadingLine(flight, 52, C_WHITE);

  drawStatusBar(snapshot.status);
}

void drawLastAircraft(const SharedState &snapshot) {
  const FlightData &flight = snapshot.lastAircraft;
  dmaDisplay->fillScreen(C_BLACK);
  printCenteredClipped(2, "LAST AIRCRAFT", C_DIM);

  if (!flight.active) {
    printCentered(26, "NO HISTORY", C_WHITE);
    printCentered(44, "TAP TO RETURN", C_DIM);
    drawStatusBar(snapshot.status);
    return;
  }

  char line[48];
  dmaDisplay->setTextSize(2);
  printCenteredClipped(
      12,
      safeText(flight.callsign, "UNKNOWN"),
      C_WHITE);
  dmaDisplay->setTextSize(1);

  // Registration is deliberately omitted: the historical card is centered
  // around the useful flight facts that the API can reliably provide.
  printCenteredClipped(
      32,
      safeText(flight.aircraft, "AIRCRAFT"),
      C_CYAN);
  if (!hasPanelText(flight.origin) &&
      !hasPanelText(flight.destination)) {
    if (flight.altitudeFt >= 0) {
      snprintf(line, sizeof(line), "ROUTE N/A  %dFT", flight.altitudeFt);
    } else {
      copyText(line, sizeof(line), "ROUTE N/A");
    }
  } else if (flight.altitudeFt >= 0) {
    snprintf(
        line,
        sizeof(line),
        "%s > %s  %dFT",
        safeText(flight.origin, "---"),
        safeText(flight.destination, "---"),
        flight.altitudeFt);
  } else {
    snprintf(
        line,
        sizeof(line),
        "%s > %s",
        safeText(flight.origin, "---"),
        safeText(flight.destination, "---"));
  }
  printCenteredClipped(43, line, C_GREEN);

  drawSpeedHeadingLine(flight, 54, C_WHITE);
  drawStatusBar(snapshot.status);
}

void drawAlert(const SharedState &snapshot) {
  const AlertData &alert = snapshot.alert;
  const bool eventEnded =
      strcmp(alert.category, "10") == 0 || strcmp(alert.category, "13") == 0;
  const bool preAlert = strcmp(alert.category, "14") == 0;
  const uint16_t accentColor =
      eventEnded ? C_GREEN : (preAlert ? C_YELLOW : C_RED);
  const char *heading =
      eventEnded ? "EVENT ENDED" : (preAlert ? "PRE-ALERT" : "!!! ALERT !!!");

  dmaDisplay->fillScreen(accentColor);
  dmaDisplay->fillRect(2, 2, PANEL_RES_X - 4, PANEL_RES_Y - 4, C_BLACK);
  printCentered(8, heading, accentColor);
  printCenteredClipped(27, safeText(alert.title, "ACTIVE WARNING"), C_WHITE);
  printCenteredClipped(45, safeText(alert.firstArea, "AREA UNKNOWN"), C_YELLOW);
  drawStatusBar(snapshot.status);
}

void drawHardwareTest(const SharedState &snapshot) {
  const PeripheralStatus &peripheral = snapshot.peripheral;
  char line[32];

  dmaDisplay->fillScreen(C_BLACK);
  dmaDisplay->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, C_CYAN);
  printCentered(4, "SENSOR TEST", C_CYAN);

  if (peripheral.lightOk && peripheral.ambientLux >= 0.0f) {
    snprintf(
        line,
        sizeof(line),
        "LIGHT %d lx",
        static_cast<int>(roundf(peripheral.ambientLux)));
  } else {
    copyText(line, sizeof(line), "LIGHT NOT FOUND");
  }
  printCenteredClipped(19, line, peripheral.lightOk ? C_WHITE : C_YELLOW);

  snprintf(
      line,
      sizeof(line),
      "BUTTON OK  #%lu",
      static_cast<unsigned long>(peripheral.buttonPresses));
  printCenteredClipped(34, line, C_GREEN);

  snprintf(
      line,
      sizeof(line),
      "%s  BR %u",
      peripheral.autoBrightness ? "AUTO" : "FIXED",
      static_cast<unsigned>(snapshot.metrics.appliedBrightness));
  printCenteredClipped(49, line, C_DIM);
  drawStatusBar(snapshot.status);
}

void drawScreensaverRadar(uint32_t nowMs, const char *timeText) {
  constexpr int16_t cx = 31;
  constexpr int16_t cy = PANEL_RES_Y / 2;
  constexpr int16_t radarRadius = 28;
  const bool warmRadar = activeVisualMode != VisualMode::DAY;
  const uint16_t gridColor =
      warmRadar ? color565(70, 25, 0) : color565(0, 18, 7);
  static bool polarMapReady = false;
  static uint8_t polarAngle[57][57];

  if (!polarMapReady) {
    for (int16_t mapY = 0; mapY < 57; ++mapY) {
      for (int16_t mapX = 0; mapX < 57; ++mapX) {
        const int16_t dx = mapX - radarRadius;
        const int16_t dy = mapY - radarRadius;
        // Zero is 12 o'clock and the angle grows clockwise, matching a
        // conventional seconds hand and the contact coordinate system.
        float angle = atan2f(dx, -dy);
        if (angle < 0.0f) angle += 2.0f * PI;
        polarAngle[mapY][mapX] = static_cast<uint8_t>(
            roundf(angle * 255.0f / (2.0f * PI)));
      }
    }
    polarMapReady = true;
  }

  // The beam is a true smooth seconds hand: NTP seconds set its absolute
  // position and microseconds provide continuous motion between ticks. There
  // is no end-of-cycle dwell or independent uptime timer to drift or stall.
  struct timeval preciseTime;
  gettimeofday(&preciseTime, nullptr);
  const float secondPosition =
      preciseTime.tv_sec > 100000
          ? (preciseTime.tv_sec % 60) +
                preciseTime.tv_usec / 1000000.0f
          : (nowMs % 60000UL) / 1000.0f;
  const float sweepAngle =
      secondPosition * (2.0f * PI / 60.0f);
  const float sweepUnit = sweepAngle * 256.0f / (2.0f * PI);
  constexpr float leadFadeUnits = 1.5f;
  constexpr float trailUnits = 18.0f;
  for (int16_t mapY = 0; mapY < 57; ++mapY) {
    const int16_t dy = mapY - radarRadius;
    for (int16_t mapX = 0; mapX < 57; ++mapX) {
      const int16_t dx = mapX - radarRadius;
      if (dx * dx + dy * dy > radarRadius * radarRadius) continue;

      const float pixelAngle =
          polarAngle[mapY][mapX] * (256.0f / 255.0f);
      float behind = sweepUnit - pixelAngle;
      if (behind < -128.0f) behind += 256.0f;
      if (behind > 128.0f) behind -= 256.0f;
      if (behind < -leadFadeUnits || behind > trailUnits) continue;

      float coverage;
      if (behind < 0.0f) {
        coverage = 1.0f + behind / leadFadeUnits;
      } else {
        const float tail = 1.0f - behind / trailUnits;
        coverage = tail * tail;
      }
      if (coverage <= 0.12f) continue;
      const uint8_t intensity =
          static_cast<uint8_t>(roundf(54.0f * coverage));
      dmaDisplay->drawPixel(
          cx + dx,
          cy + dy,
          warmRadar
              ? color565(intensity, intensity / 3U, 0)
              : color565(0, intensity, intensity / 3U));
    }
  }

  // Grid is drawn after the sweep so low-intensity tail pixels can never erase
  // the circles or crosshair in NIGHT/SLEEP at low DMA brightness.
  for (int16_t radius = 7; radius <= radarRadius; radius += 7) {
    dmaDisplay->drawCircle(cx, cy, radius, gridColor);
  }
  dmaDisplay->drawFastHLine(cx - radarRadius, cy, 57, gridColor);
  dmaDisplay->drawFastVLine(cx, cy - radarRadius, 57, gridColor);

  // A hidden target position is selected in advance. It remains invisible
  // until the physical scan beam reaches that angle.
  static bool contactActive = false;
  static bool contactPending = false;
  static uint32_t contactBornMs = 0;
  static uint32_t contactLifeMs = 0;
  static uint32_t nextContactMs = 0;
  static float pendingContactAngle = 0.0f;
  static float pendingContactRadius = 0.0f;
  static int16_t contactX = cx;
  static int16_t contactY = cy;
  if (nextContactMs == 0) {
    nextContactMs = nowMs + static_cast<uint32_t>(random(20000, 50000));
  }
  if (!contactActive &&
      !contactPending &&
      static_cast<int32_t>(nowMs - nextContactMs) >= 0) {
    pendingContactAngle =
        random(0, 10000) * (2.0f * PI / 10000.0f);
    pendingContactRadius = static_cast<float>(random(8, 27));
    contactPending = true;
  }
  if (contactPending && !contactActive) {
    float beamDistance = sweepAngle - pendingContactAngle;
    while (beamDistance > PI) beamDistance -= 2.0f * PI;
    while (beamDistance < -PI) beamDistance += 2.0f * PI;
    // Roughly one second of angular travel is the only detection window.
    if (fabsf(beamDistance) <= 0.075f) {
      contactPending = false;
      contactActive = true;
      contactBornMs = nowMs;
      // Keep the return at full intensity for the entire following sweep.
      // Once the beam comes around again, release it with a short soft fade.
      contactLifeMs = 62500UL;
      contactX = static_cast<int16_t>(
          roundf(
              cx + sinf(pendingContactAngle) *
                       pendingContactRadius));
      contactY = static_cast<int16_t>(
          roundf(
              cy - cosf(pendingContactAngle) *
                       pendingContactRadius));
    }
  }
  if (contactActive) {
    const uint32_t ageMs = nowMs - contactBornMs;
    if (ageMs >= contactLifeMs) {
      contactActive = false;
      nextContactMs =
          nowMs + static_cast<uint32_t>(random(25000, 70000));
    } else {
      constexpr uint32_t holdMs = 60000UL;
      constexpr uint32_t fadeMs = 2500UL;
      const float fade =
          ageMs < holdMs
              ? 1.0f
              : constrain(
                    1.0f -
                        (ageMs - holdMs) /
                            static_cast<float>(fadeMs),
                    0.0f,
                    1.0f);
      const uint8_t level =
          static_cast<uint8_t>(roundf(70.0f * fade));
      const uint16_t contactColor =
          warmRadar
              ? color565(level, level / 3U, 0)
              : color565(0, level, level / 3U);
      dmaDisplay->fillCircle(contactX, contactY, 1, contactColor);
    }
  }

  dmaDisplay->drawFastVLine(
      63,
      5,
      54,
      warmRadar ? color565(30, 14, 0) : color565(5, 10, 11));

  // HH:MM at text size 2 is exactly 60 pixels wide and fits the right half.
  char hours[3] = {timeText[0], timeText[1], '\0'};
  char minutes[3] = {timeText[3], timeText[4], '\0'};
  const float colonPhase = (nowMs % 1000UL) * (2.0f * PI / 1000.0f);
  const float colonBreath = 0.5f - 0.5f * cosf(colonPhase);
  const uint8_t colonLevel =
      warmRadar
          ? 62U
          : static_cast<uint8_t>(roundf(26.0f + colonBreath * 34.0f));

  dmaDisplay->setTextSize(2);
  printFixed(66, 24, hours, color565(58, 58, 58));
  printFixed(
      90,
      24,
      ":",
      warmRadar
          ? color565(62, 25, 0)
          : color565(colonLevel, colonLevel, colonLevel));
  printFixed(102, 24, minutes, color565(58, 58, 58));
  dmaDisplay->setTextSize(1);
}

float daytimeAnimationEnergy() {
  const time_t wallClock = time(nullptr);
  struct tm localTime;
  if (wallClock < 100000 ||
      localtime_r(&wallClock, &localTime) == nullptr) {
    return 0.55f;
  }
  const float hour =
      localTime.tm_hour + localTime.tm_min / 60.0f;
  float energy;
  if (hour < 6.0f || hour >= 22.0f) {
    energy = 0.04f;
  } else if (hour < 12.0f) {
    energy = 0.30f + (hour - 6.0f) / 6.0f * 0.70f;
  } else if (hour < 14.0f) {
    energy = 1.0f;
  } else {
    energy = 1.0f - (hour - 14.0f) / 8.0f * 0.96f;
  }
  energy = constrain(energy, 0.04f, 1.0f);
  return energy * energy * (3.0f - 2.0f * energy);
}

void drawStreamlinedClockCentered(
    int16_t centerX,
    int16_t centerY,
    const char *timeText,
    uint16_t color,
    uint16_t colonColor);

void drawScreensaverWave(uint32_t nowMs, const char *timeText) {
  const float energy = daytimeAnimationEnergy();
  const float spatialFrequency = 0.024f + energy * 0.076f;
  const float motionSpeed = 0.000055f + energy * 0.00048f;
  const float amplitude = 5.0f + energy * 2.5f;
  const float phase = nowMs * motionSpeed;
  for (int16_t x = 0; x < PANEL_RES_X; ++x) {
    const float waveY =
        31.5f + sinf(x * spatialFrequency + phase) * amplitude;
    const int16_t centerY = static_cast<int16_t>(floorf(waveY));
    for (int8_t offset = -2; offset <= 2; ++offset) {
      const int16_t pixelY = centerY + offset;
      const float coverage =
          constrain(1.65f - fabsf(pixelY - waveY), 0.0f, 1.0f);
      if (coverage <= 0.0f) continue;
      dmaDisplay->drawPixel(
          x,
          pixelY,
          color565(
              static_cast<uint8_t>(roundf(7.0f * coverage)),
              static_cast<uint8_t>(roundf(34.0f * coverage)),
              static_cast<uint8_t>(roundf(42.0f * coverage))));
    }
  }

  const bool warm = activeVisualMode != VisualMode::DAY;
  const float breath =
      0.5f - 0.5f * cosf((nowMs % 1000UL) * (2.0f * PI / 1000.0f));
  const uint8_t colon =
      warm ? 64U : static_cast<uint8_t>(roundf(34.0f + breath * 38.0f));
  drawStreamlinedClockCentered(
      PANEL_RES_X / 2,
      PANEL_RES_Y / 2,
      timeText,
      color565(58, 58, 58),
      warm ? color565(64, 26, 0) : color565(colon, colon, colon));
}

void drawScreensaverFire(uint32_t nowMs, const char *timeText) {
  const float energy = daytimeAnimationEnergy();
  const float phase = nowMs * 0.00055f;
  for (int16_t x = 0; x < PANEL_RES_X; ++x) {
    const float tongue =
        0.63f +
        0.23f * sinf(x * 0.095f + phase) +
        0.18f * sinf(x * 0.031f - phase * 0.68f) +
        0.09f * sinf(x * 0.19f + phase * 0.37f);
    const float height =
        5.0f + energy * 5.0f +
        max(0.0f, tongue) * (12.0f + energy * 18.0f);
    const int16_t visibleHeight = static_cast<int16_t>(ceilf(height));
    for (int16_t dy = 0; dy < visibleHeight; ++dy) {
      const float edgeCoverage =
          constrain(height - dy, 0.0f, 1.0f);
      const float strength =
          (1.0f - dy / height) * edgeCoverage;
      const uint8_t red =
          static_cast<uint8_t>(8.0f + strength * 48.0f);
      const uint8_t green =
          static_cast<uint8_t>(1.0f + strength * strength * 18.0f);
      dmaDisplay->drawPixel(
          x,
          PANEL_RES_Y - 1 - dy,
          color565(red, green, 0));
    }
  }

  char digitText[4][2] = {
      {timeText[0], '\0'},
      {timeText[1], '\0'},
      {timeText[3], '\0'},
      {timeText[4], '\0'}};
  const float colonPhase = (nowMs % 1000UL) * (2.0f * PI / 1000.0f);
  const float colonBreath = 0.5f - 0.5f * cosf(colonPhase);
  const uint8_t colonLevel =
      activeVisualMode == VisualMode::DAY
          ? static_cast<uint8_t>(roundf(22.0f + colonBreath * 30.0f))
          : 60U;
  dmaDisplay->setTextSize(1);
  const uint16_t clockColor = color565(52, 47, 38);
  auto drawBoldDigit = [&](int16_t x, const char *digit) {
    printFixed(x, 8, digit, clockColor);
    printFixed(x + 1, 8, digit, clockColor);
  };
  // Each bold glyph occupies six lit columns; the seven-pixel advance leaves
  // one true dark pixel between neighboring digits.
  drawBoldDigit(46, digitText[0]);
  drawBoldDigit(53, digitText[1]);
  printFixed(
      61,
      8,
      ":",
      activeVisualMode == VisualMode::DAY
          ? color565(colonLevel, colonLevel, colonLevel)
          : color565(60, 24, 0));
  printFixed(
      62,
      8,
      ":",
      activeVisualMode == VisualMode::DAY
          ? color565(colonLevel, colonLevel, colonLevel)
          : color565(60, 24, 0));
  drawBoldDigit(69, digitText[2]);
  drawBoldDigit(76, digitText[3]);
  dmaDisplay->setTextSize(1);
}

uint8_t ditherSixBitChannel(
    float value,
    uint8_t quantizationStep,
    uint8_t pattern) {
  value = constrain(value, 0.0f, 255.0f);
  const float scaled = value / quantizationStep;
  const uint8_t lower = static_cast<uint8_t>(floorf(scaled));
  const float fraction = scaled - lower;
  const float threshold = (pattern + 0.5f) / 8.0f;
  const uint16_t quantized =
      (lower + (fraction > threshold ? 1U : 0U)) * quantizationStep;
  return static_cast<uint8_t>(quantized > 255U ? 255U : quantized);
}

void hsvToRgb(
    float hue,
    float saturation,
    float value,
    float &red,
    float &green,
    float &blue) {
  hue -= floorf(hue);
  saturation = constrain(saturation, 0.0f, 1.0f);
  value = constrain(value, 0.0f, 1.0f);
  const float sector = hue * 6.0f;
  const int index = static_cast<int>(floorf(sector)) % 6;
  const float fraction = sector - floorf(sector);
  const float p = value * (1.0f - saturation);
  const float q = value * (1.0f - saturation * fraction);
  const float t = value * (1.0f - saturation * (1.0f - fraction));
  switch (index) {
    case 0: red = value; green = t; blue = p; break;
    case 1: red = q; green = value; blue = p; break;
    case 2: red = p; green = value; blue = t; break;
    case 3: red = p; green = q; blue = value; break;
    case 4: red = t; green = p; blue = value; break;
    default: red = value; green = p; blue = q; break;
  }
}

void drawCompactRoundedClock(
    int16_t x,
    int16_t y,
    const char *timeText,
    uint16_t color);

void drawScreensaverLavaLamp(uint32_t nowMs, const char *timeText) {
  const float phase = nowMs * 0.00012f;
  constexpr uint8_t blobCount = 8;
  float blobX[blobCount];
  float blobY[blobCount];
  float blobRadius[blobCount];

  for (uint8_t i = 0; i < blobCount; ++i) {
    const float offset = i * 0.93f;
    blobX[i] =
        8.0f + i * 16.0f +
        sinf(phase * (0.68f + i * 0.035f) + offset) *
            (6.0f + (i % 2) * 2.0f);
    blobY[i] =
        31.5f + sinf(phase * (0.82f + i * 0.055f) + offset * 1.37f) *
                    (23.0f - (i % 3) * 2.0f);
    blobRadius[i] = 6.0f + (i % 3) * 1.2f;
  }

  // The palette crosses the complete hue wheel very slowly. A spatial hue
  // offset widens each transition, avoiding a panel-wide color jump.
  const float globalHue = fmodf(nowMs / 480000.0f, 1.0f);
  const uint8_t ditherFrame =
      static_cast<uint8_t>((nowMs / (SCREENSAVER_FRAME_MS * 2U)) & 7U);

  // Full-panel smooth metaballs. Fractional centers change pixel intensity
  // every frame instead of jumping between integer positions.
  for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
    for (int16_t x = 0; x < PANEL_RES_X; ++x) {
      float influence = 0.0f;
      for (uint8_t i = 0; i < blobCount; ++i) {
        const float dx = x - blobX[i];
        const float dy = y - blobY[i];
        influence +=
            (blobRadius[i] * blobRadius[i]) / (dx * dx + dy * dy + 1.0f);
      }
      const float wax =
          constrain((influence - 0.96f) * 2.45f, 0.0f, 1.0f);
      // Keep a one-pixel antialiased boundary, but discard the broad low-level
      // halo that looked like an aura around each blob.
      if (wax <= 0.08f) continue;
      const float softWax = wax * wax * (3.0f - 2.0f * wax);
      float red;
      float green;
      float blue;
      hsvToRgb(
          globalHue + x / 280.0f + y / 850.0f,
          0.82f,
          0.22f + 0.76f * softWax,
          red,
          green,
          blue);
      const uint8_t pattern =
          static_cast<uint8_t>((x * 3 + y * 5 + ditherFrame) & 7);
      dmaDisplay->drawPixel(
          x,
          y,
          color565(
              ditherSixBitChannel(red * 63.0f * softWax, 8, pattern),
              ditherSixBitChannel(green * 63.0f * softWax, 4, pattern),
              ditherSixBitChannel(blue * 63.0f * softWax, 8, pattern)));
    }
  }

  // Compact rounded segments stay transparent between their strokes, so the
  // wax continues through the clock without a rectangular background.
  drawCompactRoundedClock(85, 51, timeText, color565(55, 55, 55));
}

void drawScreensaverRings(uint32_t nowMs) {
  static bool radiusMapReady = false;
  // 1/64-pixel cached radii avoid the quarter-pixel stepping of the previous
  // map while keeping sqrt() out of the 40 fps render path.
  static uint16_t radiusMap[PANEL_RES_Y][PANEL_RES_X];
  if (!radiusMapReady) {
    for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
      for (int16_t x = 0; x < PANEL_RES_X; ++x) {
        const float dx = (x - 63.5f) * 0.62f;
        const float dy = y - 31.5f;
        radiusMap[y][x] = static_cast<uint16_t>(
            min(65535.0f, sqrtf(dx * dx + dy * dy) * 64.0f));
      }
    }
    radiusMapReady = true;
  }
  static int32_t lastPulseMinute = -1;
  static uint32_t gravityPulseStartedMs = 0;
  const time_t wallClock = time(nullptr);
  struct tm localTime;
  if (wallClock > 100000 &&
      localtime_r(&wallClock, &localTime) != nullptr) {
    const int32_t minuteKey =
        localTime.tm_yday * 24 * 60 +
        localTime.tm_hour * 60 +
        localTime.tm_min;
    if (localTime.tm_sec == 0 && minuteKey != lastPulseMinute) {
      lastPulseMinute = minuteKey;
      gravityPulseStartedMs = nowMs;
    }
  }
  const uint32_t gravityAgeMs = nowMs - gravityPulseStartedMs;
  constexpr uint32_t gravityDurationMs = 1200;
  const bool gravityActive =
      gravityPulseStartedMs != 0 &&
      gravityAgeMs < gravityDurationMs;
  const float gravityProgress =
      gravityActive
          ? gravityAgeMs / static_cast<float>(gravityDurationMs)
          : 1.0f;
  // The contraction front starts just behind the center and travels past the
  // outer ring. Each radius is pulled inward and then smoothly released as
  // the front passes it.
  const float gravityFront = -7.0f + gravityProgress * 63.0f;
  const float phase = fmodf(nowMs * 0.00048f, 16.0f);
  for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
    for (int16_t x = 0; x < PANEL_RES_X; ++x) {
      const float physicalRadius = radiusMap[y][x] / 64.0f;
      float gravityScale = 1.0f;
      if (gravityActive) {
        const float frontDistance =
            (physicalRadius - gravityFront) / 5.0f;
        const float envelope =
            expf(-0.5f * frontDistance * frontDistance);
        gravityScale = 1.0f - 0.105f * envelope;
      }
      const float radius = physicalRadius / gravityScale;
      const float band = fmodf(radius - phase + 64.0f, 16.0f);
      const float distance = min(band, 16.0f - band);
      float coverage = constrain(1.85f - distance, 0.0f, 1.0f);
      if (coverage <= 0.0f) continue;
      coverage = coverage * coverage * (3.0f - 2.0f * coverage);
      const uint8_t level =
          static_cast<uint8_t>(roundf(34.0f * coverage));
      dmaDisplay->drawPixel(
          x,
          y,
          color565(level / 3U, level, level));
    }
  }
}

void drawSubpixelAircraft(
    float x,
    float y,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    bool drawEngines = true,
    bool drawCockpitLight = true) {
  // Head-on airliner silhouette: cockpit and fuselage in the center, wings
  // across the panel, two engines below them, and a small tailplane.
  constexpr uint32_t rows[19] = {
      0x00008000UL,
      0x0001C000UL,
      0x0001C000UL,
      0x0003E000UL,
      0x007FFF00UL,
      0x01FFFFC0UL,
      0x0007F000UL,
      0x000FF800UL,
      0x1FFFFFF8UL,
      0x7FFFFFFFUL,
      0x7FFFFFFFUL,
      0x1FFFFFFCUL,
      0x03FFFFE0UL,
      0x03E7F3E0UL,
      0x01C7F1C0UL,
      0x000FF800UL,
      0x0007F000UL,
      0x0003E000UL,
      0x0001C000UL};
  const int16_t baseX = static_cast<int16_t>(floorf(x));
  const int16_t baseY = static_cast<int16_t>(floorf(y));

  auto sample = [&](int16_t sx, int16_t sy) -> float {
    if (sx < 0 || sx >= 31 || sy < 0 || sy >= 19) return 0.0f;
    if (!drawEngines &&
        sy >= 13 && sy <= 14 &&
        ((sx >= 5 && sx <= 9) || (sx >= 21 && sx <= 25))) {
      return 0.0f;
    }
    return (rows[sy] & (1UL << sx)) != 0 ? 1.0f : 0.0f;
  };
  for (int16_t py = baseY; py <= baseY + 19; ++py) {
    for (int16_t px = baseX; px <= baseX + 31; ++px) {
      const float sourceX = px - x;
      const float sourceY = py - y;
      const int16_t sx = static_cast<int16_t>(floorf(sourceX));
      const int16_t sy = static_cast<int16_t>(floorf(sourceY));
      const float fx = sourceX - sx;
      const float fy = sourceY - sy;
      const float top =
          sample(sx, sy) * (1.0f - fx) + sample(sx + 1, sy) * fx;
      const float bottom =
          sample(sx, sy + 1) * (1.0f - fx) +
          sample(sx + 1, sy + 1) * fx;
      const float coverage = top * (1.0f - fy) + bottom * fy;
      if (coverage < 0.06f) continue;
      dmaDisplay->drawPixel(
          px,
          py,
          color565(
              static_cast<uint8_t>(red * coverage),
              static_cast<uint8_t>(green * coverage),
              static_cast<uint8_t>(blue * coverage)));
    }
  }

  // Cockpit and navigation lights make the silhouette identifiable without
  // turning it into a visually busy sprite.
  if (drawCockpitLight) {
    dmaDisplay->drawPixel(
        static_cast<int16_t>(roundf(x + 15.0f)),
        static_cast<int16_t>(roundf(y + 3.0f)),
        color565(20, 42, 56));
  }
  if (drawEngines) {
    const int16_t leftEngineX = static_cast<int16_t>(roundf(x + 7.0f));
    const int16_t rightEngineX = static_cast<int16_t>(roundf(x + 23.0f));
    const int16_t engineY = static_cast<int16_t>(roundf(y + 13.0f));
    dmaDisplay->fillCircle(leftEngineX, engineY, 1, C_BLACK);
    dmaDisplay->fillCircle(rightEngineX, engineY, 1, C_BLACK);
    dmaDisplay->drawPixel(leftEngineX, engineY, color565(12, 18, 20));
    dmaDisplay->drawPixel(rightEngineX, engineY, color565(12, 18, 20));
  }
}

void drawScreensaverAircraftBounce(uint32_t nowMs) {
  static bool initialized = false;
  static uint32_t previousMs = 0;
  static uint32_t nextDriftMs = 0;
  static float x = 20.0f;
  static float y = 18.0f;
  static float velocityX = 7.0f;
  static float velocityY = 4.8f;
  static uint8_t hueIndex = 0;
  static uint32_t cornerBannerUntilMs = 0;
  static uint32_t cornerBannerStartedMs = 0;

  if (!initialized) {
    x = static_cast<float>(random(2, 95));
    y = static_cast<float>(random(2, 43));
    velocityX = random(0, 2) == 0 ? -7.0f : 7.0f;
    velocityY = random(0, 2) == 0 ? -4.8f : 4.8f;
    previousMs = nowMs;
    nextDriftMs = nowMs + static_cast<uint32_t>(random(7000, 14000));
    initialized = true;
  }

  const float dt = min(0.08f, (nowMs - previousMs) / 1000.0f);
  previousMs = nowMs;
  x += velocityX * dt;
  y += velocityY * dt;

  bool bounced = false;
  const bool hitHorizontalEdge = x <= 0.0f || x >= 96.0f;
  const bool hitVerticalEdge = y <= 0.0f || y >= 44.0f;
  if (hitHorizontalEdge) {
    x = constrain(x, 0.0f, 96.0f);
    velocityX = -velocityX;
    bounced = true;
  }
  if (hitVerticalEdge) {
    y = constrain(y, 0.0f, 44.0f);
    velocityY = -velocityY;
    bounced = true;
  }
  if (hitHorizontalEdge && hitVerticalEdge) {
    cornerBannerStartedMs = nowMs;
    cornerBannerUntilMs = nowMs + 30000UL;
  }
  if (bounced) hueIndex = (hueIndex + 1U) % 4U;

  if (static_cast<int32_t>(nowMs - nextDriftMs) >= 0) {
    velocityY = constrain(
        velocityY + random(-12, 13) * 0.08f,
        -5.5f,
        5.5f);
    if (fabsf(velocityY) < 2.5f) {
      velocityY = velocityY < 0.0f ? -2.5f : 2.5f;
    }
    nextDriftMs = nowMs + static_cast<uint32_t>(random(7000, 14000));
  }

  constexpr uint8_t colors[][3] = {
      {44, 40, 35},
      {30, 44, 45},
      {45, 32, 22},
      {35, 30, 46}};
  drawSubpixelAircraft(
      x,
      y,
      colors[hueIndex][0],
      colors[hueIndex][1],
      colors[hueIndex][2]);

  if (static_cast<int32_t>(cornerBannerUntilMs - nowMs) > 0) {
    constexpr char message[] = "- Yes, That just happened.";
    constexpr int16_t messageWidth = (sizeof(message) - 1) * 6;
    const uint32_t travel =
        (nowMs - cornerBannerStartedMs) / 55UL;
    const int16_t messageX =
        PANEL_RES_X -
        static_cast<int16_t>(travel % (PANEL_RES_X + messageWidth));
    dmaDisplay->fillRect(0, 27, PANEL_RES_X, 10, C_BLACK);
    printFixed(
        messageX,
        28,
        message,
        activeVisualMode == VisualMode::DAY
            ? color565(48, 48, 48)
            : color565(58, 25, 3));
  }
}

uint8_t sevenSegmentMask(char digit) {
  constexpr uint8_t masks[10] = {
      0x3F, 0x06, 0x5B, 0x4F, 0x66,
      0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  return digit >= '0' && digit <= '9' ? masks[digit - '0'] : 0;
}

void drawSegmentDigit(
    int16_t x,
    int16_t y,
    uint8_t scale,
    char digit,
    uint16_t color) {
  const uint8_t mask = sevenSegmentMask(digit);
  const int16_t width = scale * 4;
  const int16_t height = scale * 7;
  const int16_t thickness = scale;
  auto segment = [&](uint8_t bit, int16_t sx, int16_t sy, int16_t sw, int16_t sh) {
    if ((mask & (1U << bit)) != 0) {
      dmaDisplay->fillRect(sx, sy, sw, sh, color);
    }
  };
  segment(0, x + thickness, y, width - 2 * thickness, thickness);
  segment(
      1,
      x + width - thickness,
      y + thickness,
      thickness,
      height / 2 - thickness);
  segment(
      2,
      x + width - thickness,
      y + height / 2,
      thickness,
      height / 2 - thickness);
  segment(
      3,
      x + thickness,
      y + height - thickness,
      width - 2 * thickness,
      thickness);
  segment(
      4,
      x,
      y + height / 2,
      thickness,
      height / 2 - thickness);
  segment(
      5,
      x,
      y + thickness,
      thickness,
      height / 2 - thickness);
  segment(
      6,
      x + thickness,
      y + height / 2 - thickness / 2,
      width - 2 * thickness,
      thickness);
}

void drawSegmentClock(
    int16_t x,
    int16_t y,
    uint8_t scale,
    const char *timeText,
    uint16_t color,
    uint16_t colonColor) {
  const int16_t digitWidth = scale * 4;
  const int16_t step = digitWidth + scale;
  drawSegmentDigit(x, y, scale, timeText[0], color);
  drawSegmentDigit(x + step, y, scale, timeText[1], color);
  const int16_t colonX = x + 2 * step;
  dmaDisplay->fillRect(
      colonX,
      y + scale * 2,
      scale,
      scale,
      colonColor);
  dmaDisplay->fillRect(
      colonX,
      y + scale * 5,
      scale,
      scale,
      colonColor);
  drawSegmentDigit(
      x + 12 * scale,
      y,
      scale,
      timeText[3],
      color);
  drawSegmentDigit(
      x + 17 * scale,
      y,
      scale,
      timeText[4],
      color);
}

void drawRoundedBar(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint8_t thickness,
    uint16_t color) {
  const int8_t radius = thickness / 2;
  if (y0 == y1) {
    for (int8_t offset = -radius; offset <= radius; ++offset) {
      dmaDisplay->drawLine(x0, y0 + offset, x1, y1 + offset, color);
    }
  } else {
    for (int8_t offset = -radius; offset <= radius; ++offset) {
      dmaDisplay->drawLine(x0 + offset, y0, x1 + offset, y1, color);
    }
  }
  dmaDisplay->fillCircle(x0, y0, radius, color);
  dmaDisplay->fillCircle(x1, y1, radius, color);
}

void drawRoundedDigitalDigit(
    int16_t x,
    int16_t y,
    char digit,
    uint16_t color) {
  const uint8_t mask = sevenSegmentMask(digit);
  auto bar = [&](uint8_t bit, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if ((mask & (1U << bit)) != 0) {
      drawRoundedBar(x0, y0, x1, y1, 3, color);
    }
  };
  bar(0, x + 2, y + 1, x + 11, y + 1);
  bar(1, x + 12, y + 3, x + 12, y + 11);
  bar(2, x + 12, y + 15, x + 12, y + 23);
  bar(3, x + 2, y + 25, x + 11, y + 25);
  bar(4, x + 1, y + 15, x + 1, y + 23);
  bar(5, x + 1, y + 3, x + 1, y + 11);
  bar(6, x + 2, y + 13, x + 11, y + 13);
}

void drawRoundedDigitalClockCentered(
    int16_t centerX,
    int16_t centerY,
    const char *timeText,
    uint16_t color,
    uint16_t colonColor) {
  constexpr int16_t totalWidth = 74;
  const int16_t x = centerX - totalWidth / 2;
  const int16_t y = centerY - 13;
  drawRoundedDigitalDigit(x, y, timeText[0], color);
  drawRoundedDigitalDigit(x + 17, y, timeText[1], color);
  dmaDisplay->fillCircle(x + 36, centerY - 5, 1, colonColor);
  dmaDisplay->fillCircle(x + 36, centerY + 5, 1, colonColor);
  drawRoundedDigitalDigit(x + 43, y, timeText[3], color);
  drawRoundedDigitalDigit(x + 60, y, timeText[4], color);
}

void drawStreamlinedDigitalDigit(
    int16_t x,
    int16_t y,
    char digit,
    uint16_t color) {
  const uint8_t mask = sevenSegmentMask(digit);
  auto bar = [&](uint8_t bit, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if ((mask & (1U << bit)) != 0) {
      // Rounded three-pixel strokes keep the text fluid without changing the
      // visual footprint of the former size-2 bitmap clock.
      drawRoundedBar(x0, y0, x1, y1, 2, color);
    }
  };
  bar(0, x + 2, y, x + 8, y);
  bar(1, x + 9, y + 2, x + 9, y + 6);
  bar(2, x + 9, y + 10, x + 9, y + 14);
  bar(3, x + 2, y + 16, x + 8, y + 16);
  bar(4, x + 1, y + 10, x + 1, y + 14);
  bar(5, x + 1, y + 2, x + 1, y + 6);
  bar(6, x + 2, y + 8, x + 8, y + 8);
}

void drawStreamlinedClockCentered(
    int16_t centerX,
    int16_t centerY,
    const char *timeText,
    uint16_t color,
    uint16_t colonColor) {
  constexpr int16_t totalWidth = 60;
  const int16_t x = centerX - totalWidth / 2;
  const int16_t y = centerY - 8;
  drawStreamlinedDigitalDigit(x, y, timeText[0], color);
  drawStreamlinedDigitalDigit(x + 13, y, timeText[1], color);
  dmaDisplay->fillCircle(x + 29, centerY - 4, 1, colonColor);
  dmaDisplay->fillCircle(x + 29, centerY + 4, 1, colonColor);
  drawStreamlinedDigitalDigit(x + 35, y, timeText[3], color);
  drawStreamlinedDigitalDigit(x + 48, y, timeText[4], color);
}

void drawCompactRoundedDigit(
    int16_t x,
    int16_t y,
    char digit,
    uint16_t color) {
  const uint8_t mask = sevenSegmentMask(digit);
  auto bar = [&](uint8_t bit, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if ((mask & (1U << bit)) != 0) {
      drawRoundedBar(x0, y0, x1, y1, 2, color);
    }
  };
  bar(0, x + 1, y, x + 5, y);
  bar(1, x + 6, y + 1, x + 6, y + 4);
  bar(2, x + 6, y + 6, x + 6, y + 9);
  bar(3, x + 1, y + 10, x + 5, y + 10);
  bar(4, x, y + 6, x, y + 9);
  bar(5, x, y + 1, x, y + 4);
  bar(6, x + 1, y + 5, x + 5, y + 5);
}

void drawCompactRoundedClock(
    int16_t x,
    int16_t y,
    const char *timeText,
  uint16_t color) {
  drawCompactRoundedDigit(x, y, timeText[0], color);
  drawCompactRoundedDigit(x + 10, y, timeText[1], color);
  dmaDisplay->fillCircle(x + 21, y + 3, 1, color);
  dmaDisplay->fillCircle(x + 21, y + 8, 1, color);
  drawCompactRoundedDigit(x + 25, y, timeText[3], color);
  drawCompactRoundedDigit(x + 35, y, timeText[4], color);
}

void drawOutlinedRoundedDigitalDigit(
    int16_t x,
    int16_t y,
    char digit,
    uint16_t color) {
  const uint8_t mask = sevenSegmentMask(digit);
  auto bar = [&](uint8_t bit, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if ((mask & (1U << bit)) == 0) return;
    drawRoundedBar(x0, y0, x1, y1, 3, color);
    drawRoundedBar(x0, y0, x1, y1, 1, C_BLACK);
  };
  bar(0, x + 2, y + 1, x + 11, y + 1);
  bar(1, x + 12, y + 3, x + 12, y + 11);
  bar(2, x + 12, y + 15, x + 12, y + 23);
  bar(3, x + 2, y + 25, x + 11, y + 25);
  bar(4, x + 1, y + 15, x + 1, y + 23);
  bar(5, x + 1, y + 3, x + 1, y + 11);
  bar(6, x + 2, y + 13, x + 11, y + 13);
}

void drawOutlinedRoundedDigitalClock(
    int16_t x,
    int16_t y,
    const char *timeText,
    uint16_t color) {
  drawOutlinedRoundedDigitalDigit(x, y, timeText[0], color);
  drawOutlinedRoundedDigitalDigit(x + 17, y, timeText[1], color);
  dmaDisplay->drawCircle(x + 36, y + 8, 2, color);
  dmaDisplay->drawCircle(x + 36, y + 18, 2, color);
  drawOutlinedRoundedDigitalDigit(x + 43, y, timeText[3], color);
  drawOutlinedRoundedDigitalDigit(x + 60, y, timeText[4], color);
}

bool radialFillContains(
    int16_t x,
    int16_t y,
    uint16_t threshold,
    bool showFill) {
  if (!showFill) return false;
  const float dx = x - 63.5f;
  const float dy = y - 31.5f;
  float angle = atan2f(dx, -dy);
  if (angle < 0.0f) angle += 2.0f * PI;
  const uint16_t angleByte =
      static_cast<uint16_t>(roundf(angle * 255.0f / (2.0f * PI)));
  return angleByte < threshold;
}

void drawNegativeRoundedBar(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t lightColor,
    uint16_t threshold,
    bool showFill) {
  constexpr int8_t radius = 1;
  for (int16_t y = min(y0, y1) - radius;
       y <= max(y0, y1) + radius;
       ++y) {
    for (int16_t x = min(x0, x1) - radius;
         x <= max(x0, x1) + radius;
         ++x) {
      const int16_t nearestX = constrain(x, min(x0, x1), max(x0, x1));
      const int16_t nearestY = constrain(y, min(y0, y1), max(y0, y1));
      const int16_t sx = x - nearestX;
      const int16_t sy = y - nearestY;
      if (sx * sx + sy * sy <= radius * radius) {
        dmaDisplay->drawPixel(
            x,
            y,
            radialFillContains(x, y, threshold, showFill)
                ? C_BLACK
                : lightColor);
      }
    }
  }
}

void drawNegativeRoundedDigitalDigit(
    int16_t x,
    int16_t y,
    char digit,
    uint16_t lightColor,
    uint16_t threshold,
    bool showFill) {
  const uint8_t mask = sevenSegmentMask(digit);
  auto bar = [&](uint8_t bit, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if ((mask & (1U << bit)) != 0) {
      drawNegativeRoundedBar(
          x0, y0, x1, y1, lightColor, threshold, showFill);
    }
  };
  bar(0, x + 2, y + 1, x + 11, y + 1);
  bar(1, x + 12, y + 3, x + 12, y + 11);
  bar(2, x + 12, y + 15, x + 12, y + 23);
  bar(3, x + 2, y + 25, x + 11, y + 25);
  bar(4, x + 1, y + 15, x + 1, y + 23);
  bar(5, x + 1, y + 3, x + 1, y + 11);
  bar(6, x + 2, y + 13, x + 11, y + 13);
}

void drawNegativeRoundedDigitalClockCentered(
    int16_t centerX,
    int16_t centerY,
    const char *timeText,
    uint16_t lightColor,
    uint16_t threshold,
    bool showFill) {
  constexpr int16_t totalWidth = 74;
  const int16_t x = centerX - totalWidth / 2;
  const int16_t y = centerY - 13;
  drawNegativeRoundedDigitalDigit(
      x, y, timeText[0], lightColor, threshold, showFill);
  drawNegativeRoundedDigitalDigit(
      x + 17, y, timeText[1], lightColor, threshold, showFill);
  for (int8_t dot = -1; dot <= 1; dot += 2) {
    const int16_t cy = centerY + dot * 5;
    for (int8_t py = -1; py <= 1; ++py) {
      for (int8_t px = -1; px <= 1; ++px) {
        if (px * px + py * py > 1) continue;
        const int16_t dotX = x + 36 + px;
        const int16_t dotY = cy + py;
        dmaDisplay->drawPixel(
            dotX,
            dotY,
            radialFillContains(dotX, dotY, threshold, showFill)
                ? C_BLACK
                : lightColor);
      }
    }
  }
  drawNegativeRoundedDigitalDigit(
      x + 43, y, timeText[3], lightColor, threshold, showFill);
  drawNegativeRoundedDigitalDigit(
      x + 60, y, timeText[4], lightColor, threshold, showFill);
}

void drawScreensaverWeather(
    const SharedState &snapshot,
    const char *timeText) {
  const bool warm = activeVisualMode != VisualMode::DAY;
  const uint16_t clockColor =
      warm ? color565(62, 30, 6) : color565(44, 55, 60);
  drawRoundedDigitalClockCentered(
      PANEL_RES_X / 2,
      17,
      timeText,
      clockColor,
      clockColor);

  struct tm localTime;
  char dateLine[20] = "--- --/--";
  const time_t current = time(nullptr);
  if (localtime_r(&current, &localTime) != nullptr) {
    strftime(dateLine, sizeof(dateLine), "%a %d/%m", &localTime);
    for (char *cursor = dateLine; *cursor != '\0'; ++cursor) {
      if (*cursor >= 'a' && *cursor <= 'z') {
        *cursor = static_cast<char>(*cursor - 'a' + 'A');
      }
    }
  }
  printCentered(36, dateLine, warm ? color565(45, 20, 3) : C_DIM);

  char weatherLine[32];
  if (snapshot.weather.valid) {
    snprintf(
        weatherLine,
        sizeof(weatherLine),
        "%+.0fC  FEELS %+.0fC",
        snapshot.weather.temperatureC,
        snapshot.weather.feelsLikeC);
  } else {
    copyText(weatherLine, sizeof(weatherLine), "--C  FEELS --C");
  }
  printCenteredClipped(
      51,
      weatherLine,
      warm ? color565(54, 25, 5) : color565(28, 43, 52));
}

void drawScreensaverRadialFill(uint32_t nowMs, const char *timeText) {
  static bool mapReady = false;
  static uint8_t clockwiseAngle[PANEL_RES_Y][PANEL_RES_X];
  if (!mapReady) {
    for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
      for (int16_t x = 0; x < PANEL_RES_X; ++x) {
        const float dx = x - 63.5f;
        const float dy = y - 31.5f;
        float angle = atan2f(dx, -dy);
        if (angle < 0.0f) angle += 2.0f * PI;
        clockwiseAngle[y][x] = static_cast<uint8_t>(
            roundf(angle * 255.0f / (2.0f * PI)));
      }
    }
    mapReady = true;
  }

  const time_t wallClock = time(nullptr);
  struct tm localTime;
  localtime_r(&wallClock, &localTime);
  // One complete revolution represents the 60 minutes of the current hour.
  // The sector is stationary during each minute. At the minute boundary it
  // eases across only the newly earned angular distance.
  static int8_t displayedMinute = -1;
  static float displayedThreshold = 0.0f;
  static float transitionFrom = 0.0f;
  static float transitionTo = 0.0f;
  static uint32_t transitionStartedMs = 0;
  static uint32_t activeTransitionDurationMs = 900;
  static bool transitionActive = false;
  constexpr uint32_t transitionDurationMs = 900;
  if (displayedMinute < 0) {
    displayedMinute = localTime.tm_min;
    displayedThreshold =
        localTime.tm_min == 0
            ? 0.0f
            : localTime.tm_min * (256.0f / 60.0f);
    transitionFrom = displayedThreshold;
    transitionTo = displayedThreshold;
  } else if (
      localTime.tm_min == 59 &&
      localTime.tm_sec == 59 &&
      displayedMinute == 59) {
    // Start the hour reset during 59:59 and finish exactly at 00:00. If this
    // frame arrives part-way through the second, use only the remaining wall
    // time instead of letting the clear spill into the new hour.
    struct timeval wallTime;
    gettimeofday(&wallTime, nullptr);
    const uint32_t remainingMs =
        max(1UL, 1000UL - wallTime.tv_usec / 1000UL);
    displayedMinute = 0;
    transitionFrom = displayedThreshold;
    transitionTo = 0.0f;
    transitionStartedMs = nowMs;
    activeTransitionDurationMs = remainingMs;
    transitionActive = true;
  } else if (displayedMinute != localTime.tm_min) {
    displayedMinute = localTime.tm_min;
    transitionFrom = displayedThreshold;
    transitionTo =
        localTime.tm_min == 0
            ? 0.0f
            : localTime.tm_min * (256.0f / 60.0f);
    transitionStartedMs = nowMs;
    activeTransitionDurationMs = transitionDurationMs;
    transitionActive = true;
  }
  if (transitionActive) {
    const uint32_t ageMs = nowMs - transitionStartedMs;
    const float progress = constrain(
        ageMs / static_cast<float>(activeTransitionDurationMs),
        0.0f,
        1.0f);
    // Cosine easing has zero velocity at both ends and avoids the visible
    // backward snap of the previous recoil animation.
    const float eased =
        0.5f - 0.5f * cosf(progress * PI);
    displayedThreshold =
        transitionFrom + (transitionTo - transitionFrom) * eased;
    if (progress >= 1.0f) {
      transitionActive = false;
      transitionFrom = transitionTo;
      displayedThreshold = transitionTo;
    }
  }
  const uint16_t threshold =
      static_cast<uint16_t>(roundf(displayedThreshold));
  const bool showFill = threshold > 0;
  const uint16_t accent =
      activeVisualMode == VisualMode::DAY
          ? color565(2, 32, 48)
          : color565(46, 17, 1);
  for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
    for (int16_t x = 0; x < PANEL_RES_X; ++x) {
      if (showFill && clockwiseAngle[y][x] < threshold) {
        dmaDisplay->drawPixel(x, y, accent);
      }
    }
  }

  const uint16_t clockColor =
      activeVisualMode == VisualMode::DAY
          ? color565(54, 57, 58)
          : color565(62, 35, 12);
  // Each clock pixel is inverted independently as the minute sector reaches
  // it: pale on black, black on the accent fill.
  drawNegativeRoundedDigitalClockCentered(
      PANEL_RES_X / 2,
      PANEL_RES_Y / 2,
      timeText,
      clockColor,
      threshold,
      showFill);
}

void fillRectClippedY(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t clipTop,
    int16_t clipBottom,
  uint16_t color) {
  const int16_t top = max(y, clipTop);
  const int16_t rectBottom = y + height;
  const int16_t bottom =
      rectBottom < clipBottom ? rectBottom : clipBottom;
  if (bottom > top) {
    dmaDisplay->fillRect(x, top, width, bottom - top, color);
  }
}

void drawSegmentDigitClipped(
    int16_t x,
    int16_t y,
    uint8_t scale,
    char digit,
    uint16_t color,
    int16_t clipTop,
    int16_t clipBottom) {
  const uint8_t mask = sevenSegmentMask(digit);
  const int16_t width = scale * 4;
  const int16_t height = scale * 7;
  const int16_t thickness = scale;
  auto segment = [&](uint8_t bit, int16_t sx, int16_t sy, int16_t sw, int16_t sh) {
    if ((mask & (1U << bit)) != 0) {
      fillRectClippedY(
          sx, sy, sw, sh, clipTop, clipBottom, color);
    }
  };
  segment(0, x + thickness, y, width - 2 * thickness, thickness);
  segment(
      1, x + width - thickness, y + thickness,
      thickness, height / 2 - thickness);
  segment(
      2, x + width - thickness, y + height / 2,
      thickness, height / 2 - thickness);
  segment(
      3, x + thickness, y + height - thickness,
      width - 2 * thickness, thickness);
  segment(
      4, x, y + height / 2,
      thickness, height / 2 - thickness);
  segment(
      5, x, y + thickness,
      thickness, height / 2 - thickness);
  segment(
      6, x + thickness, y + height / 2 - thickness / 2,
      width - 2 * thickness, thickness);
}

void drawScreensaverFlipClock(uint32_t nowMs, const char *timeText) {
  constexpr int16_t cardX[4] = {3, 34, 68, 99};
  const uint16_t cardTop =
      activeVisualMode == VisualMode::DAY
          ? color565(8, 25, 31)
          : color565(31, 12, 2);
  const uint16_t cardBottom =
      activeVisualMode == VisualMode::DAY
          ? color565(4, 14, 19)
          : color565(18, 6, 0);
  const uint16_t cardEdge =
      activeVisualMode == VisualMode::DAY
          ? color565(12, 31, 36)
          : color565(39, 16, 3);
  const uint16_t digit =
      activeVisualMode == VisualMode::DAY
          ? color565(45, 55, 58)
          : color565(62, 31, 7);
  // Card faces keep their depth shading, but the glyph itself has one
  // brightness across the fold.
  const uint16_t digitBottom = digit;
  const char currentDigits[4] = {
      timeText[0], timeText[1], timeText[3], timeText[4]};
  static char displayedDigits[4] = {'\0', '\0', '\0', '\0'};
  static char previousDigits[4] = {'0', '0', '0', '0'};
  static uint8_t changingMask = 0;
  static uint32_t changeStartedMs = 0;

  if (displayedDigits[0] == '\0') {
    memcpy(displayedDigits, currentDigits, sizeof(displayedDigits));
    memcpy(previousDigits, currentDigits, sizeof(previousDigits));
  } else if (
      memcmp(displayedDigits, currentDigits, sizeof(displayedDigits)) != 0) {
    changingMask = 0;
    memcpy(previousDigits, displayedDigits, sizeof(previousDigits));
    for (uint8_t i = 0; i < 4; ++i) {
      if (displayedDigits[i] != currentDigits[i]) {
        changingMask |= 1U << i;
      }
    }
    memcpy(displayedDigits, currentDigits, sizeof(displayedDigits));
    changeStartedMs = nowMs;
  }

  constexpr uint32_t flipDurationMs = 760;
  const uint32_t flipAgeMs = nowMs - changeStartedMs;
  const bool flipping =
      changingMask != 0 && flipAgeMs < flipDurationMs;
  const float flipPhase =
      flipping ? flipAgeMs / static_cast<float>(flipDurationMs) : 1.0f;
  const uint16_t foldShade =
      activeVisualMode == VisualMode::DAY
          ? color565(2, 8, 11)
          : color565(12, 4, 0);

  for (uint8_t i = 0; i < 4; ++i) {
    dmaDisplay->fillRoundRect(cardX[i], 8, 26, 48, 3, cardEdge);
    dmaDisplay->fillRect(cardX[i] + 1, 9, 24, 23, cardTop);
    dmaDisplay->fillRect(cardX[i] + 1, 33, 24, 22, cardBottom);
    dmaDisplay->drawFastHLine(
        cardX[i] + 3, 9, 20, color565(18, 36, 40));
    const bool digitFlipping =
        flipping && (changingMask & (1U << i)) != 0;

    if (!digitFlipping) {
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, displayedDigits[i],
          digit, 18, 32);
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, displayedDigits[i],
          digitBottom, 33, 47);
    } else if (flipPhase < 0.5f) {
      const int16_t collapse =
          static_cast<int16_t>(roundf(flipPhase * 2.0f * 13.0f));
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, previousDigits[i],
          digitBottom, 33, 47);
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, previousDigits[i],
          digit, 18 + collapse, 32);
      const int16_t inset = collapse / 3;
      dmaDisplay->drawFastHLine(
          cardX[i] + inset,
          31,
          26 - inset * 2,
          foldShade);
    } else {
      const int16_t reveal =
          static_cast<int16_t>(
              roundf((flipPhase - 0.5f) * 2.0f * 14.0f));
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, displayedDigits[i],
          digit, 18, 32);
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, previousDigits[i],
          digitBottom, 33, 47);
      dmaDisplay->fillRect(
          cardX[i] + 1, 33, 24, reveal, cardBottom);
      drawSegmentDigitClipped(
          cardX[i] + 5, 18, 4, displayedDigits[i],
          digitBottom, 33, 33 + reveal);
      if (reveal < 14) {
        dmaDisplay->drawFastHLine(
            cardX[i] + reveal / 4,
            33 + reveal,
            26 - (reveal / 4) * 2,
            foldShade);
      }
    }
    dmaDisplay->drawFastHLine(cardX[i], 32, 26, C_BLACK);
    dmaDisplay->fillCircle(cardX[i] + 2, 32, 1, cardEdge);
    dmaDisplay->fillCircle(cardX[i] + 23, 32, 1, cardEdge);
  }
  if (!flipping) changingMask = 0;
  const float pulse =
      0.5f - 0.5f * cosf((nowMs % 1000UL) * (2.0f * PI / 1000.0f));
  const uint8_t colon =
      static_cast<uint8_t>(roundf(25.0f + pulse * 28.0f));
  dmaDisplay->fillCircle(63, 25, 1, color565(colon, colon, colon));
  dmaDisplay->fillCircle(63, 40, 1, color565(colon, colon, colon));
}

void drawThickLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color,
    uint8_t thickness) {
  for (int8_t offset = -static_cast<int8_t>(thickness / 2);
       offset <= static_cast<int8_t>(thickness / 2);
       ++offset) {
    dmaDisplay->drawLine(x0 + offset, y0, x1 + offset, y1, color);
    dmaDisplay->drawLine(x0, y0 + offset, x1, y1 + offset, color);
  }
}

void drawTopViewFlybyAircraft(
    float x,
    float y,
    float angle,
    uint16_t color) {
  const float forwardX = cosf(angle);
  const float forwardY = sinf(angle);
  const float sideX = -forwardY;
  const float sideY = forwardX;
  const int16_t noseX = roundf(x + forwardX * 7.0f);
  const int16_t noseY = roundf(y + forwardY * 7.0f);
  const int16_t tailX = roundf(x - forwardX * 6.0f);
  const int16_t tailY = roundf(y - forwardY * 6.0f);
  drawThickLine(tailX, tailY, noseX, noseY, color, 3);
  drawThickLine(
      roundf(x - sideX * 7.0f - forwardX),
      roundf(y - sideY * 7.0f - forwardY),
      roundf(x + sideX * 7.0f - forwardX),
      roundf(y + sideY * 7.0f - forwardY),
      color,
      3);
  drawThickLine(
      roundf(x - sideX * 3.0f - forwardX * 5.0f),
      roundf(y - sideY * 3.0f - forwardY * 5.0f),
      roundf(x + sideX * 3.0f - forwardX * 5.0f),
      roundf(y + sideY * 3.0f - forwardY * 5.0f),
      color,
      2);
  dmaDisplay->fillCircle(noseX, noseY, 1, color);
}

void drawBootIntroOverlay(uint32_t nowMs) {
  const uint32_t ageMs = nowMs - bootIntroStartedMs;
  const uint16_t canvasColor =
      activeVisualMode == VisualMode::DAY
          ? color565(2, 34, 47)
          : color565(51, 19, 1);
  constexpr uint32_t cutDurationMs = 920;
  constexpr uint8_t bayer4x4[16] = {
      0, 8, 2, 10,
      12, 4, 14, 6,
      3, 11, 1, 9,
      15, 7, 13, 5};

  if (ageMs < cutDurationMs) {
    const float progress =
        ageMs / static_cast<float>(cutDurationMs);
    const float eased =
        progress * progress * (3.0f - 2.0f * progress);
    const float aircraftX = -18.0f + eased * 164.0f;
    for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
      for (int16_t x = 0; x < PANEL_RES_X; ++x) {
        const float cutY = 31.5f + (x - 63.5f) * 0.055f;
        const bool openedBehindAircraft =
            x < aircraftX - 5.0f &&
            fabsf(y - cutY) <= 1.4f;
        if (!openedBehindAircraft) {
          dmaDisplay->drawPixel(x, y, canvasColor);
        }
      }
    }
    drawTopViewFlybyAircraft(
        aircraftX,
        31.5f,
        0.0f,
        activeVisualMode == VisualMode::DAY
            ? color565(58, 58, 58)
            : color565(62, 31, 5));
    return;
  }

  const float openProgress = constrain(
      (ageMs - cutDurationMs) /
          static_cast<float>(BOOT_INTRO_DURATION_MS - cutDurationMs),
      0.0f,
      1.0f);
  const float eased =
      openProgress * openProgress * (3.0f - 2.0f * openProgress);
  const float opening = 1.0f + eased * 38.0f;
  for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
    for (int16_t x = 0; x < PANEL_RES_X; ++x) {
      const float cutY = 31.5f + (x - 63.5f) * 0.055f;
      const float distance = fabsf(y - cutY);
      if (distance <= opening) continue;
      const float feather = constrain(
          (distance - opening) / 4.0f,
          0.0f,
          1.0f);
      const uint8_t threshold =
          static_cast<uint8_t>(roundf(feather * 15.0f));
      if (bayer4x4[(y & 3) * 4 + (x & 3)] <= threshold) {
        dmaDisplay->drawPixel(x, y, canvasColor);
      }
    }
  }
}

void drawScreensaverAnalogClock(uint32_t nowMs) {
  struct tm localTime;
  const time_t current = time(nullptr);
  if (localtime_r(&current, &localTime) == nullptr) return;

  constexpr int16_t cx = 64;
  constexpr int16_t cy = 32;
  constexpr int16_t radius = 29;
  const bool warm = activeVisualMode != VisualMode::DAY;
  const uint16_t tickColor =
      warm ? color565(38, 16, 2) : color565(4, 26, 31);
  const uint16_t handColor =
      warm ? color565(62, 31, 6) : color565(7, 51, 58);

  for (uint8_t tick = 0; tick < 60; ++tick) {
    const float angle = tick * (2.0f * PI / 60.0f) - PI / 2.0f;
    const int16_t inner = tick % 5 == 0 ? radius - 5 : radius - 2;
    dmaDisplay->drawLine(
        cx + roundf(cosf(angle) * inner),
        cy + roundf(sinf(angle) * inner),
        cx + roundf(cosf(angle) * radius),
        cy + roundf(sinf(angle) * radius),
        tickColor);
  }

  const float secondAngle =
      localTime.tm_sec * (2.0f * PI / 60.0f) - PI / 2.0f;
  const float minuteAngle =
      localTime.tm_min * (2.0f * PI / 60.0f) - PI / 2.0f;
  const float hourAngle =
      // The hour hand advances once per completed minute instead of drifting
      // continuously with the seconds hand.
      ((localTime.tm_hour % 12) + localTime.tm_min / 60.0f) *
          (2.0f * PI / 12.0f) -
      PI / 2.0f;
  drawThickLine(
      cx,
      cy,
      cx + roundf(cosf(hourAngle) * 15.0f),
      cy + roundf(sinf(hourAngle) * 15.0f),
      handColor,
      3);
  drawThickLine(
      cx,
      cy,
      cx + roundf(cosf(minuteAngle) * 23.0f),
      cy + roundf(sinf(minuteAngle) * 23.0f),
      handColor,
      2);
  dmaDisplay->drawLine(
      cx,
      cy,
      cx + roundf(cosf(secondAngle) * 25.0f),
      cy + roundf(sinf(secondAngle) * 25.0f),
      warm ? color565(46, 8, 1) : color565(52, 7, 4));
  dmaDisplay->fillCircle(cx, cy, 2, handColor);

  static int16_t lastMidnightDay = -1;
  static uint32_t midnightFlybyStartedMs = 0;
  static float midnightFlybyAngle = 0.0f;
  if (localTime.tm_hour == 0 &&
      localTime.tm_min == 0 &&
      localTime.tm_sec == 0 &&
      lastMidnightDay != localTime.tm_yday) {
    lastMidnightDay = localTime.tm_yday;
    midnightFlybyStartedMs = nowMs;
    midnightFlybyAngle =
        random(0, 16) * (2.0f * PI / 16.0f);
  }
  const uint32_t flybyAgeMs = nowMs - midnightFlybyStartedMs;
  if (midnightFlybyStartedMs != 0 && flybyAgeMs < 1800) {
    const float progress = flybyAgeMs / 1800.0f;
    const float distance = -92.0f + progress * 184.0f;
    const float aircraftX =
        63.5f + cosf(midnightFlybyAngle) * distance;
    const float aircraftY =
        31.5f + sinf(midnightFlybyAngle) * distance;
    drawTopViewFlybyAircraft(
        aircraftX,
        aircraftY,
        midnightFlybyAngle,
        warm ? color565(62, 28, 3) : color565(34, 55, 58));
  }
}

void drawScreensaverFlightThroughStars(uint32_t nowMs) {
  struct Star {
    float x;
    float y;
    float z;
  };
  constexpr uint8_t starCount = 40;
  static Star stars[starCount];
  static bool initialized = false;
  static uint32_t previousMs = 0;

  auto resetStar = [](Star &star, bool initial) {
    star.x = random(-1000, 1001) / 1000.0f;
    star.y = random(-520, 521) / 1000.0f;
    star.z =
        initial
            ? random(35, 301) / 100.0f
            : random(220, 321) / 100.0f;
  };
  if (!initialized) {
    for (Star &star : stars) resetStar(star, true);
    previousMs = nowMs;
    initialized = true;
  }

  const float dt = min(0.08f, (nowMs - previousMs) / 1000.0f);
  previousMs = nowMs;
  constexpr float speed = 0.34f;
  for (Star &star : stars) {
    star.z -= speed * dt;
    const float previousZ = star.z + speed * dt * 2.4f;
    if (star.z < 0.18f) {
      resetStar(star, false);
      continue;
    }

    const int16_t x =
        static_cast<int16_t>(roundf(63.5f + star.x * 54.0f / star.z));
    const int16_t y =
        static_cast<int16_t>(roundf(29.0f + star.y * 54.0f / star.z));
    const int16_t previousX =
        static_cast<int16_t>(
            roundf(63.5f + star.x * 54.0f / previousZ));
    const int16_t previousY =
        static_cast<int16_t>(
            roundf(29.0f + star.y * 54.0f / previousZ));
    if (x < 0 || x >= PANEL_RES_X || y < 0 || y >= PANEL_RES_Y) {
      resetStar(star, false);
      continue;
    }

    const float closeness = constrain(1.15f - star.z / 3.0f, 0.12f, 1.0f);
    const uint8_t level =
        static_cast<uint8_t>(roundf(12.0f + closeness * 45.0f));
    const uint16_t starColor =
        activeVisualMode == VisualMode::DAY
            ? color565(level, level, level)
            : color565(level, level / 3U, 1);
    dmaDisplay->drawLine(previousX, previousY, x, y, starColor);
  }

  // A stable head-on aircraft anchors the motion and makes the effect read as
  // forward flight rather than a generic star field.
  const float airResistanceDrift =
      sinf(nowMs * 0.00043f) * 2.2f +
      sinf(nowMs * 0.00117f + 1.4f) * 0.65f;
  const float gentleLift =
      sinf(nowMs * 0.00061f + 0.8f) * 1.15f;
  const float aircraftX = 48.5f + airResistanceDrift;
  const float aircraftY = 39.5f + gentleLift;
  drawSubpixelAircraft(
      aircraftX,
      aircraftY,
      activeVisualMode == VisualMode::DAY ? 48 : 58,
      activeVisualMode == VisualMode::DAY ? 48 : 25,
      activeVisualMode == VisualMode::DAY ? 52 : 3,
      false,
      false);

  static int32_t lastBombMinute = -1;
  static uint32_t bombStartedMs = 0;
  const time_t wallClock = time(nullptr);
  struct tm localTime;
  if (wallClock > 100000 &&
      localtime_r(&wallClock, &localTime) != nullptr) {
    const int32_t minuteKey =
        localTime.tm_yday * 24 * 60 +
        localTime.tm_hour * 60 +
        localTime.tm_min;
    if (localTime.tm_hour == 16 &&
        localTime.tm_min == 20 &&
        minuteKey != lastBombMinute) {
      lastBombMinute = minuteKey;
      bombStartedMs = nowMs;
    }
  }
  const uint32_t bombAgeMs = nowMs - bombStartedMs;
  if (bombStartedMs != 0 && bombAgeMs < 3200) {
    const float seconds = bombAgeMs / 1000.0f;
    const int16_t bombX = static_cast<int16_t>(
        roundf(aircraftX + 15.0f + seconds * 1.2f));
    const int16_t bombY = static_cast<int16_t>(
        roundf(aircraftY + 10.0f + seconds * 3.0f +
               seconds * seconds * 5.0f));
    const uint16_t bombColor =
        activeVisualMode == VisualMode::DAY
            ? color565(42, 38, 31)
            : color565(52, 22, 2);
    dmaDisplay->drawPixel(bombX, bombY - 2, bombColor);
    dmaDisplay->fillCircle(bombX, bombY, 1, bombColor);
  }
}

void drawScreensaverGlyphField(uint32_t nowMs, const char *timeText) {
  constexpr uint8_t columnCount = 32;
  constexpr uint8_t rowCount = 10;
  constexpr char glyphs[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "!@#$%&*+-=?<>/\\";
  static char matrix[rowCount][columnCount];
  static float highlightHead[columnCount];
  static float highlightSpeed[columnCount];
  static uint8_t highlightTrail[columnCount];
  static bool initialized = false;
  static uint32_t previousMs = 0;
  if (!initialized) {
    // Build a fixed fine character matrix. Every column owns an independent
    // falling light head; glyph positions and values never move.
    uint32_t seed = 0x51F15EEDUL;
    for (uint8_t column = 0; column < columnCount; ++column) {
      for (uint8_t row = 0; row < rowCount; ++row) {
        seed = seed * 1664525UL + 1013904223UL;
        matrix[row][column] =
            glyphs[(seed >> 16) % (sizeof(glyphs) - 1)];
      }
      highlightHead[column] = random(-60, 101) / 10.0f;
      highlightSpeed[column] = random(45, 125) / 100.0f;
      highlightTrail[column] = random(3, 7);
    }
    matrix[4][12] = 'N';
    matrix[4][13] = 'i';
    matrix[4][14] = 'c';
    matrix[4][15] = 'e';
    previousMs = nowMs;
    initialized = true;
  }

  const float dt = min(0.10f, (nowMs - previousMs) / 1000.0f);
  previousMs = nowMs;
  dmaDisplay->setFont(&TomThumb);
  for (uint8_t column = 0; column < columnCount; ++column) {
    highlightHead[column] += highlightSpeed[column] * dt;
    if (highlightHead[column] - highlightTrail[column] > rowCount) {
      highlightHead[column] =
          -static_cast<float>(random(1, 8));
      highlightSpeed[column] = random(45, 125) / 100.0f;
      highlightTrail[column] = random(3, 7);
    }
    for (uint8_t row = 0; row < rowCount; ++row) {
      const float behind = highlightHead[column] - row;
      float glow = 0.0f;
      if (behind >= -0.35f &&
          behind <= highlightTrail[column]) {
        const float normalized =
            constrain(
                behind / max(1.0f, static_cast<float>(highlightTrail[column])),
                0.0f,
                1.0f);
        glow = powf(1.0f - normalized, 1.35f);
      }
      const uint8_t level =
          static_cast<uint8_t>(roundf(3.0f + glow * 54.0f));
      const bool leadingGlyph =
          behind >= -0.45f && behind < 0.65f;
      const uint16_t color =
          leadingGlyph
              ? (activeVisualMode == VisualMode::DAY
                     ? color565(0, level, level)
                     : color565(level, level * 2U / 3U, level / 6U))
              : (activeVisualMode == VisualMode::DAY
                     ? color565(level, 0, level)
                     : color565(level, level / 3U, 0));
      dmaDisplay->setCursor(column * 4 + 1, row * 6 + 5);
      dmaDisplay->setTextColor(color);
      dmaDisplay->print(matrix[row][column]);
    }
  }
  dmaDisplay->setFont();

  const uint16_t clockColor =
      activeVisualMode == VisualMode::DAY
          ? color565(0, 58, 61)
          : color565(62, 29, 5);
  drawOutlinedRoundedDigitalClock(53, 37, timeText, clockColor);
}

void drawScreensaver(
    const SharedState &snapshot,
    const char *timeText,
    uint32_t nowMs) {
  dmaDisplay->fillScreen(C_BLACK);
  switch (snapshot.peripheral.screensaverIndex % SCREENSAVER_COUNT) {
    case 0:
      drawScreensaverRadar(nowMs, timeText);
      break;
    case 1:
      drawScreensaverWave(nowMs, timeText);
      break;
    case 2:
      drawScreensaverFire(nowMs, timeText);
      break;
    case 3:
      drawScreensaverLavaLamp(nowMs, timeText);
      break;
    case 4:
      drawScreensaverRings(nowMs);
      break;
    case 5:
      drawScreensaverAircraftBounce(nowMs);
      break;
    case 6:
      drawScreensaverWeather(snapshot, timeText);
      break;
    case 7:
      drawScreensaverRadialFill(nowMs, timeText);
      break;
    case 8:
      drawScreensaverFlipClock(nowMs, timeText);
      break;
    case 9:
      drawScreensaverAnalogClock(nowMs);
      break;
    case 10:
      drawScreensaverFlightThroughStars(nowMs);
      break;
    case 11:
    default:
      drawScreensaverGlyphField(nowMs, timeText);
      break;
  }
  drawStatusBar(snapshot.status);
}

void drawScreensaverToAircraftTransition(
    const SharedState &snapshot,
    const char *timeText,
    uint32_t nowMs,
    uint32_t startedMs) {
  const uint32_t ageMs = nowMs - startedMs;
  const float heading =
      snapshot.flight.headingDeg >= 0
          ? snapshot.flight.headingDeg * (PI / 180.0f) - PI / 2.0f
          : 0.0f;
  const float forwardX = cosf(heading);
  const float forwardY = sinf(heading);
  const float sideX = -forwardY;
  const float sideY = forwardX;
  const uint16_t aircraftColor =
      activeVisualMode == VisualMode::DAY
          ? color565(62, 62, 62)
          : color565(63, 30, 4);
  const uint16_t cutColor =
      activeVisualMode == VisualMode::DAY
          ? color565(0, 38, 48)
          : color565(50, 18, 1);

  if (ageMs < AIRCRAFT_TRANSITION_FLYBY_MS) {
    // The actual flight heading drives the fly-by, so the transition also
    // communicates the aircraft's direction before the data card appears.
    drawScreensaver(snapshot, timeText, nowMs);
    const float progress =
        ageMs / static_cast<float>(AIRCRAFT_TRANSITION_FLYBY_MS);
    const float eased =
        progress * progress * (3.0f - 2.0f * progress);
    const float travel = -94.0f + eased * 104.0f;
    const float aircraftX = 63.5f + forwardX * travel;
    const float aircraftY = 31.5f + forwardY * travel;
    dmaDisplay->drawLine(
        roundf(63.5f - forwardX * 92.0f),
        roundf(31.5f - forwardY * 92.0f),
        roundf(aircraftX - forwardX * 9.0f),
        roundf(aircraftY - forwardY * 9.0f),
        cutColor);
    drawTopViewFlybyAircraft(
        aircraftX,
        aircraftY,
        heading,
        aircraftColor);
    return;
  }

  // The fly-by cuts a heading-aligned slit through the old saver. The live
  // flight card is already rendered below it and is revealed by a soft,
  // dithered opening rather than an abrupt full-screen switch.
  drawAircraft(snapshot);
  const float progress = constrain(
      (ageMs - AIRCRAFT_TRANSITION_FLYBY_MS) /
          static_cast<float>(
              AIRCRAFT_TRANSITION_DURATION_MS -
              AIRCRAFT_TRANSITION_FLYBY_MS),
      0.0f,
      1.0f);
  const float eased =
      progress * progress * (3.0f - 2.0f * progress);
  const float opening = 0.7f + eased * 78.0f;
  constexpr uint8_t bayer4x4[16] = {
      0, 8, 2, 10,
      12, 4, 14, 6,
      3, 11, 1, 9,
      15, 7, 13, 5};
  for (int16_t y = 0; y < PANEL_RES_Y; ++y) {
    for (int16_t x = 0; x < PANEL_RES_X; ++x) {
      const float perpendicular = fabsf(
          (x - 63.5f) * sideX + (y - 31.5f) * sideY);
      if (perpendicular <= opening) continue;
      const float blackCoverage = constrain(
          (perpendicular - opening) / 3.5f,
          0.0f,
          1.0f);
      const uint8_t threshold =
          static_cast<uint8_t>(roundf(blackCoverage * 15.0f));
      if (bayer4x4[(y & 3) * 4 + (x & 3)] <= threshold) {
        dmaDisplay->drawPixel(x, y, C_BLACK);
      }
    }
  }

  const float travel = 10.0f + min(1.0f, progress * 1.55f) * 94.0f;
  drawTopViewFlybyAircraft(
      63.5f + forwardX * travel,
      31.5f + forwardY * travel,
      heading,
      aircraftColor);
}

// =========================
// Render signatures
// =========================
bool isFresh(uint32_t receivedMs, uint32_t ttlMs, uint32_t nowMs) {
  return receivedMs != 0 && nowMs - receivedMs <= ttlMs;
}

AppState selectAppState(const SharedState &snapshot, uint32_t nowMs) {
  const bool alertUsable =
      snapshot.alert.fresh && snapshot.alert.active &&
      isFresh(snapshot.alert.receivedMs, ALERT_TTL_MS, nowMs);
  const bool flightUsable =
      snapshot.flight.fresh && snapshot.flight.active &&
      isFresh(snapshot.flight.receivedMs, FLIGHT_TTL_MS, nowMs);
  const bool hardwareTestActive =
      snapshot.peripheral.hardwareTestUntilMs != 0 &&
      static_cast<int32_t>(
          snapshot.peripheral.hardwareTestUntilMs - nowMs) > 0;

  // Safety priority is unconditional, including while a diagnostic saver is
  // selected from Serial: ALERT > live AIRCRAFT > every UI page.
  if (alertUsable) return AppState::ALERT;
  if (flightUsable) return AppState::AIRCRAFT;

  switch (snapshot.peripheral.screenOverride) {
    case ScreenOverride::IDLE:
      return AppState::IDLE;
    case ScreenOverride::LAST_AIRCRAFT:
      return AppState::LAST_AIRCRAFT;
    case ScreenOverride::AIRCRAFT:
      return AppState::AIRCRAFT;
    case ScreenOverride::HARDWARE_TEST:
      return AppState::HARDWARE_TEST;
    case ScreenOverride::SCREENSAVER:
      return AppState::SCREENSAVER;
    case ScreenOverride::ALERT:
      return AppState::ALERT;
    case ScreenOverride::AUTO:
    default:
      break;
  }

  if (hardwareTestActive) return AppState::HARDWARE_TEST;
  if (snapshot.peripheral.screensaverActive) return AppState::SCREENSAVER;
  if (snapshot.peripheral.lastAircraftView) return AppState::LAST_AIRCRAFT;
  return AppState::IDLE;
}

void buildClockText(char *timeText, size_t timeSize, char *dateText, size_t dateSize) {
  const time_t now = time(nullptr);
  struct tm localNow;
  if (now > 100000 && localtime_r(&now, &localNow) != nullptr) {
    strftime(timeText, timeSize, "%H:%M", &localNow);
    strftime(dateText, dateSize, "%d/%m", &localNow);
  } else {
    copyText(timeText, timeSize, "--:--");
    copyText(dateText, dateSize, "--/--");
  }
}

VisualMode determineVisualMode(
    const SharedState &snapshot,
    uint32_t nowMs) {
  static bool sleepLatched = false;
  static uint32_t lowLuxSinceMs = 0;
  static uint32_t brightSinceMs = 0;

  uint16_t currentMinute = 12 * 60;
  const time_t now = time(nullptr);
  struct tm localNow;
  if (now > 100000 && localtime_r(&now, &localNow) != nullptr) {
    currentMinute = localNow.tm_hour * 60 + localNow.tm_min;
  }
  const uint16_t sunrise = snapshot.solar.sunriseMin;
  const uint16_t sunset = snapshot.solar.sunsetMin;
  bool night =
      currentMinute < sunrise || currentMinute >= sunset;
  if (snapshot.peripheral.nightOverride == NightOverride::FORCE_DAY) {
    night = false;
  } else if (
      snapshot.peripheral.nightOverride == NightOverride::FORCE_NIGHT) {
    night = true;
  }

  if (!night) {
    sleepLatched = false;
    lowLuxSinceMs = 0;
    brightSinceMs = 0;
    return VisualMode::DAY;
  }

  const float lux = snapshot.peripheral.ambientLux;
  if (snapshot.peripheral.lightOk && lux >= 0.0f) {
    if (lux <= SLEEP_ENTER_LUX) {
      brightSinceMs = 0;
      if (lowLuxSinceMs == 0) lowLuxSinceMs = nowMs;
      if (!sleepLatched && nowMs - lowLuxSinceMs >= SLEEP_ENTER_MS) {
        sleepLatched = true;
      }
    } else if (lux >= SLEEP_EXIT_LUX) {
      lowLuxSinceMs = 0;
      if (brightSinceMs == 0) brightSinceMs = nowMs;
      if (sleepLatched && nowMs - brightSinceMs >= SLEEP_EXIT_MS) {
        sleepLatched = false;
      }
    } else {
      lowLuxSinceMs = 0;
      brightSinceMs = 0;
    }
  }
  return sleepLatched ? VisualMode::SLEEP : VisualMode::NIGHT;
}

void hashBytes(uint32_t &hash, const void *data, size_t size) {
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 16777619UL;
  }
}

void hashText(uint32_t &hash, const char *text) {
  hashBytes(hash, text, strlen(text) + 1);
}

template <typename T>
void hashValue(uint32_t &hash, const T &value) {
  hashBytes(hash, &value, sizeof(value));
}

uint32_t renderIntervalMs(
    AppState appState,
    const SharedState &snapshot) {
  if (appState == AppState::IDLE) return IDLE_BREATH_FRAME_MS;
  if (appState != AppState::SCREENSAVER) return 100;

  switch (snapshot.peripheral.screensaverIndex % SCREENSAVER_COUNT) {
    case 4:
      return 20;  // Rings retain extra temporal resolution.
    case 3:
      return 33;  // Slow metaballs do not need a full 40 fps.
    case 6:
      return 1000;  // Weather/date clock is static between updates.
    case 7:
      // Burst to 40 fps around the minute boundary and during the one-second
      // 59:59 -> 00:00 hour reset; remain static otherwise.
      return time(nullptr) % 60 <= 1 ||
                     time(nullptr) % 60 >= 59
                 ? 25
                 : 1000;
    case 8:
      return 40;  // Flip cards only need a subtle colon animation.
    case 11:
      return 50;  // Sparse glyph rain is intentionally calm.
    default:
      return SCREENSAVER_FRAME_MS;
  }
}

uint32_t renderWaitMs(
    uint32_t intervalMs,
    bool animated,
    uint32_t nowMs) {
  if (!animated) return intervalMs;
  return max(1UL, intervalMs - (nowMs % intervalMs));
}

uint32_t renderSignature(
    const SharedState &snapshot,
    AppState appState,
    VisualMode visualMode,
    const char *timeText,
    const char *dateText,
    uint32_t nowMs) {
  uint32_t hash = 2166136261UL;
  hashValue(hash, appState);
  hashValue(hash, visualMode);
  hashValue(hash, snapshot.status.wifiOk);
  hashValue(hash, snapshot.status.timeOk);
  hashValue(hash, snapshot.status.flightOk);
  hashValue(hash, snapshot.status.alertOk);
  hashValue(hash, snapshot.status.historyOk);
  hashValue(hash, snapshot.status.weatherOk);
  hashValue(hash, snapshot.status.wifiFailures);
  hashValue(hash, snapshot.status.timeFailures);
  hashValue(hash, snapshot.status.flightFailures);
  hashValue(hash, snapshot.status.alertFailures);
  hashValue(hash, snapshot.status.historyFailures);
  hashValue(hash, snapshot.status.weatherFailures);
  hashValue(hash, snapshot.peripheral.panelEnabled);
  hashValue(hash, snapshot.peripheral.screenOverride);

  switch (appState) {
    case AppState::ALERT:
      hashText(hash, snapshot.alert.title);
      hashText(hash, snapshot.alert.category);
      hashText(hash, snapshot.alert.firstArea);
      hashValue(hash, snapshot.alert.areaCount);
      break;

    case AppState::AIRCRAFT:
      hashValue(hash, snapshot.flight.count);
      hashText(hash, snapshot.flight.callsign);
      hashText(hash, snapshot.flight.aircraft);
      hashText(hash, snapshot.flight.origin);
      hashText(hash, snapshot.flight.destination);
      hashValue(hash, snapshot.flight.altitudeFt);
      hashValue(hash, snapshot.flight.speedKts);
      hashValue(hash, snapshot.flight.headingDeg);
      break;

    case AppState::LAST_AIRCRAFT:
      hashValue(hash, snapshot.lastAircraft.active);
      hashText(hash, snapshot.lastAircraft.callsign);
      hashText(hash, snapshot.lastAircraft.aircraft);
      hashText(hash, snapshot.lastAircraft.origin);
      hashText(hash, snapshot.lastAircraft.destination);
      hashValue(hash, snapshot.lastAircraft.altitudeFt);
      hashValue(hash, snapshot.lastAircraft.speedKts);
      hashValue(hash, snapshot.lastAircraft.headingDeg);
      break;

    case AppState::HARDWARE_TEST: {
      const int roundedLux =
          snapshot.peripheral.ambientLux >= 0.0f
              ? static_cast<int>(roundf(snapshot.peripheral.ambientLux))
              : -1;
      hashValue(hash, snapshot.peripheral.lightOk);
      hashValue(hash, roundedLux);
      hashValue(hash, snapshot.peripheral.autoBrightness);
      hashValue(hash, snapshot.peripheral.buttonPresses);
      hashValue(hash, snapshot.metrics.appliedBrightness);
      break;
    }

    case AppState::SCREENSAVER:
      hashValue(hash, snapshot.peripheral.screensaverIndex);
      if (snapshot.peripheral.screensaverIndex % SCREENSAVER_COUNT == 6) {
        hashValue(hash, snapshot.weather.valid);
        hashValue(hash, snapshot.weather.temperatureC);
        hashValue(hash, snapshot.weather.feelsLikeC);
        hashText(hash, snapshot.weather.observedAt);
        hashText(hash, timeText);
      }
      hashValue(hash, nowMs / renderIntervalMs(appState, snapshot));
      break;

    case AppState::IDLE:
      hashText(hash, timeText);
      hashText(hash, dateText);
      hashValue(hash, nowMs / renderIntervalMs(appState, snapshot));
      break;
  }

  // FNV-1a can theoretically produce zero; reserve it as "never rendered".
  return hash == 0 ? 1 : hash;
}

uint8_t ambientBrightness(
    float lux,
    uint8_t minimum,
    uint8_t maximum) {
  if (!isfinite(lux) || lux < 0.0f) return maximum;
  // Log response spans sub-lux darkness through a bright room. The exponent
  // keeps night levels close to minimum while reaching useful output around
  // 200-250 lx instead of waiting for direct sunlight.
  float ratio =
      log10f(max(0.0f, lux) + 1.0f) /
      log10f(AUTO_BRIGHTNESS_LUX_MAX + 1.0f);
  ratio = constrain(ratio, 0.0f, 1.0f);
  ratio = powf(ratio, AUTO_BRIGHTNESS_CURVE);
  return static_cast<uint8_t>(
      roundf(minimum + ratio * (maximum - minimum)));
}

uint8_t brightnessForState(
    AppState appState,
    const SharedState &snapshot,
    VisualMode visualMode) {
  if (appState == AppState::ALERT) {
    if (!snapshot.peripheral.autoBrightness ||
        snapshot.peripheral.ambientLux < 0.0f) {
      return ALERT_BRIGHTNESS;
    }
    return ambientBrightness(
        snapshot.peripheral.ambientLux,
        ALERT_BRIGHTNESS_MIN,
        ALERT_BRIGHTNESS_MAX);
  }

  if (snapshot.peripheral.manualBrightnessEnabled) {
    return snapshot.peripheral.manualBrightness;
  }

  if (visualMode == VisualMode::SLEEP) {
    switch (appState) {
      case AppState::AIRCRAFT:
        return SLEEP_AIRCRAFT_BRIGHTNESS;
      case AppState::SCREENSAVER:
        return SLEEP_SCREENSAVER_BRIGHTNESS;
      case AppState::IDLE:
      case AppState::LAST_AIRCRAFT:
        return SLEEP_IDLE_BRIGHTNESS;
      case AppState::HARDWARE_TEST:
      default:
        return 20;
    }
  }

  if (visualMode == VisualMode::NIGHT) {
    const bool sensorBrightness =
        snapshot.peripheral.autoBrightness &&
        snapshot.peripheral.ambientLux >= 0.0f;
    switch (appState) {
      case AppState::AIRCRAFT:
        if (!sensorBrightness) return NIGHT_AIRCRAFT_MAX;
        return ambientBrightness(
            snapshot.peripheral.ambientLux,
            NIGHT_AIRCRAFT_MIN,
            NIGHT_AIRCRAFT_MAX);
      case AppState::SCREENSAVER:
        if (!sensorBrightness) return NIGHT_SCREENSAVER_MAX;
        return ambientBrightness(
            snapshot.peripheral.ambientLux,
            NIGHT_SCREENSAVER_MIN,
            NIGHT_SCREENSAVER_MAX);
      case AppState::IDLE:
      case AppState::LAST_AIRCRAFT:
        if (!sensorBrightness) return NIGHT_IDLE_MAX;
        return ambientBrightness(
            snapshot.peripheral.ambientLux,
            NIGHT_IDLE_MIN,
            NIGHT_IDLE_MAX);
      case AppState::HARDWARE_TEST:
      default:
        return 40;
    }
  }

  if (!snapshot.peripheral.autoBrightness ||
      snapshot.peripheral.ambientLux < 0.0f) {
    switch (appState) {
      case AppState::ALERT:
        return ALERT_BRIGHTNESS;
      case AppState::AIRCRAFT:
        return AIRCRAFT_BRIGHTNESS;
      case AppState::HARDWARE_TEST:
        return 100;
      case AppState::SCREENSAVER:
        return SCREENSAVER_BRIGHTNESS;
      case AppState::IDLE:
      case AppState::LAST_AIRCRAFT:
      default:
        return IDLE_BRIGHTNESS;
    }
  }

  switch (appState) {
    case AppState::ALERT:
      return ALERT_BRIGHTNESS;
    case AppState::AIRCRAFT:
      return ambientBrightness(
          snapshot.peripheral.ambientLux,
          AIRCRAFT_BRIGHTNESS_MIN,
          AIRCRAFT_BRIGHTNESS_MAX);
    case AppState::HARDWARE_TEST:
      return ambientBrightness(
          snapshot.peripheral.ambientLux,
          IDLE_BRIGHTNESS_MAX,
          120);
    case AppState::SCREENSAVER:
      return ambientBrightness(
          snapshot.peripheral.ambientLux,
          SCREENSAVER_BRIGHTNESS_MIN,
          SCREENSAVER_BRIGHTNESS_MAX);
    case AppState::IDLE:
    case AppState::LAST_AIRCRAFT:
    default:
      return ambientBrightness(
          snapshot.peripheral.ambientLux,
          IDLE_BRIGHTNESS_MIN,
          IDLE_BRIGHTNESS_MAX);
  }
}

const char *appStateName(AppState appState) {
  switch (appState) {
    case AppState::ALERT:
      return "ALERT";
    case AppState::AIRCRAFT:
      return "AIRCRAFT";
    case AppState::LAST_AIRCRAFT:
      return "LAST_AIRCRAFT";
    case AppState::HARDWARE_TEST:
      return "HARDWARE_TEST";
    case AppState::SCREENSAVER:
      return "SCREENSAVER";
    case AppState::IDLE:
    default:
      return "IDLE";
  }
}

const char *visualModeName(VisualMode visualMode) {
  switch (visualMode) {
    case VisualMode::NIGHT:
      return "NIGHT";
    case VisualMode::SLEEP:
      return "SLEEP";
    case VisualMode::DAY:
    default:
      return "DAY";
  }
}

const char *nightOverrideName(NightOverride overrideMode) {
  switch (overrideMode) {
    case NightOverride::FORCE_DAY:
      return "off";
    case NightOverride::FORCE_NIGHT:
      return "on";
    case NightOverride::AUTO:
    default:
      return "auto";
  }
}

const char *screenOverrideName(ScreenOverride overrideMode) {
  switch (overrideMode) {
    case ScreenOverride::IDLE:
      return "idle";
    case ScreenOverride::LAST_AIRCRAFT:
      return "last";
    case ScreenOverride::AIRCRAFT:
      return "aircraft";
    case ScreenOverride::HARDWARE_TEST:
      return "test";
    case ScreenOverride::SCREENSAVER:
      return "saver";
    case ScreenOverride::ALERT:
      return "alert";
    case ScreenOverride::AUTO:
    default:
      return "auto";
  }
}

void renderTask(void *parameter) {
  (void)parameter;
  uint32_t displayedSignature = 0;
  uint8_t appliedBrightness = IDLE_BRIGHTNESS;
  AppState brightnessState = AppState::IDLE;
  VisualMode brightnessMode = VisualMode::DAY;
  AppState previousAppState = AppState::IDLE;
  bool aircraftTransitionActive = false;
  uint32_t aircraftTransitionStartedMs = 0;
  uint32_t lastDisplayInitAttemptMs = 0;

  for (;;) {
    if (!displayReady) {
      const SharedState standbySnapshot = copyState();
      if (!standbySnapshot.peripheral.panelEnabled) {
        vTaskDelay(pdMS_TO_TICKS(25));
        continue;
      }
      const uint32_t retryNowMs = millis();
      if (lastDisplayInitAttemptMs == 0 ||
          retryNowMs - lastDisplayInitAttemptMs >=
              DISPLAY_INIT_RETRY_MS) {
        lastDisplayInitAttemptMs = retryNowMs;
        Serial.println("[display] retrying DMA initialization");
        if (setupDisplay()) {
          bootIntroStartedMs = millis();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    const uint32_t nowMs = millis();
    const SharedState snapshot = copyState();
    const AppState appState = selectAppState(snapshot, nowMs);
    if (appState != AppState::AIRCRAFT) {
      aircraftTransitionActive = false;
    } else if (
        !aircraftTransitionActive &&
        previousAppState == AppState::SCREENSAVER) {
      aircraftTransitionActive = true;
      aircraftTransitionStartedMs = nowMs;
      displayedSignature = 0;
    }
    if (aircraftTransitionActive &&
        nowMs - aircraftTransitionStartedMs >=
            AIRCRAFT_TRANSITION_DURATION_MS) {
      aircraftTransitionActive = false;
      displayedSignature = 0;
    }
    previousAppState = appState;
    const bool bootIntroActive =
        bootIntroStartedMs != 0 &&
        nowMs - bootIntroStartedMs < BOOT_INTRO_DURATION_MS &&
        appState != AppState::ALERT &&
        appState != AppState::AIRCRAFT;
    const AppState brightnessAppState =
        bootIntroActive ? AppState::SCREENSAVER : appState;
    const VisualMode visualMode =
        determineVisualMode(snapshot, nowMs);
    const bool warmState =
        bootIntroActive ||
        appState == AppState::IDLE ||
        appState == AppState::LAST_AIRCRAFT ||
        appState == AppState::AIRCRAFT ||
        appState == AppState::SCREENSAVER;
    const VisualMode renderTheme =
        warmState ? visualMode : VisualMode::DAY;
    if (renderTheme != activeVisualMode) {
      activeVisualMode = renderTheme;
      initColors();
      displayedSignature = 0;
    }
    if (snapshot.peripheral.visualMode != visualMode) {
      lockState();
      sharedState.peripheral.visualMode = visualMode;
      unlockState();
    }
    const uint8_t targetBrightness =
        snapshot.peripheral.panelEnabled
            ? brightnessForState(
                  brightnessAppState, snapshot, visualMode)
            : 0;

    uint8_t nextBrightness = appliedBrightness;
    if (!snapshot.peripheral.panelEnabled) {
      nextBrightness = 0;
    } else if (brightnessAppState != brightnessState ||
        visualMode != brightnessMode ||
        snapshot.peripheral.manualBrightnessEnabled ||
        brightnessAppState == AppState::ALERT) {
      nextBrightness = targetBrightness;
    } else if (targetBrightness != appliedBrightness) {
      const int delta =
          static_cast<int>(targetBrightness) -
          static_cast<int>(appliedBrightness);
      const int step = max(1, abs(delta) / 6);
      nextBrightness = static_cast<uint8_t>(
          static_cast<int>(appliedBrightness) +
          (delta > 0 ? step : -step));
    }

    if (nextBrightness != appliedBrightness) {
      appliedBrightness = nextBrightness;
      dmaDisplay->setBrightness8(appliedBrightness);
      lockState();
      sharedState.metrics.appliedBrightness = appliedBrightness;
      unlockState();
    }
    brightnessState = brightnessAppState;
    brightnessMode = visualMode;

    char timeText[16];
    char dateText[16];
    buildClockText(timeText, sizeof(timeText), dateText, sizeof(dateText));

    uint32_t signature =
        renderSignature(
            snapshot,
            appState,
            visualMode,
            timeText,
            dateText,
            nowMs);
    if (bootIntroActive) {
      const uint32_t introFrame = nowMs / SCREENSAVER_FRAME_MS;
      hashValue(signature, introFrame);
    }
    if (aircraftTransitionActive) {
      const uint32_t transitionFrame = nowMs / SCREENSAVER_FRAME_MS;
      hashValue(signature, transitionFrame);
    }
    const uint32_t renderInterval =
        snapshot.peripheral.panelEnabled
            ? (bootIntroActive || aircraftTransitionActive
                   ? SCREENSAVER_FRAME_MS
                   : renderIntervalMs(appState, snapshot))
            : 250;
    const bool animated =
        snapshot.peripheral.panelEnabled &&
        (bootIntroActive ||
         aircraftTransitionActive ||
         appState == AppState::IDLE ||
         appState == AppState::SCREENSAVER);
    const uint32_t nextRenderWait =
        renderWaitMs(renderInterval, animated, nowMs);
    if (signature == displayedSignature) {
      vTaskDelay(pdMS_TO_TICKS(nextRenderWait));
      continue;
    }

    const uint32_t renderStartedUs = micros();
    // All drawing targets the hidden DMA back buffer.
    if (aircraftTransitionActive) {
      drawScreensaverToAircraftTransition(
          snapshot,
          timeText,
          nowMs,
          aircraftTransitionStartedMs);
    } else if (bootIntroActive) {
      drawScreensaver(snapshot, timeText, nowMs);
      drawBootIntroOverlay(nowMs);
    } else switch (appState) {
      case AppState::ALERT:
        drawAlert(snapshot);
        break;
      case AppState::AIRCRAFT:
        drawAircraft(snapshot);
        break;
      case AppState::LAST_AIRCRAFT:
        drawLastAircraft(snapshot);
        break;
      case AppState::HARDWARE_TEST:
        drawHardwareTest(snapshot);
        break;
      case AppState::SCREENSAVER:
        drawScreensaver(snapshot, timeText, nowMs);
        break;
      case AppState::IDLE:
        drawIdle(snapshot, timeText, dateText, nowMs);
        break;
    }

    // Publish the finished frame atomically and make the old front buffer hidden.
    dmaDisplay->flipDMABuffer();
    displayedSignature = signature;

    lockState();
    ++sharedState.metrics.renderedFrames;
    sharedState.metrics.lastRenderDurationUs = micros() - renderStartedUs;
    sharedState.metrics.lastRenderSignature = signature;
    unlockState();

    // DMA keeps scanning the front buffer at the safe HUB75 refresh rate.
    // The CPU wakes only when this screen can produce a visibly new frame.
    vTaskDelay(pdMS_TO_TICKS(
        renderWaitMs(renderInterval, animated, millis())));
  }
}

// =========================
// Network
// =========================
void setupTime() {
  configTzTime(ISRAEL_TZ, NTP_SERVER_1, NTP_SERVER_2);
}

void refreshNetworkStatus() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  bool connectionChanged = false;
  struct tm timeInfo;
  const bool timeSynced =
      connected && getLocalTime(&timeInfo, 0);

  lockState();
  connectionChanged = sharedState.status.wifiOk != connected;
  recordHealthSample(
      connected,
      sharedState.status.wifiOk,
      sharedState.status.wifiFailures);
  recordHealthSample(
      timeSynced,
      sharedState.status.timeOk,
      sharedState.status.timeFailures);

  if (connected) {
    sharedState.status.wifiRssi = WiFi.RSSI();
    const IPAddress ip = WiFi.localIP();
    snprintf(
        sharedState.status.ipAddress,
        sizeof(sharedState.status.ipAddress),
        "%u.%u.%u.%u",
        ip[0], ip[1], ip[2], ip[3]);
  } else {
    sharedState.status.wifiRssi = -127;
    copyText(
        sharedState.status.ipAddress,
        sizeof(sharedState.status.ipAddress),
        "0.0.0.0");
  }

  unlockState();

  if (connectionChanged) {
    if (connected) {
      Serial.printf("[network] connected, IP=%s, RSSI=%ld dBm\n",
                    WiFi.localIP().toString().c_str(),
                    static_cast<long>(WiFi.RSSI()));
    } else {
      Serial.println("[network] disconnected");
    }
  }
}

bool httpGetJson(
    const char *url,
    JsonDocument &doc,
    uint32_t &durationMs,
    int &httpCode,
    uint16_t timeoutMs = HTTP_TIMEOUT_MS,
    bool bufferResponse = false) {
  const uint32_t startedMs = millis();
  httpCode = 0;

  if (WiFi.status() != WL_CONNECTED) {
    durationMs = millis() - startedMs;
    return false;
  }

  WiFiClient plainClient;
  WiFiClientSecure secureClient;

  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  // HTTP/1.0 forces a content-length/connection-close response and avoids
  // chunked-stream parsing problems seen with Open-Meteo on this ESP32 core.
  http.useHTTP10(true);
  plainClient.setTimeout(timeoutMs);
  secureClient.setTimeout(timeoutMs);

  bool began = false;
  if (strncmp(url, "https://", 8) == 0) {
    secureClient.setInsecure();
    const uint32_t handshakeSeconds =
        timeoutMs / 1000U < 5U ? 5U : timeoutMs / 1000U;
    secureClient.setHandshakeTimeout(handshakeSeconds);
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(plainClient, url);
  }

  if (!began) {
    durationMs = millis() - startedMs;
    setError("HTTP begin failed");
    return false;
  }

  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "FlightAboveHead-ESP32/1.0");
  httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    char message[48];
    if (httpCode < 0) {
      snprintf(
          message,
          sizeof(message),
          "HTTP %d %s",
          httpCode,
          HTTPClient::errorToString(httpCode).c_str());
    } else {
      snprintf(message, sizeof(message), "HTTP %d", httpCode);
    }
    setError(message);
    http.end();
    durationMs = millis() - startedMs;
    return false;
  }

  DeserializationError error;
  if (bufferResponse) {
    // The history endpoint is much larger than the frequent live responses.
    // Reading it into a complete String first prevents ArduinoJson from seeing
    // a temporary empty stream as EOF and reporting IncompleteInput.
    const int expectedLength = http.getSize();
    String payload = http.getString();
    http.end();
    durationMs = millis() - startedMs;

    Serial.printf(
        "[history] body=%u/%d bytes\n",
        static_cast<unsigned>(payload.length()),
        expectedLength);
    if (expectedLength > 0 &&
        payload.length() != static_cast<size_t>(expectedLength)) {
      char message[48];
      snprintf(
          message,
          sizeof(message),
          "JSON body %u/%d bytes",
          static_cast<unsigned>(payload.length()),
          expectedLength);
      setError(message);
      return false;
    }
    error = deserializeJson(doc, payload);
  } else {
    error = deserializeJson(doc, http.getStream());
    http.end();
    durationMs = millis() - startedMs;
  }

  if (error) {
    char message[48];
    snprintf(
        message,
        sizeof(message),
        "JSON parse: %s",
        error.c_str());
    setError(message);
    return false;
  }

  return true;
}

bool parseIsoMinute(const char *iso, uint16_t &minuteOfDay) {
  if (iso == nullptr || strlen(iso) < 16 || iso[10] != 'T') return false;
  const bool digits =
      iso[11] >= '0' && iso[11] <= '9' &&
      iso[12] >= '0' && iso[12] <= '9' &&
      iso[14] >= '0' && iso[14] <= '9' &&
      iso[15] >= '0' && iso[15] <= '9';
  if (!digits || iso[13] != ':') {
    return false;
  }
  const uint8_t hour = (iso[11] - '0') * 10 + (iso[12] - '0');
  const uint8_t minute = (iso[14] - '0') * 10 + (iso[15] - '0');
  if (hour > 23 || minute > 59) return false;
  minuteOfDay = hour * 60 + minute;
  return true;
}

bool pollSolarTimes() {
  char url[256];
  snprintf(
      url,
      sizeof(url),
      "http://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f&daily=sunrise,sunset&timezone=Asia%%2FJerusalem&forecast_days=1",
      static_cast<double>(HOME_LATITUDE),
      static_cast<double>(HOME_LONGITUDE));

  JsonDocument doc;
  uint32_t durationMs = 0;
  int httpCode = 0;
  const bool success = httpGetJson(
      url,
      doc,
      durationMs,
      httpCode,
      EXTERNAL_HTTP_TIMEOUT_MS);

  lockState();
  ++sharedState.metrics.solarRequests;
  sharedState.metrics.lastSolarDurationMs = durationMs;
  sharedState.metrics.lastSolarHttpCode = httpCode;
  if (!success) {
    ++sharedState.metrics.solarFailures;
    sharedState.solar.lastFetchOk = false;
    unlockState();
    return false;
  }
  unlockState();

  const char *date = doc["daily"]["time"][0] | "";
  const char *sunriseIso = doc["daily"]["sunrise"][0] | "";
  const char *sunsetIso = doc["daily"]["sunset"][0] | "";
  uint16_t sunriseMin = 0;
  uint16_t sunsetMin = 0;
  if (date[0] == '\0' ||
      !parseIsoMinute(sunriseIso, sunriseMin) ||
      !parseIsoMinute(sunsetIso, sunsetMin) ||
      sunriseMin >= sunsetMin) {
    lockState();
    ++sharedState.metrics.solarFailures;
    sharedState.solar.lastFetchOk = false;
    unlockState();
    setError("Solar API parse failed");
    return false;
  }

  lockState();
  sharedState.solar.valid = true;
  sharedState.solar.lastFetchOk = true;
  sharedState.solar.sunriseMin = sunriseMin;
  sharedState.solar.sunsetMin = sunsetMin;
  copyText(
      sharedState.solar.date,
      sizeof(sharedState.solar.date),
      date);
  sharedState.solar.receivedMs = millis();
  unlockState();
  Serial.printf(
      "[solar] %s sunrise=%02u:%02u sunset=%02u:%02u\n",
      date,
      sunriseMin / 60,
      sunriseMin % 60,
      sunsetMin / 60,
      sunsetMin % 60);
  return true;
}

void pollAlerts() {
  JsonDocument doc;
  uint32_t durationMs = 0;
  int httpCode = 0;

  const bool success = httpGetJson(ALERTS_URL, doc, durationMs, httpCode);
  recordRequest(true, success, durationMs, httpCode);
  if (!success) {
    setApiStatus(true, false);
    return;
  }

  AlertData nextAlert;
  nextAlert.active = doc["active"] | false;
  copyPanelText(nextAlert.title, sizeof(nextAlert.title), doc["title"] | "");
  copyText(nextAlert.category, sizeof(nextAlert.category), doc["cat"] | "");

  JsonArray areas = doc["areas"].as<JsonArray>();
  if (!areas.isNull()) {
    const size_t areaSize = areas.size();
    nextAlert.areaCount =
        areaSize > 255 ? 255 : static_cast<uint8_t>(areaSize);
    if (areaSize > 0) {
      copyPanelText(
          nextAlert.firstArea,
          sizeof(nextAlert.firstArea),
          areas[0] | "");
    }
  }

  nextAlert.fresh = true;
  nextAlert.receivedMs = millis();

  lockState();
  sharedState.alert = nextAlert;
  unlockState();
  setApiStatus(true, true);
  clearErrorIfHealthy();
}

bool parseFlightEntry(
    JsonObjectConst source,
    FlightData &flight,
    uint8_t count = 1) {
  if (source.isNull()) return false;

  flight.count = count;
  copyText(flight.id, sizeof(flight.id), source["id"] | "");
  copyPanelText(
      flight.callsign,
      sizeof(flight.callsign),
      source["callsign"] | "");
  copyPanelText(
      flight.airlineIcao,
      sizeof(flight.airlineIcao),
      source["airline_icao"] | "");
  copyPanelText(
      flight.aircraft,
      sizeof(flight.aircraft),
      source["aircraft"] | "");
  copyPanelText(
      flight.registration,
      sizeof(flight.registration),
      source["registration"] | "");
  copyPanelText(
      flight.origin,
      sizeof(flight.origin),
      source["origin"] | "");
  copyPanelText(
      flight.destination,
      sizeof(flight.destination),
      source["destination"] | "");
  copyText(
      flight.updatedAt,
      sizeof(flight.updatedAt),
      source["updated_at"] | "");
  flight.altitudeFt = source["altitude_ft"] | -1;
  flight.speedKts = source["speed_kts"] | -1;
  flight.headingDeg = source["heading_deg"] | -1;
  flight.verticalSpeed = source["vertical_speed"] | 0;
  flight.active = true;
  flight.fresh = true;
  flight.receivedMs = millis();
  return true;
}

void pollFlights() {
  JsonDocument doc;
  uint32_t durationMs = 0;
  int httpCode = 0;

  const bool success = httpGetJson(FLIGHTS_URL, doc, durationMs, httpCode);
  recordRequest(false, success, durationMs, httpCode);
  if (!success) {
    setApiStatus(false, false);
    return;
  }

  FlightData nextFlight;
  nextFlight.count = doc["count"] | 0;
  copyText(
      nextFlight.updatedAt,
      sizeof(nextFlight.updatedAt),
      doc["updated_at"] | "");

  JsonArray flights = doc["flights"].as<JsonArray>();
  if (nextFlight.count > 0 && !flights.isNull() && flights.size() > 0) {
    const uint8_t count = nextFlight.count;
    parseFlightEntry(flights[0].as<JsonObjectConst>(), nextFlight, count);
    if (nextFlight.updatedAt[0] == '\0') {
      copyText(
          nextFlight.updatedAt,
          sizeof(nextFlight.updatedAt),
          doc["updated_at"] | "");
    }
  }

  nextFlight.fresh = true;
  nextFlight.receivedMs = millis();

  lockState();
  sharedState.flight = nextFlight;
  if (nextFlight.active) {
    sharedState.lastAircraft = nextFlight;
  }
  unlockState();
  setApiStatus(false, true);
  clearErrorIfHealthy();
}

bool pollHistory() {
  JsonDocument doc;
  uint32_t durationMs = 0;
  int httpCode = 0;
  const bool success =
      httpGetJson(
          HISTORY_URL,
          doc,
          durationMs,
          httpCode,
          HISTORY_HTTP_TIMEOUT_MS,
          true);

  lockState();
  ++sharedState.metrics.historyRequests;
  sharedState.metrics.lastHistoryDurationMs = durationMs;
  sharedState.metrics.lastHistoryHttpCode = httpCode;
  if (!success) ++sharedState.metrics.historyFailures;
  unlockState();
  if (!success) {
    setAuxStatus(false, false);
    return false;
  }

  JsonArray flights = doc["flights"].as<JsonArray>();
  FlightData historyFlight;
  if (flights.isNull() || flights.size() == 0 ||
      !parseFlightEntry(
          flights[0].as<JsonObjectConst>(),
          historyFlight,
          flights.size() > 255
              ? 255
              : static_cast<uint8_t>(flights.size()))) {
    lockState();
    ++sharedState.metrics.historyFailures;
    unlockState();
    setAuxStatus(false, false);
    setError("History API parse failed");
    return false;
  }

  lockState();
  if (!sharedState.lastAircraft.active ||
      strcmp(
          historyFlight.updatedAt,
          sharedState.lastAircraft.updatedAt) > 0) {
    sharedState.lastAircraft = historyFlight;
  }
  unlockState();
  setAuxStatus(false, true);
  Serial.printf(
      "[history] latest=%s %s at=%s\n",
      historyFlight.callsign,
      historyFlight.aircraft,
      historyFlight.updatedAt);
  return true;
}

bool pollWeather() {
  char url[320];
  snprintf(
      url,
      sizeof(url),
      "http://api.open-meteo.com/v1/forecast?"
      "latitude=%.6f&longitude=%.6f&"
      "current=temperature_2m,apparent_temperature&"
      "current_weather=true&forecast_days=1&"
      "timezone=Asia%%2FJerusalem",
      static_cast<double>(HOME_LATITUDE),
      static_cast<double>(HOME_LONGITUDE));

  JsonDocument doc;
  uint32_t durationMs = 0;
  int httpCode = 0;
  const bool success = httpGetJson(
      url,
      doc,
      durationMs,
      httpCode,
      EXTERNAL_HTTP_TIMEOUT_MS);
  lockState();
  ++sharedState.metrics.weatherRequests;
  sharedState.metrics.lastWeatherDurationMs = durationMs;
  sharedState.metrics.lastWeatherHttpCode = httpCode;
  if (!success) ++sharedState.metrics.weatherFailures;
  unlockState();
  if (!success) {
    Serial.printf(
        "[weather] request failed code=%d duration=%lums\n",
        httpCode,
        static_cast<unsigned long>(durationMs));
    setAuxStatus(true, false);
    return false;
  }

  JsonObject current = doc["current"].as<JsonObject>();
  JsonObject currentWeather = doc["current_weather"].as<JsonObject>();
  float temperature = current["temperature_2m"] | NAN;
  float feelsLike = current["apparent_temperature"] | NAN;
  const char *observedAt = current["time"] | "";
  if (!isfinite(temperature)) {
    temperature = currentWeather["temperature"] | NAN;
    observedAt = currentWeather["time"] | "";
  }
  // Keep the weather page useful on older Open-Meteo response variants that
  // expose current temperature but omit apparent_temperature.
  if (!isfinite(feelsLike) && isfinite(temperature)) {
    feelsLike = temperature;
  }
  if (!isfinite(temperature) || !isfinite(feelsLike)) {
    lockState();
    ++sharedState.metrics.weatherFailures;
    sharedState.weather.lastFetchOk = false;
    unlockState();
    setAuxStatus(true, false);
    setError("Weather API parse failed");
    Serial.printf(
        "[weather] parse failed code=%d duration=%lums current=%s fallback=%s\n",
        httpCode,
        static_cast<unsigned long>(durationMs),
        current.isNull() ? "missing" : "present",
        currentWeather.isNull() ? "missing" : "present");
    return false;
  }

  lockState();
  sharedState.weather.valid = true;
  sharedState.weather.lastFetchOk = true;
  sharedState.weather.temperatureC = temperature;
  sharedState.weather.feelsLikeC = feelsLike;
  copyText(
      sharedState.weather.observedAt,
      sizeof(sharedState.weather.observedAt),
      observedAt);
  sharedState.weather.receivedMs = millis();
  unlockState();
  setAuxStatus(true, true);
  Serial.printf(
      "[weather] temp=%.1fC feels=%.1fC\n",
      temperature,
      feelsLike);
  return true;
}

void networkTask(void *parameter) {
  (void)parameter;
  uint32_t lastWifiAttemptMs = 0;
  uint32_t lastAlertPollMs = 0;
  uint32_t lastFlightPollMs = 0;
  uint32_t lastSolarAttemptMs = 0;
  uint32_t lastHistoryAttemptMs = 0;
  uint32_t lastWeatherAttemptMs = 0;
  uint32_t lastStatusMs = 0;
  uint32_t lastWifiLogMs = 0;
  bool lastSolarSuccess = false;
  bool lastHistorySuccess = false;
  bool lastWeatherSuccess = false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  for (;;) {
    const uint32_t nowMs = millis();
    const bool connected = WiFi.status() == WL_CONNECTED;

    if (!connected &&
        (lastWifiAttemptMs == 0 || nowMs - lastWifiAttemptMs >= WIFI_RETRY_MS)) {
      lastWifiAttemptMs = nowMs;
      if (WIFI_SSID[0] == '\0') {
        setError("WiFi not configured");
      } else {
        Serial.println("[network] connecting");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    }

    if (lastStatusMs == 0 || nowMs - lastStatusMs >= NETWORK_STATUS_MS) {
      lastStatusMs = nowMs;
      refreshNetworkStatus();
    }

    if (lastWifiLogMs == 0 || nowMs - lastWifiLogMs >= WIFI_RETRY_MS) {
      lastWifiLogMs = nowMs;
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(
            "[network] wifi=ok ip=%s rssi=%ld dBm\n",
            WiFi.localIP().toString().c_str(),
            static_cast<long>(WiFi.RSSI()));
      } else {
        Serial.printf(
            "[network] wifi=down status=%d; retry=%lus\n",
            static_cast<int>(WiFi.status()),
            static_cast<unsigned long>(WIFI_RETRY_MS / 1000));
      }
    }

    if (connected) {
      const uint32_t solarInterval =
          lastSolarSuccess ? SOLAR_REFRESH_MS : SOLAR_RETRY_MS;
      if (lastSolarAttemptMs == 0 ||
          nowMs - lastSolarAttemptMs >= solarInterval) {
        lastSolarAttemptMs = nowMs;
        lastSolarSuccess = pollSolarTimes();
      }

      if (lastAlertPollMs == 0 || nowMs - lastAlertPollMs >= ALERT_POLL_MS) {
        lastAlertPollMs = nowMs;
        pollAlerts();
      }

      if (lastFlightPollMs == 0 || nowMs - lastFlightPollMs >= FLIGHT_POLL_MS) {
        lastFlightPollMs = nowMs;
        pollFlights();
      }

      const uint32_t historyInterval =
          lastHistorySuccess ? HISTORY_REFRESH_MS : HISTORY_RETRY_MS;
      if (lastHistoryAttemptMs == 0 ||
          nowMs - lastHistoryAttemptMs >= historyInterval) {
        lastHistoryAttemptMs = nowMs;
        lastHistorySuccess = pollHistory();
      }

      const uint32_t weatherInterval =
          lastWeatherSuccess ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS;
      if (lastWeatherAttemptMs == 0 ||
          nowMs - lastWeatherAttemptMs >= weatherInterval) {
        lastWeatherAttemptMs = nowMs;
        lastWeatherSuccess = pollWeather();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// =========================
// Diagnostics
// =========================
void printBootDiagnostics() {
  Serial.println();
  Serial.println("Flight Above Head - ESP32-S3 desktop MVP");
  Serial.printf(
      "[boot] chip=%s rev=%u cores=%u cpu=%lu MHz\n",
      ESP.getChipModel(),
      ESP.getChipRevision(),
      ESP.getChipCores(),
      static_cast<unsigned long>(ESP.getCpuFreqMHz()));
  Serial.printf(
      "[boot] flash=%lu bytes, psram=%lu bytes, free-psram=%lu bytes\n",
      static_cast<unsigned long>(ESP.getFlashChipSize()),
      static_cast<unsigned long>(ESP.getPsramSize()),
      static_cast<unsigned long>(ESP.getFreePsram()));
  Serial.printf(
      "[boot] free-heap=%lu bytes, min-free-heap=%lu bytes\n",
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getMinFreeHeap()));
  Serial.printf(
      "[boot] HUB75 GPIO R1..B2=%d,%d,%d,%d,%d,%d ABCDE=%d,%d,%d,%d,%d LAT/OE/CLK=%d,%d,%d\n",
      HUB75_R1_PIN, HUB75_G1_PIN, HUB75_B1_PIN,
      HUB75_R2_PIN, HUB75_G2_PIN, HUB75_B2_PIN,
      HUB75_A_PIN, HUB75_B_PIN, HUB75_C_PIN, HUB75_D_PIN, HUB75_E_PIN,
      HUB75_LAT_PIN, HUB75_OE_PIN, HUB75_CLK_PIN);
  Serial.printf(
      "[boot] BH1750 SDA=%d SCL=%d; touch OUT=%d active=%s\n",
      BH1750_SDA_PIN,
      BH1750_SCL_PIN,
      TOUCH_BUTTON_PIN,
      TOUCH_BUTTON_ACTIVE_HIGH ? "HIGH" : "LOW");
  Serial.printf(
      "[boot] panel driver color-order=%s; brightness idle=%u aircraft=%u alert=%u\n",
      PANEL_COLOR_ORDER == PanelColorOrder::BRG ? "BRG" : "RGB",
      IDLE_BRIGHTNESS,
      AIRCRAFT_BRIGHTNESS,
      ALERT_BRIGHTNESS);
}

void printRuntimeDiagnostics() {
  const SharedState snapshot = copyState();
  const AppState appState = selectAppState(snapshot, millis());
  const UBaseType_t networkStackWords =
      networkTaskHandle ? uxTaskGetStackHighWaterMark(networkTaskHandle) : 0;
  const UBaseType_t renderStackWords =
      renderTaskHandle ? uxTaskGetStackHighWaterMark(renderTaskHandle) : 0;

  Serial.printf(
      "[diag] up=%lus state=%s mode=%s night-override=%s wifi=%s rssi=%ld ip=%s time=%s\n",
      static_cast<unsigned long>(millis() / 1000),
      appStateName(appState),
      visualModeName(snapshot.peripheral.visualMode),
      nightOverrideName(snapshot.peripheral.nightOverride),
      snapshot.status.wifiOk ? "ok" : "down",
      static_cast<long>(snapshot.status.wifiRssi),
      snapshot.status.ipAddress,
      snapshot.status.timeOk ? "ok" : "waiting");
  Serial.printf(
      "[diag] api alert=%s req=%lu code=%d %lums; flight=%s req=%lu code=%d %lums; failures-total=%lu; streak n/t/f/a=%u/%u/%u/%u\n",
      snapshot.status.alertOk ? "ok" : "fail",
      static_cast<unsigned long>(snapshot.metrics.alertRequests),
      snapshot.metrics.lastAlertHttpCode,
      static_cast<unsigned long>(snapshot.metrics.lastAlertDurationMs),
      snapshot.status.flightOk ? "ok" : "fail",
      static_cast<unsigned long>(snapshot.metrics.flightRequests),
      snapshot.metrics.lastFlightHttpCode,
      static_cast<unsigned long>(snapshot.metrics.lastFlightDurationMs),
      static_cast<unsigned long>(snapshot.metrics.requestFailures),
      static_cast<unsigned>(snapshot.status.wifiFailures),
      static_cast<unsigned>(snapshot.status.timeFailures),
      static_cast<unsigned>(snapshot.status.flightFailures),
      static_cast<unsigned>(snapshot.status.alertFailures));
  Serial.printf(
      "[diag] solar=%s last=%s code=%d %lums date=%s rise=%02u:%02u set=%02u:%02u req=%lu fail=%lu\n",
      snapshot.solar.valid ? "valid" : "fallback",
      snapshot.solar.lastFetchOk ? "ok" : "fail",
      snapshot.metrics.lastSolarHttpCode,
      static_cast<unsigned long>(snapshot.metrics.lastSolarDurationMs),
      snapshot.solar.date[0] != '\0' ? snapshot.solar.date : "--",
      snapshot.solar.sunriseMin / 60,
      snapshot.solar.sunriseMin % 60,
      snapshot.solar.sunsetMin / 60,
      snapshot.solar.sunsetMin % 60,
      static_cast<unsigned long>(snapshot.metrics.solarRequests),
      static_cast<unsigned long>(snapshot.metrics.solarFailures));
  Serial.printf(
      "[diag] history=%s code=%d %lums req=%lu fail=%lu latest=%s; weather=%s code=%d %lums %.1fC feels=%.1fC req=%lu fail=%lu streak h/w=%u/%u\n",
      snapshot.status.historyOk ? "ok" : "fail",
      snapshot.metrics.lastHistoryHttpCode,
      static_cast<unsigned long>(snapshot.metrics.lastHistoryDurationMs),
      static_cast<unsigned long>(snapshot.metrics.historyRequests),
      static_cast<unsigned long>(snapshot.metrics.historyFailures),
      snapshot.lastAircraft.active ? snapshot.lastAircraft.callsign : "--",
      snapshot.status.weatherOk ? "ok" : "fail",
      snapshot.metrics.lastWeatherHttpCode,
      static_cast<unsigned long>(snapshot.metrics.lastWeatherDurationMs),
      snapshot.weather.temperatureC,
      snapshot.weather.feelsLikeC,
      static_cast<unsigned long>(snapshot.metrics.weatherRequests),
      static_cast<unsigned long>(snapshot.metrics.weatherFailures),
      static_cast<unsigned>(snapshot.status.historyFailures),
      static_cast<unsigned>(snapshot.status.weatherFailures));
  Serial.printf(
      "[diag] display ready=%s panel=%s screen=%s refresh=%uHz brightness=%u(%s) frames=%lu last-render=%luus signature=%08lx\n",
      displayReady ? "yes" : "no",
      snapshot.peripheral.panelEnabled ? "on" : "off",
      screenOverrideName(snapshot.peripheral.screenOverride),
      displayReady ? dmaDisplay->calculated_refresh_rate : 0,
      snapshot.metrics.appliedBrightness,
      snapshot.peripheral.manualBrightnessEnabled
          ? "manual"
          : (snapshot.peripheral.autoBrightness ? "auto" : "fixed"),
      static_cast<unsigned long>(snapshot.metrics.renderedFrames),
      static_cast<unsigned long>(snapshot.metrics.lastRenderDurationUs),
      static_cast<unsigned long>(snapshot.metrics.lastRenderSignature));
  Serial.printf(
      "[diag] memory heap=%lu min=%lu max-block=%lu psram-free=%lu; stack-free network=%uB render=%uB\n",
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getMinFreeHeap()),
      static_cast<unsigned long>(ESP.getMaxAllocHeap()),
      static_cast<unsigned long>(ESP.getFreePsram()),
      static_cast<unsigned>(networkStackWords * sizeof(StackType_t)),
      static_cast<unsigned>(renderStackWords * sizeof(StackType_t)));
  Serial.printf(
      "[diag] sensor bh1750=%s addr=0x%02X lux=%.1f auto=%s; button=%lu saver=%s/%u\n",
      snapshot.peripheral.lightOk ? "ok" : "missing",
      snapshot.peripheral.bh1750Address,
      snapshot.peripheral.ambientLux,
      snapshot.peripheral.autoBrightness ? "on" : "off",
      static_cast<unsigned long>(snapshot.peripheral.buttonPresses),
      snapshot.peripheral.screensaverActive ? "on" : "off",
      static_cast<unsigned>(snapshot.peripheral.screensaverIndex + 1));

  if (snapshot.status.lastError[0] != '\0') {
    Serial.printf("[diag] last-error=%s\n", snapshot.status.lastError);
  }
}

void processSerialCommand(char *command) {
  while (*command == ' ' || *command == '\t') {
    ++command;
  }
  size_t commandLength = strlen(command);
  while (commandLength > 0 &&
         (command[commandLength - 1] == ' ' ||
          command[commandLength - 1] == '\t')) {
    command[--commandLength] = '\0';
  }
  for (char *cursor = command; *cursor != '\0'; ++cursor) {
    if (*cursor >= 'A' && *cursor <= 'Z') {
      *cursor = static_cast<char>(*cursor - 'A' + 'a');
    }
  }

  NightOverride overrideMode;
  bool nightCommand = true;
  if (strcmp(command, "night on") == 0) {
    overrideMode = NightOverride::FORCE_NIGHT;
  } else if (strcmp(command, "night off") == 0) {
    overrideMode = NightOverride::FORCE_DAY;
  } else if (strcmp(command, "night auto") == 0) {
    overrideMode = NightOverride::AUTO;
  } else {
    nightCommand = false;
  }

  if (nightCommand) {
    lockState();
    sharedState.peripheral.nightOverride = overrideMode;
    unlockState();
    Serial.printf(
        "[command] night-override=%s\n",
        nightOverrideName(overrideMode));
    return;
  }
  if (strncmp(command, "panel ", 6) == 0) {
    const char *argument = command + 6;
    bool valid = true;
    bool enabled = true;
    if (strcmp(argument, "on") == 0) {
      enabled = true;
    } else if (strcmp(argument, "off") == 0) {
      enabled = false;
    } else {
      valid = false;
    }
    if (valid) {
      lockState();
      sharedState.peripheral.panelEnabled = enabled;
      unlockState();
      Serial.printf("[command] panel=%s\n", enabled ? "on" : "off");
      return;
    }
  }
  if (strncmp(command, "brightness ", 11) == 0) {
    const char *argument = command + 11;
    if (strcmp(argument, "auto") == 0 ||
        strcmp(argument, "sensor") == 0) {
      lockState();
      sharedState.peripheral.manualBrightnessEnabled = false;
      sharedState.peripheral.autoBrightness = true;
      unlockState();
      Serial.println("[command] brightness=auto (BH1750)");
      return;
    }
    if (strcmp(argument, "fixed") == 0 ||
        strcmp(argument, "default") == 0) {
      lockState();
      sharedState.peripheral.manualBrightnessEnabled = false;
      sharedState.peripheral.autoBrightness = false;
      unlockState();
      Serial.println("[command] brightness=fixed state defaults");
      return;
    }
    char *end = nullptr;
    const long requested = strtol(argument, &end, 10);
    if (end != argument && *end == '\0' &&
        requested >= 0 && requested <= 255) {
      lockState();
      sharedState.peripheral.manualBrightness =
          static_cast<uint8_t>(requested);
      sharedState.peripheral.manualBrightnessEnabled = true;
      sharedState.peripheral.autoBrightness = false;
      unlockState();
      Serial.printf("[command] brightness=%ld/255 manual\n", requested);
      return;
    }
    Serial.println("[command] brightness expects auto | fixed | 0..255");
    return;
  }
  if (strncmp(command, "screen ", 7) == 0) {
    const char *argument = command + 7;
    ScreenOverride screen = ScreenOverride::AUTO;
    bool valid = true;
    if (strcmp(argument, "auto") == 0) {
      screen = ScreenOverride::AUTO;
    } else if (strcmp(argument, "idle") == 0) {
      screen = ScreenOverride::IDLE;
    } else if (strcmp(argument, "last") == 0) {
      screen = ScreenOverride::LAST_AIRCRAFT;
    } else if (strcmp(argument, "aircraft") == 0) {
      screen = ScreenOverride::AIRCRAFT;
    } else if (strcmp(argument, "test") == 0) {
      screen = ScreenOverride::HARDWARE_TEST;
    } else if (strcmp(argument, "saver") == 0) {
      screen = ScreenOverride::SCREENSAVER;
    } else if (strcmp(argument, "alert") == 0) {
      screen = ScreenOverride::ALERT;
    } else {
      valid = false;
    }
    if (valid) {
      lockState();
      sharedState.peripheral.screenOverride = screen;
      unlockState();
      Serial.printf(
          "[command] screen=%s%s\n",
          screenOverrideName(screen),
          screen == ScreenOverride::AUTO
              ? " (automatic priority restored)"
              : " (forced; use 'screen auto' to release)");
      return;
    }
    Serial.println(
        "[command] screen expects auto | idle | last | aircraft | test | saver | alert");
    return;
  }
  if (strncmp(command, "saver ", 6) == 0) {
    const char *argument = command + 6;
    uint8_t index = 0;
    bool valid = true;
    lockState();
    if (strcmp(argument, "next") == 0) {
      index =
          (sharedState.peripheral.screensaverIndex + 1U) %
          SCREENSAVER_COUNT;
    } else {
      char *end = nullptr;
      const long requested = strtol(argument, &end, 10);
      if (end == argument || *end != '\0' ||
          requested < 1 || requested > SCREENSAVER_COUNT) {
        valid = false;
      } else {
        index = static_cast<uint8_t>(requested - 1);
      }
    }
    if (valid) {
      sharedState.peripheral.screensaverIndex = index;
      sharedState.peripheral.screenOverride = ScreenOverride::SCREENSAVER;
    }
    unlockState();
    if (valid) {
      Serial.printf(
          "[command] saver=%u/%u (screen forced)\n",
          static_cast<unsigned>(index + 1),
          static_cast<unsigned>(SCREENSAVER_COUNT));
    } else {
      Serial.printf(
          "[command] saver expects next | 1..%u\n",
          static_cast<unsigned>(SCREENSAVER_COUNT));
    }
    return;
  }
  if (strcmp(command, "test") == 0) {
    showHardwareTest(millis());
    Serial.println("[command] sensor test");
    return;
  }
  if (strcmp(command, "diag") == 0 ||
      strcmp(command, "status") == 0) {
    printRuntimeDiagnostics();
    return;
  }
  if (strcmp(command, "help") == 0 ||
      strcmp(command, "?") == 0) {
    Serial.println("[command] panel on | panel off");
    Serial.println("[command] brightness auto | fixed | 0..255");
    Serial.println(
        "[command] screen auto | idle | last | aircraft | test | saver | alert");
    Serial.printf(
        "[command] saver next | saver 1..%u\n",
        static_cast<unsigned>(SCREENSAVER_COUNT));
    Serial.println("[command] night auto | night on | night off");
    Serial.println("[command] test | status | diag | help");
    Serial.println(
        "[command] alert/aircraft safety priority remains active over forced screens");
    return;
  }
  if (command[0] != '\0') {
    Serial.printf("[command] unknown: %s (send help)\n", command);
  }
}

void serviceSerialCommands() {
  static char command[96];
  static size_t length = 0;
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\n' || value == '\r') {
      if (length > 0) {
        command[length] = '\0';
        processSerialCommand(command);
        length = 0;
      }
    } else if (length + 1 < sizeof(command)) {
      command[length++] = value;
    }
  }
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  // Logging must never stop button/sensor service when no host consumes USB
  // CDC output. Short writes are intentionally dropped under backpressure.
  Serial.setTxTimeoutMs(0);
#endif
  delay(300);
  randomSeed(esp_random());

  stateMutex = xSemaphoreCreateMutex();
  if (stateMutex == nullptr) {
    Serial.println("[fatal] state mutex allocation failed");
    return;
  }

  restoreUiSelection();
  printBootDiagnostics();
  pinMode(
      TOUCH_BUTTON_PIN,
      TOUCH_BUTTON_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  buttonLastRawState = readTouchButton();
  buttonRawState = buttonLastRawState;
  buttonStableState = buttonLastRawState;
  holdPanelBlankDuringPowerUp();
  setupLightSensor();
  Serial.printf(
      "[display] waiting %lums for panel power\n",
      static_cast<unsigned long>(PANEL_POWER_SETTLE_MS));
  delay(PANEL_POWER_SETTLE_MS);
  Serial.println("[display] standby; press the button to start HUB75");
  setupTime();

  const BaseType_t networkCreated = xTaskCreatePinnedToCore(
      networkTask,
      "network",
      NETWORK_TASK_STACK,
      nullptr,
      NETWORK_TASK_PRIORITY,
      &networkTaskHandle,
      NETWORK_TASK_CORE);
  const BaseType_t renderCreated = xTaskCreatePinnedToCore(
      renderTask,
      "render",
      RENDER_TASK_STACK,
      nullptr,
      RENDER_TASK_PRIORITY,
      &renderTaskHandle,
      RENDER_TASK_CORE);

  Serial.printf(
      "[boot] tasks network=%s(core %d) render=%s(core %d)\n",
      networkCreated == pdPASS ? "ok" : "FAILED",
      NETWORK_TASK_CORE,
      renderCreated == pdPASS ? "ok" : "FAILED",
      RENDER_TASK_CORE);
}

void loop() {
  static uint32_t lastDiagnosticsMs = 0;
  const uint32_t nowMs = millis();

  handleTouchButton(nowMs);
  updateLightSensor(nowMs);
  serviceSerialCommands();

  if (stateMutex != nullptr &&
#if ARDUINO_USB_CDC_ON_BOOT
      Serial &&
#endif
      (lastDiagnosticsMs == 0 || nowMs - lastDiagnosticsMs >= DIAGNOSTICS_MS)) {
    lastDiagnosticsMs = nowMs;
    printRuntimeDiagnostics();
  }

  vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
}
