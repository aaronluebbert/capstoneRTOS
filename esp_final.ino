#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// blynk config
#define BLYNK_TEMPLATE_ID "TMPL2FNdm_b-r"
#define BLYNK_TEMPLATE_NAME "Home Energy"
#define BLYNK_AUTH_TOKEN "tAli5flTOLFOiZCElF12FEAC-xMqhlS3"

char ssid[] = "Saheels iPhone";
char pass[] = "pAssword";

// pins
#define MOSFET_PIN 32
#define SWITCH_PIN 33   // momentary limit switch

// uart2 pins to stm32
#define RXD2 16
#define TXD2 17

// ina219 on esp32
Adafruit_INA219 inaESP;

// fan state
bool fanState = false;
bool lastSwitchState = HIGH;

void checkSwitch();
void sendFanData();
void receiveSTM32Data();

void setup() {
  // usb debug
  Serial.begin(115200);
  delay(1000);
  Serial.println("system starting...");

  // uart to stm32
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  // i2c for esp ina219
  Wire.begin(21, 22);
  inaESP.begin();
  inaESP.setCalibration_16V_400mA();

  // io setup
  pinMode(MOSFET_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(MOSFET_PIN, LOW);

  // connect to blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("connected to blynk");
}

void loop() {
  Blynk.run();
  checkSwitch();
  sendFanData();
  receiveSTM32Data(); // expects power,voltage,current from stm32
}

void checkSwitch() {
  bool current = digitalRead(SWITCH_PIN);

  if (lastSwitchState == HIGH && current == LOW) {
    fanState = !fanState;
    digitalWrite(MOSFET_PIN, fanState ? HIGH : LOW);

    Serial.println(fanState ? "fan on" : "fan off");
    Blynk.virtualWrite(V10, fanState ? 1 : 0);

    delay(250);  // debounce
  }

  lastSwitchState = current;
}

void sendFanData() {
  if (!fanState) return;

  float voltage = inaESP.getBusVoltage_V();       // v
  float current = inaESP.getCurrent_mA() / 1000;  // a
  float power   = inaESP.getPower_mW() / 1000;    // w

  // debug
  Serial.print("[esp ina219] v: "); Serial.print(voltage); Serial.print(" ");
  Serial.print("i: "); Serial.print(current); Serial.print(" ");
  Serial.print("p: "); Serial.println(power);

  // blynk pins for esp sensor
  Blynk.virtualWrite(V1, power);
  Blynk.virtualWrite(V2, voltage);
  Blynk.virtualWrite(V4, current);

  // optional forward to stm32
  String packet = String(power) + "," + String(voltage) + "," + String(current) + "\n";
  Serial2.print(packet);
}

void receiveSTM32Data() {
  if (Serial2.available()) {
    String dataPacket = Serial2.readStringUntil('\n');

    int firstComma = dataPacket.indexOf(',');
    int secondComma = dataPacket.indexOf(',', firstComma + 1);

    if (firstComma > 0 && secondComma > 0) {
      float power   = dataPacket.substring(0, firstComma).toFloat();
      float voltage = dataPacket.substring(firstComma + 1, secondComma).toFloat();
      float current = dataPacket.substring(secondComma + 1).toFloat();

      // debug
      Serial.print("[stm ina219] p: "); Serial.print(power); Serial.print(" ");
      Serial.print("v: "); Serial.print(voltage); Serial.print(" ");
      Serial.print("i: "); Serial.println(current);

      // blynk pins for stm sensor
      Blynk.virtualWrite(V5, power);
      Blynk.virtualWrite(V6, voltage);
      Blynk.virtualWrite(V7, current);
    }
  }
}
