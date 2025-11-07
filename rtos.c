#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// kernel config
#define RTOS_TICK_HZ     1000u   // 1 kHz tick
#define MAX_TASKS        8
#define MAX_PRIORITIES   4

typedef uint32_t tick_t;

// ipc: semaphore and queue
typedef struct { volatile int count; } sem_t;

typedef struct {
  uint16_t head, tail, size, capacity;
  void *buf;
  uint16_t elem_sz;
} rtos_queue_t;

// task control block
//  take an int state, return next state
typedef int (*task_fn)(int state);

typedef struct {
  task_fn run;
  int     state;
  tick_t  period;       // in ticks
  tick_t  next_release; // absolute tick when to run next
  uint8_t priority;     // 0 = highest
  bool    enabled;
  const char *name;
} tcb_t;

// globals
static volatile tick_t g_ticks;
static tcb_t tasks[MAX_TASKS];
static uint8_t task_count;

// timebase
void SysTick_Handler(void) {
  HAL_IncTick();
  g_ticks++;
}

// ipc impl
static inline void sem_init(sem_t *s, int initial) { s->count = initial; }
static inline void sem_give(sem_t *s) { __disable_irq(); s->count++; __enable_irq(); }
static inline bool sem_take(sem_t *s) {
  bool ok = false;
  __disable_irq();
  if (s->count > 0) { s->count--; ok = true; }
  __enable_irq();
  return ok;
}

bool q_init(rtos_queue_t *q, void *buf, uint16_t elem_sz, uint16_t cap) {
  q->buf = buf; q->elem_sz = elem_sz; q->capacity = cap;
  q->size = 0; q->head = 0; q->tail = 0;
  return true;
}

bool q_push(rtos_queue_t *q, const void *elem) {
  bool ok = false;
  __disable_irq();
  if (q->size < q->capacity) {
    memcpy((uint8_t*)q->buf + (q->tail * q->elem_sz), elem, q->elem_sz);
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;
    ok = true;
  }
  __enable_irq();
  return ok;
}

bool q_pop(rtos_queue_t *q, void *out) {
  bool ok = false;
  __disable_irq();
  if (q->size > 0) {
    memcpy(out, (uint8_t*)q->buf + (q->head * q->elem_sz), q->elem_sz);
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    ok = true;
  }
  __enable_irq();
  return ok;
}

// kernel api
int rtos_add_task(const char *name, task_fn fn, int init_state, tick_t period, uint8_t priority) {
  if (task_count >= MAX_TASKS) return -1;
  tasks[task_count] = (tcb_t){
    .run = fn,
    .state = init_state,
    .period = period,
    .next_release = 0 + period,
    .priority = priority,
    .enabled = true,
    .name = name
  };
  return task_count++;
}

static inline tick_t now(void) { return g_ticks; }

// priority-ordered dispatcher
static void rtos_dispatch(void) {
  for (uint8_t pr = 0; pr < MAX_PRIORITIES; ++pr) {
    for (uint8_t i = 0; i < task_count; ++i) {
      tcb_t *t = &tasks[i];
      if (!t->enabled || t->priority != pr) continue;
      if ((int32_t)(now() - t->next_release) >= 0) {
        t->state = t->run(t->state);
        t->next_release += t->period;
      }
    }
  }
}

// app data stubs
typedef struct { float v, i, p; uint32_t ts; } sample_t;
static rtos_queue_t sample_q; static sample_t sample_buf[64];

// forward decls
int task_sensor(int s);     //  1 kHz
int task_comm(int s);       // 20 Hz
int task_control(int s);    // 100 Hz
int task_heartbeat(int s);  // 1 Hz

int main(void) {
  HAL_Init();
  SystemClock_Config();                              // set 72 mhz
  SysTick_Config(SystemCoreClock / RTOS_TICK_HZ);    // 1 kHz rtos tick

  // TODO: MX_GPIO_Init(); MX_I2C2_Init(); MX_USART3_UART_Init(); MX_TIM3_Init();

  q_init(&sample_q, sample_buf, sizeof(sample_t), 64);

  rtos_add_task("Sensor",    task_sensor,    0, 1,    0);
  rtos_add_task("Control",   task_control,   0, 10,   1);
  rtos_add_task("Comms",     task_comm,      0, 50,   2);
  rtos_add_task("Heartbeat", task_heartbeat, 0, 1000, 3);

  for (;;) {
    rtos_dispatch();
  }
}

//  task implementations
//  take an int state, return next state

int task_sensor(int s) {
  // read ina219 via i2c and compute moving averages
  // enqueue latest sample
  return s;
}

int task_control(int s) {
  // consume latest sample
  // apply thresholds and drive mosfet/relay
  // read exti flags for switch and debounce
  return s;
}

int task_comm(int s) {
  // serialize latest data and send over uart to esp32
  // parse downlink commands and update thresholds
  return s;
}

int task_heartbeat(int s) {
  // toggle led or print diagnostics
  return s;
}
