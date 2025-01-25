//Reference the fact that I used the library examples to teach me the core functions.
//Reference CoPilot assistant autocompletion

//Include Libraries
#include "ESP32Servo.h"
#include "../lib/LiquidCrystal_I2C-master/LiquidCrystal_I2C.h"

//Setting up the lcd and servo
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

//Variable definitions
int servoPin = 19; //servo pin
int faultButtonPin = 32; //fault button pin
int faultLEDPin = 33; //fault LED pin
int angle = 0; //servo angle
int minAngle = 0; //minimum permissible hardware angle
int maxAngle = 180; //maximum permissible hardware angle
int currentServoAngle = 0; //current servo angle

//Imported data from the parser
int importedSunAngle = 0; //from the JSON parsing
int importedUserAngle = 0; // from the website
int importedVoltage = 0; //from the JSON parsing
int importedCurrent = 0; //from the JSON parsing

bool realtimeTracking = true; //default to auto control
bool dataReceived = false; //default to no data
bool faultDetected = false; //default to no fault

#include <Arduino.h>
#include <WiFi.h>
#include <InfluxDbClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>

#define INFLUXDB_URL "http//pi:8125"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> Generate API Token)
#define INFLUXDB "plant1"


// Replace with your network credentials
const char* ssid = "Coverdale";
const char* password = "NOHACKSSS";
const char* mqtt_server = "pi";
const char* mqtt_port = "1883";
const char* mqtt_topic = "plant1";

WiFiClient espClient;
PubSubClient client(espClient);

bool fault = false;
const int faultButton = 32;
const int faultLED = 33;
const int SCL_PIN = 22;
const int SDA_PIN = 21; // Changed to 21 as 33 is used for faultLED


  float voltage = 0.0;
  float current = 0.0;
  float angle = 0.0;

InfluxDBClient dbClient(INFLUXDB_URL, INFLUXDB);

void handleButtonPress() {
  fault = true;
  Serial.print("Fault detected");
  digitalWrite(33, HIGH);
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  pinMode(faultButton, INPUT_PULLUP);
  pinMode(33, OUTPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  pinMode(faultButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(faultButton), handleButtonPress, FALLING);
  //client.setServer(mqtt_server, mqtt_port);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);


}



//Function to connect to MQTT broker
void connectMQTT() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client")) { // MQTT client ID
      Serial.println("Connected to MQTT!");
    } else {
      Serial.print("Failed. Retrying in 5 seconds...");
      delay(5000);
    }
  }
}
String jsonData = "";
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
  
  // Serial.print("Voltage: ");
  // Serial.println(voltage);
  // Serial.print("Current: ");
  // Serial.println(current);
  // Serial.print("Sun Angle: ");
  // Serial.println(angle);
  lcd.init();
  lcd.backlight();
  myservo.attach(servoPin); //attach to pin 2
}

void loop() {

  currentServoAngle = myservo.read(); //grab the current servo angle
  //run the data acquisition function

//Checking the fault button status
if (digitalRead(faultButtonPin) == true) {
  //condition where a fault has occured, flip the faultDetected boolean, run the servo fault condition function, and display indicators
  faultDetected = true;
  servoFault(faultDetected);
  lcd.print("Fault Detected");
  faultLEDPin = HIGH;
} else {
//Normal run condition

  faultDetected = false;

  //control the servo based on the user's desired functionality
  //*Add important delays to prevent the servo from moving too quickly
  if (realtimeTracking == true) {
    autoControl(importedSunAngle);
  } else {
    manualControl(importedUserAngle);
  }

  if (!dataReceived) {
  lcd.print("Awaiting Data...");
  } else {
  lcd.print("Current: " + importedCurrent);
  lcd.print("Voltage: " + importedVoltage);
  lcd.print("Angle: " + angle);
  } 

}

//USEFUL INFO FOR OPERATION
  //lcd.write | function to write an int to the display 
  //lcd.print | function to print a string to the display
  //lcd.clear | funciton to clear off what is currently on the screen

  //myservo.write(angle);
  //delay(ms);
}


