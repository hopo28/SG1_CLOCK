/*

Known bugs:
sometimes inverts seconds fade after dial sequence, resets itself after getting to 60 though


*/

#include <Time.h>  
#include <Wire.h>  
#include <DS1307RTC.h>
#include <EEPROM.h>
#include "FastLED.h"

#define NUM_LEDS 60
#define LED_DATA_PIN 3
#define LED_BRIGHTNESS 25 //default brightness
#define LDR_PIN A7
#define NUM_BUTTONS 3
#define NUM_COLOURSETS 3
#define WH_MIN 2//wormhole min and max run times (Seconds)
#define WH_MAX 5
#define PRESS_SHORT 30//in milliseconds, for debouncing

CRGB leds[NUM_LEDS];

unsigned long timerSec = 0;
int currentSec = 0;

//default colours
byte COLOUR_HOUR[3] = {255,127,000};//RED
byte COLOUR_MIN[3]  = {000,255,127};//GREEN
byte COLOUR_SEC[3]  = {064,000,255};//BLUE
byte COLOUR_DOTS[3] = {30,30,30};//dim white
byte COLOUR_BG[3] = {0,0,0};//black

byte COLOUR_CHEVRON[3] = {255,142,000};//orangish
byte COLOUR_WORMHOLE[3] = {63,128,255};//blueish

int colourSet = 0;//default colour set to use
int LEDBrightness = LED_BRIGHTNESS;

unsigned long buttonPressedTimer = 0;
int buttonPressed = 200;
boolean buttonPressedFlag = false;
byte buttonPressedOrigValue = 0;
int buttons[NUM_BUTTONS] = {4,5,6};

//struct colour
typedef struct
{
  byte r;
  byte g;
  byte b;
} myColour;

//struct colour profile
typedef struct
{
  myColour ColHour;
  myColour ColMin;
  myColour ColSec;
  myColour ColDots;
  myColour ColBg;
} colourProfile;

colourProfile myColourProfiles[NUM_COLOURSETS];

void setup()
{
  Serial.begin(9600);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC, gets time now
  setSyncInterval(3600);  //default is 300 secs (5 mins), set to 1 hour
  
  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time");      

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  LEDS.setBrightness(LEDBrightness);
  testLEDStrip();
  //retriveColours();
  printMenu();
  //random seed
  pinMode(A7, INPUT);
  randomSeed(analogRead(7));
  
//set up inputs
  for(int i = 0; i < NUM_BUTTONS; i++)
  {
    pinMode(buttons[i], INPUT);           // set pin to input
    digitalWrite(buttons[i], HIGH);       // turn on pullup r
  }
  
  setupColourProfiles();
  applyColourSet(0);
  /*
  //temp  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  for testings   -  -  -  -  -  -  -  
  pinMode(A0, OUTPUT);
  digitalWrite(A0, LOW);//fake gnd pin
  */
}

void loop()
{
  checkButtons();
  if (Serial.available() > 0)
  {
    doMenu(Serial.read());
  }
  displayClock();
  checkLight();
  checkChime();//on the hour
}

void checkButtons()
{
  for(int i = 0; i < NUM_BUTTONS; i++)//for each input
  {
    if(!digitalRead(buttons[0+i]))//if the one being checked is pressed
    {
      if(buttonPressedFlag == false)//if its first time press
      {
        //Serial.print("Button ");
        //Serial.print(i);
        //Serial.println(" Pressed");
        
        //log details and start time
        buttonPressedFlag = true;
        buttonPressedTimer = millis();
        buttonPressed = i;
        switch(i)
        {
          case 0: buttonPressedOrigValue = minute(); break;
          case 1: buttonPressedOrigValue = hour(); break;
          case 2: buttonPressedOrigValue = colourSet; break;
        }
      }
      else//(buttonPressedFlag == true)
      {//this means this button has been pressed before and is still being pressed
        //Serial.print(i);
        unsigned long timeTemp = (millis()-buttonPressedTimer);//how long it was pressed for
        if(timeTemp < PRESS_SHORT)
        {
          //Serial.println("INVALID");//do sod all          
        }
        else
        {
          //Serial.print("ADVANCE");
          switch(i)
          {
            case 0: /*Serial.print(" mins to ");     Serial.println(buttonPressedOrigValue+1);*/ updateMin(buttonPressedOrigValue+1); break;
            case 1: /*Serial.print(" hours to ");    Serial.println(buttonPressedOrigValue+1);*/ updateHour(buttonPressedOrigValue+1); break;
            case 2: /*Serial.println(" colour to "); Serial.println(buttonPressedOrigValue+1);*/ applyColourSet(buttonPressedOrigValue+1); break;
          }
        }
      }
    }
    else//if the one being checked is NOT pressed
    {			
      if((buttonPressed == i) && (buttonPressedFlag == true))//if it WAS pressed
      {
        //Serial.println("unpressed");
        buttonPressedFlag = false;
        digitalClockDisplay();//show new settings
      }
    }
  }	
}

