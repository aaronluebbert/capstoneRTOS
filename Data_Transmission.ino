/*************************************************************
  Project: Smart Home Energy Monitor (Capstone)
  Role: Wireless Gateway (ESP32)
  
  Hardware Connections:
  ESP32 GPIO 16 (RX2) <--> STM32 TX Pin
  ESP32 GPIO 17 (TX2) <--> STM32 RX Pin
  ESP32 GND           <--> STM32 GND (Crucial!)
 *************************************************************/

// Template ID and Name
#define BLYNK_TEMPLATE_ID "TMPL2FNdm_b-r"
#define BLYNK_TEMPLATE_NAME "Home Energy"
#define BLYNK_AUTH_TOKEN "tAli5flTOLFOiZCElF12FEAC-xMqhlS3"

// Include Libraries
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// WiFi Credentials
char ssid[] = "Saheels iPhone";
char pass[] = "pAssword";

// UART Pins (Serial2)
#define RXD2 16
#define TXD2 17

void setup() {
  // 1. Debug Serial
  Serial.begin(115200);
  
  // 2. Data Serial (UART to STM32)
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  Serial.println("System Starting...");

  // 3. Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Connected to Blynk!");
}

void loop() {
  Blynk.run(); 
  receiveFromSTM32(); 
}

// -----------------------------------------------------------------
// FUNCTION: Receive Data (STM32 -> ESP32 -> Phone)
// -----------------------------------------------------------------
void receiveFromSTM32() {
  if (Serial2.available()) {
    // Expected Format: "12.5,120.0,0.1\n" (Power,Voltage,Current)
    String dataPacket = Serial2.readStringUntil('\n');
    
    // Find positions of the commas
    int firstComma = dataPacket.indexOf(',');
    int secondComma = dataPacket.indexOf(',', firstComma + 1);
    
    // Ensure we found both commas (valid packet)
    if (firstComma > 0 && secondComma > 0) {
      
      // 1. Extract Strings
      String powerStr = dataPacket.substring(0, firstComma);
      String voltageStr = dataPacket.substring(firstComma + 1, secondComma);
      String currentStr = dataPacket.substring(secondComma + 1);
      
      // 2. Convert to Floats
      float power = powerStr.toFloat();
      float voltage = voltageStr.toFloat();
      float current = currentStr.toFloat();
      
      // 3. Debug Print
      Serial.print("Rx STM32 -> P: ");
      Serial.print(power);
      Serial.print("W | V: ");
      Serial.print(voltage);
      Serial.print("V | I: ");
      Serial.print(current);
      Serial.println("A");

      // 4. Send to Blynk
      Blynk.virtualWrite(V1, power);    // Power
      Blynk.virtualWrite(V2, voltage);  // Voltage
      Blynk.virtualWrite(V4, current);  // Current (New!)
    }
  }
}

// -----------------------------------------------------------------
// FUNCTION: Control Command (Phone -> ESP32 -> STM32)
// -----------------------------------------------------------------
BLYNK_WRITE(V3) {
  int pinValue = param.asInt(); 
  
  if (pinValue == 1) {
    Serial.println("Sending: FAN ON");
    Serial2.write('1'); 
  } else {
    Serial.println("Sending: FAN OFF");
    Serial2.write('0'); 
  }
}