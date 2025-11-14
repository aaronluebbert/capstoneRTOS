// rtos_therm_fan_arduino.ino — cooperative rtos-ish loop for stm32f103c8t6 using arduino ide
#include <Arduino.h>
#include <math.h>

// pins
//  adjust if your wiring differs
#define THERM_PIN       A0        // pa0 on blue pill
#define FAN_PIN         PB0       // transistor/relay input
#define LED_PIN         PC13      // built-in led on blue pill (active-low)

// scheduler config
//  three periodic tasks, priority = index order
struct Task {
  int   (*run)(int state);     //  take an int state, return next state
  uint32_t period_us;          //  period in microseconds
  uint32_t next_due;           //  micros timestamp for next release
  bool enabled;
  const char* name;
  int state;
};

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

// thermistor model (beta equation)
//  10k ntc, beta 3950, series 10k, 3.3 v adc, 12-bit adc on f103 (0..4095)
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
int task_sensor(int s);     //  1 kHz
int task_control(int s);    // 50 ms
int task_fan_led(int s);    // 10 ms

// task table
static Task tasks[] = {
  { task_sensor,   1000,    0, true, "Sensor",   0 },   // 1 kHz
  { task_control,  50000,   0, true, "Control",  0 },   // 50 ms
  { task_fan_led,  10000,   0, true, "FanLED",   0 },   // 10 ms
};

// helper to schedule next release
static inline void schedule_task(Task& t, uint32_t now_us) {
  t.next_due = now_us + t.period_us;
}

// adc read and conversion
//  divider: vout = vref * (r_th / (r_th + r_series))
//  solve r_th, then beta equation to temperature
static float counts_to_celsius(uint16_t counts) {
  if (counts == 0) counts = 1;
  if (counts >= (uint16_t)ADC_MAX_COUNTS) counts = (uint16_t)(ADC_MAX_COUNTS - 1);

  float vout = (VREF_VOLT * (float)counts) / ADC_MAX_COUNTS;
  float r_th = (TH_SERIES_OHMS * vout) / (VREF_VOLT - vout);

  float inv_t = (1.0f / TH_REF_TEMP_K) + (1.0f / TH_BETA) * logf(r_th / TH_NOMINAL_OHMS);
  float temp_k = 1.0f / inv_t;
  return temp_k - 273.15f;
}

// setup hardware
void setup() {
  // note: on stm32duino core, pin names like PB0 and PC13 are valid
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);        // fan off

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);       // pc13 is active-low, high = led off

  pinMode(THERM_PIN, INPUT_ANALOG);  // ensure analog mode on A0

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

    // handle micros wrap by signed delta
    int32_t delta = (int32_t)(now - t.next_due);
    if (delta >= 0) {
      t.state = t.run(t.state);
      schedule_task(t, now);
    }
  }

  // optional tiny sleep to reduce spin if nothing due right now
  // delayMicroseconds(50);
}

//  sensor task: read adc, convert to celsius, update moving average
int task_sensor(int s) {
  // multiple samples help tame switching noise
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

//  control task: apply hysteresis and drive fan pin
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

//  fan led task: mirror fan state to led (pc13 active-low)
int task_fan_led(int s) {
  digitalWrite(LED_PIN, g_fan_on ? LOW : HIGH);
  return s;
}
