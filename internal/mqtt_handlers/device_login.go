package mqtt_handlers

import (
	"NSI-semester-work/internal/db"
	"NSI-semester-work/internal/model"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	MQTT "github.com/eclipse/paho.mqtt.golang"
	"log"
	"os"
)

func HandleDeviceLogin(client MQTT.Client, msg MQTT.Message, database *db.Database) {
	var device model.Device
	if err := json.Unmarshal(msg.Payload(), &device); err != nil {
		log.Printf("Error decoding JSON: %s", err)
		return
	}
	log.Println(device)

	actionTemplateId, err := database.FetchTemplateActions(device.DeviceType)
	if actionTemplateId == -1 {
		if !errors.Is(err, sql.ErrNoRows) {
			fmt.Printf("unable to fetch template actionTemplateId %s\n", err)
			return
		}
	}

	device.ActionsTemplateId = actionTemplateId
	err = database.RegisterDevice(&device)
	if err != nil {
		log.Println(err)
	}

	deviceId, err := database.GetDeviceIDByUUID(device.UUID)
	if err != nil {
		log.Println("Failed to fetch device id")
		return
	}

	stateJson, err := database.GetDeviceStates(deviceId)
	if err != nil {
		log.Printf("Failed to fetch device states: %s", err)
		return
	}

	responseTopic := os.Getenv("MQTT_LOGIN_RESPONSE_TOPIC") + device.UUID
	responsePayload := fmt.Sprintf("{\"login\": \"successful\", \"state\": %s}", stateJson)
	token := client.Publish(responseTopic, 0, false, []byte(responsePayload))
	token.Wait()
}
