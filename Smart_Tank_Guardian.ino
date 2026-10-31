/*
  ============================================================
      SMART TANK GUARDIAN - FINAL ESP32 VERSION
  ============================================================

  Hardware:
  - ESP32
  - 1.3-inch SH1106 OLED 128x64 I2C
  - HC-SR04 ultrasonic sensor
  - MQ-series analog gas sensor
  - Passive buzzer
  - LED
  - Push button

  Required libraries:
  - U8g2 Library (olikraus)

  Pin configuration:
  OLED SDA       GPIO 21
  OLED SCL       GPIO 22

  HC-SR04 TRIG   GPIO 5
  HC-SR04 ECHO   GPIO 18

  Gas sensor AO  GPIO 34
  Buzzer         GPIO 25
  LED            GPIO 26
  Button         GPIO 27

  IMPORTANT:
  1. Do not connect HC-SR04 ECHO 5V directly to ESP32.
     Use a voltage divider to reduce it to 3.3V.

  2. Do not connect a gas sensor output above 3.3V to GPIO 34.
     Use a voltage divider.

  3. Do not connect a 9V battery directly to the ESP32 3.3V pin.

  4. MQ gas sensors are not certified safety devices.

  5. This program assumes:
     - LED is active HIGH
     - Button is connected between GPIO 27 and GND
     - OLED is SH1106 128x64 I2C
     - Buzzer is passive
  ============================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>

// ============================================================
// OLED CONFIGURATION
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// ------------------------------------------------------------------
// SH1106/U8g2 compatibility display wrapper.
// Keeps the original project's drawing/animation code intact while
// using the correct driver for the common 1.3-inch 128x64 SH1106 OLED.
// ------------------------------------------------------------------
#define SSD1306_WHITE 1
#define SSD1306_BLACK 0
#define SSD1306_SWITCHCAPVCC 0

class TankDisplay : public Print {
private:
  U8G2_SH1106_128X64_NONAME_F_HW_I2C oled;
  uint8_t textSize = 1;
  uint16_t textColor = SSD1306_WHITE;
  int16_t cursorX = 0;
  int16_t cursorY = 0;
  bool wrap = true;

  void selectFont() {
    if (textSize <= 1) oled.setFont(u8g2_font_6x10_tf);
    else oled.setFont(u8g2_font_10x20_tf);
  }

  int16_t fontAscent() const {
    return textSize <= 1 ? 9 : 18;
  }

  int16_t charAdvance(uint8_t c) {
    (void)c;
    return textSize <= 1 ? 6 : 10;
  }

  void drawCharInternal(uint8_t c) {
    selectFont();
    if (c == '\n') {
      cursorX = 0;
      cursorY += (textSize <= 1 ? 10 : 20);
      return;
    }
    if (wrap && cursorX + charAdvance(c) > SCREEN_WIDTH) {
      cursorX = 0;
      cursorY += (textSize <= 1 ? 10 : 20);
    }
    oled.setDrawColor(textColor ? 1 : 0);
    oled.drawGlyph(cursorX, cursorY + fontAscent(), c);
    cursorX += charAdvance(c);
  }

public:
  TankDisplay() : oled(U8G2_R0, U8X8_PIN_NONE) {}

  bool begin(uint8_t /*power*/, uint8_t address) {
    oled.setI2CAddress(address << 1);
    oled.begin();
    return true;
  }

  void setAddress(uint8_t address) {
    oled.setI2CAddress(address << 1);
  }

  void clearDisplay() {
    oled.clearBuffer();
    oled.setDrawColor(1);
  }

  void display() {
    oled.sendBuffer();
  }

  void setTextSize(uint8_t size) {
    textSize = (size == 0) ? 1 : size;
    selectFont();
  }

  void setTextColor(uint16_t color) {
    textColor = color ? 1 : 0;
    oled.setDrawColor(textColor);
  }

  void setCursor(int16_t x, int16_t y) {
    cursorX = x;
    cursorY = y;
  }

  void setTextWrap(bool value) { wrap = value; }

  void getTextBounds(const String &text, int16_t x, int16_t y,
                     int16_t *x1, int16_t *y1,
                     uint16_t *w, uint16_t *h) {
    selectFont();
    int16_t width = oled.getStrWidth(text.c_str());
    if (x1) *x1 = x;
    if (y1) *y1 = y;
    if (w) *w = (uint16_t)((width > 0) ? width : 0);
    if (h) *h = (uint16_t)(textSize <= 1 ? 10 : 20);
  }

  size_t write(uint8_t c) override {
    drawCharInternal(c);
    return 1;
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawPixel(x, y);
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawLine(x0, y0, x1, y1);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawHLine(x, y, w);
  }

  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawFrame(x, y, w, h);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawBox(x, y, w, h);
  }

  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawRFrame(x, y, w, h, r);
  }

  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawRBox(x, y, w, h, r);
  }

  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawCircle(x, y, r);
  }

  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawDisc(x, y, r);
  }

  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawLine(x0, y0, x1, y1);
    oled.drawLine(x1, y1, x2, y2);
    oled.drawLine(x2, y2, x0, y0);
  }

  void fillScreen(uint16_t color) {
    oled.setDrawColor(color ? 1 : 0);
    oled.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  }
};

TankDisplay display;

uint8_t oledAddress = 0x3C;
bool oledReady = false;

// ============================================================
// PIN CONFIGURATION
// ============================================================

const uint8_t PIN_OLED_SDA = 21;
const uint8_t PIN_OLED_SCL = 22;

const uint8_t PIN_ULTRASONIC_TRIG = 5;
const uint8_t PIN_ULTRASONIC_ECHO = 18;

