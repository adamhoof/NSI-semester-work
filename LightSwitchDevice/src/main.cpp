#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "env.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
volatile bool lightState = false;

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");

    char msg[length + 1];
    strncpy(msg, (char*) payload, length);
    msg[length] = '\0';
    Serial.println(msg);

    if (strcmp(topic, toggle_topic.c_str()) == 0) {
        if (strcmp(msg, "Light_state") == 0) {
            StaticJsonDocument<200> doc;
            lightState = !lightState;
            doc["Action_name"] = "Light_state";
            lightState ? doc["Light_state"] = "On" : doc["Light_state"] = "Off";

            Serial.println("Device turned ON");

            char jsonBuffer[512];
            serializeJson(doc, jsonBuffer);
            mqttClient.publish(state_topic.c_str(), jsonBuffer);
        }
    } else if (strcmp(topic, login_response_topic.c_str()) == 0) {
        mqttClient.subscribe(toggle_topic.c_str());
        Serial.println("subscribed");
    }
}


void setup_wifi()
{
    delay(10);
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFiClass::status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect_mqtt_mqttClient()
{
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(mqttClientId)) {
            Serial.println("connected");

            StaticJsonDocument<200> doc;
            doc["uuid"] = mqttClientId;
            doc["name"] = name;
            doc["device_type"] = deviceType;

            char jsonBuffer[512];
            serializeJson(doc, jsonBuffer);

            mqttClient.subscribe(login_response_topic.c_str());
            mqttClient.publish(login_request_topic.c_str(), jsonBuffer);
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
    Serial.begin(115200);
    setup_wifi();
    mqttClient.setServer(mqtt_broker, mqtt_port);
    mqttClient.setCallback(mqttCallback);

    reconnect_mqtt_mqttClient();
}

void loop()
{
    if (!mqttClient.connected()) {
        reconnect_mqtt_mqttClient();
    }
    mqttClient.loop();
}
