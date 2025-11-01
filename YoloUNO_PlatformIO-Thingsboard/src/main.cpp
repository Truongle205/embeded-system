#include <Arduino.h>
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include <DHT.h>
#include <array>

#define LED_PIN   48
#define DHT_PIN   GPIO_NUM_12
#define DHTTYPE   DHT11

constexpr char WIFI_SSID[]       = "abcd";
constexpr char WIFI_PASSWORD[]   = "123456789";

constexpr char TOKEN[]           = "UqbTL3i8dMm9sema9EsU";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

constexpr uint32_t MAX_MESSAGE_SIZE   = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD  = 115200U;

constexpr uint32_t TELEMETRY_PERIOD_MS = 10000U;

constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr char LED_MODE_ATTR[]          = "ledMode";
constexpr char LED_STATE_ATTR[]         = "ledState";

constexpr std::array<const char*, 2U> SHARED_ATTRIBUTES_LIST = {
  LED_STATE_ATTR,
  BLINKING_INTERVAL_ATTR
};

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

DHT dht(DHT_PIN, DHTTYPE);

EventGroupHandle_t xEvt;
SemaphoreHandle_t  xTbMutex;

#define EV_WIFI_OK  (1 << 0)
#define EV_TB_OK    (1 << 1)

volatile bool     attributesChanged = false;
volatile bool     ledState          = false;
volatile uint16_t blinkingInterval  = 1000U;

RPC_Response setLedSwitchState(const RPC_Data &data) {
  bool newState = data;
  digitalWrite(LED_PIN, newState);
  ledState = newState;
  attributesChanged = true;
  Serial.print("[RPC] setLedSwitchValue -> "); Serial.println(newState);
  return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 1U> callbacks = {
  RPC_Callback{ "setLedSwitchValue", setLedSwitchState }
};

void processSharedAttributes(const Shared_Attribute_Data &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    const char* key = it->key().c_str();
    if (!key) continue;

    if (strcmp(key, BLINKING_INTERVAL_ATTR) == 0) {
      uint32_t v = it->value().as<uint32_t>();
      if (v >= 10 && v <= 60000) {
        blinkingInterval = (uint16_t)v;
        Serial.printf("[ATTR] blinkingInterval = %u ms\n", (unsigned)v);
      }
    } else if (strcmp(key, LED_STATE_ATTR) == 0) {
      bool v = it->value().as<bool>();
      ledState = v;
      digitalWrite(LED_PIN, v);
      Serial.printf("[ATTR] ledState = %d\n", (int)v);
    }
  }
  attributesChanged = true;
}


const Shared_Attribute_Callback attributes_callback(
  &processSharedAttributes,
  SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend()
);
const Attribute_Request_Callback attribute_shared_request_callback(
  &processSharedAttributes,
  SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend()
);


void TaskWiFi(void *pv) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Connecting...");
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
        vTaskDelay(500);
        Serial.print(".");
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] OK, IP: %s\n", WiFi.localIP().toString().c_str());
        xEventGroupSetBits(xEvt, EV_WIFI_OK);
      } else {
        Serial.println("[WiFi] Failed");
        xEventGroupClearBits(xEvt, EV_WIFI_OK);
      }
    }
    vTaskDelay(2000);
  }
}

void TaskTB(void *pv) {
  for (;;) {
    xEventGroupWaitBits(xEvt, EV_WIFI_OK, pdFALSE, pdFALSE, portMAX_DELAY);

    if (!tb.connected()) {
      Serial.printf("[TB] Connecting %s:%u ...\n", THINGSBOARD_SERVER, THINGSBOARD_PORT);
      if (xSemaphoreTake(xTbMutex,3000) == pdTRUE) {
        bool ok = tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT);
        if (ok) {
          if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend()))
            Serial.println("[TB] RPC_Subscribe failed");
          if (!tb.Shared_Attributes_Subscribe(attributes_callback))
            Serial.println("[TB] Shared_Attributes_Subscribe failed");
          if (!tb.Shared_Attributes_Request(attribute_shared_request_callback))
            Serial.println("[TB] Shared_Attributes_Request failed");

          tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
          xEventGroupSetBits(xEvt, EV_TB_OK);
          Serial.println("[TB] Connected & Subscribed");
        } else {
          Serial.println("[TB] Connect failed");
          xEventGroupClearBits(xEvt, EV_TB_OK);
        }
        xSemaphoreGive(xTbMutex);
      }
    }
    vTaskDelay(2000);
  }
}