const uint8_t PIN_GAS_SENSOR = 34;

const uint8_t PIN_BUZZER = 25;
const uint8_t PIN_LED = 26;
const uint8_t PIN_BUTTON = 27;

// ============================================================
// USER SETTINGS
// ============================================================

const float TANK_HEIGHT_CM = 30.0f;
const float DISTANCE_CORRECTION_CM = 0.0f;

const bool LED_ACTIVE_HIGH = true;

const unsigned long GAS_WARMUP_TIME_MS = 30000UL;

const int GAS_WARNING_DELTA = 180;
const int GAS_DANGER_DELTA = 400;

const int GAS_WARNING_ABSOLUTE = 1500;
const int GAS_DANGER_ABSOLUTE = 2400;

const int WATER_EMPTY_PERCENT = 3;
const int WATER_LOW_PERCENT = 20;
const int WATER_HIGH_PERCENT = 85;
const int WATER_OVERFLOW_PERCENT = 95;

// ============================================================
// TIMING CONFIGURATION
// ============================================================

const unsigned long SENSOR_INTERVAL_MS = 500;
const unsigned long DISPLAY_INTERVAL_MS = 100;
const unsigned long BUTTON_DEBOUNCE_MS = 60;

const unsigned long LED_WARNING_INTERVAL_MS = 350;
const unsigned long LED_DANGER_INTERVAL_MS = 150;

const unsigned long BUZZER_WARNING_INTERVAL_MS = 1100;
const unsigned long BUZZER_DANGER_INTERVAL_MS = 500;

const unsigned long SPLASH_DURATION_MS = 3500;

// ============================================================
// HISTORY CONFIGURATION
// ============================================================

const uint8_t HISTORY_LENGTH = 24;

int waterHistory[HISTORY_LENGTH];
uint8_t historyIndex = 0;
bool historyReady = false;

// ============================================================
// ENUMERATIONS
// ============================================================

enum WaterState {
  WATER_SENSOR_ERROR,
  WATER_EMPTY,
  WATER_LOW,
  WATER_NORMAL,
  WATER_HIGH,
  WATER_OVERFLOW
};

enum GasState {
  GAS_WARMING,
  GAS_SAFE,
  GAS_WARNING,
  GAS_DANGER
};

enum ScreenMode {
  SCREEN_DASHBOARD,
  SCREEN_AIR,
  SCREEN_HISTORY,
  SCREEN_HEALTH
};

enum WaterTrend {
  TREND_UNKNOWN,
  TREND_RISING,
  TREND_FALLING,
  TREND_STABLE
};

// ============================================================
// GLOBAL SENSOR VALUES
// ============================================================

WaterState waterState = WATER_SENSOR_ERROR;
GasState gasState = GAS_WARMING;
ScreenMode screenMode = SCREEN_DASHBOARD;
WaterTrend waterTrend = TREND_UNKNOWN;

float measuredDistanceCm = -1.0f;
float waterLevelCm = 0.0f;

int waterPercent = -1;
int previousWaterPercent = -1;

int rawGasValue = 0;
int filteredGasValue = 0;
int gasBaseline = 0;

int highestWaterPercent = 0;
int highestGasValue = 0;

// ============================================================
// GLOBAL SYSTEM STATE
// ============================================================

unsigned long bootTime = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastBuzzerUpdate = 0;

uint16_t animationFrame = 0;

bool splashFinished = false;
bool ledState = false;
bool buzzerState = false;

bool sensorHasValidDistance = false;
bool gasSensorSaturated = false;

bool stableButtonState = HIGH;
bool previousButtonReading = HIGH;
unsigned long lastButtonChangeTime = 0;

// ============================================================
// TEXT HELPERS
// ============================================================

void setWhiteText() {
  display.setTextColor(SSD1306_WHITE);
}

void setBlackText() {
  display.setTextColor(SSD1306_BLACK);
}

void drawCenteredText(
  const String &text,
  int y,
  uint8_t size,
  uint16_t color = SSD1306_WHITE
) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.setTextSize(size);
  display.setTextColor(color);

  display.getTextBounds(
    text,
    0,
    y,
    &x1,
    &y1,
    &width,
    &height
  );

  int x = (SCREEN_WIDTH - width) / 2;

  display.setCursor(x, y);
  display.print(text);

  setWhiteText();
}

void drawRightText(
  const String &text,
  int rightX,
  int y,
  uint8_t size
) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);

  display.getTextBounds(
    text,
    0,
    y,
    &x1,
    &y1,
    &width,
    &height
  );

  display.setCursor(rightX - width, y);
  display.print(text);
}

// ============================================================
// STATUS TEXT
// ============================================================

String getWaterStateText() {
  switch (waterState) {

    case WATER_SENSOR_ERROR:
      return "SENSOR ERR";

    case WATER_EMPTY:
      return "EMPTY";

    case WATER_LOW:
      return "LOW";

    case WATER_NORMAL:
      return "NORMAL";

    case WATER_HIGH:
      return "HIGH";

    case WATER_OVERFLOW:
      return "OVERFLOW";
  }

  return "UNKNOWN";
}

String getGasStateText() {
  switch (gasState) {

    case GAS_WARMING:
      return "WARMING";

    case GAS_SAFE:
      return "SAFE";

    case GAS_WARNING:
      return "WARNING";

    case GAS_DANGER:
      return "DANGER";
  }

  return "UNKNOWN";
}

String getTrendText() {
  switch (waterTrend) {

    case TREND_RISING:
      return "RISING";

    case TREND_FALLING:
      return "FALLING";

    case TREND_STABLE:
      return "STABLE";

    default:
      return "WAITING";
  }
}