void updateMin(int myMin)
{
  if(myMin == 60)
    myMin = 0;
    
  int timeDatArray[6] = {0,0,0,0,0,0};
  timeDatArray[5] = year();
  timeDatArray[4] = month();
  timeDatArray[3] = day();
  timeDatArray[0] = hour();
  timeDatArray[1] = myMin;
  timeDatArray[2] = second();

  setTime(timeDatArray[0],
          timeDatArray[1],
          timeDatArray[2],
          timeDatArray[3],
          timeDatArray[4],
          timeDatArray[5]);

  time_t t = now();
  RTC.set(t);
}

void updateHour(int myHour)
{
  int timeDatArray[6] = {0,0,0,0,0,0};
  timeDatArray[5] = year();
  timeDatArray[4] = month();
  timeDatArray[3] = day();
  timeDatArray[0] = myHour;
  timeDatArray[1] = minute();;
  timeDatArray[2] = second();

  setTime(timeDatArray[0],
          timeDatArray[1],
          timeDatArray[2],
          timeDatArray[3],
          timeDatArray[4],
          timeDatArray[5]);

  time_t t = now();
  RTC.set(t);  
}

void checkLight()
{
//  Serial.println(analogRead(LDR_PIN));
  int reading = 0;
  reading += analogRead(LDR_PIN);
  reading += analogRead(LDR_PIN);
  reading += analogRead(LDR_PIN);
  reading += analogRead(LDR_PIN);
  reading += analogRead(LDR_PIN);
  reading = reading / 5;//average it         
  int brightness = map(reading,0,1023,255,0);
  LEDS.setBrightness(brightness);
}

void checkChime()
{
  int sequence = random(0,6);
  if (minute() == 0 && second() == 0)
  {
    now();//gets time now, forces resync
    setSyncInterval(3600);//set sync time to every hour, eg every chime
    switch(sequence)
    {
      case 1: dial(); dialFailSparks(); break;
      case 2: dial(); kawoosh(); wormhole(); wormholeEnd(); break;
      case 3: dial(); kawoosh(); wormhole(); wormholeUnstable(); break;
      case 4: dialIn(); kawoosh(); wormhole(); wormholeEnd(); break;
      case 5: dialIn(); kawoosh(); wormhole(); wormholeUnstable(); break;
      default: dial(); dialFail(); break;
    }
  }
}


