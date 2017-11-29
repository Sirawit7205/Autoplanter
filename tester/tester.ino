#include<DHT.h>

#define DHTPIN 19
#define POWER 9
#define START 8
#define LIGHT 5
#define PUMP 3
#define SOILMS A0
#define LGTLVL A3

unsigned long buttonM1=0,buttonM2=0,sensorM=0;
DHT dht(DHTPIN,DHT11);

void setup() {
  pinMode(LIGHT,OUTPUT);
  pinMode(PUMP,OUTPUT);
  pinMode(POWER,INPUT);
  pinMode(START,INPUT);

  dht.begin();
  Serial.begin(115200);
}

void loop() {
  bool pump=false,light=false;
  
  if(digitalRead(POWER)==LOW&&(unsigned long)(millis()-buttonM1)>1000)
  {
      buttonM1=millis();
      pump=!pump;
  }
   
  if(digitalRead(START)==LOW&&(unsigned long)(millis()-buttonM2)>1000)
  {
      buttonM2=millis();
      light=!light;
  }

  if(pump)
    analogWrite(PUMP,20);
  if(light)
    analogWrite(LIGHT,255)

  //sensors
  if((unsigned long)(millis())-sensorM>1000)
  {
    sensorM=millis();
    Serial.print("Temp: ");
    Serial.println(dht.readTemperature());
    Serial.print("Air hum: ");
    Serial.println(dht.readHumidity());
    Serial.print("Soil hum: ");
    Serial.println(analogRead(SOILMS));
    Serial.print("Light: ");
    Serial.println(analogRead(LGTLVL));
  }
}
