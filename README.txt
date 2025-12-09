# home energy monitor — stm32 rtos + esp32 blynk gateway

this repo contains two sketches that work together:

1) **stm32f103c8t6 (blue pill)**  
   a very small cooperative “rtos” that:
   - reads a 10k thermistor (raw adc)
   - drives two 8-led neopixel strips:
     - loading animation (always running)
     - temperature indicator (green/yellow/red)
   - reads **one ina219** over i2c
   - sends **power,voltage,current** over uart to the esp32

2) **esp32-wroom-32**  
   a gateway that:
   - reads **one ina219** locally over i2c (fan/led rail)
   - toggles a fan through a mosfet using a momentary limit switch
   - receives the stm32 csv over uart
   - pushes both data sets to blynk

---

## contents
- `esp32` sketch: blynk + local ina219 + fan toggle + uart rx from stm32  
- `stm32` sketch: cooperative task loop + thermistor + neopixels + ina219 + uart tx

---

## hardware
- stm32f103c8t6 “blue pill”
- esp32-wroom-32
- 2x ina219 current sensors  
  - **ina #1 on stm32 i2c**
  - **ina #2 on esp32 i2c**
- 2x ws2812/neopixel strips, **8 leds each**
- 10k thermistor + fixed resistor (voltage divider)
- dc fan + logic-level mosfet
- momentary limit switch
- external 5v supply for neopixels + fan (as needed)

---

## electrical notes
- **common ground** between stm32, esp32, led power, and fan power is required.
- neopixels:
  - power from a stable 5v supply (not from the blue pill)
  - add 330–470Ω series resistor on each data line
  - add ~1000µf capacitor across 5v/gnd near the strips
  - if you see flicker, use a 3.3v→5v level shifter for data

---

## wiring

### stm32

**neopixels**
- loading strip data → `pb13`
- temp strip data → `pb15`
- both strips 5v + gnd from external supply, gnd shared with stm32

**thermistor**
- voltage divider powered from **3.3v**
- divider midpoint → `a0`
- other end → gnd  
  (order of thermistor vs fixed resistor doesn’t matter for raw-threshold logic, but it will invert the curve)

**ina219 #1 on stm32**
- sda → `pb7`
- scl → `pb6`
- vcc → 3.3v
- gnd → gnd

**uart to esp32**
- stm32 `pa9` (tx1) → esp32 `gpio16` (rx2)
- stm32 `pa10` (rx1) ← esp32 `gpio17` (tx2) *(optional; only needed if you later add two-way messages)*

---

### esp32

**ina219 #2 on esp32**
- sda → `gpio21`
- scl → `gpio22`
- vcc → 3.3v
- gnd → gnd

**fan control**
- mosfet gate → `gpio32`
- fan power from appropriate supply
- add a gate pulldown resistor if needed

**limit switch**
- switch input → `gpio33`
- uses `input_pullup` in code, so wire switch to gnd

---

## data format

### stm32 → esp32 (uart)
the stm32 sends a header once, then values:


**current stm32 units**
the stm32 uses:
- `getPower_mW()` → **mW**
- `getCurrent_mA()` → **mA**
- `getBusVoltage_V()` → **V**

the esp32 code currently treats parsed values as **w / v / a** for blynk.

recommended options:
- **option a (preferred):** convert on stm32 before sending  
  - power_w = power_mw / 1000  
  - current_a = current_ma / 1000
- **option b:** keep as-is but label widgets accordingly (mW, mA)

---

## software dependencies

install via arduino library manager:

**stm32 sketch**
- adafruit neopixel
- adafruit ina219

**esp32 sketch**
- blynk
- adafruit ina219

---

## board packages
- **stm32**: stm32duino core  
  select a blue pill compatible target, e.g.:
  - “bluepill f103c8” or
  - “generic stm32f103c series”
- **esp32**: espressif esp32 core

---

## build + flash

### 1) stm32 build
1. open the stm32 sketch
2. select the blue pill board
3. confirm pins match the hardware:
   - loading neopixel: `pb13`
   - temp neopixel: `pb15`
   - thermistor: `a0`
   - i2c: `pb6/pb7`
   - uart1: `pa9/pa10`
4. upload (st-link or your preferred method)
5. open serial monitor @ 115200 to confirm:
   - thermistor levels change
   - ina219 initializes (no “not found”)
   - csv lines mirror to usb

**temperature thresholds**
the current raw thresholds are:
- `COOL_MAX = 800`
- `WARM_MAX = 1000`

these are expected to be tuned to the actual divider and environment.

---

### 2) esp32 build
1. open the esp32 sketch
2. update:
   - blynk template id/name/auth token
   - wifi ssid/password
3. confirm pins:
   - mosfet: `32`
   - switch: `33`
   - uart2: rx `16`, tx `17`
   - i2c: sda `21`, scl `22`
4. upload
5. open serial monitor @ 115200 to confirm:
   - local ina219 reads when fan is on
   - stm32 uart csv is parsed and printed
   - blynk connection succeeds

---

## blynk virtual pin map

**esp32-local ina219 (fan/led rail)**
- `V1` power
- `V2` voltage
- `V4` current

**stm32 ina219 (received over uart)**
- `V5` power
- `V6` voltage
- `V7` current

**fan state**
- `V10` 0/1

---

## expected behavior

**stm32**
- loading strip runs continuously
- temp strip shows:
  - green = cool
  - yellow = warm
  - red = hot
- thermistor thresholds drive only the leds (fan logic is on esp32)
- ina219 measurements are sent over uart

**esp32**
- limit switch toggles fan state
- when fan is on:
  - reads local ina219 and updates blynk
- always:
  - listens for stm32 csv and updates blynk

---

## troubleshooting

**no uart data on esp32**
- confirm shared ground
- confirm crossed tx/rx:
  - stm `pa9` → esp `gpio16`
- confirm baud matches 115200
- verify stm32 is sending by watching its usb serial mirror

**ina219 not found**
- confirm sda/scl pins
- confirm 3.3v power
- run an i2c scanner on the target board if needed

**thermistor “not reading”**
- use `A0` in code (already set)
- power divider from 3.3v (not 5v)
- optional stability settings:
  ```cpp
  analogReadResolution(12);
  pinMode(A0, INPUT_ANALOG);
