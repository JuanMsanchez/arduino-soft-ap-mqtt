#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <DHT.h>


//define pins variables
#define TRIGGER_PIN D6 //nodemcu notation
#define ONBOARD_LED D0 //nodemcu notation
#define DHTPIN D5 // what pin we’re connected to
#define RELAY1_PIN D3 //nodemcu notation
#define RELAY2_PIN D4 //nodemcu notation

//mqtt config variables
WiFiClient espClient;
PubSubClient client(espClient);
int  mqtt_port = 1883;
char mqtt_srvr[40] = "192.168.0.14";
char mqtt_user[40];
char mqtt_pass[40];
char mqtt_alias[40];
char relay1_alias[40] = "relay1";
char relay2_alias[40] = "relay2";

//set dht pin and model
DHT dht(DHTPIN, DHT11,15);

int loopCounter;
// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_mqtt_user("mqtt_user", "mqtt user", mqtt_user, 40);
WiFiManagerParameter custom_mqtt_pass("mqtt_password", "mqtt password", mqtt_pass, 40);
WiFiManagerParameter custom_mqtt_alias("mqtt_alias", "mqtt_alias", mqtt_alias, 40);
WiFiManagerParameter custom_relay1_alias("relay1_alias", "relay1_alias", relay1_alias, 40);
WiFiManagerParameter custom_relay2_alias("relay2_alias", "relay2_alias", relay2_alias, 40);

//flag for saving data to the filesystem
bool shouldSaveConfig = false;
String clientId;

void setup() {
  //set the pin modes
  pinMode(TRIGGER_PIN, INPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH); //realay off
  digitalWrite(RELAY2_PIN, HIGH); //realay off
  digitalWrite(ONBOARD_LED, HIGH);
  clientId = String(ESP.getChipId());
  Serial.begin(115200);
  Serial.println("\n Starting");
  delay(10);
  dht.begin();//init dht sensor
  client.setServer(mqtt_srvr, mqtt_port);//start the mqtt client
  client.setCallback(callback);
  loadFSconfig(); //load the configuration from the filesystem
  loopCounter = 0;
}


void loop() {
  //when config button is pressed start in AP mode
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    startAPConfigPortal(); //enter on a blocking loop until is connected
  }

  if (shouldSaveConfig) {
    //set mqtt_pass and mqtt_user with the AP form inputs values
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_alias, custom_mqtt_alias.getValue());

    Serial.println("saving config");
    //DynamicJsonBuffer jsonBuffer;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["mqtt_alias"] = mqtt_alias;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println("");

    json.printTo(configFile);
    configFile.close();
    shouldSaveConfig = false; //reset the saveConfigCallback flag
  }

  if(!client.connected()){
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    Serial.print("mqtt_user=");
    Serial.print(mqtt_user);
    Serial.print(" mqtt_pass=");
    Serial.print(mqtt_pass);
    Serial.print(" mqtt_alias=");
    Serial.println(mqtt_alias);

    //if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected to mqtt server");
      char relayTopic[40];
      char reportTopic[40];
      
      sprintf(relayTopic, "users/%s/%s/write/actuator/#", mqtt_user, clientId.c_str());
      sprintf(reportTopic, "users/%s/report", mqtt_user);
      client.subscribe(relayTopic);
      client.subscribe(reportTopic);
      Serial.print("subscribed to ");
      Serial.println(relayTopic);
      Serial.print("subscribed to ");
      Serial.println(reportTopic);      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
  }else{
    client.loop();
    // Once connected, start publishing...
    float t= dht.readTemperature();
    float h= dht.readHumidity();
    int r1= digitalRead(RELAY1_PIN);
    int r2= digitalRead(RELAY2_PIN);
    char humidity[6];
    char temperature[6];
    char relay1[6];
    char relay2[6];
    
    //here we grab the last valid value
    if( !isnan(t) ){
      dtostrf(h,4, 2, humidity);
    }
    if( !isnan(t) ){
      dtostrf(t,4, 2, temperature);
    }

    if(r1 == HIGH){
      strcpy(relay1, "false");
    }else{
      strcpy(relay1, "true");
    }

    if(r2 == HIGH){
      strcpy(relay2, "false");
    }else{
      strcpy(relay2, "true");
    }
    
    char topic[40];
    char data[40];

    //three iterations before publishing
    loopCounter++;
    if(loopCounter == 3){
      //publish temperature
      sprintf(topic, "users/%s/%s/sensor/temperature", mqtt_user, clientId.c_str());
      sprintf(data, "{\"value\":%s,\"origin\":\"%s\"}", temperature, mqtt_alias);
      client.publish(topic, data);
      logPublish(topic, data);
      
      //publish humidity
      sprintf(topic, "users/%s/%s/sensor/humidity", mqtt_user, clientId.c_str());
      sprintf(data, "{\"value\":%s,\"origin\":\"%s\"}", humidity, mqtt_alias);
      client.publish(topic, data);
      logPublish(topic, data);

      //publish relay1
      sprintf(topic, "users/%s/%s/actuator/%s", mqtt_user, clientId.c_str(), relay1_alias);
      sprintf(data, "{\"value\":%s,\"origin\":\"%s\"}", relay1, mqtt_alias);
      client.publish(topic, data);
      logPublish(topic, data);

      //publish relay2
      sprintf(topic, "users/%s/%s/actuator/%s", mqtt_user, clientId.c_str(), relay2_alias);
      sprintf(data, "{\"value\":%s,\"origin\":\"%s\"}", relay2, mqtt_alias);
      client.publish(topic, data);
      logPublish(topic, data);
      
      loopCounter = 0;
      Serial.println("6 seconds delay");
    }
  }
  // Wait 2 seconds before looping
  delay(2000);
}