void displayClock()
{
  //do LED clock output
  for(int i = 0; i < NUM_LEDS; i++)//blank it first
  {
    leds[i] = CRGB(COLOUR_BG[0],COLOUR_BG[1],COLOUR_BG[2]);//but dont show yet
  }
    
  for(int i = 0; i < 12; i++)//set up the number points
  {
    leds[i*5] = CRGB(COLOUR_DOTS[0],COLOUR_DOTS[1],COLOUR_DOTS[2]);
  }
  
  int myHour = hour()*5;
  
  if (hour() >= 12)//make sure it works in the afternoon
    myHour = (hour() - 12) * 5;

  if(minute() >= (12*0)){/*do nothing*/}//increment an LED for every 12 mins through the hour
  if(minute() >= (12*1)){myHour++;}
  if(minute() >= (12*2)){myHour++;}
  if(minute() >= (12*3)){myHour++;}
  if(minute() >= (12*4)){myHour++;}
  
  int lastSecond;
  if(second() == 0)
    lastSecond = 59;
  else
    lastSecond = second()-1;

  int myHourM2 = myHour-2;
  if(myHour == 0 || myHour == 1)
    myHourM2 = (58 + myHour);

  int myHourM1 = myHour-1;
  if(myHour == 0)
    myHourM1 = 59;
    
  int myHourP1 = myHour+1;
  if(myHour == 59)
    myHourP1 = 0;

  int myHourP2 = myHour+2;
  if(myHour == 58 || myHour == 59)
    myHourP2 = (myHour - 58);
  
  //for a spread hour... but faded
  leds[myHourM2]   = CRGB(COLOUR_HOUR[0]/4,COLOUR_HOUR[1]/4,COLOUR_HOUR[2]/4);
  leds[myHourM1]   = CRGB(COLOUR_HOUR[0]/2,COLOUR_HOUR[1]/2,COLOUR_HOUR[2]/2);
  leds[myHour]   = CRGB(COLOUR_HOUR[0],COLOUR_HOUR[1],COLOUR_HOUR[2]);
  leds[myHourP1]   = CRGB(COLOUR_HOUR[0]/2,COLOUR_HOUR[1]/2,COLOUR_HOUR[2]/2);
  leds[myHourP2]   = CRGB(COLOUR_HOUR[0]/4,COLOUR_HOUR[1]/4,COLOUR_HOUR[2]/4);
  
  int myMinM1 = minute()-1;
  if(minute() == 0)
    myMinM1 = 59;
  
  int myMinP1 = minute()+1;
  if(minute() == 59)
    myMinP1 = 0;  
  
  //now blend mins in (half and half)
  float blend,iblend;
  blend = (1.0/4.0); iblend = 1.0-blend;
  leds[myMinM1] = CRGB((COLOUR_MIN[0]*blend) + (leds[myMinM1].r*iblend),(COLOUR_MIN[1]*blend) + (leds[myMinM1].g*iblend),(COLOUR_MIN[2]*blend) + (leds[myMinM1].b*iblend));
  blend = (3.0/4.0); iblend = 1.0 - blend;
  leds[minute()] = CRGB((COLOUR_MIN[0]*blend) + (leds[minute()].r*iblend),(COLOUR_MIN[1]*blend) + (leds[minute()].g*iblend),(COLOUR_MIN[2]*blend) + (leds[minute()].b*iblend));
  blend = (1.0/4.0); iblend = 1.0 - blend;
  leds[myMinP1] = CRGB((COLOUR_MIN[0]*blend) + (leds[myMinP1].r*iblend),(COLOUR_MIN[1]*blend) + (leds[myMinP1].g*iblend),(COLOUR_MIN[2]*blend) + (leds[myMinP1].b*iblend));

  //now do seconds blending (smooth transitions over the top)
  if((second() > currentSec) || ((second() == 0) && (currentSec == 59)))//if it has incrmented or reset
  {
    timerSec = millis();//log when it happened
    currentSec = second();
//    Serial.println(currentSec);//DEBUG
  }

  float prop = 0.0;//proportion will be between 0 and 1 for balance based on how far through the second we are
  //prop = ((float)(millis()-timerSec));
  //prop = prop / 1000.0;
  prop = ((float)(millis()-timerSec)) / 1000.0;
  float iprop = 1.0-prop;
    
  //smooth seconds - blends OVER what is already there
  leds[second()] = CRGB((COLOUR_SEC[0]*prop) + (leds[currentSec].r*iprop),
                        (COLOUR_SEC[1]*prop) + (leds[currentSec].g*iprop),
                        (COLOUR_SEC[2]*prop) + (leds[currentSec].b*iprop));                      
  
  leds[lastSecond] = CRGB((COLOUR_SEC[0]*iprop) + (leds[lastSecond].r*prop),
                          (COLOUR_SEC[1]*iprop) + (leds[lastSecond].g*prop),
                          (COLOUR_SEC[2]*iprop) + (leds[lastSecond].b*prop));//the previous one
  
  FastLED.show();
}


/*------------------------------------------- EEPROM THINGS -------------------------------------------*/

void saveBrightness()
{
  EEPROM.write(3, LEDBrightness);
}

void retriveBrightness()
{
  LEDBrightness = EEPROM.read(3);
}

void saveColours()
{
  EEPROM.write(4,COLOUR_HOUR[0]);
  EEPROM.write(5,COLOUR_HOUR[1]);
  EEPROM.write(6,COLOUR_HOUR[2]);
  EEPROM.write(7,COLOUR_MIN[0]);
  EEPROM.write(8,COLOUR_MIN[1]);
  EEPROM.write(9,COLOUR_MIN[2]);
  EEPROM.write(10,COLOUR_SEC[0]);
  EEPROM.write(11,COLOUR_SEC[1]);
  EEPROM.write(12,COLOUR_SEC[2]);
}

