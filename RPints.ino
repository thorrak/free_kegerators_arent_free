#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <string>

void setup_wifi(); // Add this line at the top
void ICACHE_RAM_ATTR pulseCounter1(); // Add this line at the top
void ICACHE_RAM_ATTR pulseCounter2(); // Add this line at the top
void callback(char* topic, byte* payload, unsigned int length); // Add this line at the top
void reconnect(); // Add this line at the top
void pulseCounter1(); // Add this line at the top
void pulseCounter2(); // Add this line at the top
const char* mqtt_topic = "rpints/pours"; // Add this line at the top
void sendTemp(float temp, const char* probe, const char* unit, const char* timestamp); // Add this line at the top


// WiFi Settings
const char* ssid = "SSID";
const char* password = "SSID_Password";

// MQTT Settings
const char* mqtt_server = "RPints_IP";
const int mqtt_port = 1883;
const char* mqtt_user = "RaspberryPints";
const char* mqtt_pass = "RaspberryPints";

// Flow Sensor 1
const int flowPin1 = D2;
const int tapNumber1 = 4;  // Change for each tap
volatile unsigned long pulseCount1 = 0;
volatile unsigned long lastTimeSent = 0;  // Covers both flow sensors since they share the same publish timing

// Flow Sensor 2
const int flowPin2 = D3;
const int tapNumber2 = 5;  // Change for each tap
volatile unsigned long pulseCount2 = 0;

// OneWire Settings
#define SENSOR_PIN D7  // The ESP8266 pin connected to DS18B20 sensor's DQ pin

OneWire oneWire(SENSOR_PIN);
DallasTemperature DS18B20(&oneWire);

float temperature_C;  // temperature in Celsius
float temperature_F;  // temperature in Fahrenheit

WiFiClient espClient;
PubSubClient client(espClient);

static unsigned long tempTime = 0;

char probeName[24] = "Garage";

void setup() {
  Serial.begin(115200);
 
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  //OneWire
  DS18B20.begin();  // initialize the DS18B20 sensor
 
  pinMode(flowPin1, INPUT_PULLUP);
  pinMode(flowPin2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(flowPin1), pulseCounter1, FALLING);
  attachInterrupt(digitalPinToInterrupt(flowPin2), pulseCounter2, FALLING);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Send pulse data every second to rpints/pours topic 
  if (millis() - lastTimeSent > 1000){
    char payload[100];
    // Format to match Arduino serial protocol
    // Adjust format based on your Arduino firmware version
    
    snprintf(payload, sizeof(payload), "P;%d;%d;%d", -1, tapNumber1, pulseCount1);
    snprintf(payload, sizeof(payload), "P;%d;%d;%d", -1, tapNumber2, pulseCount2);

    // Alternative JSON format (if your setup uses JSON):
    // snprintf(payload, sizeof(payload),
    //          "{\"tap\":\"tap%d\",\"pulses\":%d}",
    //          tapNumber, pulseCount);

    client.publish("rpints/pours", payload);
    Serial.print("Sent: ");
    Serial.println(payload);

    pulseCount1 = 0;
    attachInterrupt(digitalPinToInterrupt(flowPin1), pulseCounter1, FALLING);
    pulseCount2 = 0;
    attachInterrupt(digitalPinToInterrupt(flowPin2), pulseCounter2, FALLING);

    lastTimeSent = millis();  // Update the time gate for the next publish
  }

  //OneWire
    if (millis() - tempTime > 2000) {
        DS18B20.requestTemperatures();             // send the command to get temperatures
      String timestamp = getTimestamp();
    temperature_C = DS18B20.getTempCByIndex(0);  // read temperature in °C
    temperature_F = temperature_C * 9 / 5 + 32;  // convert °C to °F

    sendTemp(temperature_F, probeName, "F", timestamp.c_str());   

    Serial.print("Temperature: ");
    //Serial.print(temperature_C);  // print the temperature in °C
    //Serial.print("°C");
    //Serial.print("  ~  ");        // separator between °C and °F
    Serial.print(temperature_F);  // print the temperature in °F
    Serial.println("°F");

    tempTime = millis();
  }
}

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void ICACHE_RAM_ATTR pulseCounter1() {
  pulseCount1++;
}

void ICACHE_RAM_ATTR pulseCounter2() {
  pulseCount2++;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId1 = "ESP8266-tap" + String(tapNumber1);
    if (client.connect(clientId1.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Connected!");
      // Subscribe to commands from RPints
      client.subscribe("rpints");
      Serial.println("Subscribed to rpints topic");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
    Serial.print("Connecting to MQTT...");
    String clientId2 = "ESP8266-tap" + String(tapNumber2);
    if (client.connect(clientId2.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Connected!");
      // Subscribe to commands from RPints
      client.subscribe("rpints");
      Serial.println("Subscribed to rpints topic");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle commands from RaspberryPints
  Serial.print("Message received on ");
  Serial.print(topic);
  Serial.print(": ");

  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println(message);

  // Parse and handle commands here
  // Example: valve control, configuration updates, etc.
}

String getTimestamp() {
 time_t now;
 struct tm timeinfo;
 if (!getLocalTime(&timeinfo)) {
   return "0";
 }

 char buffer[30];
 strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
 return String(buffer);
}

void sendTemp(float temp, const char* probe, const char* unit, const char* timestamp)
{
   char payload[100];
   snprintf(payload, sizeof(payload),
            "T,%s,%.2f,%s,%s",
            probe,
            temp,
            unit,
            timestamp);

   client.publish(mqtt_topic, payload);
}