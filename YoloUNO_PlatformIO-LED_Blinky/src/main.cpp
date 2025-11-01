// #include <Arduino.h>

// void TaskLEDControl(void *pvParameters) {
//   pinMode(GPIO_NUM_48, OUTPUT); // Initialize LED pin
//   int ledState = 0;
//   while(1) {
    
//     if (ledState == 0) {
//       digitalWrite(GPIO_NUM_48, HIGH); // Turn ON LED
//     } else {
//       digitalWrite(GPIO_NUM_48, LOW); // Turn OFF LED
//     }
//     ledState = 1 - ledState;
//     vTaskDelay(5000);
//   }
// }


// void setup() {
//   // put your setup code here, to run once:
//   Serial.begin(115200);
//   xTaskCreate(TaskLEDControl, "LED Control", 2048, NULL, 2, NULL);
// }

// void loop() {
//   // Serial.println("Hello Custom Board");
//   // delay(1000);
// }
#include <Arduino.h>

// Định nghĩa chân LED (GPIO48 trên ESP32-S3)
#define LED_PIN GPIO_NUM_48
int ledState = 0;
// Task điều khiển LED nháy 2s/lần
void TaskBlink(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);  // cấu hình chân làm OUTPUT

  while (true) {
    if (ledState == 0) {
    digitalWrite(LED_PIN, HIGH);  // bật LED
    Serial.println("LED ON");
    } else {
    digitalWrite(LED_PIN, LOW);   // tắt LED
    Serial.println("LED OFF");
    }
    ledState = 1 - ledState;
    vTaskDelay(pdMS_TO_TICKS(2000));  // delay 2000 ms (2s)
  }
}

void setup() {
  Serial.begin(115200);

  // Tạo task LED, stack 2048 bytes, priority 1
  xTaskCreate(
    TaskBlink,
    "BlinkLED",
    2048,
    NULL,
    1,
    NULL
  );
}

void loop() {
  // Không cần dùng loop khi dùng FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}
