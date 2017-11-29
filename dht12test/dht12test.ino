#include <Wire.h>

#include <dht.h>



dht12 DHT(0x5c);
dht1wire DHT11(4,dht::DHT11);



void setup()

{

  Serial.begin(9600);

  Serial.println(F("DHT12 TEST PROGRAM"));

}



void loop()

{

  unsigned long b = micros();

  dht::ReadStatus chk = DHT.read();

  unsigned long e = micros();

  unsigned long bb = micros();

  dht::ReadStatus chk2 = DHT11.read();

  unsigned long ee = micros();



  Serial.print(F("Read sensor: "));

  switch (chk)

  {

    case dht::OK:

      Serial.print(F("OK, took "));

      Serial.print (e - b); Serial.print(F(" usec, "));

      break;

    case dht::ERROR_CHECKSUM:

      Serial.println(F("Checksum error"));

      break;

    case dht::ERROR_TIMEOUT:

      Serial.println(F("Timeout error"));

      break;

    case dht::ERROR_CONNECT:

      Serial.println(F("Connect error"));

      break;

    case dht::ERROR_ACK_L:

      Serial.println(F("AckL error"));

      break;

    case dht::ERROR_ACK_H:

      Serial.println(F("AckH error"));

      break;

    default:

      Serial.println(F("Unknown error"));

      break;

  }



  Serial.print(F("Humidity: "));

  Serial.print((float)DHT.getHumidity()/(float)10);

  Serial.print(F("%, "));



  Serial.print(F(". Temperature (degrees C): "));

  Serial.print((float)DHT.getTemperature()/(float)10);



  Serial.print(F(", Dew Point (degrees C): "));

  Serial.println(DHT.dewPoint());

  

  delay(4000);

}

//

// END OF FILE

//