void retriveColours()
{
  COLOUR_HOUR[0] = EEPROM.read(4);
  COLOUR_HOUR[1] = EEPROM.read(5);
  COLOUR_HOUR[2] = EEPROM.read(6);
  COLOUR_MIN[0] = EEPROM.read(7);
  COLOUR_MIN[1] = EEPROM.read(8);
  COLOUR_MIN[2] = EEPROM.read(9);
  COLOUR_SEC[0] = EEPROM.read(10);
  COLOUR_SEC[1] = EEPROM.read(11);
  COLOUR_SEC[2] = EEPROM.read(12);
}


/*------------------------------------------- MENU THINGS -------------------------------------------*/

void printMenu()
{
  Serial.println(" -- Hopo's LED Clock -- ");
  digitalClockDisplay();
  Serial.print(" Brightness: ");
  if(LEDBrightness == 000)
    Serial.println("AUTO");
  else
    Serial.println(LEDBrightness);
  Serial.println(" - MENU - ");
  Serial.println(" G = Get Settings");
  Serial.println(" S = Set Date/Time");
  Serial.println(" B = Set Brightness");
  Serial.println(" C = Set Colours (RGB)");
  Serial.println(" E = Set Easy Colours");
}

void doMenu(char selection)
{
  switch(selection)
  {
    case 'G'://print time out
	  digitalClockDisplay();
          Serial.print(" Brightness: ");
          Serial.println(LEDBrightness);
	  break;
    case 'S'://set time menu
	  setTimeMenu();
          printMenu();
	  break;
    case 'B'://set brightness menu
	  setBrightnessMenu();
          printMenu();
	  break;
    case 'C'://set colours menu
	  setColoursMenu();
          printMenu();
	  break;
    case 'E'://set Easy colours menu
	  setEasyColoursMenu();
          printMenu();
	  break;
    default:
	  Serial.println("Unknown command");
	  break;
  }
}

void setTimeMenu()
{
  int timeDatArray[6] = {0,0,0,0,0,0};
 
  Serial.println("Enter Year (YY)");
  timeDatArray[5] = get2DigitFromSerial();
  Serial.println("Enter Month (MM)");
  timeDatArray[4] = get2DigitFromSerial();
  Serial.println("Enter Day (DD)");
  timeDatArray[3] = get2DigitFromSerial();
  Serial.println("Enter Hour (hh)");
  timeDatArray[0] = get2DigitFromSerial();
  Serial.println("Enter Minute (mm)");
  timeDatArray[1] = get2DigitFromSerial();
  Serial.println("Enter Second (ss)");
  timeDatArray[2] = get2DigitFromSerial();

  setTime(timeDatArray[0],
          timeDatArray[1],
          timeDatArray[2],
          timeDatArray[3],
          timeDatArray[4],
          timeDatArray[5]);

  time_t t = now();
  RTC.set(t);  
}

void setBrightnessMenu()
{
  Serial.println("Enter Brightness (000-255) [000=Auto]");
  LEDBrightness = get3DigitFromSerial();
  LEDS.setBrightness(LEDBrightness);
  saveBrightness();
}

void setColoursMenu()
{
  Serial.println("Enter Hour Red Component (000-255)");
    COLOUR_HOUR[0]  = get3DigitFromSerial();
  Serial.println("Enter Hour Green Component (000-255)");  
    COLOUR_HOUR[1]  = get3DigitFromSerial();
  Serial.println("Enter Hour Blue Component (000-255)");
    COLOUR_HOUR[2]  = get3DigitFromSerial();
  
  Serial.println("Enter Minute Red Component (000-255)");  
    COLOUR_MIN[0]  = get3DigitFromSerial();
  Serial.println("Enter Minute Green Component (000-255)");  
    COLOUR_MIN[1]  = get3DigitFromSerial();
  Serial.println("Enter Minute Blue Component (000-255)");
    COLOUR_MIN[2]  = get3DigitFromSerial();

  Serial.println("Enter Second Red Component (000-255)");  
    COLOUR_SEC[0]  = get3DigitFromSerial();
  Serial.println("Enter Second Green Component (000-255)");  
    COLOUR_SEC[1]  = get3DigitFromSerial();
  Serial.println("Enter Second Blue Component (000-255)");  
    COLOUR_SEC[2]  = get3DigitFromSerial();

  saveColours();
}

