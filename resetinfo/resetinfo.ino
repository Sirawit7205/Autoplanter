/*
 * Control program for Automatic Planter v1.0
 * by Sirawit Lappisatepun and WWPS project
 * for subject CPE101 project "Automatic Sunflower Sprout Planter"
 * Copyrighted under CC BY-NC-SA 4.0
 */

#include<Wire.h>
#include<SPI.h>
#include<EEPROM.h>
#include<LedControl.h>
#include<ESP8266WiFi.h>
#include<ESP8266WebServer.h>
#include<user_interface.h>
#include<Esp.h>

//SPI pins
#define SCK 14
#define MOSI 13
#define CS 16

//I2C pins
#define SCL 5
#define SDA 4

//UI pins
#define POWER 11
#define START 9
#define LIGHT 12
#define PUMP 15

//analog pins from PCF8591
#define DEVCLS 0
#define SOILMS 1
#define WATLVL 2
#define LGTLVL 3

//misc global data
int ADC=0x48;
int DHT=0xb8;
volatile bool readyBlink=false,internetBlink=false;   //blink trigger that will be catch by interrupts
volatile char msg[10]={}; //0=right digit, 1=left digit, 2-5=ON/READY/INTERNET/ERROR

//global declarations
LedControl led=LedControl(MOSI,SCK,CS,1);
os_timer_t timer;

void setup() 
{
  //pins config
  pinMode(CS,OUTPUT);
  //pinMode(LIGHT,OUTPUT);
  pinMode(PUMP,OUTPUT);
  //pinMode(POWER,INPUT);
  //pinMode(START,INPUT);
  
  //UI init
  led.shutdown(0,false);
  led.setIntensity(0,8);
  led.clearDisplay(0);

  //i2c init
  Wire.begin(SDA,SCL);

  //UI init
  UIUpdateInit();

  //virtual EEPROM init
  EEPROM.begin(512); //allocate 0.5kb of storage
}

void loop() 
{
  int ledLVL,ledDUR,pumpLVL,pumpDUR,flashAddress=0;
  unsigned long startMillis=0,loopMillis=0;

  if(digitalRead(POWER)==LOW)      //check for soft switch on state
  {

    //turn ON and READY light on
    msg[2]='1';
    msg[3]='1';
    
    //check ready conditions
    /*while(ADCRead(DEVCLS)==0||ADCRead(WATLVL)==0) //box open or low water
    {
      readyBlink=true;    //interrupts will catch this              
    }
    
    //stop READY blink
    readyBlink=false;

    while(digitalRead(START)==HIGH)
    {
      //infinite loop until start
    }

    //turn off READY light, set days left to 7 days
    msg[0]='7';
    msg[1]='0';
    msg[3]='0';

    //record start time
    startMillis = millis();
    while((unsigned long)(millis()-startMillis)<604800000) //1 week time in millis
    {
      loopMillis=millis();    //initial lED time
      if(ledLogicControl(&ledLVL,&ledDUR)==1)
      {
        while((unsigned long)(millis()-loopMillis)<ledDUR)  //duration defined by logic func
        {
          analogWrite(LIGHT,ledLVL);  //intensity defined by logic func
        }
        analogWrite(LIGHT,0);   //turns light off
      }

      loopMillis=millis();    //initial pump time
      if(pumpLogicControl(&pumpLVL,&pumpDUR)==1)
      {
        while((unsigned long)(millis()-loopMillis)<pumpDUR) //duration defined by logic func
        {
          analogWrite(PUMP,pumpLVL);  //intensity defined by logic func
        }
        analogWrite(PUMP,0);    //turns pump off
      }

      //flashBackup(&flashAddress,0); //TODO:List all data that need to be saved and implement procedure
      errorChecking(); //check for errors
      delay(600000); //stop for 10 mins
    }*/
  }
}

//helper function for PCF comm
int ADCRead(int pin)
{
  byte input;
  Wire.beginTransmission(ADC);
  Wire.write(pin);
  Wire.endTransmission();
  Wire.requestFrom(ADC,1);
  input=Wire.read();
  return (int)input;
}

//helper function for DHT comm
void DHTRead(double *temp, double *humid)
{
  byte tempF,tempD,humF,humD,chk;
  int chkOK=0;

  do{
    Wire.beginTransmission(DHT);
    Wire.write(0);
    Wire.endTransmission();
    Wire.requestFrom(DHT,5);
    tempF=Wire.read();
    tempD=Wire.read();
    humF=Wire.read();
    humD=Wire.read();
    chk=Wire.read();
    
    //verify checksum
    if(tempF+tempD+humF+humD==chk)
      chkOK=1;
  } while(chkOK==0);

  //return value
  (*temp)=(double)tempF+tempD;
  (*humid)=(double)humF+humD;
}

//UI output control via MAX7219
void uiControl(volatile char msg[])
{
  led.setChar(0,0,msg[0],false);    //right digit
  led.setChar(0,1,msg[1],false);    //left digit
  for(int i=2;i<=5;i++)             //4 LEDs, each handle as decimal points from digit 2-5
  {
    if(msg[i]==1)
      led.setChar(0,i,'.',false);
    else
      led.setChar(0,i,' ',false);  
  }
}

//on/off condition check for LED
int ledLogicControl(int *lvl,int *dur)
{
  
}

//on/off condition check for pump
int pumpLogicControl(int *lvl,int *dur)
{
  int soilHum;
  double temp,airHum;

  //read temp+hum data
  DHTRead(&temp,&airHum);
  soilHum=ADCRead(SOILMS);
}

//initialize time interrupts
void UIUpdateInit()
{
  os_timer_setfn(&timer,UIUpdateExec,NULL);   //set callback function after timer ended
  os_timer_arm(&timer,1000,true);   //start time interrupts
}

//callback function for time interrupts
void UIUpdateExec(void *Arg)
{
  if(readyBlink)      //change state of READY LED
  {
    if(msg[3]=='.')
      msg[3]==' ';
    else
      msg[3]=='.';
  }
  if(internetBlink)   //change state of INTER LED
  {
    if(msg[4]=='.')
      msg[4]==' ';
    else
      msg[4]=='.';
  }

  //use uiControl to update the display
  uiControl(msg);
}

//save important data in Flash memory of ESP12E
void flashBackup(int *address,char data)
{
  EEPROM.write((*address),data);    //this is not the same as Arduino's EEPROM!
  EEPROM.commit();    //save changes

  //overflow protection
  if((*address)==512)
    (*address)=0;
}

//error checking function
void errorChecking()
{
  
}

