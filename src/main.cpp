#include <Arduino.h>

#include "ESPAsyncWebServer.h"
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <Schedule.h>

#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <PubSubClient.h>

#include "Config.h"

#define DEFAULT_HOST "esp-thermometer"

#define DHT_PIN 2
#define DHT_TYPE DHT11

#define MQTT_MESSAGE_DELAY 5 // Delay of the mqtt messages in seconds

static const char html[] PROGMEM = "<!DOCTYPE html><html lang='de' class=''><head> <meta charset='utf-8'> <meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no' /> <title>ESP Thermometer konfiguration</title> <style> div, fieldset, input, select { padding: 5px; font-size: 1em; } h1 { text-align: center } fieldset { background-color: #fff; } p { margin: 0.5em 0; } input { width: 100%%; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing: border-box; } input[type=checkbox], input[type=radio] { width: 1em; margin-right: 6px; vertical-align: -1px; } select { width: 100%%; } textarea { resize: none; width: 98%%; height: 318px; padding: 5px; overflow: auto; } body { text-align: center; font-family: verdana; } td { padding: 0px; } button { border: 0; border-radius: 0.3rem; background-color: #1fa3ec; color: #fff; line-height: 2.4rem; font-size: 1.2rem; width: 100%%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer; } button:hover { background-color: #0e70a4; } .bgrn { background-color: #47c266; } .bgrn:hover { background-color: #5aaf6f; } .btnr { background-color: #ff0000; } .btnr:hover { background-color: #cc0000; } a { text-decoration: none; } .p { float: left; text-align: left; } .q { float: right; text-align: right; } </style></head><body> <div style='text-align:left;display:inline-block;min-width:340px;'> <h1>Einstellungen</h1> <form method='get' action='config'> <fieldset> <legend> <b>&nbsp;WLAN&nbsp;</b> </legend> <p> <b>WLAN SSID</b><br /> <input id='wifi-ssid' name='wifi-ssid' placeholder='SSID' maxlength='31' value='%s' required> </p> <p> <b>WLAN Passwort</b><br /> <input id='wifi-passwd' name='wifi-passwd' type='password' placeholder='Passwort' maxlength='31' value='%s' required> </p> <p> <b>Hostname</b><br /> <input id='host' name='host' placeholder='Hostname' value='esp-thermometer' maxlength='31' value='%s' required> </p><br /> </fieldset><br /> <fieldset> <legend> <b>&nbsp;MQTT&nbsp;</b> </legend> <p> <b>Broker IP-Adresse</b><br /> <input id='broker-ip' name='broker-ip' placeholder='MQTT Broker IP-Adresse' pattern='(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)_*(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)_*){3}' maxlength='15' value='%s' required> </p> <p> <b>MQTT Port</b><br /> <input name='mqtt-port' type='number' min='0' max=' 65535' placeholder='Port' value='%u' required> </p> <p> <b>MQTT Nutzername</b><br /> <input id='mqtt-user' name='mqtt-user' placeholder='MQTT Nutzername' maxlength='31' value='%s' required> </p> <p> <b>MQTT Passwort</b><br /> <input id='mqtt-passwd' name='mqtt-passwd' type='password' placeholder='Passwort' maxlength='31' value='%s' required> </p> <p> <b>MQTT Topic</b><br /> <input id='mqtt-topic' name='mqtt-topic' placeholder='MQTT Topic' maxlength='254' value='%s' required> </p><br /> </fieldset><br /> <fieldset> <legend> <b>&nbsp;Allgemein&nbsp;</b> </legend> <p> <b>Temperaturanpassung in C&deg (-10 C&deg - 10 C&deg)</b><br /> <input name='temp-correction' type='number' min='-10' max='10' placeholder='Wert in C&deg' value='%d' required> </p><br /> </fieldset><br /> <button name='save' type='submit' class='button bgrn'>Speichern</button><br /> </form> <br /> <form action='/rst'> <button name='reset' class='button btnr'>Zurücksetzen</button> </form> </div></body></html>";

Config config;
DNSServer dnsServer;
AsyncWebServer webServer(80);
WiFiEventHandler connectedHandler, disconnectedHandler;

// Thermometer stuff
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHT_TYPE);

/** Last time I tried to connect to WiFi */
unsigned long lastConnectTry = 0;
unsigned long lastMQTTConnect = 0;
unsigned long lastMQTTPublish = 0;

void onHTTPRequest(AsyncWebServerRequest *request);
void onReceivedConfig(AsyncWebServerRequest *request);
void onReceivedReset(AsyncWebServerRequest *request);

void initApMode();
void initDNSServer();
void initWebServer();

void connectWiFi();
void wifiOnDisconnect(const WiFiEventStationModeDisconnected &event);
void wifiOnConnect(const WiFiEventStationModeGotIP &event);

