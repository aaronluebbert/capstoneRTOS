// rtos_therm_fan_ledstrip.ino — cooperative rtos-ish loop

#include <Arduino.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>

// pins
// adjust if your wiring differs
#define THERM_PIN       A0         // analog input for thermistor
#define FAN_PIN         PB0        // fan transistor / relay input
#define LED_PIN         PC13       // built-in led (active-low)
#define LED_STRIP_PIN   D7         // data pin to din of ws2812 strip

// ws2812 strip config
#define LED_COUNT       8

Adafruit_NeoPixel strip(LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

// scheduler config
struct Task {
  int   (*run)(int state);       // take an int state, return next state
  uint32_t period_us;            // period in microseconds
  uint32_t next_due;             // micros timestamp for next release
  bool enabled;
  const char* name;
  int state;
};

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

// thermistor model (beta equation)
// 10k ntc, beta 3950, series 10k, 3.3 v adc, 12-bit adc (0..4095)
static const float TH_NOMINAL_OHMS = 10000.0f;
static const float TH_BETA         = 3950.0f;
static const float TH_SERIES_OHMS  = 10000.0f;
static const float TH_REF_TEMP_K   = 298.15f;     // 25 c in kelvin
static const float VREF_VOLT       = 3.3f;
static const float ADC_MAX_COUNTS  = 4095.0f;

// hysteresis thresholds
static const float TEMP_ON_C  = 35.0f;   // fan turns on at or above this
static const float TEMP_OFF_C = 32.0f;   // fan turns off at or below this

// moving average window
#define AVG_WINDOW 32

// shared state
volatile bool  g_fan_on = false;
volatile float g_temp_c = 25.0f;

// moving average buffer
static float avg_buf[AVG_WINDOW];
static uint32_t avg_idx = 0;
static uint32_t avg_count = 0;

// forward decls
int task_sensor(int s);       // 1 khz
int task_control(int s);      // 50 ms
int task_fan_led(int s);      // 10 ms
int task_led_strip(int s);    // 100 ms

// task table
static Task tasks[] = {
  { task_sensor,    1000,     0, true, "Sensor",   0 },   // 1 khz
  { task_control,   50000,    0, true, "Control",  0 },   // 50 ms
  { task_fan_led,   10000,    0, true, "FanLED",   0 },   // 10 ms
  { task_led_strip, 100000,   0, true, "LEDStrip", 0 },   // 100 ms
};

// helper to schedule next release
static inline void schedule_task(Task& t, uint32_t now_us) {
  t.next_due = now_us + t.period_us;
}

// adc read and conversion
// divider: vout = vref * (r_th / (r_th + r_series))
static float counts_to_celsius(uint16_t counts) {
  if (counts == 0) counts = 1;
  if (counts >= (uint16_t)ADC_MAX_COUNTS) counts = (uint16_t)(ADC_MAX_COUNTS - 1);

  float vout = (VREF_VOLT * (float)counts) / ADC_MAX_COUNTS;
  float r_th = (TH_SERIES_OHMS * vout) / (VREF_VOLT - vout);

  float inv_t = (1.0f / TH_REF_TEMP_K) + (1.0f / TH_BETA) * logf(r_th / TH_NOMINAL_OHMS);
  float temp_k = 1.0f / inv_t;
  return temp_k - 273.15f;
}

// helper for led strip: set all pixels to one color
static void strip_set_all(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// setup hardware
void setup() {
  // fan and status led
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);        // fan off

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);       // pc13 is active-low, high = led off

  // thermistor adc
  pinMode(THERM_PIN, INPUT_ANALOG);

  // led strip init
  strip.begin();
  strip.setBrightness(20);           // adjust as needed
  strip.clear();
  strip.show();

  // precompute initial schedule
  uint32_t now = micros();
  for (size_t i = 0; i < ARRAY_LEN(tasks); ++i) {
    schedule_task(tasks[i], now);
  }
}

