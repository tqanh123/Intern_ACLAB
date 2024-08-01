#include <AdafruitIO.h>
#include <AdafruitIO_WiFi.h>
#include "RelayStatus.h"

#define IO_USERNAME "tqanh"
#define IO_KEY "aio_CJBZ53SOYTagic066QfNML2nR13t"

// #define WIFI_SSID "HoBa Home CN6_L3_5G "
// #define WIFI_PASS "0338440977"
#define WIFI_SSID "ACLAB"
#define WIFI_PASS "ACLAB2023"

#define RS485 Serial2
#define LED_PIN 2
#define TXD 8
#define RXD 9
#define BAUD_RATE 9600

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *status = io.feed("relayStatus");

// Define Task
// void TaskOnOffRelay(void *pvParameters);

void sendModbusCommand(const uint8_t command[1], size_t length)
{
  for (size_t i = 0; i < length; i++)
  {
    RS485.write(command[1]);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  RS485.begin(BAUD_RATE, SERIAL_8N1, TXD, RXD);

  sendModbusCommand(relay_OFF[0], sizeof(relay_OFF[0]));

  while (!Serial);

  connectToAdafruit();

  // Create tasks
  xTaskCreate(TaskOnOffRelay, "On Off Relay", 2048, NULL, 2, NULL);
}

void connectToAdafruit() {
  Serial.println("Connecting to Adafruit IO");
  io.connect();
  status->onMessage(handleMessage);

  while (io.status() < AIO_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println(io.statusText());
  status->get();
}

void loop()
{
  io.run();
}

void TaskOnOffRelay(void *pvParameters) {
  while(1) {
    Serial.println("On Off Relay task running.");

    for (int i = 0; i < 32; i++) {
      Serial.println("Relay " + String(i) + " turned ON");
      sendModbusCommand(relay_ON[i], sizeof(relay_ON[0]));
    }

    for (int i = 0; i < 32; i++) {
      Serial.println("Relay " + String(i) + " turned OFF");
      sendModbusCommand(relay_OFF[i], sizeof(relay_OFF[0]));
    }

    delay(5000);
  }
}

void handleMessage(AdafruitIO_Data *data)
{
  String message = data->value();

  if (message.startsWith("!RELAY") && message.endsWith("#"))
  {
    int indexStart = message.indexOf('!') + 6;
    int indexEnd = message.indexOf(':');
    String indexStr = message.substring(indexStart, indexEnd);
    int index = indexStr.toInt();

    int statusStart = indexEnd + 1;
    int statusEnd = message.indexOf('#');
    String statusStr = message.substring(statusStart, statusEnd);

    // Debug prints
    Serial.print("Raw message: ");
    Serial.println(message);
    Serial.print("Index string: ");
    Serial.println(indexStr);
    Serial.print("Index: ");
    Serial.println(index);
    Serial.print("Status string: ");
    Serial.println(statusStr);

    // Send the Modbus command for the specific relay
    if (statusStr == "ON" && index < (sizeof(relay_ON) / sizeof(relay_ON[0])))
    {
      sendModbusCommand(relay_ON[index], sizeof(relay_ON[0]));
      Serial.println("Relay " + String(index) + " turned ON");
    }
    else if (statusStr == "OFF" && index < (sizeof(relay_OFF) / sizeof(relay_OFF[0])))
    {
      sendModbusCommand(relay_OFF[index], sizeof(relay_OFF[0]));
      Serial.println("Relay " + String(index) + " turned OFF");
    }
    else
    {
      Serial.println("Invalid command");
    }

    String sendData = String(index) + '-' + statusStr;
    status->save(sendData);
    Serial.println("Data sent to Adafruit IO: " + sendData);
  }
}