/*
 * Control program for Automatic Planter v3.0
 * by Sirawit Lappisatepun and WWPS project
 * for subject CPE101 project "Automatic Sunflower Sprout Planter"
 * Copyrighted under CC BY-NC-SA 4.0
 */
 
#include<SPI.h>
#include<DHT.h>
#include<EEPROM.h>
#include<LedControl.h>
#include<SoftwareSerial.h>
#include<ESP8266_Lib.h>
#include<BlynkSimpleShieldEsp8266.h>
#include<string.h>

//SPI pins
#define SCK 13
#define MOSI 11
#define CS 10

//DHT11 pins
#define DHTPIN 19

//I/O pins
#define POWER 9
#define START 8
#define LIGHT 5
#define PUMP 3

//ESP comm pins
#define ESPRX 6
#define ESPTX 7

//analog pins
#define DEVCLS A2
#define SOILMS A0
#define WATLVL A1
#define LGTLVL A3

//time multipliers in millis
#define seconds 1000
#define minutes 60000
#define hours 3600000
#define days 60000    //debug
//#define days 86400000

//misc global data
struct saveData{
  const char verify[20]="AutoplanterSave";
  char wifiname[20];
  char wifipass[20];
  char authtoken[50];
  unsigned long elapsed;
};

volatile bool powerState=false,startState=false,internetState=false;
bool ledOn[5]={},err[10]={};    //ON/READY/INTERNET/ERROR
char leftDigit='-',rightDigit='-'; //2 digits 7-segment LED
unsigned long initM,startM,currentM=0,saveM=0,oldM=0;     //millis storing var
unsigned long lightTimeM=0,pumpTimeM=0,lastBtnM=0;

char wifiName[20]={"2107_2.4G"},wifiPass[20]={"21077777"},authToken[50]={"6545676d36324653b53c8a6edeee39fb"};  //internet config

//global declarations
LedControl led=LedControl(MOSI,SCK,CS,1);
SoftwareSerial ESPSerial(ESPRX,ESPTX);
DHT dht(DHTPIN,DHT11);
ESP8266 wifi(&ESPSerial);
WidgetLCD lcd(V0);    //Blynk virtual LCD

void setup() 
{
  saveData temp;
  int failCount=0;
  
  //pins config
  pinMode(CS,OUTPUT);
  pinMode(LIGHT,OUTPUT);
  pinMode(PUMP,OUTPUT);
  pinMode(POWER,INPUT);
  pinMode(START,INPUT);
  
  //UI init
  led.shutdown(0,false);
  led.setIntensity(0,8);
  led.clearDisplay(0);
  leftDigit='1';
  rightDigit='n';
  uiControl();

  //DHT11 init
  dht.begin();

  //hold start button to reset EEPROM
  if(digitalRead(START)==LOW)
    eepromClear();

  //see if there's any data in EEPROM
  EEPROM.get(0,temp);
  if(strcmp(temp.verify,"AutoplanterSave")==0)
  {
    strcpy(wifiName,temp.wifiname);
    strcpy(wifiPass,temp.wifipass);
    strcpy(authToken,temp.authtoken);
    if(temp.elapsed!=0)
    {
      powerState=true;
      startState=true;
      oldM=temp.elapsed;
    }
  }
  
  //debug
  Serial.begin(115200);
  
  //Software Serial + Blynk ESP init
  ESPSerial.begin(9600);
  delay(10);
  
  Blynk.config(wifi,authToken);
  while(Blynk.connectWiFi(wifiName,wifiPass)==0&&failCount<3)
    failCount++;
  if(failCount<3)
    Blynk.connect();

  //change UI to ready
  leftDigit='-';
  rightDigit='-';
  uiControl();
}

