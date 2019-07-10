#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

#define DHT_PIN 2
#define DHT_TYPE DHT11
#define DHT_DEVIATION                                                          \
  4 // The deviation of the used DHT to correct the temp reading

#define WIFI_SSID "your_ssid"
#define WIFI_PSK "ypur_passwd"
#define WIFI_HOSTNAME "ESP-Thermometer"

#define MQTT_BROKER "192.168.178.200"
#define MQTT_USER "your_usr"
#define MQTT_PASSWORD "your_passwd"
#define MQTT_FRIENDLY_NAME "ESP_thermometer"
#define MQTT_SENSOR_TOPIC "lukas/sensors/temp/esp1"
#define MQTT_MESSAGE_DELAY 5 // Delay of the mqtt messages in seconds

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHT_TYPE);

/**
 * Connects the ESP to the specified WIFI an to the specified MQTT broker.
 * Blocks until both connections are established.
 */
void connect() {
  // If WIFI is not connected, connect to it
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to WIFI ");
    Serial.print(WIFI_SSID);
    Serial.print("...");

    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address:");
    Serial.print(WiFi.localIP());
    Serial.println("\n");
  }

  // If mqtt is not connected, connect to it
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT broker...");
    if (!mqttClient.connect(MQTT_FRIENDLY_NAME, MQTT_USER, MQTT_PASSWORD)) {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
  Serial.println("Connected to MQTT broker\n");

  // Enable MDNS
  if (!MDNS.begin(WIFI_HOSTNAME)) {
    Serial.println("MDNS could not be set up");
  }
}

/**
 * Publishes temperature and humidity as JSON object to the MQTT broker.
 *
 * @param temperature The temperature to send (in degrees)
 * @param humidity The humidty value to send (in percent)
 */
void publishData(float temperature, float humidity) {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject &jObj = jsonBuffer.createObject();

  jObj["temperature"] = (String)temperature;
  jObj["humidity"] = (String)humidity;

  char data[200];
  jObj.printTo(data, jObj.measureLength() + 1);
  mqttClient.publish(MQTT_SENSOR_TOPIC, data, true);
  yield();
}

void setup() {
  delay(10);
  Serial.begin(115200);
  Serial.println("Starting WIFI thermometer...\n");

  dht.begin();

  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  mqttClient.setServer(MQTT_BROKER, 1883);

  connect();
}

void loop() {
  if (!mqttClient.connected()) {
    connect();
  }

  mqttClient.loop();
  delay(10); // --> Improves WIFI stability

  // Serial.print("Alive since ");
  // Serial.print(millis());
  // Serial.println(" milliseconds");

  // Read sensor data !!!!!!!!!!!
  float humidity = dht.readHumidity();
  float temp = dht.readTemperature() - DHT_DEVIATION; // In celsius

  if (isnan(humidity) || isnan(temp)) {
    Serial.println("ERROR: Failed to read from DHT sensor!");
    return;
  } else {
    publishData(temp, humidity);
  }

  delay(MQTT_MESSAGE_DELAY * 1000);
}