// cooperative dispatcher using micros
void loop() {
  uint32_t now = micros();

  // simple priority scan in table order
  for (size_t i = 0; i < ARRAY_LEN(tasks); ++i) {
    Task& t = tasks[i];
    if (!t.enabled) continue;

    int32_t delta = (int32_t)(now - t.next_due);
    if (delta >= 0) {
      t.state = t.run(t.state);
      schedule_task(t, now);
    }
  }

  // optional tiny sleep to reduce spin if nothing due
  // delayMicroseconds(50);
}

// sensor task: read adc, convert to celsius, update moving average
int task_sensor(int s) {
  uint32_t acc = 0;
  const int nsamp = 4;
  for (int i = 0; i < nsamp; ++i) acc += (uint32_t)analogRead(THERM_PIN);
  uint16_t raw = (uint16_t)(acc / nsamp);

  float temp_c = counts_to_celsius(raw);

  avg_buf[avg_idx] = temp_c;
  avg_idx = (avg_idx + 1) % AVG_WINDOW;
  if (avg_count < AVG_WINDOW) avg_count++;

  float sum = 0.0f;
  for (uint32_t i = 0; i < avg_count; ++i) sum += avg_buf[i];
  g_temp_c = sum / (float)avg_count;

  return s;
}

// control task: apply hysteresis and drive fan pin
int task_control(int s) {
  bool want_on = g_fan_on;

  if (!g_fan_on && g_temp_c >= TEMP_ON_C) want_on = true;
  if (g_fan_on && g_temp_c <= TEMP_OFF_C) want_on = false;

  if (want_on != g_fan_on) {
    g_fan_on = want_on;
    digitalWrite(FAN_PIN, g_fan_on ? HIGH : LOW);
  }
  return s;
}

// fan led task: mirror fan state to led (pc13 active-low)
int task_fan_led(int s) {
  digitalWrite(LED_PIN, g_fan_on ? LOW : HIGH);
  return s;
}

// led strip task: nonblocking version of your animation
int task_led_strip(int s) {
  // state layout:
  // 0 = red chase
  // 1 = solid green
  // 2 = solid blue
  // 3 = solid white
  // 4 = off

  // this task runs every 100 ms
  static int chase_idx = 0;
  static int step_count = 0;

  switch (s) {
    case 0: {
      // red chase, 8 steps * 100 ms = 800 ms
      strip.clear();
      strip.setPixelColor(chase_idx, strip.Color(255, 0, 0));
      strip.show();

      chase_idx = (chase_idx + 1) % LED_COUNT;
      step_count++;

      if (step_count >= LED_COUNT) {
        step_count = 0;
        s = 1;        // move to solid green
      }
    } break;

    case 1: {
      // solid green for 500 ms -> 5 steps * 100 ms
      if (step_count == 0) {
        strip_set_all(0, 255, 0);
      }
      step_count++;
      if (step_count >= 5) {
        step_count = 0;
        s = 2;
      }
    } break;

    case 2: {
      // solid blue for 500 ms
      if (step_count == 0) {
        strip_set_all(0, 0, 255);
      }
      step_count++;
      if (step_count >= 5) {
        step_count = 0;
        s = 3;
      }
    } break;

    case 3: {
      // solid white for 500 ms
      if (step_count == 0) {
        strip_set_all(255, 255, 255);
      }
      step_count++;
      if (step_count >= 5) {
        step_count = 0;
        s = 4;
      }
    } break;

    case 4: {
      // off for 1000 ms -> 10 steps * 100 ms
      if (step_count == 0) {
        strip_set_all(0, 0, 0);
      }
      step_count++;
      if (step_count >= 10) {
        step_count = 0;
        chase_idx = 0;
        s = 0;      // back to red chase
      }
    } break;

    default:
      s = 0;
      step_count = 0;
      chase_idx = 0;
      break;
  }

  return s;
}