void loop() 
{
  int lightLVL=0,pumpLVL=0,startHum,dayCount=1;
  bool lightOn=false,pumpOn=false;
  long lightDUR=0,pumpDUR=0;
  
  if(digitalRead(POWER)==LOW&&(unsigned long)(millis()-lastBtnM)>500)
    {
      lastBtnM=millis();
      powerState=!powerState;
      //Serial.println("PWR");
    }
  
    initM=millis();    //save start time as ref
    while(powerState==true)
    {
      ledOn[0]=true;      //POWER LED
  
      //initial conditions check
      if(analogRead(DEVCLS)<300)
      {
        //Serial.println("E0");
        ledOn[3]=true;
        leftDigit='E';
        rightDigit='0';
      }
      else if(analogRead(WATLVL)<400)
      {
        //Serial.println("E1");
        ledOn[3]=true;
        leftDigit='E';
        rightDigit='1';      
      }
      else
      {
        //Serial.println("INTCHKP");
        ledOn[3]=false;
        leftDigit='o';
        rightDigit='n';
      }
  
      if(digitalRead(START)==LOW&&(unsigned long)(millis()-lastBtnM)>500)
      {
        lastBtnM=millis();
        startState=!startState;
        //Serial.println("STR");
      }
  
      if(startState==true&&ledOn[3]==false)
      { 
        if(oldM==0)
          {
            startM=millis();
            dayCount=1;
          }
        else
          {
            startM=(unsigned long)(millis()-oldM);
            dayCount=oldM/days;
          }
  
        ledOn[1]=true;
        leftDigit='0';
        rightDigit=(8-dayCount)+'0';
        uiControl();
        Blynk.notify("The planting cycle has started!");  
        
        while((unsigned long)(millis()-startM)<=7*days)
        {
          if(lightOn==true)
            {
              analogWrite(LIGHT,lightLVL);
            }
          else
            {
              analogWrite(LIGHT,0);
            }
          if(pumpOn==true)
            {
              analogWrite(PUMP,pumpLVL);
            }
          else
            {
              analogWrite(PUMP,0);
            }
                    
          //time dependent events (inner block)
          if((unsigned long)(millis()-startM)>(days*dayCount))
          {
            dayCount++;
            rightDigit=(8-dayCount)+'0';
          }
          
          if((unsigned long)(millis()-currentM)>=seconds)
          {
            //Serial.println("UIUPDI");
            currentM=millis();
            if(errorChecking(lightOn,pumpOn,startHum)==false)
             {
                ledOn[3]=false;
                leftDigit='0';
                rightDigit=(8-dayCount)+'0';
             }
            uiControl();
            if(Blynk.connected())
              blynkHandling(dayCount,lightOn,pumpOn);
          }
  
          if((unsigned long)(millis()-lightTimeM)>=lightDUR)
          {
            lightOn=lightLogicControl(&lightLVL,&lightDUR);
            if(lightOn==true)
              lightTimeM=millis();
          }
  
          if((unsigned long)(millis()-pumpTimeM)>=pumpDUR)
          {
            pumpOn=pumpLogicControl(&pumpLVL,&pumpDUR);
            if(pumpOn==true)
            {
              startHum=analogRead(SOILMS);
              pumpTimeM=millis();
            }
          }
  
          if((unsigned long)(millis()-saveM)>=5*seconds)
          {
            eepromBackup();
            saveM=millis();
          }
        }
  
        startState=false;
        analogWrite(LIGHT,0);
        analogWrite(PUMP,0);
        leftDigit='-';
        rightDigit='-';
        ledOn[1]=false;
        oldM=0;
        eepromBackup();
        uiControl();
        Blynk.notify("The planting cycle has finished!");
      }
  
      //turning off
      if(digitalRead(POWER)==LOW&&(unsigned long)(millis()-lastBtnM)>500)
      {
        lastBtnM=millis();
        powerState=!powerState;
      }
  
      //time dependent events (outer block)
      if((unsigned long)(millis()-currentM)>=seconds)
      {
        //Serial.println("UIUPDO");
        currentM=millis();
        uiControl();
        if(Blynk.connected())
          blynkHandling(dayCount,lightOn,pumpOn);
      }
    }
    //turn off all displays
    leftDigit='-';
    rightDigit='-';
    ledOn[0]=false;
    ledOn[1]=false;
    ledOn[2]=false;
    ledOn[3]=false;
    uiControl();
}

//helper function for DHT comm
void DHTRead(double *temp, double *humid)
{
  double tmp,hum;
  tmp=dht.readTemperature();
  hum=dht.readHumidity();
  if(isnan(tmp)||isnan(hum))
  {
    (*temp)=25;
    (*humid)=50;
  }
  else
  {
    (*temp)=tmp;
    (*humid)=hum;    
  }
}

//on/off condition check for LED
bool lightLogicControl(int *lvl,long *dur)
{
  int light;
  light=analogRead(LGTLVL);
  
  //Serial.print("LIGHT: ");
  //Serial.println(analogRead(LGTLVL));

  //commented for debugging
  if((unsigned long)(millis()-startM)<2*days)
  {
    return false; //no light in first 2 days
  }
  else if(light<100)
  {
    (*lvl)=255-(map(light,0,1023,0,255));
    (*dur)=6*hours;
    return true;
  }
  else
    return false;
}

