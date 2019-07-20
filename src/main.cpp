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

// The hostname used if nothing is set in the config or there is no config
#define DEFAULT_HOST "esp-thermometer"

// The config for the dht sensor
#define DHT_PIN 2
#define DHT_TYPE DHT11

// The config website
static const char html[] PROGMEM = "<!DOCTYPE html><html lang='de' class=''><head> <meta charset='utf-8'> <meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no' /> <title>ESP Thermometer konfiguration</title> <style> div, fieldset, input, select { padding: 5px; font-size: 1em; } h1 { text-align: center } fieldset { background-color: #fff; } p { margin: 0.5em 0; } input { width: 100%%; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing: border-box; } input[type=checkbox], input[type=radio] { width: 1em; margin-right: 6px; vertical-align: -1px; } select { width: 100%%; } textarea { resize: none; width: 98%%; height: 318px; padding: 5px; overflow: auto; } body { text-align: center; font-family: verdana; } td { padding: 0px; } button { border: 0; border-radius: 0.3rem; background-color: #1fa3ec; color: #fff; line-height: 2.4rem; font-size: 1.2rem; width: 100%%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer; } button:hover { background-color: #0e70a4; } .bgrn { background-color: #47c266; } .bgrn:hover { background-color: #5aaf6f; } .btnr { background-color: #ff0000; } .btnr:hover { background-color: #cc0000; } a { text-decoration: none; } .p { float: left; text-align: left; } .q { float: right; text-align: right; } </style></head><body> <div style='text-align:left;display:inline-block;min-width:340px;'> <h1>Einstellungen</h1> <form method='get' action='config'> <fieldset> <legend> <b>&nbsp;WLAN&nbsp;</b> </legend> <p> <b>WLAN SSID</b><br /> <input id='wifi-ssid' name='wifi-ssid' placeholder='SSID' maxlength='31' value='%s' required> </p> <p> <b>WLAN Passwort</b><br /> <input id='wifi-passwd' name='wifi-passwd' type='password' placeholder='Passwort' maxlength='31' value='%s' required> </p> <p> <b>Hostname</b><br /> <input id='host' name='host' placeholder='Hostname' value='esp-wifi-thermometer' maxlength='31' value='%s' required> </p><br /> </fieldset><br /> <fieldset> <legend> <b>&nbsp;MQTT&nbsp;</b> </legend> <p> <b>Broker IP-Adresse</b><br /> <input id='broker-ip' name='broker-ip' placeholder='MQTT Broker IP-Adresse' pattern='(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)_*(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)_*){3}' maxlength='15' value='%s' required> </p> <p> <b>MQTT Port</b><br /> <input name='mqtt-port' type='number' min='0' max=' 65535' placeholder='Port' value='%u' required> </p> <p> <b>MQTT Nutzername</b><br /> <input id='mqtt-user' name='mqtt-user' placeholder='MQTT Nutzername' maxlength='31' value='%s' required> </p> <p> <b>MQTT Passwort</b><br /> <input id='mqtt-passwd' name='mqtt-passwd' type='password' placeholder='Passwort' maxlength='31' value='%s' required> </p> <p> <b>MQTT Topic</b><br /> <input id='mqtt-topic' name='mqtt-topic' placeholder='MQTT Topic' maxlength='254' value='%s' required> </p><br /> </fieldset><br /> <fieldset> <legend> <b>&nbsp;Allgemein&nbsp;</b> </legend> <p> <b>Temperaturanpassung in C&deg (-10 C&deg - 10 C&deg)</b><br /> <input name='temp-correction' type='number' min='-10' max='10' placeholder='Wert in C&deg' value='%d' required> </p> <p> <b>MQTT Nachrichten Frequenz (2-65536 Sekunden)</b><br /> <input name='mqtt-delay' type='number' min='2' max='65536' placeholder='Wert in Sekunden' value='%u' required> </p><br /> </fieldset><br /> <button name='save' type='submit' class='button bgrn'>Speichern</button><br /> </form> <br /> <form action='/rst'> <button name='reset' class='button btnr'>Zurücksetzen</button> </form> </div></body></html>";

//All the server objects needed
Config config;
DNSServer dnsServer;
AsyncWebServer webServer(80);
WiFiEventHandler connectedHandler, disconnectedHandler;

// Thermometer stuff
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHT_TYPE);

// Connection tries, used to determine if a reconnect should be done
unsigned long lastConnectTry = 0;
unsigned long lastMQTTConnect = 0;
unsigned long lastMQTTPublish = 0;