String getTrendSymbol() {
  switch (waterTrend) {

    case TREND_RISING:
      return "^";

    case TREND_FALLING:
      return "v";

    case TREND_STABLE:
      return "-";

    default:
      return "?";
  }
}

// ============================================================
// SYSTEM CONDITIONS
// ============================================================

bool waterWarningActive() {
  return waterState == WATER_LOW ||
         waterState == WATER_HIGH;
}

bool waterDangerActive() {
  return waterState == WATER_OVERFLOW;
}

bool gasWarningActive() {
  return gasState == GAS_WARNING;
}

bool gasDangerActive() {
  return gasState == GAS_DANGER;
}

bool systemDangerActive() {
  return waterDangerActive() ||
         gasDangerActive();
}

bool systemWarningActive() {
  return waterWarningActive() ||
         gasWarningActive();
}

// ============================================================
// BUTTON
// ============================================================

bool buttonPressedEvent() {

  bool currentReading = digitalRead(PIN_BUTTON);

  if (currentReading != previousButtonReading) {
    lastButtonChangeTime = millis();
  }

  if (
    millis() - lastButtonChangeTime >=
    BUTTON_DEBOUNCE_MS
  ) {

    if (currentReading != stableButtonState) {

      stableButtonState = currentReading;

      if (stableButtonState == LOW) {

        previousButtonReading = currentReading;
        return true;
      }
    }
  }

  previousButtonReading = currentReading;

  return false;
}

void moveToNextScreen() {

  switch (screenMode) {

    case SCREEN_DASHBOARD:
      screenMode = SCREEN_AIR;
      break;

    case SCREEN_AIR:
      screenMode = SCREEN_HISTORY;
      break;

    case SCREEN_HISTORY:
      screenMode = SCREEN_HEALTH;
      break;

    case SCREEN_HEALTH:
      screenMode = SCREEN_DASHBOARD;
      break;
  }
}

// ============================================================
// BUZZER
// ============================================================

void buzzerOff() {
  noTone(PIN_BUZZER);
  buzzerState = false;
}

void playStartupSound() {

  tone(PIN_BUZZER, 700, 100);
  delay(130);

  tone(PIN_BUZZER, 950, 100);
  delay(130);

  tone(PIN_BUZZER, 1300, 160);
  delay(200);

  buzzerOff();
}

void playPageSound() {
  tone(PIN_BUZZER, 1200, 70);
}

void playWarningTone() {
  tone(PIN_BUZZER, 900, 120);
}

void playDangerTone() {
  tone(PIN_BUZZER, 1600, 180);
}

void updateBuzzer() {

  if (!splashFinished) {
    buzzerOff();
    return;
  }

  unsigned long now = millis();

  if (systemDangerActive()) {

    if (
      now - lastBuzzerUpdate >=
      BUZZER_DANGER_INTERVAL_MS
    ) {

      lastBuzzerUpdate = now;

      if (buzzerState) {
        buzzerOff();
      } else {
        playDangerTone();
        buzzerState = true;
      }
    }

    return;
  }

  if (systemWarningActive()) {

    if (
      now - lastBuzzerUpdate >=
      BUZZER_WARNING_INTERVAL_MS
    ) {

      lastBuzzerUpdate = now;

      if (buzzerState) {
        buzzerOff();
      } else {
        playWarningTone();
        buzzerState = true;
      }
    }

    return;
  }

  buzzerOff();
}

// ============================================================
// LED
// ============================================================

void writeLED(bool state) {

  bool output =
    LED_ACTIVE_HIGH ? state : !state;

  digitalWrite(
    PIN_LED,
    output ? HIGH : LOW
  );
}

void updateLED() {

  if (!splashFinished) {
    writeLED(false);
    return;
  }

  unsigned long now = millis();

  if (systemDangerActive()) {

    if (
      now - lastLEDUpdate >=
      LED_DANGER_INTERVAL_MS
    ) {

      lastLEDUpdate = now;

      ledState = !ledState;

      writeLED(ledState);
    }

    return;
  }

  if (systemWarningActive()) {

    if (
      now - lastLEDUpdate >=
      LED_WARNING_INTERVAL_MS
    ) {

      lastLEDUpdate = now;

      ledState = !ledState;

      writeLED(ledState);
    }

    return;
  }

  writeLED(true);
}

// ============================================================
// ULTRASONIC SENSOR
// ============================================================

float readSingleDistance() {

  digitalWrite(
    PIN_ULTRASONIC_TRIG,
    LOW
  );

  delayMicroseconds(3);

  digitalWrite(
    PIN_ULTRASONIC_TRIG,
    HIGH
  );

  delayMicroseconds(10);

  digitalWrite(
    PIN_ULTRASONIC_TRIG,
    LOW
  );

  unsigned long duration =
    pulseIn(
      PIN_ULTRASONIC_ECHO,
      HIGH,
      30000UL
    );

  if (duration == 0) {
    return -1.0f;
  }

  float distance =
    (duration * 0.0343f) / 2.0f;

  distance += DISTANCE_CORRECTION_CM;

  if (
    distance < 2.0f ||
    distance > 400.0f
  ) {
    return -1.0f;
  }

  return distance;
}

float readAverageDistance() {

  const uint8_t sampleCount = 5;

  float total = 0.0f;
  uint8_t validSamples = 0;

  for (
    uint8_t i = 0;
    i < sampleCount;
    i++
  ) {

    float reading =
      readSingleDistance();

    if (reading > 0.0f) {

      total += reading;
      validSamples++;
    }

    delay(8);
  }

  if (validSamples == 0) {
    return -1.0f;
  }

  return total / validSamples;
}

