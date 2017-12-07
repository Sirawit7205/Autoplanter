/*
 * Control program for Automatic Planter v3.0
 * by Sirawit Lappisatepun and WWPS project
 * for subject CPE101 project "Automatic Sunflower Sprout Planter"
 * Copyrighted under CC BY-NC-SA 4.0
 */
 
#include <SPI.h>
#include <DHT.h>
#include <EEPROM.h>
#include <LedControl.h>
#include <SoftwareSerial.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <FuzzyRule.h>
#include <FuzzyComposition.h>
#include <Fuzzy.h>
#include <FuzzyRuleConsequent.h>
#include <FuzzyOutput.h>
#include <FuzzyInput.h>
#include <FuzzyIO.h>
#include <FuzzySet.h>
#include <FuzzyRuleAntecedent.h>

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
#define days 86400000

//misc global data
struct saveData{
  const char verify[20]="AutoplanterSave";
  //char wifiname[16];
  //char wifipass[16];
  //char authtoken[35];
  unsigned long elapsed;
};

volatile bool powerState=false,startState=false;
bool ledOn[5]={},err[5]={},lightCheck[10]={};    //ON/READY/INTERNET/ERROR
char leftDigit='-',rightDigit='-'; //2 digits 7-segment LED
unsigned long startM,oldM=0;     //millis storing var

char wifiName[]={"2107_2.4G"},wifiPass[]={"21077777"},authToken[]={"6545676d36324653b53c8a6edeee39fb"};  //internet config

//global declarations
LedControl led=LedControl(MOSI,SCK,CS,1);
SoftwareSerial ESPSerial(ESPRX,ESPTX);
DHT dht(DHTPIN,DHT11);
ESP8266 wifi(&ESPSerial);
WidgetLED blynkLED(V5);    //Blynk LED
Fuzzy* fuzzy = new Fuzzy();

//fuzzy rulesets
FuzzySet* soilL = new FuzzySet(400,500,1023,1023);
FuzzySet* soilM = new FuzzySet(300,400,400,500);
FuzzySet* soilH = new FuzzySet(0,0,300,400);

FuzzySet* airtL = new FuzzySet(0,0,25,40);
FuzzySet* airtH = new FuzzySet(25,40,50,50);

FuzzySet* airhL = new FuzzySet(20,20,50,70);
FuzzySet* airhH = new FuzzySet(50,70,90,90);

FuzzySet* pumpO = new FuzzySet(0,0,0,0);
FuzzySet* pumpL = new FuzzySet(0,30,50,80);
FuzzySet* pumpH = new FuzzySet(50,80,100,100);

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

  //fuzzy init
  //initFuzzy();
  
  //hold start button to reset EEPROM
  if(digitalRead(START)==LOW)
    eepromClear();

  //see if there's any data in EEPROM
  EEPROM.get(0,temp);
  if(strcmp(temp.verify,"AutoplanterSave")==0)
  {
    //strcpy(wifiName,temp.wifiname);
    //strcpy(wifiPass,temp.wifipass);
    //strcpy(authToken,temp.authtoken);
    if(temp.elapsed!=0)
    {
      powerState=true;
      startState=true;
      oldM=temp.elapsed;
    }
  }
  
  //debug
  //Serial.begin(115200);
  
  //Software Serial + Blynk ESP init
  ESPSerial.begin(9600);
  delay(10);

  //Blynk.begin(authToken,wifi,wifiName,wifiPass);

  Blynk.config(wifi,authToken);
  while(Blynk.connectWiFi(wifiName,wifiPass)==0&&failCount<3)
  {
    failCount++;
  }
  if(failCount<3)
    Blynk.connect();

  //change UI to ready
  leftDigit='-';
  rightDigit='-';
  uiControl();
}

