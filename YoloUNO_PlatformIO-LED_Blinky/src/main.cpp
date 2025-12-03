#include <Arduino.h>


#define LED_PIN GPIO_NUM_48
int ledState = 0;
void TaskBlink(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);  

  while (true) {
    if (ledState == 0) {
    digitalWrite(LED_PIN, HIGH);  
    Serial.println("LED ON");
    } else {
    digitalWrite(LED_PIN, LOW);   
    Serial.println("LED OFF");
    }
    ledState = 1 - ledState;
    vTaskDelay(pdMS_TO_TICKS(500));  
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreate(TaskBlink, "BlinkLED", 2048, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}