void connectMQTT();
void publishMQTTData(float temperature, float humidity);

void setup() {
    WiFi.setAutoReconnect(false);
    WiFi.setAutoConnect(false);
    WiFi.persistent(false);

    delay(1000);
    Serial.begin(9600);
    Serial.println("\n[ESP] Started WIFI thermometer...\n");

    // Load and check config
    config.loadConfig();
    Serial.println(config.flashInitialized() ? "[CONFIG] Flash is initialized" : "[CONFIG] Flash is uninitialized");
    Serial.println(config.isValid() ? "[CONFIG] Loaded valid config from flash" : "[CONFIG] No valid config stored in flash");

    dht.begin();

    initApMode();
    initDNSServer();
    initWebServer();
    connectWiFi();
}

void loop() {
    dnsServer.processNextRequest();
    MDNS.update();

    // Reconnect wifi
    if (WiFi.status() != WL_CONNECTED && millis() > (lastConnectTry + 12000)) {
        connectWiFi();
    }

    delay(1000);

    // Skip mqtt stuff because config is invalid or wifi is not availavble
    if (!config.isValid() || WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Try mqtt connection and reconnect if its time for it
    if (!mqttClient.connected() && millis() > (lastMQTTConnect + 15000)) {
        connectMQTT();
        return;
    }

    mqttClient.loop();
    delay(50); // For stability

    // Publish every 10 seconds
    if (millis() > (lastMQTTPublish + 10000)) {
        // Read sensor data !!!!!!!!!!!
        float humidity = dht.readHumidity();
        float temp = dht.readTemperature() + config.getTempCorrection(); // In celsius

        if (isnan(humidity) || isnan(temp)) {
            Serial.println("[DHT] Failed to read from DHT sensor!");
            return;
        } else {
            publishMQTTData(temp, humidity);
        }
    }

    yield();
}

void initApMode() {
    connectedHandler = WiFi.onStationModeGotIP(wifiOnConnect);
    disconnectedHandler = WiFi.onStationModeDisconnected(wifiOnDisconnect);

    // Start access point
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    const char *host = config.isValid() ? config.getHostname() : DEFAULT_HOST;
    WiFi.softAP(host);
    delay(500);
    WiFi.mode(WIFI_AP_STA);
    Serial.printf("[WIFI] Starting access point mode with ssid: '%s'\n", host);
}

void initDNSServer() {
    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", WiFi.softAPIP());

    // MDNS setup
    const char *host = config.isValid() ? config.getHostname() : DEFAULT_HOST;
    MDNS.begin(host);
    MDNS.addService("http", "tcp", 80);

    Serial.println("[DNS] DNS server and MDNS started");
    Serial.printf("[DNS] Hostname is set to: '%s'\n", host);
}

void initWebServer() {
    // Init webserver
    webServer.on("/", onHTTPRequest);
    webServer.on("/generate_204", onHTTPRequest); //Android captive portal. Maybe not needed. Might be handled by notFound handler.
    webServer.on("/fwlink", onHTTPRequest);       //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
    webServer.onNotFound(onHTTPRequest);          //Any page

    webServer.on("/config", onReceivedConfig);
    webServer.on("/rst", onReceivedReset);

    webServer.begin();

    Serial.println("[WEBSERVER] Webserver started");
}

void onHTTPRequest(AsyncWebServerRequest *request) {
    size_t bufferSize = strlen_P(html) + sizeof(config) + 1;
    char *htmlBuffer = (char *)calloc(bufferSize, sizeof(char));

    if (config.isValid()) {
        snprintf_P(htmlBuffer, bufferSize, html, config.getSSID(), config.getWifiPassword(), config.getHostname(), config.getMqttIP(), (unsigned int)config.getMqttPort(),
                   config.getMqttUsername(), config.getMqttPassword(), config.getMqttTopic(), (int)config.getTempCorrection());
    } else {
        snprintf_P(htmlBuffer, bufferSize, html, "", "", "", "", 1883, "", "", "", 0);
    }

    request->send(200, "text/html", htmlBuffer);
    free(htmlBuffer);

    Serial.print("[WEBSERVER] Responded to http client: ");
    Serial.println(IPAddress(request->client()->getRemoteAddress()));
}

void onReceivedConfig(AsyncWebServerRequest *request) {
    Config newConfig;
    newConfig.setSSID((char *)request->arg("wifi-ssid").c_str());
    newConfig.setWifiPassword((char *)request->arg("wifi-passwd").c_str());
    newConfig.setHostname((char *)request->arg("host").c_str());
    newConfig.setMqttIP((char *)request->arg("broker-ip").c_str());
    newConfig.setMqttPort(request->arg("mqtt-port").toInt());
    newConfig.setMqttUsername((char *)request->arg("mqtt-user").c_str());
    newConfig.setMqttPassword((char *)request->arg("mqtt-passwd").c_str());
    newConfig.setMqttTopic((char *)request->arg("mqtt-topic").c_str());
    newConfig.setTempCorrection(request->arg("temp-correction").toInt());

    newConfig.setMagicNumber(MAGIC_NUMBER);

    if (newConfig.isValid()) {
        newConfig.saveConfig();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "<h1>OK saved new config! Restarting in 3 seconds...</h1> <p> You are redirected automatically in 8 seconds</p>");
        String url = String("url=http://") + String(config.getHostname()) + String(".local");
        response->addHeader("refresh", "8;" + url);
        request->send(response);

        Serial.println("[ESP] Saved new config. Restarting in 3 seconds...");

        //Runs the code in 3 seconds
        schedule_function(
            []() {
                delay(3000);
                ESP.restart();
            });

    } else {
        Serial.println("[ESP] Received bad config. Ignoring it!");

        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "<h1>An error occured...please go back to where you came from.</h1><p>You are redirected automatically in 5 seconds</p>");
        String url = String("url=http://") + String(config.getHostname()) + String(".local");
        response->addHeader("refresh", "5;" + url);
        request->send(response);
    }
}