void loop() 
{
  int lightLVL=0,pumpLVL=0,dayCount=1;
  bool lightOn=false,pumpOn=false,isError=false;
  unsigned long lightDUR=0,pumpDUR=0,lightTimeM=0,pumpTimeM=0,lastBtnM=0,currentM=0,saveM=0;
  
  if(digitalRead(POWER)==LOW&&(unsigned long)(millis()-lastBtnM)>500)
    {
      lastBtnM=millis();
      powerState=!powerState;
      //Serial.println("PWR");
    }
  
    while(powerState==true)
    {
      ledOn[0]=true;      //POWER LED
      if(Blynk.connected())
        ledOn[2]=true;
      else
        ledOn[2]=false;
        
      //initial conditions check
      if(errorChecking(lightOn,pumpOn)==false)
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
            Blynk.notify("The planting cycle has started!");
          }
        else
          {
            startM=(unsigned long)(millis()-oldM);
            dayCount=oldM/days;
            Blynk.notify("The planting cycle has resumed after power loss.");
          }
  
        ledOn[1]=true;
        leftDigit='0';
        rightDigit=(8-dayCount)+'0';
        uiControl();  
        
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
          if(pumpOn==true&&err[0]==false&&err[1]==false)
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
          
          if((unsigned long)(millis()-currentM)>=5*seconds)
          {
            //Serial.println("UIUPDI");
            currentM=millis();
            isError=errorChecking(lightOn,pumpOn);
            if(isError==false)
             {
                ledOn[3]=false;
                leftDigit='0';
                rightDigit=(8-dayCount)+'0';
             }
            uiControl();
          }
  
          if((unsigned long)(millis()-lightTimeM)>=lightDUR)
          {
            lightOn=lightLogicControl(dayCount,&lightLVL,&lightDUR);
            if(lightOn==true)
              lightTimeM=millis();
          }
  
          if((unsigned long)(millis()-pumpTimeM)>=pumpDUR)
          {
            pumpOn=pumpLogicControl(&pumpLVL,&pumpDUR);
            if(pumpOn==true)
              pumpTimeM=millis();
          }
  
          if((unsigned long)(millis()-saveM)>=15*minutes)
          {
            eepromBackup();
            if(Blynk.connected())
              blynkHandling(dayCount,isError);
            saveM=millis();
          }
        }
  
        startState=false;
        isError=false;
        for(int i=3;i<=6;i++)
        {
          lightCheck[i]=false;
        }
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
          blynkHandling(dayCount,isError);
      }
    }
    //turn off all displays
    leftDigit='-';
    rightDigit='-';
    for(int i=0;i<4;i++)
      ledOn[i]=false;
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
bool lightLogicControl(int dayCount,int *lvl,unsigned long *dur)
{
  if(dayCount==7) //full light in the last day
  {
    (*lvl)=255;
    (*dur)=24*hours;
    return true;
  }
  else if(dayCount>2&&dayCount<7&&lightCheck[dayCount]==false)
  {
    lightCheck[dayCount]=true;
    (*lvl)=127;
    (*dur)=12*hours;
    return true;
  }
  else
    return false;
}

