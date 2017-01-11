#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <DHT.h>


//define pins variables
#define TRIGGER_PIN D1 //nodemcu notation
#define ONBOARD_LED D0 //nodemcu notation
#define DHTPIN D2 // what pin we’re connected to

//mqtt config variables
WiFiClient espClient;
PubSubClient client(espClient);
int  mqtt_port = 1883;
char mqtt_srvr[40] = "192.168.0.14";
char mqtt_user[40];
char mqtt_pass[40];

//set dht pin and model
DHT dht(DHTPIN, DHT11,15);

int loopCounter;
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
  delay(10);
  dht.begin();//init dht sensor
  client.setServer(mqtt_srvr, mqtt_port);//start the mqtt client
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
    
    Serial.println("saving config");
    //DynamicJsonBuffer jsonBuffer;
    StaticJsonBuffer<100> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;

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
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(ESP.getChipId());
     
    // Attempt to connect
    Serial.print("mqtt_user=");
    Serial.print(mqtt_user);
    Serial.print(" mqtt_pass=");
    Serial.println(mqtt_pass);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected to mqtt server");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
  }else{
    // Once connected, start publishing...
    float t= dht.readHumidity();
    float h= dht.readHumidity(); 
    char humidity[4];
    char temperature[4];

    if( !isnan(t) ){
      dtostrf(h,4, 2, humidity);
    }
    if( !isnan(t) ){
      dtostrf(t,4, 2, temperature);
    }  
    
    char topic[40];
    char data[40];
    sprintf(topic, "users/%s/", mqtt_user);
    sprintf(data, "{\"temperature\":%s;\"humidity\":%s}", temperature, humidity);

    loopCounter++;
    if(loopCounter == 3){
      Serial.print("publishing to topic: ");
      Serial.print(topic);
      Serial.print(" with data: ");
      Serial.println(data);      
      
      client.publish(topic, data);
      loopCounter = 0;
      Serial.println("6 seconds delay");
    }
  }
  // Wait 2 seconds before looping
  delay(2000);
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
        //DynamicJsonBuffer jsonBuffer;
        StaticJsonBuffer<100> jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          Serial.print("mqtt_user=");
          Serial.print(mqtt_user);
          Serial.print(" mqtt_pass=");
          Serial.println(mqtt_pass);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}
