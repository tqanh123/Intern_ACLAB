#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "LiquidCrystal_I2C.h";
#include "DHT20.h"
#include <Adafruit_NeoPixel.h>

const char WIFI_SSID[] = "ACLAB";     // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "ACLAB2023";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADRRESS[] = "broker.hivemq.com";  // CHANGE TO MQTT BROKER'S ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "TQA-mqtt";  

const char MQTT_USERNAME[] = "";                        
const char MQTT_PASSWORD[] = "";                        

// The MQTT topics that ESP32 should publish/subscribe
const char PUBLISH_TOPIC[] = "esp32-MQTT";    
const char SUBSCRIBE_TOPIC[] = "esp32-MQTT";  

// Define your tasks here
void TaskBlink(void *pvParameters);
void TaskTemperatureHumidity(void *pvParameters);
void TaskSoilMoistureAndRelay(void *pvParameters);
void TaskLightAndLED(void *pvParameters);
void TaskLEDMoving(void *pvParameters);
// void TaskAutoFan(void *pvParameters);
void TaskMQTT(void *pvParameters);

//Define your components here
LiquidCrystal_I2C lcd(0x21, 16, 2);
DHT20 DHT;
WiFiClient network;
MQTTClient mqtt = MQTTClient(256);
Adafruit_NeoPixel rgb(4, D3, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  DHT.begin();
  lcd.init();
  lcd.backlight();
  rgb.begin();

  xTaskCreate( TaskBlink, "Task Blink" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate( TaskTemperatureHumidity, "Task Temperature" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate( TaskSoilMoistureAndRelay, "Task Soil Moisture" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate( TaskLightAndLED, "Task Light and LED" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate( TaskLEDMoving, "Task LED Moving" ,2048  ,NULL  ,2 , NULL);
  // xTaskCreate( TaskAutoFan, "Task Auto Fan", 2048, NULL, 2, NULL);
  xTaskCreate( TaskMQTT, "Task MQTT", 2048, NULL, 2, NULL);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("ESP32 - Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  connectToMQTT();
}

void loop() {
}

void TaskBlink(void *pvParameters) {  // This is a task.
  //uint32_t blink_delay = *((uint32_t *)pvParameters);

  // initialize digital LED_BUILTIN on pin 13 as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  

  while(1) {                          
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED ON
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED OFF
    delay(200);
  }
}


void TaskTemperatureHumidity(void *pvParameters) {  // This is a task.
  //uint32_t blink_delay = *((uint32_t *)pvParameters);

  while(1) {              
    Serial.println("Task Temperature and Humidity");
    DHT.read();
    Serial.println(DHT.getTemperature());
    Serial.println(DHT.getHumidity());         
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Nhiet Do: ");
    lcd.print(DHT.getTemperature());
    lcd.setCursor(0, 1);
    lcd.print("Do Am: ");
    lcd.print(DHT.getHumidity());
    delay(5000);
  }
}

void TaskSoilMoistureAndRelay(void *pvParameters) {
  //uint32_t blink_delay = *((uint32_t *)pvParameters);

  pinMode(D5, OUTPUT);

  while(1) {                          
    Serial.println("Task Soild and Relay");
    //Serial.println(analogRead(A0));
    
    if(analogRead(A1) > 500){
      digitalWrite(D5, HIGH);
    }
    if(analogRead(A1) < 50){
      digitalWrite(D5, LOW);
    }
    delay(1000);
  }
}

void TaskLightAndLED(void *pvParameters) {
  //uint32_t blink_delay = *((uint32_t *)pvParameters);

  while(1) {                          
    Serial.println("Task Light and LED");
    Serial.println(analogRead(A0));
    if(analogRead(A0) < 1000){
      rgb.setPixelColor(0, rgb.Color(255,0,0));
      rgb.setPixelColor(1, rgb.Color(255,0,0));
      rgb.show();
    }
    if(analogRead(A0) > 1250){
      rgb.setPixelColor(0, rgb.Color(0,0,0));
      rgb.setPixelColor(1, rgb.Color(0,0,0));
      rgb.show();
    }
    delay(1000);
  }
}

void TaskLEDMoving(void *pvParameters) {
  //uint32_t blink_delay = *((uint32_t *)pvParameters);

  while(1) {                          
    Serial.println("Task Moving and LED");
    Serial.println(digitalRead(D7));
    if(digitalRead(D7) == 1){
      rgb.setPixelColor(2, rgb.Color(0,0,255));
      rgb.setPixelColor(3, rgb.Color(0,0,255));
      rgb.show();
      delay(3000);
    }
    if(digitalRead(D7) == 0){
      rgb.setPixelColor(2, rgb.Color(0,0,0));
      rgb.setPixelColor(3, rgb.Color(0,0,0));
      rgb.show();
    }
    delay(1000);
  }
}

// void TaskAutoFan(void *pvParameters) {
//   pinMode(A2, OUTPUT);

//   while(1) {
//     Serial.print("Task Auto Fan");
//      if(DHT.getTemperature() > 27){
//         analogWrite(A2, 100);
//      }
//      else{
//       analogWrite(A2, 0);
//      }
//      delay(1000);
//   }
// }

void TaskMQTT(void *pvParameters) {
  pinMode(A2, OUTPUT);
  
  while(1){
    mqtt.loop();
    delay(1000);
  }
}

void connectToMQTT() {
  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);

  // Create a handler for incoming messages
  mqtt.onMessage(messageHandler);

  Serial.print("ESP32 - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("ESP32 - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("ESP32 - Subscribed to the topic: ");
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("ESP32 - MQTT broker Connected!");
}

void messageHandler(String &topic, String &payload) {
  if (payload == "FAN ON") {
    analogWrite(A2, 100);
  }
  if (payload == "FAN OFF") {
    analogWrite(A2, 0);
  }
}