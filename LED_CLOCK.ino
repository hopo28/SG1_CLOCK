#include <Time.h>  
#include <Wire.h>  
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include "FastLED.h"


/* PINOUTS
RTC SDA - A4
RTC SCK - A5
RTC DS  - D2
RGB LED - D6
*/

#define NUM_LEDS 60
#define LED_DATA_PIN 6
#define LED_BRIGHTNESS 23
CRGB leds[NUM_LEDS];

//default colours
byte COLOUR_SEC[3]  = {000,000,255};//BLUE
byte COLOUR_MIN[3]  = {000,255,000};//GREEN
byte COLOUR_HOUR[3] = {255,000,000};//RED

void setup()  {
  Serial.begin(9600);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time");      

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.setBrightness(LED_BRIGHTNESS);
  testLEDStrip();
  printMenu();
   
}

void loop()
{
  if (Serial.available() > 0)
  {
    doMenu(Serial.read());
  }
  displayClock();
}


void displayClock()
{
  //do LED clock output
  for(int i = 0; i < NUM_LEDS; i++)//blank it first
  {
    leds[i] = CRGB::Black;
  }
  leds[hour()]   = CRGB(COLOUR_HOUR[0],COLOUR_HOUR[1],COLOUR_HOUR[2]);
  leds[minute()] = CRGB(COLOUR_MIN[0],COLOUR_MIN[1],COLOUR_MIN[2]);
  leds[second()] = CRGB(COLOUR_SEC[0],COLOUR_SEC[1],COLOUR_SEC[2]);
  FastLED.show();
}


void printMenu()
{
  Serial.println(" -- Hopo's LED Clock -- ");
  digitalClockDisplay();
  Serial.println(" - MENU - ");
  Serial.println(" S = Set time");
  Serial.println(" G = Get time");
  //Serial.println();
  //Serial.println();
}

void doMenu(char selection)
{
  switch(selection)
  {
    case 'G':
	  //print time out
	  digitalClockDisplay();
	  break;
	case 'S':
	  //set time menu
	  setTimeMenu();
	  break;
	default:
	  Serial.println("Unknown command");
	  break;
  }
  printMenu();
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


int get2DigitFromSerial()
{
  int retVal = 0;
  while (!(Serial.available() > 0));
  {
    retVal = (10 * (Serial.read() - '0'));
  }
  while (!(Serial.available() > 0));
  {
    retVal += (Serial.read() - '0');
  }
  return retVal;
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

  //setTime(9,30,50,13,1,2012);
  time_t t = now();
  RTC.set(t);
  digitalClockDisplay();  
}

void digitalClockDisplay(){
  // digital clock display of the time
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

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
