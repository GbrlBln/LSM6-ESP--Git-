#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <time.h>
#include <LSM6DSOSensor.h>

#define BAUD 115200
#define DEV_I2C Wire

#define INT_1 4

const char *ssid = "Enter your SSID";
const char *pwd = "Enter your SSID password";

const char* mqtt_server = "Enter the ip address of the device the MQTT broker is running on";

const int mqtt_port = 1883;
const char* topicOut = "Enter chosen topic for outgoing data";
const char* topicIn = "Enter chosen topic for incoming data";
const char* mqttUser = "Enter the chosen MQTT user";
const char* mqttPass = "Enter the chosen MQTT password";

char report[256];

LSM6DSOSensor accGyr(&DEV_I2C);
WiFiClient espClient;
PubSubClient client(espClient);

volatile bool toggle = false;

void callback(char* topic, byte* payload, int length){
  toggle = (char)payload[0] == '1';
}
                             
void reconnect(){
  while (!client.connected()){
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if(client.connect(clientId.c_str(), mqttUser, mqttPass)){
      client.subscribe(topicIn);
    }else{
      delay(5000);
    }
  }
}

void connectToMqtt(const char* server, const int port){
  client.setServer(server, port);
  client.setCallback(callback);
}

void connectToSSID(){
  int n = WiFi.scanNetworks();
  // loop through scanned networks, and connect to work or home network
  if(n > 0){
    for(int i = 0; i < n; i++){

      if(WiFi.SSID(i) == ssid){
        WiFi.begin(ssid, pwd);
        // establish connection
        while( WiFi.status() != WL_CONNECTED){
          delay(1000);
        }
        connectToMqtt(mqtt_server, mqtt_port);
      }
    }
  }
}

void setup(){
  
  pinMode(D4, OUTPUT);
  digitalWrite(D4, HIGH);
  connectToSSID();
  configTime(0,0, "pool.ntp.org");
  DEV_I2C.begin();
  
  accGyr.begin();
  accGyr.Enable_X();
  accGyr.Enable_G(); 

  accGyr.Set_X_FS(LSM6DSO_16g);
  accGyr.Set_G_FS(LSM6DSO_2000dps);

}

void sendOrientation()
{

  struct timeval tv;
  int32_t accelerometer[3];
  int32_t gyroscope[3];

  accGyr.Get_X_Axes(accelerometer);
  accGyr.Get_G_Axes(gyroscope); 
  gettimeofday(&tv, nullptr);
  uint64_t epoch_millis = (uint64_t)tv.tv_sec * 1000 + tv.tv_usec/1000;

  sprintf(report, "%s,%d,%d,%d,%d,%d,%d",
    String(epoch_millis).c_str(),
    accelerometer[0], accelerometer[1], accelerometer[2],
    gyroscope[0], gyroscope[1], gyroscope[2]);

  client.publish(topicOut, report);

}

void loop(){

  if(WiFi.status() == WL_CONNECTED){
    LSM6DSO_Event_Status_t status;
    accGyr.Get_X_Event_Status(&status);

    if(!client.connected()){
      reconnect();
    }

    if(client.connected()){
      digitalWrite(D4, LOW);
    }

    client.loop();

    if(toggle == 1){
      sendOrientation();
    }
  }

}