//on/off condition check for pump
bool pumpLogicControl(int *lvl,unsigned long *dur)
{
  int soilHum;
  double temp,airHum;

  //read temp+hum data
  DHTRead(&temp,&airHum);
  soilHum=analogRead(SOILMS);

  if(temp>35)     //too high temperature
    return false;
  else if((unsigned long)(millis()-startM)<=6*hours)
  {
    (*lvl)=100;
    (*dur)=15*minutes;
    return true;
  }
  else
  {
    fuzzy->setInput(1,soilHum);
    fuzzy->setInput(2,temp);
    fuzzy->setInput(3,airHum);
    
    fuzzy->fuzzify();
    (*lvl)=(int)fuzzy->defuzzify(1);
    (*dur)=30*minutes;
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
  
  //strcpy(current.wifiname,wifiName);
  //strcpy(current.wifipass,wifiPass);
  //strcpy(current.authtoken,authToken);
  
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
bool errorChecking(bool lightOn,bool pumpOn)
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

  //Error 4: Pump failure NOT ACCURATE
  /*if(pumpOn==true&&((unsigned long)(millis()-pumpTimeM)>15*minutes)&&startHum-(analogRead(SOILMS)<50))
  {
    isError=true;
    err[4]=true;
    ledOn[3]=true;
    leftDigit='E';
    rightDigit='4';    
  }
  else
    err[4]=false;*/
    
  return isError;
}

//handles Blynk pushing
void blynkHandling(int dayCount,bool isError)
{
  double temp,hum;
  
  DHTRead(&temp,&hum);

  Blynk.virtualWrite(V0,8-dayCount);            //day count
  Blynk.virtualWrite(V1,analogRead(SOILMS));    //SOILMS
  Blynk.virtualWrite(V2,analogRead(LGTLVL));    //LGTLVL
  Blynk.virtualWrite(V3,(int)temp);             //AIRTMP
  Blynk.virtualWrite(V4,(int)hum);              //AIRHUM
  
  if(isError)                                   //Error LED 
    blynkLED.on();
  else
    blynkLED.off();
}

void initFuzzy()
{
  //soil moisture input
  FuzzyInput* soilmoist = new FuzzyInput(1);
  soilmoist->addFuzzySet(soilL);
  soilmoist->addFuzzySet(soilM);
  soilmoist->addFuzzySet(soilH);
  fuzzy->addFuzzyInput(soilmoist);

  //air temp input
  FuzzyInput* airtemp = new FuzzyInput(2);
  airtemp->addFuzzySet(airtL);
  airtemp->addFuzzySet(airtH);
  fuzzy->addFuzzyInput(airtemp);

  //air humidity input
  FuzzyInput* airhum = new FuzzyInput(3);
  airhum->addFuzzySet(airhL);
  airhum->addFuzzySet(airhH);
  fuzzy->addFuzzyInput(airhum);

  //pump output
  FuzzyOutput* pump = new FuzzyOutput(1);
  pump->addFuzzySet(pumpO);
  pump->addFuzzySet(pumpL);
  pump->addFuzzySet(pumpH);
  fuzzy->addFuzzyOutput(pump);

  //rule 1: SOILMS[L] AND AIRHUM[L] = PUMP[H]
  FuzzyRuleAntecedent* rule1 = new FuzzyRuleAntecedent();
  rule1->joinWithAND(soilL,airhL);
  //rule 2: SOILMS[H] AND AIRHUM[L] = PUMP[L]
  FuzzyRuleAntecedent* rule2 = new FuzzyRuleAntecedent();
  rule1->joinWithAND(soilH,airhL);  
  //rule 3: SOILMS[M] AND AIRHUM[H] = PUMP[L]
  FuzzyRuleAntecedent* rule3 = new FuzzyRuleAntecedent();
  rule1->joinWithAND(soilM,airhH);  
  //rule 4: SOILMS[H] AND AIRHUM[H] = PUMP[O]
  FuzzyRuleAntecedent* rule4 = new FuzzyRuleAntecedent();
  rule1->joinWithAND(soilH,airhH);  
  //rule 5: AIRTMP[L] AND AIRHUM[H] = PUMP[O]
  FuzzyRuleAntecedent* rule5 = new FuzzyRuleAntecedent();
  rule1->joinWithAND(airtL,airhH);  

  //rule PUMP[L]
  FuzzyRuleAntecedent* rulepumpL = new FuzzyRuleAntecedent();
  rulepumpL->joinWithOR(rule2,rule3);
  //rule PUMP[O]
  FuzzyRuleAntecedent* rulepumpO = new FuzzyRuleAntecedent();
  rulepumpO->joinWithOR(rule4,rule5);
  
  //cons 1: PUMP[H]
  FuzzyRuleConsequent* conspumpH = new FuzzyRuleConsequent();
  conspumpH->addOutput(pumpH);
  //cons 2: PUMP[L]
  FuzzyRuleConsequent* conspumpL = new FuzzyRuleConsequent();
  conspumpL->addOutput(pumpH);
  //cons 3: PUMP[O]
  FuzzyRuleConsequent* conspumpO = new FuzzyRuleConsequent();
  conspumpO->addOutput(pumpH);

  //Fuzzyrule 1: PUMP[H]
  FuzzyRule* fuzzyRule1 = new FuzzyRule(1,rule1,conspumpH);
  fuzzy->addFuzzyRule(fuzzyRule1);
  //Fuzzyrule 2: PUMP[L]
  FuzzyRule* fuzzyRule2 = new FuzzyRule(2,rulepumpL,conspumpL);
  fuzzy->addFuzzyRule(fuzzyRule2);
  //Fuzzyrule 3: PUMP[O]
  FuzzyRule* fuzzyRule3 = new FuzzyRule(3,rulepumpO,conspumpO);
  fuzzy->addFuzzyRule(fuzzyRule3);
}

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
