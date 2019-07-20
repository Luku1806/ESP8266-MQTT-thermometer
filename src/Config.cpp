#include "Config.h"

#include <EEPROM.h>

void Config::eraseConfigFlash() {
    // Reset EEPROM bytes to '0' for the length of the data structure
    EEPROM.begin(sizeof(config_struct));
    Serial.println(sizeof(config_struct));
    Serial.println(EEPROM.length());
    for (size_t i = 0; i < sizeof(config_struct); i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.end();
}

void Config::saveConfig() {
    // Save configuration from RAM into EEPROM
    config_struct.magicNumber = MAGIC_NUMBER;
    EEPROM.begin(sizeof(cfg));
    EEPROM.put(0, config_struct);
    EEPROM.end(); // Free RAM copy of structure
    config_struct.magicNumber = 0;
}

void Config::loadConfig() {
    // Loads configuration from EEPROM into RAM
    EEPROM.begin(sizeof(config_struct));
    EEPROM.get(0, config_struct);
    EEPROM.end();
}

void Config::printConfig() {
    Serial.println("\nConfig is set to:");

    Serial.print("WIFI-SSID: ");
    Serial.println(getSSID());

    Serial.print("WIFI-PSK: ");
    Serial.println(getWifiPassword());

    Serial.print("WIFI-HOSTNAME: ");
    Serial.println(getHostname());

    Serial.print("MQTT-BROKER-IP: ");
    Serial.println(getMqttIP());

    Serial.print("MQTT-BROKER-PORT: ");
    Serial.println(getMqttPort());

    Serial.print("MQTT-USER: ");
    Serial.println(getMqttUsername());

    Serial.print("MQTT-PASSWORD: ");
    Serial.println(getMqttPassword());

    Serial.print("MQTT-TOPIC: ");
    Serial.println(getMqttTopic());

    Serial.print("TEMP-CORRECTION: ");
    Serial.println(getTempCorrection());

    Serial.print("MESSAGE-DELAY: ");
    Serial.println(getMessageDelay());
}

bool Config::flashInitialized() {
    return config_struct.magicNumber == MAGIC_NUMBER;
}

bool Config::isValid() {
    return (config_struct.wifi_ssid != NULL && strlen(config_struct.wifi_ssid) > 0) &&
           (config_struct.wifi_passwd != NULL && strlen(config_struct.wifi_passwd) > 0) &&
           (config_struct.wifi_hostname != NULL && strlen(config_struct.wifi_hostname) > 0) &&
           (config_struct.mqtt_ip != NULL && strlen(config_struct.mqtt_ip) > 0) &&
           (config_struct.mqtt_user != NULL && strlen(config_struct.mqtt_user) > 0) &&
           (config_struct.mqtt_passwd != NULL && strlen(config_struct.mqtt_passwd) > 0) &&
           (config_struct.mqtt_topic != NULL && strlen(config_struct.mqtt_topic) > 0) &&
           config_struct.temp_correct != NAN && config_struct.messageDelay != NAN &&
           flashInitialized();
}

// Getter
char *Config::getSSID() { return config_struct.wifi_ssid; }

char *Config::getWifiPassword() { return config_struct.wifi_passwd; }

char *Config::getHostname() { return config_struct.wifi_hostname; }

char *Config::getMqttIP() { return config_struct.mqtt_ip; }

unsigned short Config::getMqttPort() { return config_struct.mqtt_port; }

char *Config::getMqttUsername() { return config_struct.mqtt_user; }

char *Config::getMqttPassword() { return config_struct.mqtt_passwd; }

char *Config::getMqttTopic() { return config_struct.mqtt_topic; }

int8 Config::getTempCorrection() { return config_struct.temp_correct; }

uint16 Config::getMessageDelay() { return config_struct.messageDelay; };

int Config::getMagicNumber() { return config_struct.magicNumber; }

// Setter
bool Config::setSSID(char ssid[]) {
    strcpy(config_struct.wifi_ssid, ssid);
    return true;
}

bool Config::setWifiPassword(char passwd[]) {
    strcpy(config_struct.wifi_passwd, passwd);
    return true;
}

bool Config::setHostname(char hostname[]) {
    strcpy(config_struct.wifi_hostname, hostname);
    return true;
}

bool Config::setMqttIP(char ip[]) {
    strcpy(config_struct.mqtt_ip, ip);
    return true;
}

bool Config::setMqttPort(unsigned short port) {
    config_struct.mqtt_port = port;
    return true;
}

bool Config::setMqttUsername(char user[]) {
    strcpy(config_struct.mqtt_user, user);
    return true;
}

bool Config::setMqttPassword(char passwd[]) {
    strcpy(config_struct.mqtt_passwd, passwd);
    return true;
}

bool Config::setMqttTopic(char topic[]) {
    strcpy(config_struct.mqtt_topic, topic);
    return true;
}

bool Config::setTempCorrection(int8 value) {
    config_struct.temp_correct = value;
    return true;
}

bool Config::setMessageDelay(uint16 delay) {
    config_struct.messageDelay = delay;
    return true;
}

bool Config::setMagicNumber(int value) {
    config_struct.magicNumber = value;
    return true;
}