// ============================================================
// WATER CALCULATION
// ============================================================

void updateWaterTrend() {

  if (
    waterPercent < 0 ||
    previousWaterPercent < 0
  ) {

    waterTrend = TREND_UNKNOWN;
    return;
  }

  int difference =
    waterPercent -
    previousWaterPercent;

  if (difference >= 2) {

    waterTrend = TREND_RISING;

  } else if (difference <= -2) {

    waterTrend = TREND_FALLING;

  } else {

    waterTrend = TREND_STABLE;
  }
}

void addWaterHistory(int value) {

  if (value < 0) {
    return;
  }

  waterHistory[historyIndex] =
    value;

  historyIndex++;

  if (historyIndex >= HISTORY_LENGTH) {

    historyIndex = 0;
    historyReady = true;
  }
}

void calculateWaterLevel() {

  if (measuredDistanceCm < 0.0f) {

    sensorHasValidDistance = false;

    waterState = WATER_SENSOR_ERROR;

    waterPercent = -1;

    waterLevelCm = 0.0f;

    waterTrend = TREND_UNKNOWN;

    return;
  }

  sensorHasValidDistance = true;

  waterLevelCm =
    TANK_HEIGHT_CM -
    measuredDistanceCm;

  if (waterLevelCm < 0.0f) {
    waterLevelCm = 0.0f;
  }

  if (waterLevelCm > TANK_HEIGHT_CM) {
    waterLevelCm = TANK_HEIGHT_CM;
  }

  previousWaterPercent =
    waterPercent;

  waterPercent =
    (int)round(
      (waterLevelCm /
       TANK_HEIGHT_CM) *
      100.0f
    );

  waterPercent =
    constrain(
      waterPercent,
      0,
      100
    );

  updateWaterTrend();

  if (
    waterPercent >
    highestWaterPercent
  ) {

    highestWaterPercent =
      waterPercent;
  }

  addWaterHistory(
    waterPercent
  );

  if (
    waterPercent <=
    WATER_EMPTY_PERCENT
  ) {

    waterState =
      WATER_EMPTY;

  } else if (
    waterPercent <=
    WATER_LOW_PERCENT
  ) {

    waterState =
      WATER_LOW;

  } else if (
    waterPercent >=
    WATER_OVERFLOW_PERCENT
  ) {

    waterState =
      WATER_OVERFLOW;

  } else if (
    waterPercent >=
    WATER_HIGH_PERCENT
  ) {

    waterState =
      WATER_HIGH;

  } else {

    waterState =
      WATER_NORMAL;
  }
}

// ============================================================
// GAS SENSOR
// ============================================================

int readGasAverage() {

  const uint8_t sampleCount = 12;

  long total = 0;

  for (
    uint8_t i = 0;
    i < sampleCount;
    i++
  ) {

    total +=
      analogRead(
        PIN_GAS_SENSOR
      );

    delay(2);
  }

  return
    (int)(
      total /
      sampleCount
    );
}

void updateGasSensor() {

  rawGasValue = readGasAverage();

  // ADC=4095 continuously means the analog input is saturated.
  // Do not use a saturated reading to create/update the baseline.
  gasSensorSaturated = (rawGasValue >= 4090);

  if (gasSensorSaturated) {
    gasState = GAS_WARMING;
    return;
  }

  if (filteredGasValue == 0 || filteredGasValue >= 4090) {
    filteredGasValue = rawGasValue;
  } else {
    filteredGasValue =
      (filteredGasValue * 85L + rawGasValue * 15L) / 100L;
  }

  if (filteredGasValue > highestGasValue) {
    highestGasValue = filteredGasValue;
  }

  unsigned long elapsed = millis() - bootTime;

  if (elapsed < GAS_WARMUP_TIME_MS) {
    gasState = GAS_WARMING;

    if (gasBaseline == 0) {
      gasBaseline = filteredGasValue;
    } else {
      gasBaseline =
        (gasBaseline * 95L + filteredGasValue * 5L) / 100L;
    }

    return;
  }

  if (gasBaseline <= 0) {
    gasBaseline = filteredGasValue;
  }

  int gasDifference = filteredGasValue - gasBaseline;

  if (gasDifference >= GAS_DANGER_DELTA ||
      filteredGasValue >= GAS_DANGER_ABSOLUTE) {
    gasState = GAS_DANGER;

  } else if (gasDifference >= GAS_WARNING_DELTA ||
             filteredGasValue >= GAS_WARNING_ABSOLUTE) {
    gasState = GAS_WARNING;

  } else {
    gasState = GAS_SAFE;

    gasBaseline =
      (gasBaseline * 995L + filteredGasValue * 5L) / 1000L;
  }
}
// ============================================================
// SENSOR UPDATE
// ============================================================

void updateSensors() {

  unsigned long now =
    millis();

  if (
    now - lastSensorRead <
    SENSOR_INTERVAL_MS
  ) {

    return;
  }

  lastSensorRead = now;

  measuredDistanceCm =
    readAverageDistance();

  calculateWaterLevel();

  updateGasSensor();

  animationFrame++;

  if (animationFrame > 1000) {
    animationFrame = 0;
  }
}

// ============================================================
// OLED GRAPHIC HELPERS
// ============================================================