//on/off condition check for pump
bool pumpLogicControl(int *lvl,long *dur)
{
  int soilHum;
  double temp,airHum;

  //read temp+hum data
  DHTRead(&temp,&airHum);
  soilHum=analogRead(SOILMS);

  /*Serial.print("TEMP: ");
  Serial.println(temp);
  Serial.print("AIRHUM: ");
  Serial.println(airHum);
  Serial.print("SOILHUM: ");
  Serial.println(soilHum);*/

  if(soilHum>500)   //too much humidity
  {
    return false;
  }
  else
  {
    if(temp>30)
      (*lvl)=airHum*2;
    else
      (*lvl)=airHum;
      
    (*dur)=(soilHum/10)*minutes;
    return true;
  }
}

//UI output control via MAX7219
void uiControl()
{
  led.setChar(0,0,leftDigit,ledOn[1]);
  led.setChar(0,1,rightDigit,ledOn[0]);
  for(int i=2;i<=3;i++)
  {
    if(ledOn[i]==true)
      led.setChar(0,i,'.',false);
    else
      led.setChar(0,i,' ',false);  
  }
}

//save important data in EEPROM (1kb space)
void eepromBackup()
{
  saveData current;
  
  strcpy(current.wifiname,wifiName);
  strcpy(current.wifipass,wifiPass);
  strcpy(current.authtoken,authToken);
  
  if(startState==true)
    current.elapsed=(unsigned long)(millis()-startM);
  else
    current.elapsed=0;

  EEPROM.put(0,current);
}

void eepromClear()
{
  for(int i=0;i<EEPROM.length();i++)
    EEPROM.write(i,0);

  ledOn[3]=true;
  uiControl();
  delay(500);
  ledOn[3]=false;
  uiControl();
}

//error checking function
bool errorChecking(bool lightOn,bool pumpOn,int startHum)
{
  bool isError=false;

  //Error 0: Case not closed
  if(analogRead(DEVCLS)<300)
  {
    isError=true;
    err[0]=true;
    ledOn[3]=true;
    leftDigit='E';
    rightDigit='0';
  }
  else 
    err[0]=false;

  //Error 1: Low water level
  if(analogRead(WATLVL)<300)
  {
    isError=true;
    err[1]=true;
    ledOn[3]=true;
    leftDigit='E';
    rightDigit='1';
  }
  else
    err[1]=false;

  //Error 2: DHT11 sensor failure
  if(isnan(dht.readTemperature())||isnan(dht.readHumidity()))
  {
    isError=true;
    err[2]=true;
    ledOn[3]=true;
    leftDigit='E';
    rightDigit='2';
  }
  else
    err[2]=false;

  //Error 3: LED light failure
  if(lightOn==true&&analogRead(LGTLVL)<100)
  {
    isError=true;
    err[3]=true;
    ledOn[3]=true;
    leftDigit='E';
    rightDigit='3';
  }
  else
    err[3]=false;

  //Error 4: Pump failure
  if(pumpOn==true&&((unsigned long)(millis()-pumpTimeM)>10*minutes)&&(analogRead(SOILMS)-startHum<200))
  {
    isError=true;
    err[4]=true;
    ledOn[3]=true;
    leftDigit='E';
    rightDigit='4';    
  }
  else
    err[4]=false;
    
  return isError;
}

//handles Blynk pushing
void blynkHandling(int dayCount,bool lightOn,bool pumpOn)
{
  int i;
  char msg[20]={},buff[5]={};
  double temp,hum;
  
  DHTRead(&temp,&hum);

  Blynk.virtualWrite(V1,analogRead(SOILMS));    //SOILMS
  Blynk.virtualWrite(V2,analogRead(LGTLVL));    //LGTLVL
  Blynk.virtualWrite(V3,temp);                  //AIRTMP
  Blynk.virtualWrite(V4,hum);                   //AIRHUM
  
  lcd.clear();                                  //VIRLCD
  if(startState==true)
  {
    lcd.print(0,0,"Run: ");
    lcd.print(5,0,8-dayCount);
    lcd.print(7,0,"days left");

    for(i=0;i<20;i++)
      msg[i]=0;
    if(lightOn==true)
      strcat(msg,"L ");
    if(pumpOn==true)
      strcat(msg,"P ");
    for(i=0;i<5;i++)
    {
      if(err[i]==true)
      {
        strcat(msg,"E");
        strcat(msg,itoa(i,buff,10));
        strcat(msg," ");
      }
    }
    
    lcd.print(0,1,"Stat: ");
    lcd.print(6,1,msg);
  }
  else
  {
    lcd.print(0,0,"Idle");
  }
}

