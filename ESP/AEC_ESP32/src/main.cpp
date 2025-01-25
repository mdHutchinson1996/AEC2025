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
}

void loop() {

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