void drawHeader(
  const String &title,
  uint8_t page
) {

  display.fillRect(
    0,
    0,
    SCREEN_WIDTH,
    12,
    SSD1306_WHITE
  );

  display.setTextColor(
    SSD1306_BLACK
  );

  display.setTextSize(1);

  display.setCursor(3, 3);

  display.print(title);

  int signalLevel =
    1 +
    ((animationFrame / 4) % 3);

  for (int i = 0; i < 3; i++) {

    int barHeight =
      3 + i * 2;

    if (i < signalLevel) {

      display.fillRect(
        108 + i * 4,
        10 - barHeight,
        2,
        barHeight,
        SSD1306_BLACK
      );

    } else {

      display.drawRect(
        108 + i * 4,
        10 - barHeight,
        2,
        barHeight,
        SSD1306_BLACK
      );
    }
  }

  display.setCursor(123, 3);

  display.print(page);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.drawFastHLine(
    0,
    13,
    SCREEN_WIDTH,
    SSD1306_WHITE
  );
}

void drawNavigationDots(
  uint8_t selected
) {

  const int startX = 52;
  const int y = 61;

  for (
    uint8_t i = 0;
    i < 4;
    i++
  ) {

    int x =
      startX + i * 8;

    if (i == selected) {

      display.fillCircle(
        x,
        y,
        2,
        SSD1306_WHITE
      );

    } else {

      display.drawCircle(
        x,
        y,
        1,
        SSD1306_WHITE
      );
    }
  }
}

void drawButtonHint() {

  display.setTextSize(1);

  display.setCursor(3, 57);

  display.print("BTN:NEXT");
}

void drawCheckIcon(
  int x,
  int y
) {

  display.drawCircle(
    x + 7,
    y + 7,
    7,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 3,
    y + 7,
    x + 6,
    y + 10,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 6,
    y + 10,
    x + 12,
    y + 3,
    SSD1306_WHITE
  );
}

void drawWarningIcon(
  int x,
  int y
) {

  display.drawTriangle(
    x + 8,
    y,
    x,
    y + 15,
    x + 16,
    y + 15,
    SSD1306_WHITE
  );

  display.setTextColor(
    SSD1306_BLACK
  );

  display.setTextSize(1);

  display.setCursor(
    x + 6,
    y + 5
  );

  display.print("!");

  display.setTextColor(
    SSD1306_WHITE
  );
}

void drawCrossIcon(
  int x,
  int y
) {

  display.drawCircle(
    x + 8,
    y + 8,
    8,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 4,
    y + 4,
    x + 12,
    y + 12,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 12,
    y + 4,
    x + 4,
    y + 12,
    SSD1306_WHITE
  );
}

void drawStatusBadge(
  const String &text,
  int x,
  int y,
  int width
) {

  bool danger =
    systemDangerActive();

  if (danger) {

    display.fillRoundRect(
      x,
      y,
      width,
      13,
      3,
      SSD1306_WHITE
    );

    display.setTextColor(
      SSD1306_BLACK
    );

  } else {

    display.drawRoundRect(
      x,
      y,
      width,
      13,
      3,
      SSD1306_WHITE
    );

    display.setTextColor(
      SSD1306_WHITE
    );
  }

  display.setTextSize(1);

  int16_t x1;
  int16_t y1;
  uint16_t widthText;
  uint16_t heightText;

  display.getTextBounds(
    text,
    0,
    0,
    &x1,
    &y1,
    &widthText,
    &heightText
  );

  display.setCursor(
    x + (width - widthText) / 2,
    y + 3
  );

  display.print(text);

  display.setTextColor(
    SSD1306_WHITE
  );
}

void drawProgressBar(
  int x,
  int y,
  int width,
  int height,
  int percent
) {

  percent =
    constrain(
      percent,
      0,
      100
    );

  display.drawRoundRect(
    x,
    y,
    width,
    height,
    3,
    SSD1306_WHITE
  );

  int fillWidth =
    ((width - 4) * percent) /
    100;

  if (fillWidth > 0) {

    display.fillRoundRect(
      x + 2,
      y + 2,
      fillWidth,
      height - 4,
      2,
      SSD1306_WHITE
    );
  }
}

// ============================================================
// ANIMATED TANK
// ============================================================

void drawAnimatedTank(
  int x,
  int y,
  int width,
  int height
) {

  display.drawRoundRect(
    x,
    y,
    width,
    height,
    4,
    SSD1306_WHITE
  );

  display.drawRoundRect(
    x + 2,
    y + 2,
    width - 4,
    height - 4,
    3,
    SSD1306_WHITE
  );

  if (waterPercent < 0) {

    display.setTextSize(1);

    display.setCursor(
      x + 10,
      y + 22
    );

    display.print("NO");

    display.setCursor(
      x + 7,
      y + 32
    );

    display.print("DATA");

    return;
  }

  int innerX =
    x + 4;

  int innerY =
    y + 4;

  int innerWidth =
    width - 8;

  int innerHeight =
    height - 8;

  int fillHeight =
    (innerHeight * waterPercent) /
    100;

  int waterTop =
    innerY +
    innerHeight -
    fillHeight;

  if (fillHeight > 0) {

    display.fillRect(
      innerX,
      waterTop,
      innerWidth,
      fillHeight,
      SSD1306_WHITE
    );

    int waveOffset =
      ((animationFrame / 2) % 2 == 0)
      ? 0
      : 2;

    for (
      int waveX = innerX + 2;
      waveX <
        innerX +
        innerWidth -
        2;
      waveX += 8
    ) {

      display.drawPixel(
        waveX + waveOffset,
        waterTop,
        SSD1306_BLACK
      );

      display.drawPixel(
        waveX + 2 + waveOffset,
        waterTop + 1,
        SSD1306_BLACK
      );
    }

    if (fillHeight > 12) {

      int bubbleX =
        innerX +
        6 +
        (
          (animationFrame * 2) %
          max(
            1,
            innerWidth - 10
          )
        );

      int bubbleY =
        innerY +
        innerHeight -
        5 -
        (
          (animationFrame * 2) %
          max(
            1,
            fillHeight - 4
          )
        );

      display.drawPixel(
        bubbleX,
        bubbleY,
        SSD1306_BLACK
      );
    }
  }

  String text =
    String(waterPercent) +
    "%";

  int16_t x1;
  int16_t y1;
  uint16_t textWidth;
  uint16_t textHeight;

  display.setTextSize(1);

  display.getTextBounds(
    text,
    0,
    0,
    &x1,
    &y1,
    &textWidth,
    &textHeight
  );

  int textX =
    x +
    (width - textWidth) /
    2;

  int textY =
    y +
    (height - textHeight) /
    2;

  if (
    waterTop <
    textY + textHeight
  ) {

    display.setTextColor(
      SSD1306_BLACK
    );

  } else {

    display.setTextColor(
      SSD1306_WHITE
    );
  }

  display.setCursor(
    textX,
    textY
  );

  display.print(text);

  display.setTextColor(
    SSD1306_WHITE
  );
}