void onHTTPRequest(AsyncWebServerRequest *request);
void onReceivedConfig(AsyncWebServerRequest *request);
void onReceivedReset(AsyncWebServerRequest *request);

void initApMode();
void initDNSServer();
void initWebServer();

bool connectWiFi();
void wifiOnDisconnect(const WiFiEventStationModeDisconnected &event);
void wifiOnConnect(const WiFiEventStationModeGotIP &event);

bool connectMQTT();
bool publishMQTTData(float temperature, float humidity);

void sendMQTTData();

void setup() {
    // We are doing all this ourselves
    WiFi.setAutoReconnect(false);
    WiFi.setAutoConnect(false);
    WiFi.persistent(false);

    delay(500);
    Serial.begin(9600);
    Serial.println("\n[ESP] Started WIFI thermometer...\n");

    // Load and check config
    config.loadConfig();
    Serial.println(config.flashInitialized() ? "[CONFIG] Flash is initialized" : "[CONFIG] Flash is uninitialized");
    Serial.println(config.isValid() ? "[CONFIG] Loaded valid config from flash" : "[CONFIG] No valid config stored in flash");

    // Start dht sensor reading
    dht.begin();

    // Attach wifi handlers to recognize when wifi is dis-/connected
    connectedHandler = WiFi.onStationModeGotIP(wifiOnConnect);
    disconnectedHandler = WiFi.onStationModeDisconnected(wifiOnDisconnect);

    // Start everything needed to work
    initApMode();
    initDNSServer();
    initWebServer();
    connectWiFi();
}

void loop() {
    // Update DNS services
    dnsServer.processNextRequest();
    MDNS.update();

    // Give time to the webserver to do its thing
    delay(500);

    // If wifi is not connected already or after trying to connect, return because it is needed for mqtt
    if (!connectWiFi()) {
        return;
    }

    // If mqtt is not already connected or trying to connect failed, retrun because we dont even need to try sending data
    if (!connectMQTT()) {
        return;
    }

    mqttClient.loop();
    delay(50);

    // Finally publish data to mqtt broker
    sendMQTTData();

    yield();
}

/**
 * Initialises and starts the acces point mode
 */
void initApMode() {
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    const char *host = config.isValid() ? config.getHostname() : DEFAULT_HOST;
    WiFi.softAP(host);
    delay(500);
    WiFi.mode(WIFI_AP_STA);
    Serial.printf("[WIFI] Starting access point mode with ssid: '%s'\n", host);
}

/**
 * Initialieses and starts the DNS server and the MDNS responder.
 * Only has to be called on startup.
 */
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

/**
 * Initialises and starts the webserver.
 * Only has to be called on startup.
 */
void initWebServer() {
    webServer.on("/", onHTTPRequest);
    webServer.on("/generate_204", onHTTPRequest); //Android captive portal. Maybe not needed. Might be handled by notFound handler.
    webServer.on("/fwlink", onHTTPRequest);       //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
    webServer.onNotFound(onHTTPRequest);          //Any page

    webServer.on("/config", onReceivedConfig);
    webServer.on("/rst", onReceivedReset);

    webServer.begin();

    Serial.println("[WEBSERVER] Webserver started");
}

/**
 * Callback function for the http server.
 * Builds and sends responses to normal http requests, sending the config page.
 */
void onHTTPRequest(AsyncWebServerRequest *request) {
    size_t bufferSize = strlen_P(html) + sizeof(config) + 1;
    char *htmlBuffer = (char *)calloc(bufferSize, sizeof(char));

    if (config.isValid()) {
        snprintf_P(htmlBuffer, bufferSize, html, config.getSSID(), config.getWifiPassword(), config.getHostname(), config.getMqttIP(), (uint)config.getMqttPort(),
                   config.getMqttUsername(), config.getMqttPassword(), config.getMqttTopic(), (int)config.getTempCorrection(), (uint)config.getMessageDelay());
    } else {
        snprintf_P(htmlBuffer, bufferSize, html, "", "", "", "", 1883, "", "", "", 0, (uint)10);
    }

    request->send(200, "text/html", htmlBuffer);
    free(htmlBuffer);

    Serial.print("[WEBSERVER] Responded to http client: ");
    Serial.println(IPAddress(request->client()->getRemoteAddress()));
}

/**
 * Callback function for the http server.
 * Reacts to http requests containing config information.
 * Checks the config and if its valid, saves the config and restarts the esp.
 */
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
    newConfig.setMessageDelay(request->arg("mqtt-delay").toInt());

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

/**
 * Callback function for the http server.
 * Reacts to http respnses, responding a reset.
 * Resets the flash memory and resrarts the esp.
 */