void setEasyColoursMenu()
{
  CRGB temp;
  Serial.println("Enter Hour Colour (000-255)");
  temp = CRGB(Wheel(get3DigitFromSerial()));
  COLOUR_HOUR[0] = temp.r;
  COLOUR_HOUR[1] = temp.g;
  COLOUR_HOUR[2] = temp.b;
  Serial.println("Enter Minute Colour (000-255)");
  temp = CRGB(Wheel(get3DigitFromSerial()));
  COLOUR_MIN[0] = temp.r;
  COLOUR_MIN[1] = temp.g;
  COLOUR_MIN[2] = temp.b;  
  Serial.println("Enter Second Colour (000-255)");
  temp = CRGB(Wheel(get3DigitFromSerial()));
  COLOUR_SEC[0] = temp.r;
  COLOUR_SEC[1] = temp.g;
  COLOUR_SEC[2] = temp.b;
  saveColours();
}

/*------------------------------------------- PRINTY THINGS -------------------------------------------*/

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(" Time:  ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
  Serial.print(" Colour Set: ");
  Serial.println(colourSet);
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*------------------------------------------- INPUT THINGS -------------------------------------------*/

int get2DigitFromSerial()
{
  int retVal = 0;
  while (!(Serial.available() > 0));
  {
    retVal += (10 * (Serial.read() - '0'));
  }
  while (!(Serial.available() > 0));
  {
    retVal += (Serial.read() - '0');
  }
  return retVal;
}

int get3DigitFromSerial()
{
  int retVal = 0;
  while (!(Serial.available() > 0));
  {
    retVal += (100 * (Serial.read() - '0'));
  }
  while (!(Serial.available() > 0));
  {
    retVal += (10 * (Serial.read() - '0'));
  }
  while (!(Serial.available() > 0));
  {
    retVal += (Serial.read() - '0');
  }
  return retVal;
}

/*------------------------------------------- COLOUR/PATTERN THINGS -------------------------------------------*/

void applyColourSet(int profile)
{
  if(profile>=NUM_COLOURSETS)//correct for overflow
    profile = 0;
  
  colourSet = profile;//set the int  
  //make colourset active
  COLOUR_HOUR[0] = myColourProfiles[profile].ColHour.r;
  COLOUR_HOUR[1] = myColourProfiles[profile].ColHour.g;
  COLOUR_HOUR[2] = myColourProfiles[profile].ColHour.b;
  COLOUR_MIN[0] = myColourProfiles[profile].ColMin.r;
  COLOUR_MIN[1] = myColourProfiles[profile].ColMin.g;
  COLOUR_MIN[2] = myColourProfiles[profile].ColMin.b;
  COLOUR_SEC[0] = myColourProfiles[profile].ColSec.r;
  COLOUR_SEC[1] = myColourProfiles[profile].ColSec.g;
  COLOUR_SEC[2] = myColourProfiles[profile].ColSec.b;
  COLOUR_DOTS[0] = myColourProfiles[profile].ColDots.r;
  COLOUR_DOTS[1] = myColourProfiles[profile].ColDots.g;
  COLOUR_DOTS[2] = myColourProfiles[profile].ColDots.b;
  COLOUR_BG[0] = myColourProfiles[profile].ColBg.r;
  COLOUR_BG[1] = myColourProfiles[profile].ColBg.g;
  COLOUR_BG[2] = myColourProfiles[profile].ColBg.b;
}

void setupColourProfiles()
{
  
  //default with dots
  myColourProfiles[0].ColHour.r = 255;
  myColourProfiles[0].ColHour.g = 127;
  myColourProfiles[0].ColHour.b = 000;
  myColourProfiles[0].ColMin.r = 000;
  myColourProfiles[0].ColMin.g = 255;
  myColourProfiles[0].ColMin.b = 127;
  myColourProfiles[0].ColSec.r = 064;
  myColourProfiles[0].ColSec.g = 000;
  myColourProfiles[0].ColSec.b = 255;
  myColourProfiles[0].ColDots.r = 30;
  myColourProfiles[0].ColDots.g = 30;
  myColourProfiles[0].ColDots.b = 30;
  myColourProfiles[0].ColBg.r = 000;
  myColourProfiles[0].ColBg.g = 000;
  myColourProfiles[0].ColBg.b = 000;
  //no dots
  myColourProfiles[1].ColHour.r = 255;
  myColourProfiles[1].ColHour.g = 127;
  myColourProfiles[1].ColHour.b = 000;
  myColourProfiles[1].ColMin.r = 000;
  myColourProfiles[1].ColMin.g = 255;
  myColourProfiles[1].ColMin.b = 127;
  myColourProfiles[1].ColSec.r = 064;
  myColourProfiles[1].ColSec.g = 000;
  myColourProfiles[1].ColSec.b = 255;
  myColourProfiles[1].ColDots.r = 0;
  myColourProfiles[1].ColDots.g = 0;
  myColourProfiles[1].ColDots.b = 0;
  myColourProfiles[1].ColBg.r = 000;
  myColourProfiles[1].ColBg.g = 000;
  myColourProfiles[1].ColBg.b = 000;
  //fully dark
  myColourProfiles[2].ColHour.r = 000;
  myColourProfiles[2].ColHour.g = 000;
  myColourProfiles[2].ColHour.b = 000;
  myColourProfiles[2].ColMin.r = 000;
  myColourProfiles[2].ColMin.g = 000;
  myColourProfiles[2].ColMin.b = 000;
  myColourProfiles[2].ColSec.r = 000;
  myColourProfiles[2].ColSec.g = 000;
  myColourProfiles[2].ColSec.b = 000;
  myColourProfiles[2].ColDots.r = 000;
  myColourProfiles[2].ColDots.g = 000;
  myColourProfiles[2].ColDots.b = 000;
  myColourProfiles[2].ColBg.r = 000;
  myColourProfiles[2].ColBg.g = 000;
  myColourProfiles[2].ColBg.b = 000;
}

void dialFailSparks()
{//fails with sparks
  int fadeDelay = 10;
  for(int k = 0; k < 200; k++)//fadedown sequence
  {
    int spark = random(0,59);
    getChevronBG(0);//quick way to blank it
    addChevronFade(1 ,200-k);
    addChevronFade(2 ,200-k);
    addChevronFade(3 ,200-k);
    addChevronFade(4 ,200-k);
    addChevronFade(5 ,200-k);
    addChevronFade(6 ,200-k);
    addChevronFade(7 ,200-k);
    addChevronFade(8 ,200-k);
    addChevronFade(9 ,200-k);
//run every 5th time or something
    leds[spark] = CRGB(255,200,64);
    leds[spark+1] = CRGB(255,200,64);
          
    delay(fadeDelay);
    FastLED.show();//show it
    }
}

void kawoosh()
{//run a stargate kawoosh effect
//white flash
  for(int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB(255,255,255);//but dont show yet
  }
  FastLED.show();//show it
  delay(100);
  int fadeDelay = 1;
//fadeup  
  for(int k = 0; k < 200; k++)
  {
    for(int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CRGB(k,k,k);//but dont show yet
    }
    delay(fadeDelay);
    FastLED.show();//show it
  }
//hold
  delay(500);
//fadedown to blue
  for(int k = 0; k < 200; k++)//fadedown sequence
  {
    for(int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CRGB(200-k,200-(k/2),200);//but dont show yet
    }
    delay(fadeDelay);
    FastLED.show();//show it
  }
}

void wormholeEnd()
{//gracefull ending of a wormhole
  while(LEDS.getBrightness() > 5)//reduce brightness to nothing
  {
    runSparkle(CRGB(COLOUR_WORMHOLE[0],COLOUR_WORMHOLE[1],COLOUR_WORMHOLE[2]), 20, 4);
	LEDS.setBrightness(LEDS.getBrightness()-4);//reduce by 4 each time
  }
  getChevronBG(0);//blank it
  FastLED.show();//show it
  delay(1000);//black for a bit
  //brightness will be re-set when normal loop resumes
}

void dialIn()
{//all chevrons fade in
  int fadeDelay = 3;
  getChevronBG(0);//blank it
  for(int j = 0; j < 200; j++)//fadeup sequence
	{
	  addChevronFade(1 ,j);
	  addChevronFade(2 ,j);
	  addChevronFade(3 ,j);
	  addChevronFade(4 ,j);
	  addChevronFade(5 ,j);
	  addChevronFade(6 ,j);
	  addChevronFade(7 ,j);
	  addChevronFade(8 ,j);
	  addChevronFade(9 ,j);
	  delay(fadeDelay);
	  FastLED.show();//show it
	}
}

//No it just sparkles doesn't do the black breaks.
//After doing unstable as part of the sequence

void wormholeUnstable()
{//ripple effect with black outs, which increase until collapse (following a kawoosh)
  int i = 15;
  int delayTime = 20;
  while(i != 1)
  {
    int black = random(0,i);
	if (black == 1 || black == 2)//double the chances
	{
          for(int i = 0; i < NUM_LEDS; i++)//blank it first
          {
            leds[i] = CRGB::Black;//but dont show yet
          }
	  FastLED.show();//show it
	  delay(200);
	  i--;//increase probability
	}
	else
	{
      //getChevronBG(9);
	  runSparkle(CRGB(COLOUR_WORMHOLE[0],COLOUR_WORMHOLE[1],COLOUR_WORMHOLE[2]), delayTime, 1);
	}
  }
  //hold black for a bit
  leds[i] = CRGB::Black;//but dont show yet
  FastLED.show();//show it
  delay(delayTime*50);
}

void wormhole()
{//ripple effect (following a kawoosh)
  //getChevronBG(9);//omitted chevrons to give more focus on other effects
  int runTime = random(WH_MIN,WH_MAX);//in seconds
  unsigned long timeToStop = millis()+(1000*runTime);
  while(millis() < timeToStop)
  {
    runSparkle(CRGB(COLOUR_WORMHOLE[0],COLOUR_WORMHOLE[1],COLOUR_WORMHOLE[2]), 20, 1);
  }
}

void dialFail()
{//fails, powerdown of chevrons
  int fadeDelay = 5;
  for(int k = 0; k < 200; k++)//fadedown sequence
	{
	  getChevronBG(0);	
	  addChevronFade(1 ,200-k);
	  addChevronFade(2 ,200-k);
	  addChevronFade(3 ,200-k);
	  addChevronFade(4 ,200-k);
	  addChevronFade(5 ,200-k);
	  addChevronFade(6 ,200-k);
	  addChevronFade(7 ,200-k);
	  addChevronFade(8 ,200-k);
	  addChevronFade(9 ,200-k);
	  delay(fadeDelay);
	  FastLED.show();//show it
    }
}

void dial()
{
  //run a stargate dialing effect
  
  int fadeDelay = 1;
  int dialDelay = 40;
  int chevrons = 0;
  int dotPosition = 0;
  
  //need to do additive fading
  
  for(int i = 0; i < 9; i++)
  {
    int dialAmmount = random(20,59);//alter this to make it dial diffent ammounts
    int dialDirection = random(0,5);//do ifs to determine direction
    boolean dialingFlag = true;
	
    while(dialingFlag)
    {
      getChevronBG(i);	
      if(dialAmmount != 0)//if we are still moving the dot
      {
        leds[dotPosition] = CRGB(100,100,100);//white
        dialAmmount--;//reduce the distance to dial
	if(dialDirection >2)
	  dotPosition++;//increase dot
	else
	  dotPosition--;//decrease dot
	//checks
	if(dotPosition > 59)
	  dotPosition=0;
	if(dotPosition < 0)
	  dotPosition = 59;
	delay(dialDelay);
      }
      else//we have finished
      {
        //locking in sequence
	for(int j = 0; j < 200; j++)//fadeup sequence
	{
	  //fade up i+1 and 9
	  addChevronFade(i+1 ,j);
	  addChevronFade(9 ,j);
	  delay(fadeDelay);
	  FastLED.show();//show it
	}
	for(int k = 0; k < 200; k++)//fadedown sequence
	{
	  //then fade down 9 (unless i+1=9)
	  if(i+1 != 9)//if not 9 then fade down
	  {
	    addChevronFade(9 ,200-k);
	    delay(fadeDelay);
	    FastLED.show();//show it
	  }
	  else//last run
	  {
	    delay(fadeDelay*10);
	  }
	}
	dialingFlag = false;//we are done
      }  
    FastLED.show();//show it
    }
  }
}

void addChevronFade(int chevron,float fadeAmmount)
{//adds a single chevron to the array at the faded amount(0-255)
  float blend;
  blend = (fadeAmmount/255.0);
  int led1 = 0;
  int led2 = 0;
  int led3 = 0;
  if(chevron == 1){led1 = 6; led2 = led1+1; led3 = led1+2;}
  if(chevron == 2){led1 = 12; led2 = led1+1; led3 = led1+2;}
  if(chevron == 3){led1 = 19; led2 = led1+1; led3 = led1+2;}
  if(chevron == 4){led1 = 39; led2 = led1+1; led3 = led1+2;}
  if(chevron == 5){led1 = 46; led2 = led1+1; led3 = led1+2;}
  if(chevron == 6){led1 = 52; led2 = led1+1; led3 = led1+2;}
  if(chevron == 7){led1 = 26; led2 = led1+1; led3 = led1+2;}
  if(chevron == 8){led1 = 32; led2 = led1+1; led3 = led1+2;}
  if(chevron == 9){led1 = 59; led2 = 0; led3 = 1;}

  leds[led1] = CRGB((COLOUR_CHEVRON[0]*blend),
                    (COLOUR_CHEVRON[1]*blend),
                    (COLOUR_CHEVRON[2]*blend));
                    
  leds[led2] = CRGB((COLOUR_CHEVRON[0]*blend),
                    (COLOUR_CHEVRON[1]*blend),
                    (COLOUR_CHEVRON[2]*blend));
                    
  leds[led3] = CRGB((COLOUR_CHEVRON[0]*blend),
                    (COLOUR_CHEVRON[1]*blend),
                    (COLOUR_CHEVRON[2]*blend));
}

void getChevronBG(int chevrons)
{//set up a background with chevrons already lit
  for(int i = 0; i < NUM_LEDS; i++)//blank it first
  {
    leds[i] = CRGB::Black;//but dont show yet
  }
  //add chevrons for current state (correct order now)
  switch(chevrons)
  {//a dropthrough switch statement
    case 0: /*do nothing at all*/ break;
    
    case 9: leds[59] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[0] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[1] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);

    case 8: leds[32] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[33] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[34] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            
    case 7: leds[26] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[27] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[28] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            
    case 6: leds[52] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[53] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[54] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
				
    case 5: leds[46] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[47] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[48] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);

    case 4: leds[39] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[40] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[41] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);

    case 3: leds[19] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[20] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[21] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
		 
    case 2: leds[12] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[13] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[14] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
				
    case 1: leds[6] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
            leds[7] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);
	    leds[8] = CRGB(COLOUR_CHEVRON[0],COLOUR_CHEVRON[1],COLOUR_CHEVRON[2]);

    default: /*do nothing*/ break;
  }
}

