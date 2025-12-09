#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h>

// ====== Pins ======
const uint8_t LOADING_PIN = PB13;
const uint8_t TEMP_PIN    = PB15;
const uint8_t THERM_PIN   = A0;   // ADC

// ====== LED strips ======
const uint8_t NUM_LEDS = 8;
Adafruit_NeoPixel loadingStrip(NUM_LEDS, LOADING_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel tempStrip(NUM_LEDS, TEMP_PIN, NEO_GRB + NEO_KHZ800);

// ====== INA219 ======
Adafruit_INA219 ina219; // default addr (usually 0x40)

// ====== RAW ADC thresholds ======
uint16_t COOL_MAX = 800;
uint16_t WARM_MAX = 1000;

// ====== Shared state ======
volatile uint16_t thermRaw = 0;
volatile uint8_t level = 0; // 0 cool, 1 warm, 2 hot

// INA values to send
volatile float Power_mW = 0.0f;
volatile float Voltage_V = 0.0f;
volatile float Current_mA = 0.0f;

// ====== Minimal task struct ======
struct Task {
  uint32_t period;
  uint32_t last;
  void (*run)();
};

// Task prototypes
void thermistorTask();
void loadingTask();
void tempStripTask();
void inaTask();
void uartTask();

// Keep it simple: 5 tasks total
Task tasks[] = {
  {200, 0, thermistorTask},
  {100, 0, loadingTask},
  {200, 0, tempStripTask},
  {200, 0, inaTask},
  {500, 0, uartTask},
};
const uint8_t NUM_TASKS = sizeof(tasks) / sizeof(tasks[0]);

// ============================
// Task 1: thermistor -> level
// ============================
void thermistorTask() {
  thermRaw = analogRead(THERM_PIN);

  if (thermRaw <= COOL_MAX) level = 0;
  else if (thermRaw <= WARM_MAX) level = 1;
  else level = 2;
}

// ============================
// Task 2: loading animation
// ============================
void loadingTask() {
  static uint8_t idx = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    loadingStrip.setPixelColor(i, 0);
  }
  loadingStrip.setPixelColor(idx, loadingStrip.Color(0, 80, 255));
  loadingStrip.show();

  idx = (idx + 1) % NUM_LEDS;
}

// ============================
// Task 3: temp strip color
// ============================
void tempStripTask() {
  uint32_t color;

  if (level == 0) color = tempStrip.Color(0, 255, 0);
  else if (level == 1) color = tempStrip.Color(255, 160, 0);
  else color = tempStrip.Color(255, 0, 0);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    tempStrip.setPixelColor(i, color);
  }
  tempStrip.show();
}

// ============================
// Task 4: read one INA219
// ============================
void inaTask() {
  Voltage_V  = ina219.getBusVoltage_V();
  Current_mA = ina219.getCurrent_mA();
  Power_mW   = ina219.getPower_mW();
}

// ============================
// Task 5: UART send values
// Format: Power,Voltage,Current
// ============================
void uartTask() {
  Serial1.print(Power_mW, 2);
  Serial1.print(",");
  Serial1.print(Voltage_V, 2);
  Serial1.print(",");
  Serial1.print(Current_mA, 2);
  Serial1.print("\n");

  // optional mirror to PC for sanity
  Serial.print(Power_mW, 2);
  Serial.print(",");
  Serial.print(Voltage_V, 2);
  Serial.print(",");
  Serial.print(Current_mA, 2);
  Serial.print("\n");
}

// ============================
// Setup/Loop
// ============================
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200); // STM UART1: TX=PA9 RX=PA10

  Wire.begin(); // default I2C: PB6/PB7

  loadingStrip.begin();
  tempStrip.begin();
  loadingStrip.show();
  tempStrip.show();

  if (!ina219.begin()) {
    Serial.println("INA219 not found");
  }

  // Header once (if your ESP expects it)
  Serial1.println("Power,Voltage,Current");
  Serial.println("Power,Voltage,Current");
}

void loop() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < NUM_TASKS; i++) {
    if (now - tasks[i].last >= tasks[i].period) {
      tasks[i].last = now;
      tasks[i].run();
    }
  }
}