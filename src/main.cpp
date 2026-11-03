#include <Arduino.h>

#include "secrets.h"

#include "com/elemental/biblioteca/network/core/RingBuffer.h"
#include "com/elemental/biblioteca/network/core/MessageParser.h"
#include "com/elemental/biblioteca/network/core/CommandQueue.h"
#include "com/elemental/biblioteca/network/core/ESPAtDriver.h"
#include "com/elemental/biblioteca/network/WiFiManager.h"
#include "com/elemental/biblioteca/network/MqttClient.h"

using namespace com::elemental::biblioteca::network;


// ESP-01 en Serial1 (pines 18/19 del Mega). Ajusta baudios/puerto a tu cableado.
#define ESP_SERIAL       Serial3
#define ESP_SERIAL_BAUD  115200


static core::RingBuffer<128> ringBuffer;
static core::MessageParser   parser;
static core::CommandQueue    queue;

static core::ESPAtDriver esp(ESP_SERIAL, ringBuffer, parser, queue);
static WiFiManager       wifi(esp);
static MqttClient        mqtt(wifi, esp);


static void onMqttMessage(
    const char* topic,
    const uint8_t* payload,
    size_t length
)
{
    Serial.print(F("[MQTT] "));
    Serial.print(topic);
    Serial.print(F(" -> "));
    Serial.write(payload, length);
    Serial.println();
}


void setup()
{
    Serial.begin(115200);

    esp.begin(ESP_SERIAL_BAUD);

    if(!wifi.begin())
        Serial.println(F("[MAIN] WiFi no conectado, MqttClient reintentara"));

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCredentials(MQTT_USER, MQTT_PASS);
    mqtt.setClientId(MQTT_CLIENT_ID);
    mqtt.onMessage(onMqttMessage);

    // Se registra ahora y se re-suscribe sola en cada reconexion.
    mqtt.subscribe(MQTT_TOPIC);
}


void loop()
{
    mqtt.update();

    // Ejemplo de publicacion (cuando mqtt.connected() es true):
    // mqtt.publish(MQTT_TOPIC, "1");
}