void runSparkle(CRGB myColour, int wait, int times) 
{
  for(int i = 0; i < times; i++)
  {
    sparkle(myColour.r,myColour.g ,myColour.b,wait);
  }
}
  
void sparkle(byte C0, byte C1, byte C2, int wait) 
{
  for(int i = 0; i < NUM_LEDS; i++)//make a strips-worth of data
  {
    byte whiteChance = random(0,256);
    byte FC0 = 0;
    byte FC1 = 0;
    byte FC2 = 0;
	/* - trimmed out white for SG1 clock
    if(whiteChance>252)//occasionally
    {
      FC0=255;
      FC1=255;
      FC2=255;
    }
    else
    {
	*/
      //dull down by a random ammount
      FC0=C0/(255/whiteChance);
      FC1=C1/(255/whiteChance);
      FC2=C2/(255/whiteChance);		
      //to dull down the colourd bit so the white looks brighter	
      FC0=FC0/3;
      FC1=FC1/3;
      FC2=FC2/3;
    //}
    //leds[i] = CRGB(i,FC0,FC1,FC2);//shimmer effect
    leds[i] = CRGB(FC0,FC1,FC2);
  }
  delay(wait);
  LEDS.show();
}

void testLEDStrip()
{
  LEDS.setBrightness(23);
  for(int whiteLed = 0; whiteLed < NUM_LEDS; whiteLed = whiteLed + 1)
  {
    leds[whiteLed] = CRGB::White;
    FastLED.show();
    delay(10);
    leds[whiteLed] = CRGB::Black;
  }
  LEDS.setBrightness(LEDBrightness);//reset brightness
}

void showAllColour(CRGB colour)
{
  for(int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = colour;
  }
  FastLED.show();
}

CRGB Wheel(byte WheelPos) {
	if(WheelPos < 85)//in first 3rd
	{
		return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);//no blue component
	}
	else if(WheelPos < 170) //in second 3rd
	{
		WheelPos -= 85;
		return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);//no green component
	}
	else//in last 3rd
	{
		WheelPos -= 170;
		return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);//no red component
	}
}
