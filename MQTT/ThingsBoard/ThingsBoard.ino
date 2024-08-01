#include <WiFi.h>
#include <ThingsBoard.h>
#include <Arduino_MQTT_Client.h>
#include <DHT20.h>

#define WIFI_AP "ACLAB"
#define WIFI_PASS "ACLAB2023"

#define TB_SERVER "thingsboard.cloud"
#define TOKEN "epug7d37lqory14sp46b"

constexpr uint16_t MAX_MESSAGE_SIZE = 128U;

// define task
void TaskTemperatureHumidity(void *pvParameters);

// define component
DHT20 dht;
WiFiClient espClient;
Arduino_MQTT_Client mqttClient(espClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    WiFi.begin(WIFI_AP, WIFI_PASS, 6);
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi.");
  } else {
    Serial.println("\nConnected to WiFi");
  }
}

void connectToThingsBoard() {
  if (!tb.connected()) {
    Serial.println("Connecting to ThingsBoard server");
    
    if (!tb.connect(TB_SERVER, TOKEN)) {
      Serial.println("Failed to connect to ThingsBoard");
    } else {
      Serial.println("Connected to ThingsBoard");
    }
  }
}

void sendDataToThingsBoard(float temp, int hum) {
  String jsonData = "{\"temperature\":" + String(temp) + ", \"humidity\":" + String(hum) + "}";
  tb.sendTelemetryJson(jsonData.c_str());
  Serial.println("Data sent");
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Create Task 
  xTaskCreate( TaskTemperatureHumidity, "Task Temperature" ,2048  ,NULL  ,2 , NULL);

  connectToWiFi();
  // connectToThingsBoard();
}

void loop() {
}

void TaskTemperatureHumidity(void *pvParameters) {  // This is a task.
  //uint32_t blink_delay = *((uint32_t *)pvParameters);

  while(1) {              
    Serial.println("Task Temperature and Humidity");
    dht.read();
    
    float temp = dht.getTemperature();
    int hum = dht.getHumidity();

    Serial.println(temp);
    Serial.println(hum);

    // if (!tb.connected()) {
    //   connectToThingsBoard();
    // }

    // sendDataToThingsBoard(temp, hum);

    delay(5000);

    // tb.loop();
  }
}