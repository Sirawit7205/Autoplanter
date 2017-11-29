#include<LiquidCrystal.h>
#include<DHT.h>

LiquidCrystal lcd(12,11,5,4,3,2);
DHT dht(8,DHT11);

void setup() {
  // put your setup code here, to run once:
  dht.begin();
  lcd.begin(16,2);
}

void loop() {
  // put your main code here, to run repeatedly:
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Air Hum: ");
  lcd.print(dht.readHumidity());
  lcd.setCursor(0,1);
  lcd.print("Air Temp: ");
  lcd.print(dht.readTemperature());
  delay(5000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Soil Hum: ");
  lcd.print(analogRead(A1));
  lcd.setCursor(0,1);
  lcd.print("Light: ");
  lcd.print(analogRead(A0));
  delay(5000);
}
