#include <Time.h>  
#include <TimeAlarms.h>
#include <Wire.h>  
#include <DS1307RTC.h>
#include <EEPROM.h>
#include "FastLED.h"

#define NUM_LEDS 60
#define LED_DATA_PIN 6
#define LED_BRIGHTNESS 25
#define ALARM_BRIGHTNESS 255
CRGB leds[NUM_LEDS];

unsigned long timerSec = 0;
int currentSec = 0;

//default colours
byte COLOUR_SEC[3]  = {000,000,255};//BLUE
byte COLOUR_MIN[3]  = {000,255,000};//GREEN
byte COLOUR_HOUR[3] = {255,000,000};//RED

//versions for serial.print
int alarmHour = 0;
int alarmMin = 0;
int alarmSec = 0;

int LEDBrightness = LED_BRIGHTNESS;

void setup()  {
  Serial.begin(9600);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time");      

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  LEDS.setBrightness(LEDBrightness);
  testLEDStrip();
  retriveAlarm();
  printMenu();
}

void loop()
{
  if (Serial.available() > 0)
  {
    doMenu(Serial.read());
  }
  displayClock();
  Alarm.delay(10);//to allow alarm to trigger
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
  
  leds[myHour]   = CRGB(COLOUR_HOUR[0],COLOUR_HOUR[1],COLOUR_HOUR[2]);
  leds[minute()] = CRGB(COLOUR_MIN[0],COLOUR_MIN[1],COLOUR_MIN[2]);
  leds[second()] = CRGB(COLOUR_SEC[0],COLOUR_SEC[1],COLOUR_SEC[2]);//work out how to do smooth transitions
  
  //smooth transition (overrides previuos second)  
  if((second() > currentSec) || (second() == 0))//if it has incrmented or reset
  {
    timerSec = millis();//log when it happened
    currentSec = second();
  }

  float proportion = 0.0;//this will be between 0 and 1 for brightness based on how far through the second we are
  proportion = (float)(millis()-timerSec);
  proportion = proportion / 1000.0;
  leds[second()] = CRGB(COLOUR_SEC[0]*proportion,COLOUR_SEC[1]*proportion,COLOUR_SEC[2]*proportion);
  leds[second()-1] = CRGB(COLOUR_SEC[0]*(1-proportion),COLOUR_SEC[1]*(1-proportion),COLOUR_SEC[2]*(1-proportion));//the previous one

  if(myHour == minute())
    leds[minute()] = CRGB((COLOUR_HOUR[0] + COLOUR_MIN[0]) / 2,
                          (COLOUR_HOUR[1] + COLOUR_MIN[1]) / 2,
                          (COLOUR_HOUR[2] + COLOUR_MIN[2]) / 2);
  
  if(second() == minute())
    leds[second()] = CRGB((COLOUR_SEC[0] + COLOUR_MIN[0]) / 2,
                          (COLOUR_SEC[1] + COLOUR_MIN[1]) / 2,
                          (COLOUR_SEC[2] + COLOUR_MIN[2]) / 2);
  
  if(second() == myHour)
    leds[second()] = CRGB((COLOUR_SEC[0] + COLOUR_HOUR[0]) / 2,
                          (COLOUR_SEC[1] + COLOUR_HOUR[1]) / 2,
                          (COLOUR_SEC[2] + COLOUR_HOUR[2]) / 2);
  
  if(myHour == minute() == second())
    leds[second()] = CRGB((COLOUR_SEC[0] + COLOUR_HOUR[0] + COLOUR_MIN[0]) / 3,
                          (COLOUR_SEC[1] + COLOUR_HOUR[1] + COLOUR_MIN[1]) / 3,
                          (COLOUR_SEC[2] + COLOUR_HOUR[2] + COLOUR_MIN[2]) / 3);
  
  FastLED.show();
}


void printMenu()
{
  Serial.println(" -- Hopo's LED Clock -- ");
  digitalClockDisplay();
  digitalAlarmDisplay();
  Serial.print(" Brightness: ");
  Serial.println(LEDBrightness);
  Serial.println(" - MENU - ");
  Serial.println(" G = Get Settings");
  Serial.println(" S = Set Time");
  Serial.println(" A = Set Alarm");
  Serial.println(" B = Set Brightness");
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
    default:
	  Serial.println("Unknown command");
	  break;
  }
}

void testLEDStrip()
{
   for(int whiteLed = 0; whiteLed < NUM_LEDS; whiteLed = whiteLed + 1)
   {
      leds[whiteLed] = CRGB::White;
      FastLED.show();
      delay(10);
      leds[whiteLed] = CRGB::Black;
   }
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
  Serial.println("Enter Brightness (000-255)");
  LEDBrightness = get3DigitFromSerial();
  LEDS.setBrightness(LEDBrightness);
}

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
