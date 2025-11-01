#include <Arduino.h>
#include <DHT.h>

#define LED_PIN  GPIO_NUM_48
DHT dht(GPIO_NUM_12, DHT11);

void TaskBlink(void *pvParameters) {
  int ledState = 0;
  pinMode(LED_PIN, OUTPUT);
  while (true) {
    if (ledState == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
    ledState = 1 - ledState;
    vTaskDelay(2000);
  }
}

void TaskTemperature_Humidity(void *pvParameters) {
  dht.begin();
  while (1) {
    double tem = dht.readTemperature(); 
    double hum = dht.readHumidity();

    Serial.print("Temp: "); Serial.print(tem); Serial.print(" *C  ");
    Serial.print("Humi: "); Serial.print(hum); Serial.println(" %");
    Serial.println();
    vTaskDelay(5000);     
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreate(TaskBlink, "BlinkLED", 2048, NULL, 1, NULL);
  xTaskCreate(TaskTemperature_Humidity, "TemHum", 2048, NULL, 2, NULL);
}

void loop() {
}