//Function Definitions

//Function to aquired data from the I2C 
//code to grab data from I2C
//once the data is grabbed, set dataReceived to true and update the import variables


int manualControl(int userAngle) {
  //This function will take in the user's input and move the servo to that angle
  if (userAngle < minAngle) {
    myservo.write(minAngle);
  } else if (userAngle > maxAngle) {
    myservo.write(maxAngle);
  } else {
  myservo.write(userAngle);
  }
}

int autoControl(int sunAngle) {
  //This function will take in the sun's angle and move the servo to that angle
  if (sunAngle < minAngle) {
    myservo.write(minAngle);
  } else if (sunAngle > maxAngle) {
    myservo.write(maxAngle);
  } else {
  myservo.write(sunAngle);
  }
}

bool servoFault(bool buttonStatus) {
  //This function will check if the fault button is pressed and return a boolean
  if (buttonStatus == false) {
    dataReceived = false;
  }
  return dataReceived;
}

//General Requirements
//Servo needs to be controlled to match the "angle of the sun"
//The LCD needs to display the current, voltage, and angle.
//The fault button needs to be checked 
//The fault LED needs to be turned on if the fault button is pressed



  currentServoAngle = myservo.read(); //grab the current servo angle
  //run the data acquisition function

//Checking the fault button status
if (digitalRead(faultButtonPin) == true) {
  //condition where a fault has occured, flip the faultDetected boolean, run the servo fault condition function, and display indicators
  faultDetected = true;
  servoFault(faultDetected);
  lcd.print("Fault Detected");
  faultLEDPin = HIGH;
} else {
//Normal run condition

  faultDetected = false;

  //control the servo based on the user's desired functionality
  //*Add important delays to prevent the servo from moving too quickly
  if (realtimeTracking == true) {
    autoControl(importedSunAngle);
  } else {
    manualControl(importedUserAngle);
  }

  if (!dataReceived) {
  lcd.print("Awaiting Data...");
  } else {
  lcd.print("Current: " + importedCurrent);
  lcd.print("Voltage: " + importedVoltage);
  lcd.print("Angle: " + angle);
  } 

}

//USEFUL INFO FOR OPERATION
  //lcd.write | function to write an int to the display 
  //lcd.print | function to print a string to the display
  //lcd.clear | funciton to clear off what is currently on the screen

  //myservo.write(angle);
  //delay(ms);
}


//Function Definitions

//Function to aquired data from the I2C 
//code to grab data from I2C
//once the data is grabbed, set dataReceived to true and update the import variables


int manualControl(int userAngle) {
  //This function will take in the user's input and move the servo to that angle
  if (userAngle < minAngle) {
    myservo.write(minAngle);
  } else if (userAngle > maxAngle) {
    myservo.write(maxAngle);
  } else {
  myservo.write(userAngle);
  }
}

int autoControl(int sunAngle) {
  //This function will take in the sun's angle and move the servo to that angle
  if (sunAngle < minAngle) {
    myservo.write(minAngle);
  } else if (sunAngle > maxAngle) {
    myservo.write(maxAngle);
  } else {
  myservo.write(sunAngle);
  }
}

bool servoFault(bool buttonStatus) {
  //This function will check if the fault button is pressed and return a boolean
  if (buttonStatus == false) {
    dataReceived = false;
  }
  return dataReceived;
}

//General Requirements
//Servo needs to be controlled to match the "angle of the sun"
//The LCD needs to display the current, voltage, and angle.
//The fault button needs to be checked 
//The fault LED needs to be turned on if the fault button is pressed



  connectMQTT();
  readI2CData();
  if (client.connected()) {
    client.publish("solar/plant1", jsonData.c_str());
    Serial.println("Published JSON data to MQTT");
  } else {
    Serial.println("Failed to publish JSON data to MQTT");
  }
  delay(1000);
}