void onReceivedReset(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "<h1>Resetting device in 3 seconds!</h1><p>You are redirected automatically in 8 seconds</p>");
    String url = String("url=http://") + String(DEFAULT_HOST) + String(".local");
    response->addHeader("refresh", "8;" + url);
    request->send(response);

    Serial.println("[ESP] Resetted flash. Restarting in 3 seconds...");

    //Runs the code in 3 seconds
    schedule_function(
        []() {
            config.eraseConfigFlash();
            delay(3000);
            ESP.restart();
        });
}

/**
 * Connect the esp to wifi.
 * 
 * @return True if WiFi is connected. False if WiFi is disconnected.
 */
bool connectWiFi() {
    if (!config.isValid()) {
        return false;
    }

    // If already connected, return true
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    // Check if its time to connect. If not return false, because WiFi can not be connected at this point
    if (millis() < (lastConnectTry + 12000)) {
        return false;
    }

    Serial.printf("[WIFI] Connecting to WiFi: %s\n", config.getSSID());
    WiFi.begin(config.getSSID(), config.getWifiPassword());

    lastConnectTry = millis();

    delay(1500);

    // Check if wifi is connected after 1,5 seconds
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    } else {
        return false;
    }
}

/**
 * Callback to be called when WiFi disconnects.
 * This reenables the acces point mode.
 */
void wifiOnDisconnect(const WiFiEventStationModeDisconnected &event) {
    if (WiFi.getMode() != WIFI_AP_STA) {
        //Runs the code when its time for it
        schedule_function(
            []() {
                Serial.println("[WIFI] WiFi disconnected!");
                initApMode();
                MDNS.notifyAPChange();
            });
    }
}

/**
 * Callback to be called when WiFi connects.
 * This closes the acces point.
 */
void wifiOnConnect(const WiFiEventStationModeGotIP &event) {
    Serial.print("[WIFI] Connected to WiFi: ");
    Serial.println(WiFi.SSID());
    Serial.print("[NETWORK] Got IPv4: ");
    Serial.println(WiFi.localIP());

    MDNS.notifyAPChange();
    WiFi.mode(WIFI_STA); //close AP network
    Serial.println("[WIFI] Disabling access point mode");
}

/**
 * Tries to connect to the mqtt broker.
 * 
 * @return True if connection was succesfull, else false.
 */
bool connectMQTT() {
    // MQTT is already connected
    if (mqttClient.connected()) {
        return true;
    }

    // MQTT is not connected, but its not time to try to connect it
    if (millis() < (lastMQTTConnect + 15000)) {
        return false;
    }

    //MQTT is not connected but its time to try to connect, try to connect now
    Serial.print("[MQTT] Connecting to MQTT broker ");
    Serial.println(config.getMqttIP());

    lastMQTTConnect = millis();

    mqttClient.setServer(config.getMqttIP(), config.getMqttPort());
    if (!mqttClient.connect(config.getHostname(), config.getMqttUsername(), config.getMqttPassword())) {
        Serial.print("[MQTT] Connection to broker failed. Reconnecting in 15 seconds! Error code is:");
        Serial.println(mqttClient.state());
        return false;
    } else {
        Serial.println("[MQTT] Connected to MQTT broker");
        return true;
    }
}

/**
 * Publishes temperature and humidity as JSON object to the MQTT broker.
 * 
 * @param temperature The temperature to send (in degrees) 
 * @param humidity The humidty value to send (in percent)
*/
bool publishMQTTData(float temperature, float humidity) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &jObj = jsonBuffer.createObject();

    jObj["temperature"] = (String)temperature;
    jObj["humidity"] = (String)humidity;

    char data[200];
    jObj.printTo(data, jObj.measureLength() + 1);

    lastMQTTPublish = millis();

    if (mqttClient.connected() && !mqttClient.publish(config.getMqttTopic(), data, true)) {
        Serial.print("[MQTT] Publishing sensor data failed due to error: ");
        Serial.println(mqttClient.getWriteError());

        yield();
        return false;
    } else {
        yield();
        return true;
    }
}

/**
 * If its time for it, publishes the data to the mqtt broker
 */
void sendMQTTData() {
    // Publish mqtt packets every ? seconds
    if (millis() > lastMQTTPublish + (config.getMessageDelay() * 1000)) {
        float humidity = dht.readHumidity();
        float temp = dht.readTemperature() + config.getTempCorrection();

        if (isnan(humidity) || isnan(temp)) {
            Serial.println("[DHT] Failed to read from DHT sensor!");
            return;
        } else {
            publishMQTTData(temp, humidity);
        }
    }
}