#include <AdafruitIO.h>
#include <AdafruitIO_WiFi.h>
#include "DHT20.h"
#include <LiquidCrystal_I2C.h>

#define IO_USERNAME "tqanh"
#define IO_KEY     "aio_vSqY75rAh6R5OaUKoMK5NqShscQi"

#define WIFI_SSID "Putie"
#define WIFI_PASS "04122003@"

// Initialize Adafruit IO client
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *mqtt = io.feed("MQTT");

// Define Task
// void TaskBlink(void *pvParameters);
void TaskMQTT(void *pvParameters);
void handleMessage(AdafruitIO_Data *data);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial to initialize
  
  // connect to Adafruit
  connectToAdafruit();

  // Create tasks
  // xTaskCreate( TaskBlink, "Task Blink" ,2048  ,NULL  ,2 , NULL);
  // xTaskCreate(TaskReadTemperatureAndHumidity, "Task Read Temp And Humi", 2048, NULL, 2, NULL);
  xTaskCreate(TaskMQTT, "Task MQTT", 2048, NULL, 2, NULL);
}

void connectToAdafruit() {
  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // set up a message handler for the 'relayStatus' feed.
  // the handleMessage function (defined below)
  // will be called whenever a message is
  // received from adafruit io.
  mqtt->onMessage(handleMessage);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  mqtt->get();
}

void loop() {
  io.run();
}

void TaskMQTT(void *pvParameters) {
  pinMode(LED_BUILTIN, OUTPUT);
  
  while(1) {
    Serial.println("Task mqtt running.");
    delay(1000);
  }
}

void handleMessage(AdafruitIO_Data *data) {
  Serial.print("Received <- ");
  Serial.println(data->value());
  
  if (data->toPinLevel() == HIGH) {
    Serial.println("ON");
    // analogWrite(A1, 100);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else {
    Serial.println("OFF");
    // analogWrite(A1, 0);
    digitalWrite(LED_BUILTIN, LOW); 
  }
}