void onReceivedReset(AsyncWebServerRequest *request) {
    config.eraseConfigFlash();

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "<h1>Resetting device in 3 seconds!</h1><p>You are redirected automatically in 8 seconds</p>");
    String url = String("url=http://") + String(DEFAULT_HOST) + String(".local");
    response->addHeader("refresh", "8;" + url);
    request->send(response);

    Serial.println("[ESP] Resetted flash. Restarting in 3 seconds...");

    //Runs the code in 3 seconds
    schedule_function(
        []() {
            delay(3000);
            ESP.restart();
        });
}

void connectWiFi() {
    lastConnectTry = millis();

    if (!config.isValid()) {
        //Serial.println("[WIFI] Not trying to connect to WiFi because config is invalid");
        return;
    }

    Serial.printf("[WIFI] Connecting to WiFi: %s\n", config.getSSID());
    WiFi.begin(config.getSSID(), config.getWifiPassword());
}

void wifiOnDisconnect(const WiFiEventStationModeDisconnected &event) {
    if (WiFi.getMode() != WIFI_AP_STA) {
        //Runs the code when its time for it
        schedule_function(
            []() {
                WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
                const char *host = config.isValid() ? config.getHostname() : DEFAULT_HOST;
                WiFi.softAP(host);
                WiFi.mode(WIFI_AP_STA);

                MDNS.notifyAPChange();

                Serial.println("[WIFI] WiFi disconnected! Enabling access point mode");
            });
    }
}

void wifiOnConnect(const WiFiEventStationModeGotIP &event) {
    Serial.print("[WIFI] Connected to WiFi: ");
    Serial.println(WiFi.SSID());
    Serial.print("[NETWORK] Got IPv4: ");
    Serial.println(WiFi.localIP());

    MDNS.notifyAPChange();
    WiFi.mode(WIFI_STA); //close AP network
    Serial.println("[WIFI] Disabling access point mode");
}

void connectMQTT() {
    //If mqtt is not connected, connect to it
    mqttClient.setServer(config.getMqttIP(), config.getMqttPort());
    if (!mqttClient.connected()) {
        Serial.print("[MQTT] Connecting to MQTT broker ");
        Serial.println(config.getMqttIP());
        wifiClient = WiFiClient();
        if (!mqttClient.connect(config.getHostname(), config.getMqttUsername(), config.getMqttPassword())) {
            Serial.print("[MQTT] Connection to broker failed. Reconnecting in 15 seconds! Error code is:");
            Serial.println(mqttClient.state());
        } else {
            Serial.println("[MQTT] Connected to MQTT broker ");
        }
    }
    lastMQTTConnect = millis();
}

/**
 * Publishes temperature and humidity as JSON object to the MQTT broker.
 * 
 * @param temperature The temperature to send (in degrees) 
 * @param humidity The humidty value to send (in percent)
*/
void publishMQTTData(float temperature, float humidity) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &jObj = jsonBuffer.createObject();

    jObj["temperature"] = (String)temperature;
    jObj["humidity"] = (String)humidity;

    char data[200];
    jObj.printTo(data, jObj.measureLength() + 1);

    if (mqttClient.connected() && !mqttClient.publish(config.getMqttTopic(), data, true)) {
        Serial.print("[MQTT] Publishing sensor data failed due to error: ");
        Serial.println(mqttClient.getWriteError());
    }

    lastMQTTPublish = millis();
    yield();
}