// ============================================================
// OLED SCREENS
// ============================================================

void drawSplashScreen() {

  display.clearDisplay();

  display.drawRoundRect(
    2,
    2,
    124,
    59,
    6,
    SSD1306_WHITE
  );

  int scanY =
    10 +
    ((millis() / 30) % 44);

  display.drawFastHLine(
    9,
    scanY,
    110,
    SSD1306_WHITE
  );

  drawCenteredText(
    "SMART TANK",
    15,
    2
  );

  drawCenteredText(
    "GUARDIAN",
    36,
    2
  );

  display.setTextSize(1);

  display.setCursor(
    91,
    51
  );

  uint8_t dots =
    (millis() / 400) % 4;

  for (
    uint8_t i = 0;
    i < dots;
    i++
  ) {

    display.print(".");
  }

  display.display();
}

void drawWarmupScreen() {

  display.clearDisplay();

  drawHeader(
    "AIR CALIBRATION",
    2
  );

  unsigned long elapsed =
    millis() -
    bootTime;

  int progress =
    (int)(
      (elapsed * 100UL) /
      GAS_WARMUP_TIME_MS
    );

  progress =
    constrain(
      progress,
      0,
      100
    );

  drawCenteredText(
    "SENSOR WARMUP",
    18,
    1
  );

  drawCenteredText(
    "PLEASE WAIT",
    28,
    1
  );

  display.drawRoundRect(
    8,
    39,
    112,
    12,
    4,
    SSD1306_WHITE
  );

  int fillWidth =
    (progress * 106) /
    100;

  if (fillWidth > 0) {

    display.fillRoundRect(
      11,
      42,
      fillWidth,
      6,
      2,
      SSD1306_WHITE
    );
  }

  drawCenteredText(
    String(progress) + "%",
    53,
    1
  );

  display.display();
}

void drawDashboardScreen() {

  display.clearDisplay();

  drawHeader(
    "TANK GUARDIAN",
    1
  );

  drawAnimatedTank(
    5,
    17,
    43,
    37
  );

  display.setTextSize(1);

  display.setCursor(
    54,
    17
  );

  display.print(
    "WATER LEVEL"
  );

  display.setTextSize(2);

  display.setCursor(
    54,
    27
  );

  if (waterPercent >= 0) {

    display.print(
      waterPercent
    );

    display.print("%");

  } else {

    display.print("--");
  }

  display.setTextSize(1);

  display.setCursor(
    54,
    43
  );

  display.print("TREND ");

  display.print(
    getTrendSymbol()
  );

  display.setCursor(
    54,
    53
  );

  if (
    waterState ==
    WATER_OVERFLOW
  ) {

    display.print(
      "STOP FILL"
    );

  } else if (
    waterState ==
    WATER_LOW
  ) {

    display.print(
      "REFILL"
    );

  } else if (
    waterState ==
    WATER_SENSOR_ERROR
  ) {

    display.print(
      "CHECK SENSOR"
    );

  } else {

    display.print(
      getWaterStateText()
    );
  }

  if (
    waterState ==
    WATER_OVERFLOW
  ) {

    drawCrossIcon(
      108,
      17
    );

  } else if (
    waterState ==
      WATER_LOW ||
    waterState ==
      WATER_HIGH ||
    waterState ==
      WATER_SENSOR_ERROR
  ) {

    drawWarningIcon(
      108,
      17
    );

  } else {

    drawCheckIcon(
      108,
      17
    );
  }

  drawNavigationDots(0);

  drawButtonHint();

  display.display();
}

void drawAirScreen() {

  display.clearDisplay();

  drawHeader(
    "AIR SAFETY",
    2
  );

  display.setTextSize(1);

  display.setCursor(
    4,
    17
  );

  display.print(
    "RAW AIR VALUE"
  );

  display.setTextSize(2);

  display.setCursor(
    4,
    27
  );

  if (
    gasState ==
    GAS_WARMING
  ) {

    display.print("--");

  } else {

    display.print(
      filteredGasValue
    );
  }

  display.setTextSize(1);

  display.setCursor(
    4,
    49
  );

  display.print(
    "BASE "
  );

  display.print(
    gasBaseline
  );

  int gasPercent =
    map(
      constrain(
        filteredGasValue,
        0,
        4095
      ),
      0,
      4095,
      0,
      100
    );

  display.setTextSize(1);

  display.setCursor(
    73,
    17
  );

  display.print(
    "AIR LEVEL"
  );

  drawProgressBar(
    73,
    27,
    50,
    9,
    gasPercent
  );

  display.setCursor(
    73,
    40
  );

  if (
    gasState ==
    GAS_DANGER
  ) {

    display.print(
      "EVACUATE"
    );

    drawStatusBadge(
      "DANGER",
      73,
      48,
      50
    );

  } else if (
    gasState ==
    GAS_WARNING
  ) {

    display.print(
      "CHECK AIR"
    );

    drawStatusBadge(
      "WARNING",
      73,
      48,
      50
    );

  } else if (
    gasState ==
    GAS_WARMING
  ) {

    display.print(
      "CALIBRATE"
    );

    drawStatusBadge(
      "WAIT",
      73,
      48,
      50
    );

  } else {

    display.print(
      "AIR CLEAN"
    );

    drawStatusBadge(
      "SAFE",
      73,
      48,
      50
    );
  }

  drawNavigationDots(1);

  drawButtonHint();

  display.display();
}

