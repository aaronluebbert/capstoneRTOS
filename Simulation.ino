/*************************************************************
  Project: Wireless Test (Simulation Mode)
  Role: Standalone Test with Power, Voltage, and Current
 *************************************************************/

// 1. BLYNK CONFIGURATION
// -----------------------------------------------------------
#define BLYNK_TEMPLATE_ID "TMPL2FNdm_b-r"
#define BLYNK_TEMPLATE_NAME "Home Energy"
#define BLYNK_AUTH_TOKEN "tAli5flTOLFOiZCElF12FEAC-xMqhlS3"

// 2. WIFI CONFIGURATION
// -----------------------------------------------------------
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "Saheels iPhone";        
char pass[] = "pAssword";    

// Define the Fan Pin (e.g., GPIO 4)
#define FAN_PIN 4 

// 3. SETUP TIMERS
// -----------------------------------------------------------
BlynkTimer timer;

// This function generates fake data (Simulating the STM32)
void sendFakeSensorData() {
  // 1. Generate Random Power (10W to 50W)
  float fakePower = random(100, 500) / 10.0; 
  
  // 2. Generate Random Voltage (118V to 122V)
  float fakeVoltage = random(1180, 1220) / 10.0;

  // 3. Calculate Current (I = P / V)
  // We calculate this so the math actually makes sense!
  float fakeCurrent = (fakePower / fakeVoltage) * 1000;

  // Print to Serial Monitor
  Serial.print("[SIMULATION] Power: ");
  Serial.print(fakePower);
  Serial.print("W | Voltage: ");
  Serial.print(fakeVoltage);
  Serial.print("V | Current: ");
  Serial.print(fakeCurrent, 3); // Print with 3 decimal places
  Serial.println("A");

  // Send to Blynk App
  Blynk.virtualWrite(V1, fakePower);   // Power on V1
  Blynk.virtualWrite(V2, fakeVoltage); // Voltage on V2
  Blynk.virtualWrite(V4, fakeCurrent); // Current on V4 <--- NEW!
}

void setup() {
  // Debug console
  Serial.begin(115200);

  // Configure Fan Pin
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW); // Start with Fan OFF

  Serial.println("Starting Wireless Test...");
  
  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Run the data function every 2 seconds
  timer.setInterval(2000L, sendFakeSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}

// 4. TEST CONTROL (Button on Pin V3)
// -----------------------------------------------------------
BLYNK_WRITE(V3) {
  int pinValue = param.asInt(); // 1 = ON, 0 = OFF
  
  if (pinValue == 1) {
    Serial.println(">>> APP BUTTON PRESSED: TURN FAN ON <<<");
    digitalWrite(FAN_PIN, HIGH); 
  } else {
    Serial.println(">>> APP BUTTON PRESSED: TURN FAN OFF <<<");
    digitalWrite(FAN_PIN, LOW); 
  }
}