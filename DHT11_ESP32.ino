#define BLYNK_TEMPLATE_ID "TMPLmmujQfDh"
#define BLYNK_DEVICE_NAME "DHT11"
#define BLYNK_AUTH_TOKEN "lU0C24qtH-gU4gnBnqbYPip7lwhKO9HZ"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <DHT.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Home";
char pass[] = "02101967";

BlynkTimer timer;

#define DHTPIN 21
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

#define SOILPIN 34
int greenPin = 23;
int redPin = 22;

void sendSensor() {
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  int moist = analogRead(SOILPIN);
  int moist_map = map(moist, 4095, 0, 0, 100);
  if (isnan(hum) || isnan(temp) || isnan(moist)){
    Serial.println("Failed to read from DHT Sensor");
    return;
  }
  //You can send any value at any time
  //Please don't send more than 10 values at any time

  Blynk.virtualWrite(V0, temp);
  Blynk.virtualWrite(V1, hum);
  Blynk.virtualWrite(V2, moist_map);
  Serial.print("Temperature : ");
  Serial.print(temp);
  Serial.print(" || Humidity : ");
  Serial.print(hum);
  Serial.print(" || Moisture : ");
  Serial.println(moist_map);
}

void setup() {
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);
  dht.begin();
  timer.setInterval(100L, sendSensor);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
}

void loop(){
  Blynk.run();
  timer.run();
  float temper = dht.readTemperature();
  float humi = dht.readHumidity();
  int moisture = analogRead(SOILPIN);
  int moisture_map = map(moisture, 4095, 0, 0, 100);
  if (isnan(humi) || isnan(temper) || isnan(moisture)){
    Serial.println("Failed to read from DHT Sensor");
    return;
  }
  else if(humi > 60 || moisture_map < 10 || temper > 45){
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
  }
  else{
    digitalWrite(greenPin, HIGH);
    digitalWrite(redPin, LOW);
  }
}