void drawHistoryGraph() {

  const int graphX = 5;
  const int graphY = 20;
  const int graphWidth = 118;
  const int graphHeight = 30;

  display.drawRect(
    graphX,
    graphY,
    graphWidth,
    graphHeight,
    SSD1306_WHITE
  );

  display.drawFastHLine(
    graphX,
    graphY +
      graphHeight / 2,
    graphWidth,
    SSD1306_WHITE
  );

  display.drawFastHLine(
    graphX,
    graphY +
      graphHeight / 4,
    graphWidth,
    SSD1306_WHITE
  );

  display.drawFastHLine(
    graphX,
    graphY +
      (graphHeight * 3) / 4,
    graphWidth,
    SSD1306_WHITE
  );

  uint8_t count =
    historyReady
    ? HISTORY_LENGTH
    : historyIndex;

  if (count < 2) {

    drawCenteredText(
      "COLLECTING DATA",
      32,
      1
    );

    return;
  }

  int previousValue = -1;
  int previousX = -1;
  int previousY = -1;

  for (
    uint8_t i = 0;
    i < count;
    i++
  ) {

    uint8_t position;

    if (historyReady) {

      position =
        (
          historyIndex +
          i
        ) %
        HISTORY_LENGTH;

    } else {

      position = i;
    }

    int value =
      constrain(
        waterHistory[position],
        0,
        100
      );

    int x =
      graphX +
      2 +
      (
        i *
        (graphWidth - 5)
      ) /
      max(
        1,
        count - 1
      );

    int y =
      graphY +
      graphHeight -
      3 -
      (
        value *
        (graphHeight - 6) /
        100
      );

    if (
      previousValue >= 0 &&
      previousX >= 0 &&
      previousY >= 0
    ) {

      display.drawLine(
        previousX,
        previousY,
        x,
        y,
        SSD1306_WHITE
      );
    }

    display.fillCircle(
      x,
      y,
      1,
      SSD1306_WHITE
    );

    previousValue = value;
    previousX = x;
    previousY = y;
  }
}

void drawHistoryScreen() {

  display.clearDisplay();

  drawHeader(
    "LEVEL HISTORY",
    3
  );

  drawHistoryGraph();

  display.setTextSize(1);

  display.setCursor(
    5,
    53
  );

  display.print(
    "MAX "
  );

  display.print(
    highestWaterPercent
  );

  display.print("%");

  display.setCursor(
    72,
    53
  );

  display.print(
    "DIST "
  );

  if (
    measuredDistanceCm >=
    0.0f
  ) {

    display.print(
      measuredDistanceCm,
      0
    );

    display.print(
      "cm"
    );

  } else {

    display.print("--");
  }

  drawNavigationDots(2);

  drawButtonHint();

  display.display();
}

void drawHealthScreen() {

  display.clearDisplay();

  drawHeader(
    "SYSTEM HEALTH",
    4
  );

  display.setTextSize(1);

  display.setCursor(
    5,
    18
  );

  display.print(
    "ULTRASONIC"
  );

  display.setCursor(
    94,
    18
  );

  if (
    sensorHasValidDistance
  ) {

    display.print(
      "OK"
    );

  } else {

    display.print(
      "ERROR"
    );
  }

  display.setCursor(
    5,
    31
  );

  display.print(
    "GAS SENSOR"
  );

  display.setCursor(
    94,
    31
  );

  if (
    gasState ==
    GAS_WARMING
  ) {

    display.print(
      "WAIT"
    );

  } else {

    display.print(
      "OK"
    );
  }

  display.setCursor(
    5,
    44
  );

  display.print(
    "OLED DISPLAY"
  );

  display.setCursor(
    94,
    44
  );

  if (oledReady) {

    display.print(
      "OK"
    );

  } else {

    display.print(
      "ERROR"
    );
  }

  display.drawFastHLine(
    5,
    52,
    118,
    SSD1306_WHITE
  );

  display.setCursor(
    5,
    55
  );

  if (
    systemDangerActive()
  ) {

    display.print(
      "ACTION REQUIRED"
    );

  } else if (
    systemWarningActive()
  ) {

    display.print(
      "CHECK SYSTEM"
    );

  } else {

    display.print(
      "SYSTEM NOMINAL"
    );
  }

  drawNavigationDots(3);

  drawButtonHint();

  display.display();
}

// ============================================================
// EMERGENCY SCREEN
// ============================================================