void TaskTelemetry(void *pv) {
  uint32_t last = 0;
  for (;;) {
    xEventGroupWaitBits(xEvt, EV_TB_OK, pdFALSE, pdFALSE, portMAX_DELAY);

    if (millis() - last >= TELEMETRY_PERIOD_MS) {
      last = millis();

      float h = dht.readHumidity();
      float t = dht.readTemperature(); 

      if (isnan(t) || isnan(h)) {
        Serial.println("[DHT11] read failed (NaN) — kiểm tra DATA/GND/3V3/pull-up");
      }

      if (xSemaphoreTake(xTbMutex, 500) == pdTRUE) {
        if (!isnan(t)) tb.sendTelemetryData("temperature", t);
        if (!isnan(h)) tb.sendTelemetryData("humidity", h);

        tb.sendTelemetryData("rssi", WiFi.RSSI());
        tb.sendTelemetryData("channel", WiFi.channel());
        tb.sendTelemetryData("bssid", WiFi.BSSIDstr().c_str());
        tb.sendTelemetryData("localIp", WiFi.localIP().toString().c_str());
        tb.sendTelemetryData("ssid", WiFi.SSID().c_str());
        xSemaphoreGive(xTbMutex);
      }
    }
    vTaskDelay(50);
  }
}

void TaskAttributes(void *pv) {
  for (;;) {
    xEventGroupWaitBits(xEvt, EV_TB_OK, pdFALSE, pdFALSE, portMAX_DELAY);

    if (attributesChanged) {
      attributesChanged = false;
      bool st = digitalRead(LED_PIN);
      if (xSemaphoreTake(xTbMutex, 500) == pdTRUE) {
        tb.sendAttributeData(LED_STATE_ATTR, st);
        xSemaphoreGive(xTbMutex);
        Serial.printf("[ATTR] Sent %s = %d\n", LED_STATE_ATTR, (int)st);
      }
    }
    vTaskDelay(100);
  }
}

void TaskTBLoop(void *pv) {
  for (;;) {
    EventBits_t bits = xEventGroupGetBits(xEvt);
    if ((bits & EV_TB_OK) && tb.connected()) {
      if (xSemaphoreTake(xTbMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        tb.loop();
        xSemaphoreGive(xTbMutex);
      }
      vTaskDelay(10);
    } else {
      vTaskDelay(200);
    }
  }
}

void TaskBlink(void *pv) {
  pinMode(LED_PIN, OUTPUT);
  bool on = false;
  for (;;) {
    digitalWrite(LED_PIN, on ? HIGH : LOW);
    on = !on;
    vTaskDelay(1000);
  }
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();

  xEvt     = xEventGroupCreate();
  xTbMutex = xSemaphoreCreateMutex();

  xTaskCreate(TaskWiFi,      "WiFi",      4096, nullptr, 2, nullptr);
  xTaskCreate(TaskTB,        "TB",        6144, nullptr, 3, nullptr);
  xTaskCreate(TaskTBLoop,    "TBloop",    4096, nullptr, 3, nullptr);
  xTaskCreate(TaskTelemetry, "Telemetry", 4096, nullptr, 2, nullptr);
  xTaskCreate(TaskAttributes,"Attributes",2048, nullptr, 2, nullptr);
  xTaskCreate(TaskBlink,     "Blink",     2048, nullptr, 1, nullptr);
}

void loop() {
}
