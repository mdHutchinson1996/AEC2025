//Reference the fact that I used the library examples to teach me the core functions.
//Reference CoPilot assistant autocompletion

//Include Libraries
#include "ESP32Servo.h"
#include "../lib/LiquidCrystal_I2C-master/LiquidCrystal_I2C.h"
#include <Arduino.h>
#include <WiFi.h>
#include <InfluxDbClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// #define INFLUXDB_URL "http//pi:8125"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> Generate API Token)
// #define INFLUXDB "plant1"


//Variable definitions
int servoPin = 19; //servo pin
//int angle = 0; //servo angle
int minAngle = 0; //minimum permissible hardware angle
int maxAngle = 180; //maximum permissible hardware angle
//int currentServoAngle = 0; //current servo angle

//Imported data from the parser
int importedUserAngle = 0; // from the website


bool realtimeTracking = true; //default to auto control

//bool faultDetected = false; //default to no fault

String jsonData = "";

//Setting up the lcd and servo
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

// Replace with your network credentials
const char* ssid = "Coverdale";
const char* password = "NOHACKSSS";
// const char* mqtt_server = "pi";
// const char* mqtt_port = "1883";
// const char* mqtt_topic = "plant1";

// WiFiClient espClient;
// PubSubClient client(espClient);

bool fault = false;
bool faultDisplayed = false;
const int faultButton = 32;
const int faultLED = 33;
const int SCL_PIN = 22;
const int SDA_PIN = 21; // Changed to 21 as 33 is used for faultLED

float voltage = 0.0;
float current = 0.0;
float angle = 0.0;

// InfluxDBClient dbClient(INFLUXDB_URL, INFLUXDB);

void faultButtonPress() {
  fault = true;
  // faultDisplayed = true;
  Serial.print("Fault detected");
  digitalWrite(faultLED, HIGH);
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  pinMode(faultButton, INPUT_PULLUP);
  pinMode(faultLED, OUTPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  pinMode(faultButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(faultButton), faultButtonPress, FALLING);
  //client.setServer(mqtt_server, mqtt_port);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  lcd.init();
  lcd.backlight();
  myservo.attach(servoPin); //attach to pin 2
}

//Function to connect to MQTT broker
// void connectMQTT() {
//   while (!client.connected()) {
//     Serial.println("Connecting to MQTT...");
//     if (client.connect("ESP32Client")) { // MQTT client ID
//       Serial.println("Connected to MQTT!");
//     } else {
//       Serial.print("Failed. Retrying in 5 seconds...");
//       delay(5000);
//     }
//   }
// }

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

void loop() {

//currentServoAngle = myservo.read(); //grab the current servo angle
  //run the data acquisition function

//Checking the fault button status
  if (!fault) {

  //Normal run condition
    //control the servo based on the user's desired functionality
    //*Add important delays to prevent the servo from moving too quickly

    readI2CData();

    if (realtimeTracking == true) {
      autoControl((int)angle);
    } else {
      manualControl(importedUserAngle);
    }

    lcd.setCursor(0, 0);
    lcd.print("C: " + (String)current +" V: " + (String)voltage);
    lcd.setCursor(0, 1);
    lcd.print("A: " + (String)angle);

    //connectMQTT();
    
    // if (client.connected()) {
    //   client.publish("solar/plant1", jsonData.c_str());
    //   Serial.println("Published JSON data to MQTT");
    // } else {
    //   Serial.println("Failed to publish JSON data to MQTT");
    // }
    delay(1000);
  }
  else{
    if(!faultDisplayed){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fault Detected");
      faultDisplayed = true;
    }
    
  }

  // Ensure WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Specify the URL for the HTTP request
    http.begin("pi:8086");

    // Send HTTP GET request
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload);
    } else {
      Serial.println("Error on HTTP GET request");
    }
    http.end();

    // Send HTTP POST request
    http.begin("pi:8086");
    http.addHeader("Content-Type", "application/json");
    int postHttpCode = http.POST(jsonData);
    if (postHttpCode > 0) {
      String postPayload = http.getString();
      Serial.println(postPayload);
    } else {
      Serial.println("Error on HTTP POST request");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }

  delay(5000); // Delay between requests
}
