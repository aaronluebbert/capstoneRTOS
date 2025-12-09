#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// pins
const uint8_t LOADING_PIN = PB13;
const uint8_t TEMP_PIN    = PB1;
const uint8_t THERM_PIN   = A0;

// led strips
const uint8_t NUM_LEDS = 8;
Adafruit_NeoPixel loadingStrip(NUM_LEDS, LOADING_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel tempStrip(NUM_LEDS, TEMP_PIN, NEO_GRB + NEO_KHZ800);

// adc thresholds
uint16_t COOL_MAX = 800;
uint16_t WARM_MAX = 1000;

// shared state
volatile uint16_t thermRaw = 0;
volatile uint8_t level = 0; // 0 cool, 1 warm, 2 hot

// task struct
struct Task {
  uint32_t period;
  uint32_t last;
  void (*run)();
};

void thermistorTask();
void loadingTask();
void tempStripTask();
void uartTask();

Task tasks[] = {
  {200, 0, thermistorTask},
  {100, 0, loadingTask},
  {200, 0, tempStripTask},
  {500, 0, uartTask},
};
const uint8_t NUM_TASKS = sizeof(tasks) / sizeof(tasks[0]);

// task 1: read thermistor + classify
void thermistorTask() {
  thermRaw = analogRead(THERM_PIN);

  if (thermRaw <= COOL_MAX) level = 0;
  else if (thermRaw <= WARM_MAX) level = 1;
  else level = 2;

  // quick fake numbers for bring-up
  Voltage = 12.0f; 
  Current = (level == 0) ? 0.2f : (level == 1) ? 0.6f : 1.0f;
  Power   = Voltage * Current;
}

// task 2: loading animation
void loadingTask() {
  static uint8_t idx = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    loadingStrip.setPixelColor(i, 0);
  }
  loadingStrip.setPixelColor(idx, loadingStrip.Color(0, 80, 255));
  loadingStrip.show();

  idx = (idx + 1) % NUM_LEDS;
}

// task 3: temp strip color
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

// task 4: uart send power,voltage,current
void uartTask() {
  // format: power,voltage,current\n
  Serial1.print(Power, 2);
  Serial1.print(",");
  Serial1.print(Voltage, 2);
  Serial1.print(",");
  Serial1.print(Current, 2);
  Serial1.print("\n");
}

// setup/loop
void setup() {
  loadingStrip.begin();
  tempStrip.begin();
  loadingStrip.show();
  tempStrip.show();

  Serial.begin(115200);    // usb debug
  Serial1.begin(115200);   // uart to esp (pa9/pa10)
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
