/*
References:
- ESP32Servo: https://github.com/madhephaestus/ESP32Servo (Author: madhephaestus)
- LiquidCrystal_I2C: https://github.com/johnrickman/LiquidCrystal_I2C (Author: johnrickman)
- ArduinoJson: https://github.com/bblanchon/ArduinoJson (Author: Benoît Blanchon)
- WiFi: https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi (Author: Espressif Systems)
- HTTPClient: https://github.com/espressif/arduino-esp32/tree/master/libraries/HTTPClient (Author: Espressif Systems)
- Wire: https://github.com/espressif/arduino-esp32/tree/master/libraries/Wire (Author: Espressif Systems)
- GitHub Copilot: Used for autocompletion assistance
*/

//Include Libraries
#include "ESP32Servo.h"
#include "../lib/LiquidCrystal_I2C-master/LiquidCrystal_I2C.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>


//Variable definitions
int servoPin = 19; //servo pin
int minAngle = 0; //minimum permissible hardware angle
int maxAngle = 180; //maximum permissible hardware angle
//int currentServoAngle = 0; //current servo angle

// variables for web server responses
int importedUserAngle = 0; // from the website
bool realtimeTracking = true; //default to auto control

String jsonData = "";

//Setting up the lcd and servo
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

// Replace with your network credentials
const char* ssid = "Coverdale";
const char* password = "NOHACKSSS";


bool fault = false;
bool faultDisplayed = false;
const int faultButton = 32;
const int faultLED = 33;
const int SCL_PIN = 22;
const int SDA_PIN = 21; // Changed to 21 as 33 is used for faultLED

float voltage = 0.0;
float current = 0.0;
float angle = 0.0;

// On fault at plant (button pushed)
void faultButtonPress() {
  fault = true;
  Serial.print("Fault detected");
  digitalWrite(faultLED, HIGH);
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Initialize fault button and LED and attached interrupt
  pinMode(faultButton, INPUT_PULLUP);
  pinMode(faultLED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(faultButton), faultButtonPress, FALLING);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  //setup wire for I2C communication
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  //setup lcd and servo
  lcd.init();
  lcd.backlight();
  myservo.attach(servoPin); //attach to pin 2
}

//read I2C data from nano for voltage, current, and angle
void readI2CData() {
  Wire.requestFrom(0x55, 32); // Request 32 bytes from I2C address 0x55
  jsonData = "";
  while (Wire.available()) {
    char c = Wire.read();
    if (c == 0xFF) break; // End of data
    jsonData += c;
  }
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  
  voltage = doc["V"];
  current = doc["C"];
  angle = doc["A"];
  
  Serial.print("Voltage: ");
  Serial.println(voltage);
  Serial.print("Current: ");
  Serial.println(current);
  Serial.print("Sun Angle: ");
  Serial.println(angle);
}

// for override control angle set on server
void manualControl(int userAngle) {
  //This function will take in the user's input and move the servo to that angle
  if (userAngle < minAngle) {
    myservo.write(minAngle);
  } else if (userAngle > maxAngle) {
    myservo.write(maxAngle);
  } else {
  myservo.write(userAngle);
  }
}

// normal operating controls, set servo angle to read-time sun angle
void autoControl(int sunAngle) {
  //This function will take in the sun's angle and move the servo to that angle
  if (sunAngle < minAngle) {
    myservo.write(minAngle);
  } else if (sunAngle > maxAngle) {
    myservo.write(maxAngle);
  } else {
  myservo.write(sunAngle);
  }
}

// Check fault status from server
void checkFaultStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://pi.com/plant1/fault");

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      fault = doc["fault"];
    } else {
      Serial.println("Error on HTTP GET request for fault status: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// Check override status from server
void checkOverrideStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://pi.com/plant1");

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      realtimeTracking = doc["manual_override"];
      importedUserAngle = doc["override_angle"];
    } else {
      Serial.println("Error on HTTP GET request for override status: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

//Main loop
void loop() {
//Checking the fault button status
  if (!fault) {
    //Normal run condition
    // Read data from nano
    readI2CData();
    //check for override from server
    if (realtimeTracking == true) {
      autoControl((int)angle);
    } else {
      manualControl(importedUserAngle);
    }
    //update LCD with current data
    lcd.setCursor(0, 0);
    lcd.print("C: " + (String)current +" V: " + (String)voltage);
    lcd.setCursor(0, 1);
    lcd.print("A: " + (String)angle);
  }
  else{
    if(!faultDisplayed){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fault Detected");
      faultDisplayed = true;
    }
    
  }
  //check for serverside fault status change
  checkFaultStatus();
  checkOverrideStatus();

  delay(1000); // Delay between requests
}
