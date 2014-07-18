#include <Time.h>  
#include <TimeAlarms.h>
#include <Wire.h>  
#include <DS1307RTC.h>
#include <EEPROM.h>
#include "FastLED.h"

#define NUM_LEDS 60
#define LED_DATA_PIN 3
#define LED_BRIGHTNESS 25 //default brightness
#define ALARM_BRIGHTNESS 255
#define LDR_PIN A7

#define BUT_PIN_1 4
#define BUT_PIN_2 5
#define BUT_PIN_3 6
#define BUT_PIN_4 7
CRGB leds[NUM_LEDS];

unsigned long timerSec = 0;
int currentSec = 0;

//default colours
byte COLOUR_HOUR[3] = {255,000,000};//RED
byte COLOUR_MIN[3]  = {000,255,000};//GREEN
byte COLOUR_SEC[3]  = {000,000,255};//BLUE

/*alt colours
byte COLOUR_HOUR[3] = {255,127,000};//RED
byte COLOUR_MIN[3]  = {000,255,127};//GREEN
byte COLOUR_SEC[3]  = {127,000,255};//BLUE
*/

//versions for serial.print
int alarmHour = 0;
int alarmMin = 0;
int alarmSec = 0;

int LEDBrightness = LED_BRIGHTNESS;

void setup()
{
  Serial.begin(9600);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time");      

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  retriveBrightness();
  LEDS.setBrightness(LEDBrightness);
  testLEDStrip();
  retriveColours();
  retriveAlarm();
  printMenu();
}

void loop()
{
  if (Serial.available() > 0)
  {
    doMenu(Serial.read());
  }
  if(year() < 2014)
    errorPattern();
  else
    displayClock();
  Alarm.delay(10);//to allow alarm to trigger
  if(LEDBrightness == 0)//if user has requested auto
    checkLight();
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

void displayClock()
{
  //do LED clock output
  for(int i = 0; i < NUM_LEDS; i++)//blank it first
  {
    leds[i] = CRGB::Black;//but dont show yet
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
  prop = (float)(millis()-timerSec);
  prop = prop / 1000.0;
  float iprop = 1-prop;
    
  //smooth seconds - blends OVER what is already there
  
  leds[second()] = CRGB((COLOUR_SEC[0]*prop) + (leds[second()].r*iprop),
                        (COLOUR_SEC[1]*prop) + (leds[second()].g*iprop),
                        (COLOUR_SEC[2]*prop) + (leds[second()].b*iprop));                      
  
  leds[lastSecond] = CRGB((COLOUR_SEC[0]*iprop) + (leds[lastSecond].r*prop),
                          (COLOUR_SEC[1]*iprop) + (leds[lastSecond].g*prop),
                          (COLOUR_SEC[2]*iprop) + (leds[lastSecond].b*prop));//the previous one
  
  FastLED.show();
}


/*------------------------------------------- EEPROM THINGS -------------------------------------------*/

void saveAlarm()
{
  EEPROM.write(0, alarmHour);
  EEPROM.write(1, alarmMin);
  EEPROM.write(2, alarmSec);
  Alarm.alarmRepeat(alarmHour,alarmMin,alarmSec, TheAlarm);
}

void retriveAlarm()
{
  alarmHour = EEPROM.read(0);
  alarmMin = EEPROM.read(1);
  alarmSec = EEPROM.read(2);
  Alarm.alarmRepeat(alarmHour,alarmMin,alarmSec, TheAlarm);
}

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
  digitalAlarmDisplay();
  Serial.print(" Brightness: ");
  if(LEDBrightness == 000)
    Serial.println("AUTO");
  else
    Serial.println(LEDBrightness);
  Serial.println(" - MENU - ");
  Serial.println(" G = Get Settings");
  Serial.println(" S = Set Date/Time");
  Serial.println(" A = Set Alarm");
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
	  digitalAlarmDisplay();
          Serial.print(" Brightness: ");
          Serial.println(LEDBrightness);
	  break;
    case 'S'://set time menu
	  setTimeMenu();
          printMenu();
	  break;
    case 'A'://set alarm menu
	  setAlarmMenu();
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

void setAlarmMenu()
{
  Serial.println("Enter Alarm Hour (hh)");
  alarmHour = get2DigitFromSerial();
  Serial.println("Enter Alarm Minute (mm)");
  alarmMin = get2DigitFromSerial();
  Serial.println("Enter Alarm Second (ss)");
  alarmSec = get2DigitFromSerial();
  saveAlarm();
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
}

void digitalAlarmDisplay()
{
  Serial.print(" Alarm: ");
  Serial.print(alarmHour);
  printDigits(alarmMin);
  printDigits(alarmSec);
  Serial.println(" Daily");
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

void TheAlarm()
{
  Serial.println("ALARM!");
  LEDS.setBrightness(ALARM_BRIGHTNESS);//full bright
  for(int i = 0; i < 10; i++)
  {
    showAllColour(CRGB::White);
    delay(100);
    showAllColour(CRGB::Black);
    delay(100);
  }
  LEDS.setBrightness(LEDBrightness);//reset brightness
}

void errorPattern()
{
  Serial.println("ERROR - Please Set Time/Date");
  LEDS.setBrightness(23);
  showAllColour(CRGB::Red);
  delay(1000);
  showAllColour(CRGB::Black);
  delay(1000);
  LEDS.setBrightness(LEDBrightness);//reset brightness
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
