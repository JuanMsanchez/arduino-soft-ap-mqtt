#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ESP8266WebServer.h>
#include <DNSServer.h>

//global variables
#define TRIGGER_PIN D1 //nodemcu notation
#define ONBOARD_LED D0 //nodemcu notation

//mqtt config variables
WiFiClient espClient;
PubSubClient client(espClient);
int  mqtt_port = 8883;
char mqtt_srvr[40] = "192.168.0.17";
char mqtt_user[40];
char mqtt_pass[40];



// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_mqtt_user("mqtt_user", "mqtt user", mqtt_user, 40);
WiFiManagerParameter custom_mqtt_pass("mqtt_password", "mqtt password", mqtt_pass, 40);

//flag for saving data to the filesystem
bool shouldSaveConfig = false;

void setup() {
  //set the pin modes
  pinMode(TRIGGER_PIN, INPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH);

  Serial.begin(115200);
  Serial.println("\n Starting");

  client.setServer(mqtt_srvr, mqtt_port);//start the mqtt client
  loadFSconfig(); //load the configuration from the filesystem
}


void loop() {
  //when config button is pressed start in AP mode
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    startAPConfigPortal(); //enter on a blocking loop until is connected
  }

  //set mqtt_pass and mqtt_user with the AP form inputs values
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    shouldSaveConfig = false; //reset the saveConfigCallback flag
  }

  if(!client.connected()){
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected to mqtt server");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }else{
    // Once connected, publish an announcement...
    char topic[40];
    sprintf(topic, "users/%s/", mqtt_user);
    Serial.print("publishing to topic: ");
    Serial.println(topic);
    client.publish(topic, "hello alice!");
  }
}

void startAPConfigPortal(){
  //blink the nodemcu onboard led to notify the user that the AP mode is on
  digitalWrite(ONBOARD_LED, LOW);
  delay(250);
  digitalWrite(ONBOARD_LED, HIGH);

  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);

  if (!wifiManager.startConfigPortal("OnDemandAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void saveConfigCallback(){
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//load the configuration from the file system
void loadFSconfig(){
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}
