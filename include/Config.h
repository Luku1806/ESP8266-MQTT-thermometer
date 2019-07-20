#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define MAGIC_NUMBER 0xC0FFEE

class Config {

    typedef struct cfg_struct {
        int magicNumber;        // Useed to determine if flash is initialized
        char wifi_ssid[32];     // SSID of WiFi
        char wifi_passwd[32];   // Password of WiFi
        char wifi_hostname[32]; // Password of WiFi

        char mqtt_ip[16];                // Ip address or hostname of MQTT broker
        unsigned short mqtt_port = 1833; // Port of MQTT broker
        char mqtt_user[32];              // Username for MQTT broker
        char mqtt_passwd[32];            // Password for MQTT broker
        char mqtt_topic[255];            // MQTT publish topic

        int8 temp_correct;        // Value for temperature correction
        uint16 messageDelay = 10; // The time to wait between to mqtt publishes
    } cfg;

  private:
    cfg config_struct;

  public:
    void eraseConfigFlash();
    void loadConfig();
    void saveConfig();

    void printConfig();

    bool flashInitialized();
    bool isValid();

    //Getter
    char *getSSID();
    char *getWifiPassword();
    char *getHostname();

    char *getMqttIP();
    unsigned short getMqttPort();
    char *getMqttUsername();
    char *getMqttPassword();
    char *getMqttTopic();

    int8 getTempCorrection();
    uint16 getMessageDelay();

    int getMagicNumber();

    //Setter
    bool setSSID(char ssid[]);
    bool setWifiPassword(char passwd[]);
    bool setHostname(char hostname[]);

    bool setMqttIP(char ip[]);
    bool setMqttPort(unsigned short port);
    bool setMqttUsername(char user[]);
    bool setMqttPassword(char passwd[]);
    bool setMqttTopic(char topic[]);

    bool setTempCorrection(int8 value);
    bool setMessageDelay(uint16 delay);

    bool setMagicNumber(int val);
};

#endif