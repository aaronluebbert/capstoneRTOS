#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

#define MOSFET_PIN 32
#define SWITCH_PIN 33

bool fanState = false;
bool lastSwitchState = HIGH;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);   // SDA=21, SCL=22

  ina219.begin();
  ina219.setCalibration_16V_400mA();  

  pinMode(MOSFET_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  digitalWrite(MOSFET_PIN, LOW);
}

void loop() {
  bool currentSwitch = digitalRead(SWITCH_PIN);

  // Toggle logic
  if (lastSwitchState == HIGH && currentSwitch == LOW) {
    fanState = !fanState;
    digitalWrite(MOSFET_PIN, fanState ? HIGH : LOW);
    Serial.println(fanState ? "Fan ON" : "Fan OFF");
    delay(200);
  }

  lastSwitchState = currentSwitch;

  // Print sensor data only while fan is running
  if (fanState) {
    float busVoltage = ina219.getBusVoltage_V();      // V
    float current_mA = ina219.getCurrent_mA();        // mA
    float power_mW = ina219.getPower_mW();            // mW

    Serial.print("Voltage: "); Serial.print(busVoltage); Serial.println(" V");
    Serial.print("Current: "); Serial.print(current_mA); Serial.println(" mA");
    Serial.print("Power: ");   Serial.print(power_mW);   Serial.println(" mW");
    Serial.println("---");
  }

  delay(100);
}
