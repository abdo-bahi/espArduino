// #include <esp32.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
//pins :
#define DHTPIN 14
#define RELAY_PIN 17 

void irrigation();

const int sensorPin = 32;
//changable variables
// ******* mcu ID changable ******
          unsigned long mcuID = 1;
// ******* mcu ID changable ******
          uint8_t threshold = 30;// 30%threshold 
          int irrigationTime = 10;//10 seconds
          unsigned long sleepingTime = 10;//10 seconds


const char* ssid = "abdo10s";
const char* password = "10011001";
String url = "http://192.168.161.144/";
int sensorValue = 0;
int dryValue = 2803; // Minimum sensor analogue value (dry soil)
int wetValue = 982; // Maximum sensor analogue value (wet soil)
int moisturePercentage = 0;

WiFiClient wifiClient;
HTTPClient http;
uint8_t offline = 0;

DHT dht(DHTPIN, DHT11);
void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
   digitalWrite(RELAY_PIN, HIGH);//off
  // wifiLogic
  WiFi.begin(ssid, password);
  while ((WiFi.status() != WL_CONNECTED && (offline < 60)) && (mcuID != (0))) {
    delay(1000);
    offline ++;
    Serial.print(".");
  }
  dht.begin();
}
void loop() {  
  delay(4000);
    //Check WiFi connection status
  if((WiFi.status()== WL_CONNECTED) && (mcuID != 0)){
      // Sensor variables
  float temperature =  dht.readTemperature();
  float humidity =  dht.readHumidity(); 
  sensorValue = analogRead(sensorPin);  
  moisturePercentage = map(sensorValue, wetValue, dryValue , 100, 0);
  //prepare sensors readings :
  if (isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    temperature = -1;
  }
  if (isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    humidity = -1;
  }
  if(moisturePercentage < 0){
    moisturePercentage = 0;
  }  
  Serial.println("soilMoisture : "+ String(moisturePercentage));

  if(moisturePercentage == 153){
    moisturePercentage = -1;
  }else if(moisturePercentage > 100){
    moisturePercentage= 100;
  }

        //post sensors data 
      http.begin(url + "api/sensordata/");
      http.addHeader("Content-Type", "application/json");
      String jsonOutput;
      const size_t CAPACITY = JSON_OBJECT_SIZE(1);
      StaticJsonDocument<CAPACITY> doc;
      JsonObject object = doc.to<JsonObject>();
      object["mcuId"]=mcuID;
      object["soilMoisture"]=moisturePercentage;
      object["temperature"]=(int) temperature;
      object["humidity"]=(int) humidity;

          serializeJson(doc, jsonOutput);
          Serial.println(String(jsonOutput));
          int code = http.POST(String(jsonOutput));
           if(code > 0){
              Serial.println("StatusCode : "+ String(code) + "\npayload : "+ String(http.getString()));
          }
          else{      
            Serial.println("no server post connection !!");
          offline=1;
              }
      http.end();
      //get update state
      http.begin(url+"api/mcu/" + String(mcuID)+"/");
       code = http.GET();
      if(code > 0){
        offline = 0;
        String payload = http.getString();
          Serial.println("StatusCode : "+ String(code) + "\npayload : "+ payload);
      DynamicJsonDocument ddoc(1024);
      deserializeJson(ddoc, payload);
      if(ddoc.containsKey("threshold")&&(ddoc["threshold"]<=100)&&(ddoc["threshold"] >= 0)){
        threshold = ddoc["threshold"];
        Serial.println("threshold added :"+ String(threshold));
      }   
      if(ddoc.containsKey("irrigationTime")&&(ddoc["irrigationTime"]<=36000)&&(ddoc["irrigationTime"]>=5)){
        irrigationTime = ddoc["irrigationTime"];
        Serial.println("irrigationTime added :"+ String(irrigationTime));
      }
      if(ddoc.containsKey("sleepingTime")&&(ddoc["sleepingTime"]>=5)){
        sleepingTime = ddoc["sleepingTime"];
        Serial.println("sleepingTime added :"+ String(sleepingTime));
      }
      if (ddoc["waterPumpVal"]) {
      Serial.println("******starteIrrigation*****");
      irrigation();
      }
      }
      else{        
          Serial.println("no server get connection !!");
          offline=1;
      }
      http.end();     

    }
    else { //Offline mode :
       Serial.println("no wifi connection !!");
          offline=1;
    }
    if(offline > 0){
      irrigation();
    }
    //Sleep mode
    delay(sleepingTime*1000 - 4000);

}

void irrigation(){
  //calculer les valeurs
  int soilMoisture = 0;
  soilMoisture = map(analogRead(sensorPin), wetValue, dryValue , 100, 0);
  float temperature =  dht.readTemperature();  
  
  if((soilMoisture <= threshold )&&(temperature > 0)){  digitalWrite(RELAY_PIN, LOW);//on
  unsigned long lastTime = millis(); 
  while((soilMoisture < threshold )&&(((irrigationTime*1000) + lastTime) > millis())&&(temperature > 0)){
    //for the soil sensor to read again
    delay(2000);
    temperature =  dht.readTemperature();
      if (isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      temperature = -1;}  
      Serial.println("Irrigation loop");
    soilMoisture = map(analogRead(sensorPin), wetValue, dryValue , 100, 0);
  }
  digitalWrite(RELAY_PIN, HIGH);//off
  if((WiFi.status()== WL_CONNECTED) && (mcuID != 0)){
      http.begin(url + "api/mcu/" + String(mcuID)+"/");
      http.addHeader("Content-Type", "application/json");
      String jsonOutput;
      const size_t CAPACITY = JSON_OBJECT_SIZE(1);
      StaticJsonDocument<CAPACITY> doc;
      JsonObject object = doc.to<JsonObject>();
      object["waterPumpVal"]=false;
          serializeJson(doc, jsonOutput);
          Serial.println(String(jsonOutput));
          int code = http.PUT(String(jsonOutput));
           if(code > 0){
              Serial.println("StatusCode : "+ String(code) + "\npayload : "+ String(http.getString()));
          }
          else{      
            Serial.println("no server PUT connection !!");
          }
      http.end();
  }}
}

