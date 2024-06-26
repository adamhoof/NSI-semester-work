#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "env.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

SemaphoreHandle_t mutex;
unsigned long lastMessageTime = 0;
volatile long messageInterval = 60000;

void setup_wifi()
{
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFiClass::status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    Serial.print("Message:");

    char msg[length + 1];
    strncpy(msg, (char*) payload, length);
    msg[length] = '\0';
    Serial.println(msg);

    std::string topicStr(topic);
    std::string payloadStr((char*)payload, length);

    if (strcmp(topic, number_input_topic.c_str()) == 0) {
        long newInterval = atol(msg);

        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            messageInterval = newInterval;
            xSemaphoreGive(mutex);
        }
        StaticJsonDocument<200> doc;
        doc["Action_name"] = "Interval_ms";
        doc["Interval_ms"] = msg;

        Serial.println("Changed message interval");

        char jsonBuffer[512];
        serializeJson(doc, jsonBuffer);
        mqttClient.publish(state_topic.c_str(), jsonBuffer);
    } else if (strcmp(topic, login_response_topic.c_str()) == 0) {
        Serial.println("Received login response");
        StaticJsonDocument<512> doc;  // Adjust size based on expected payload complexity
        DeserializationError error = deserializeJson(doc, payloadStr);

        if (error) {
            Serial.print("deserializeJson() failed with code ");
            Serial.println(error.c_str());
            return;
        }

        // Extracting the login status and device states from the JSON payload
        const char* loginStatus = doc["login"];  // "successful"
        JsonObject stateObj = doc["state"].as<JsonObject>();

        Serial.println("Login Status: ");
        Serial.println(loginStatus);
        if (strcmp(loginStatus, "successful") != 0){
            ESP.restart();
        }

        const char* interval = stateObj["Interval_ms"];
        if (strcmp(interval,"") != 0){
            long newInterval = atol(interval);

            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                messageInterval = newInterval;
                xSemaphoreGive(mutex);
            }
        }

        mqttClient.subscribe(number_input_topic.c_str());
    }
}

void reconnect_mqtt()
{
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(mqttClientId)) {
            Serial.println("connected");
            StaticJsonDocument<200> loginDoc;
            loginDoc["uuid"] = mqttClientId;
            loginDoc["name"] = name;
            loginDoc["device_type"] = deviceType;

            char loginJsonBuffer[512];
            serializeJson(loginDoc, loginJsonBuffer);

            mqttClient.subscribe(login_response_topic.c_str());
            mqttClient.publish(login_request_topic.c_str(), loginJsonBuffer);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup()
{
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        ESP.restart();
    }

    Serial.begin(115200);
    setup_wifi();
    mqttClient.setServer(mqtt_broker, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    reconnect_mqtt();
}

void sendRandomValue() {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastMessageTime > messageInterval) {
            lastMessageTime = currentMillis;

            double randomValue = random(0, 100) / 1.0;

            StaticJsonDocument<200> stateDoc;
            stateDoc["Soil_moisture"] = randomValue;

            char stateJsonBuffer[512];
            serializeJson(stateDoc, stateJsonBuffer);

            while (!mqttClient.publish(provide_value_topic.c_str(), stateJsonBuffer)) {
                delay(2000);
                mqttClient.publish(provide_value_topic.c_str(), stateJsonBuffer);
            }

            Serial.print("Published random value: ");
            Serial.println(randomValue);
        }
        xSemaphoreGive(mutex);
    }
}

void loop()
{
    if (!mqttClient.connected()) {
        reconnect_mqtt();
    }
    mqttClient.loop();

    sendRandomValue();
}
