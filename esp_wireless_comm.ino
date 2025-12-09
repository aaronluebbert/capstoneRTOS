  /*************************************************************
   BLYNK + Two INA219 + Toggle Limit Switch
   ESP32-WROOM-32 Integrated Sketch
*************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL2FNdm_b-r"
#define BLYNK_TEMPLATE_NAME "Home Energy"
#define BLYNK_AUTH_TOKEN "tAli5flTOLFOiZCElF12FEAC-xMqhlS3"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include <Wire.h>
#include <Adafruit_INA219.h>

char ssid[] = "Saheels iPhone";
char pass[] = "pAssword";

// Pins
#define MOSFET_PIN 32
#define SWITCH_PIN 33   // momentary limit switch

// UART2 pins (STM32)
#define RXD2 16
#define TXD2 17

// INA219 objects
Adafruit_INA219 inaESP;      // ESP32-connected INA219 (fan/LED)
Adafruit_INA219 inaSTM32;    // optional: read data via UART from STM32

// Fan state
bool fanState = false;
bool lastSwitchState = HIGH;

void setup() {
  // Debug serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("System Starting...");

  // UART2 (STM32)
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  // I2C for ESP32 INA219 (fan/LED)
  Wire.begin(21, 22);
  inaESP.begin();
  inaESP.setCalibration_16V_400mA();  // suitable for 5V fan/LED

  // Pins
  pinMode(MOSFET_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(MOSFET_PIN, LOW);

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Connected to Blynk!");
}

void loop() {
  Blynk.run();
  checkSwitch();
  sendFanData();
  receiveSTM32Data(); // Read INA219 data from STM32 via UART
}

/*************************************************************
    TOGGLE SWITCH HANDLER
*************************************************************/
void checkSwitch() {
  bool current = digitalRead(SWITCH_PIN);

  if (lastSwitchState == HIGH && current == LOW) {
    fanState = !fanState;
    digitalWrite(MOSFET_PIN, fanState ? HIGH : LOW);

    Serial.println(fanState ? "Fan ON" : "Fan OFF");
    Blynk.virtualWrite(V10, fanState ? 1 : 0);

    delay(250);  // debounce
  }

  lastSwitchState = current;
}

/*************************************************************
    READ ESP32 INA219 & SEND TO BLYNK + SERIAL
*************************************************************/
void sendFanData() {
  if (!fanState) return;

  float voltage = inaESP.getBusVoltage_V();       // V
  float current = inaESP.getCurrent_mA() / 1000;  // A
  float power   = inaESP.getPower_mW() / 1000;    // W

  // Debug
  Serial.print("[ESP32 INA219] Voltage: "); Serial.print(voltage); Serial.println(" V");
  Serial.print("[ESP32 INA219] Current: "); Serial.print(current * 1000); Serial.println(" mA");
  Serial.print("[ESP32 INA219] Power: ");   Serial.print(power); Serial.println(" W");
  Serial.println("-------------------");

  // Blynk Virtual Pins
  Blynk.virtualWrite(V1, power);      // Power (W)
  Blynk.virtualWrite(V2, voltage);    // Voltage (V)
  Blynk.virtualWrite(V4, current);    // Current (A)

  // Also forward to STM32 (optional)
  String packet = String(power) + "," + String(voltage) + "," + String(current) + "\n";
  Serial2.print(packet);
}

/*************************************************************
    RECEIVE STM32 DATA (INA219 #2) VIA UART
*************************************************************/
void receiveSTM32Data() {
  if (Serial2.available()) {
    String dataPacket = Serial2.readStringUntil('\n');

    int firstComma = dataPacket.indexOf(',');
    int secondComma = dataPacket.indexOf(',', firstComma + 1);

    if (firstComma > 0 && secondComma > 0) {
      float power   = dataPacket.substring(0, firstComma).toFloat();
      float voltage = dataPacket.substring(firstComma + 1, secondComma).toFloat();
      float current = dataPacket.substring(secondComma + 1).toFloat();

      Serial.print("[STM32 INA219] Power: "); Serial.print(power); Serial.print(" W | ");
      Serial.print("Voltage: "); Serial.print(voltage); Serial.print(" V | ");
      Serial.print("Current: "); Serial.print(current); Serial.println(" A");

      // Send STM32 data to Blynk
      Blynk.virtualWrite(V5, power);
      Blynk.virtualWrite(V6, voltage);
      Blynk.virtualWrite(V7, current);
    }
  }
}