void logPublish(char* topic, char* payload){
  Serial.print("publishing to topic: ");
  Serial.print(topic);
  Serial.print(" with data: ");
  Serial.println(payload);
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
  wifiManager.addParameter(&custom_mqtt_alias);
  wifiManager.addParameter(&custom_relay1_alias);
  wifiManager.addParameter(&custom_relay2_alias);

  String apNetworkName = "iot-" + clientId;
   
  if (!wifiManager.startConfigPortal(apNetworkName.c_str())) {
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
        //DynamicJsonBuffer jsonBuffer;
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_alias, json["mqtt_alias"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          Serial.print("mqtt_user=");
          Serial.print(mqtt_user);
          Serial.print(" mqtt_pass=");
          Serial.print(mqtt_pass);
          Serial.print(" mqtt_alias=");
          Serial.println(mqtt_alias);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived: ");
  Serial.print(topic);
  Serial.println();
  Serial.print("With payload: ");
  char payloadToChar[100];
  sprintf(payloadToChar, "%s", payload);
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  
  Serial.println();
  
  char currentTopic[50];
  char * params;
  params = strtok(topic, "/");

  while (params != NULL){
    strcpy(currentTopic, params);
    params = strtok (NULL, "/");
  } 

  Serial.print("Current topic: ");
  Serial.println(currentTopic);
  
  if(strcmp(currentTopic,"report") == 0){
    Serial.println("Registering device");
    char reportTopic[40];
    sprintf(reportTopic, "users/%s/%s/register", mqtt_user, clientId.c_str());
    client.publish(reportTopic, "{\"sensors\": [ \"temperature\",\"humidity\" ], \"actuators\": [\"cooler\", \"ligth\"]}");
  }

  if(strcmp(currentTopic,relay1_alias) == 0){
    Serial.println("Updating relay 1 state");
    StaticJsonBuffer<50> jsonBuffer;
    JsonObject& data = jsonBuffer.parseObject(payloadToChar);      
    data.printTo(Serial);
    if(strcmp(data["value"],"true") == 0){
      digitalWrite(RELAY1_PIN, LOW);
    }else{
      digitalWrite(RELAY1_PIN, HIGH);
    }
  }
  
  if(strcmp(currentTopic,relay2_alias) == 0){
    Serial.println("Updating relay 2 state");
    StaticJsonBuffer<50> jsonBuffer;
    JsonObject& data = jsonBuffer.parseObject(payloadToChar);      
    data.printTo(Serial);
    if(strcmp(data["value"],"true") == 0){
      digitalWrite(RELAY2_PIN, LOW);
    }else{
      digitalWrite(RELAY2_PIN, HIGH);
    }
  }
}