void drawEmergencyScreen() {

  bool alternate =
    ((millis() / 350) % 2) == 0;

  if (alternate) {

    display.fillScreen(
      SSD1306_WHITE
    );

    display.setTextColor(
      SSD1306_BLACK
    );

  } else {

    display.clearDisplay();

    display.setTextColor(
      SSD1306_WHITE
    );
  }

  display.drawRect(
    2,
    2,
    124,
    60,
    SSD1306_BLACK
  );

  display.setTextSize(2);

  display.setCursor(
    16,
    7
  );

  display.print(
    "WARNING"
  );

  display.setTextSize(1);

  display.setCursor(
    14,
    29
  );

  if (
    waterState ==
    WATER_OVERFLOW
  ) {

    display.print(
      "TANK OVERFLOW"
    );

  } else if (
    gasState ==
    GAS_DANGER
  ) {

    display.print(
      "AIR DANGER"
    );

  } else {

    display.print(
      "SYSTEM DANGER"
    );
  }

  display.setCursor(
    20,
    42
  );

  display.print(
    "CHECK SYSTEM!"
  );

  display.setCursor(
    30,
    54
  );

  display.print(
    "BTN: CHANGE"
  );

  display.setTextColor(
    SSD1306_WHITE
  );

  display.display();
}

// ============================================================
// DISPLAY DISPATCHER
// ============================================================

void updateDisplay() {

  if (!oledReady) return;

  unsigned long now =
    millis();

  if (
    now - lastDisplayUpdate <
    DISPLAY_INTERVAL_MS
  ) {

    return;
  }

  lastDisplayUpdate = now;

  if (!splashFinished) {

    drawSplashScreen();

    return;
  }

  if (
    gasState ==
    GAS_WARMING
  ) {

    drawWarmupScreen();

    return;
  }

  if (
    systemDangerActive()
  ) {

    drawEmergencyScreen();

    return;
  }

  switch (screenMode) {

    case SCREEN_DASHBOARD:

      drawDashboardScreen();

      break;

    case SCREEN_AIR:

      drawAirScreen();

      break;

    case SCREEN_HISTORY:

      drawHistoryScreen();

      break;

    case SCREEN_HEALTH:

      drawHealthScreen();

      break;
  }
}

// ============================================================
// SERIAL DEBUGGING
// ============================================================

void printSerialStatus() {

  static unsigned long lastPrint = 0;

  if (
    millis() - lastPrint <
    2000
  ) {

    return;
  }

  lastPrint =
    millis();

  Serial.println();

  Serial.println(
    "=========================================="
  );

  Serial.println(
    "        SMART TANK GUARDIAN"
  );

  Serial.println(
    "=========================================="
  );

  Serial.print(
    "Distance: "
  );

  Serial.print(
    measuredDistanceCm,
    1
  );

  Serial.println(
    " cm"
  );

  Serial.print(
    "Water level: "
  );

  if (waterPercent >= 0) {

    Serial.print(
      waterPercent
    );

    Serial.println("%");

  } else {

    Serial.println(
      "ERROR"
    );
  }

  Serial.print(
    "Water status: "
  );

  Serial.println(
    getWaterStateText()
  );

  Serial.print(
    "Water trend: "
  );

  Serial.println(
    getTrendText()
  );

  Serial.print(
    "Gas raw: "
  );

  Serial.println(
    rawGasValue
  );

  Serial.print(
    "Gas filtered: "
  );

  Serial.println(
    filteredGasValue
  );

  Serial.print(
    "Gas baseline: "
  );

  Serial.println(
    gasBaseline
  );

  Serial.print(
    "Gas status: "
  );

  Serial.println(
    getGasStateText()
  );

  Serial.println(
    "=========================================="
  );
}

// ============================================================
// SETUP
// ============================================================

bool initializeOLED() {

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);

  auto i2cPresent = [](uint8_t address) -> bool {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
  };

  if (i2cPresent(0x3C)) {
    oledAddress = 0x3C;
    display.begin(SSD1306_SWITCHCAPVCC, oledAddress);
    oledReady = true;
    return true;
  }

  if (i2cPresent(0x3D)) {
    oledAddress = 0x3D;
    display.begin(SSD1306_SWITCHCAPVCC, oledAddress);
    oledReady = true;
    return true;
  }

  oledReady = false;
  return false;
}
void setup() {

  Serial.begin(
    115200
  );

  delay(200);

  pinMode(
    PIN_ULTRASONIC_TRIG,
    OUTPUT
  );

  pinMode(
    PIN_ULTRASONIC_ECHO,
    INPUT
  );

  pinMode(
    PIN_BUZZER,
    OUTPUT
  );

  pinMode(
    PIN_LED,
    OUTPUT
  );

  pinMode(
    PIN_BUTTON,
    INPUT_PULLUP
  );

  digitalWrite(
    PIN_ULTRASONIC_TRIG,
    LOW
  );

  writeLED(false);

  buzzerOff();

  analogReadResolution(12);

  analogSetPinAttenuation(
    PIN_GAS_SENSOR,
    ADC_11db
  );

  if (!initializeOLED()) {
    Serial.println("OLED NOT FOUND at 0x3C/0x3D - continuing sensor operation");
  } else {
    Serial.print("OLED SH1106 FOUND at 0x");
    Serial.println(oledAddress, HEX);
  }

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextWrap(false);

  for (
    uint8_t i = 0;
    i < HISTORY_LENGTH;
    i++
  ) {

    waterHistory[i] = 0;
  }

  bootTime =
    millis();

  drawSplashScreen();

  playStartupSound();

  delay(300);
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  unsigned long now =
    millis();

  if (
    !splashFinished &&
    now - bootTime >=
      SPLASH_DURATION_MS
  ) {

    splashFinished = true;
  }

  if (
    buttonPressedEvent() &&
    splashFinished
  ) {

    moveToNextScreen();

    playPageSound();
  }

  updateSensors();

  updateLED();

  updateBuzzer();

  updateDisplay();

  printSerialStatus();

  delay(1);
}
