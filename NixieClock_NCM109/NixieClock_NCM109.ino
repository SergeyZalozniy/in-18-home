#define IN_18
//#define IN_12

#include <SPI.h>
#include <Wire.h>
#include <Timezone.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <FastLED.h>

#ifdef IN_18
#include "doIndication318_HW1.x.h"
#endif

#ifdef IN_12
#include "doIndication109.h"
#endif


#define GPS_BUFFER_LENGTH 83

char GPS_Package[GPS_BUFFER_LENGTH];
byte GPS_position=0;

struct GPS_DATE_TIME
{
  byte GPS_hours;
  byte GPS_minutes;
  byte GPS_seconds;
  byte GPS_day;
  byte GPS_mounth;
  int GPS_year; 
  bool GPS_Valid_Data=false;
  unsigned long GPS_Data_Parsed_time;
};

GPS_DATE_TIME GPS_Date_Time;

boolean UD, LD; // DOTS control;

byte addr[8];

#define kColorCount 24
CRGBArray<kColorCount> colors;

const byte RedLedPin = 9; //MCU WDM output for red LEDs 9-g
const byte GreenLedPin = 6; //MCLEpinU WDM output for green LEDs 6-b
const byte BlueLedPin = 3; //MCU WDM output for blue LEDs 3-r
const byte pinBuzzer = 2;
const byte pinUpperDots = 12; //HIGH value light a dots
const byte pinLowerDots = 8; //HIGH value light a dots
const byte pinTemp = 7;
bool RTC_present;
#define US_DateFormat 1
#define EU_DateFormat 0

OneWire ds(pinTemp);
bool TempPresent = false;
#define CELSIUS 1
#define FAHRENHEIT 0

String stringToDisplay = "000000"; // Conten of this string will be displayed on tubes (must be 6 chars length)
int menuPosition = 0; 

byte blinkMask = B00000000; //bit mask for blinkin digits (1 - blink, 0 - constant light)
int blankMask = B00000000; //bit mask for digits (1 - off, 0 - on)

byte dotPattern = B00000000; //bit mask for separeting dots (1 - on, 0 - off)

#define DS1307_ADDRESS 0x68
byte zero = 0x00; //workaround for issue #527
int RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year, RTC_day_of_week;

#define TimeIndex        0
#define DateIndex        1
#define AlarmIndex       2
#define hModeIndex       3
#define TemperatureIndex 4
#define TimeZoneIndex    5
#define TimeHoursIndex   6
#define TimeMintuesIndex 7
#define TimeSecondsIndex 8
#define DateFormatIndex  9 
#define DateDayIndex     10
#define DateMonthIndex   11
#define DateYearIndex    12
#define AlarmHourIndex   13
#define AlarmMinuteIndex 14
#define AlarmSecondIndex 15
#define Alarm01          16
#define hModeValueIndex  17
#define DegreesFormatIndex 18
#define HoursOffsetIndex 19

#define FirstParent      TimeIndex
#define LastParent       TimeZoneIndex
#define SettingsCount    (HoursOffsetIndex+1)
#define NoParent         0
#define NoChild          0

//-------------------------------0--------1--------2-------3--------4--------5--------6--------7--------8--------9----------10-------11---------12---------13-------14-------15---------16---------17--------18----------19
//                     names:  Time,   Date,   Alarm,   12/24, Temperature,TimeZone,hours,   mintues, seconds, DateFormat, day,    month,   year,      hour,   minute,   second alarm01  hour_format Deg.FormIndex HoursOffset
//                               1        1        1       1        1        1        1        1        1        1          1        1          1          1        1        1        1            1         1        1
int parent[SettingsCount] = {NoParent, NoParent, NoParent, NoParent,NoParent,NoParent,1,       1,       1,       2,         2,       2,         2,         3,       3,       3,       3,       4,           5,        6};
int firstChild[SettingsCount] = {6,       9,       13,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int lastChild[SettingsCount] = { 8,      12,       16,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int value[SettingsCount] = {     0,       0,       0,      0,       0,       0,       0,       0,       0,  EU_DateFormat,  0,       0,         0,         0,       0,       0,       0,       24,          0,        2};
int maxValue[SettingsCount] = {  0,       0,       0,      0,       0,       0,       23,      59,      59, US_DateFormat,  31,      12,        99,       23,      59,      59,       1,       24,     FAHRENHEIT,    14};
int minValue[SettingsCount] = {  0,       0,       0,      12,      0,       0,       00,      00,      00, EU_DateFormat,  1,       1,         00,       00,      00,      00,       0,       12,      CELSIUS,     -12};
int blinkPattern[SettingsCount] = {  
  B00000000, //0
  B00000000, //1
  B00000000, //2
  B00000000, //3
  B00000000, //4
  B00000000, //5
  B00000011, //6
  B00001100, //7
  B00110000, //8
  B00111111, //9
  B00000011, //10
  B00001100, //11
  B00110000, //12
  B00000011, //13
  B00001100, //14
  B00110000, //15
  B11000000, //16
  B00001100, //17
  B00111111, //18
  B00000011, //19
};

bool editMode = false;

long downTime = 0;
long upTime = 0;
const long settingDelay = 150;
bool BlinkUp = false;
bool BlinkDown = false;
unsigned long enteringEditModeTime = 0;
bool RGBLedsOn = true;
byte RGBLEDsEEPROMAddress = 0;
byte HourFormatEEPROMAddress = 1;
byte AlarmTimeEEPROMAddress = 2; //3,4,5
byte AlarmArmedEEPROMAddress = 6;
byte LEDsLockEEPROMAddress = 7;
byte LEDsRedValueEEPROMAddress = 8;
byte LEDsGreenValueEEPROMAddress = 9;
byte LEDsBlueValueEEPROMAddress = 10;
byte DegreesFormatEEPROMAddress = 11;
byte HoursOffsetEEPROMAddress = 12;
byte DateFormatEEPROMAddress = 13;

///////////////////

int fireforks[] = {0, 0, 1, //1
                   -1, 0, 0, //2
                   0, 1, 0, //3
                   0, 0, -1, //4
                   1, 0, 0, //5
                   0, -1, 0
                  }; //array with RGB rules (0 - do nothing, -1 - decrese, +1 - increse

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w = 1);

int functionDownButton = 0;
int functionUpButton = 0;
bool LEDsLock = false;

//antipoisoning transaction
bool modeChangedByUser = false;
bool transactionInProgress = false; //antipoisoning transaction
#define timeModePeriod 60000
#define dateModePeriod 5000
long modesChangePeriod = timeModePeriod;
//end of antipoisoning transaction

bool GPS_sync_flag=false;
#define gpsSerial Serial
boolean gpsStatus[] = {false, false, false, false, false, false, false};
unsigned long start;


/*******************************************************************************************************
  Init Programm
*******************************************************************************************************/
void setup()
{
  Wire.begin();

  if (EEPROM.read(HourFormatEEPROMAddress) != 12) value[hModeValueIndex] = 24; else value[hModeValueIndex] = 12;
  if (EEPROM.read(RGBLEDsEEPROMAddress) != 0) RGBLedsOn = true; else RGBLedsOn = false;
  if (EEPROM.read(AlarmTimeEEPROMAddress) == 255) value[AlarmHourIndex] = 0; else value[AlarmHourIndex] = EEPROM.read(AlarmTimeEEPROMAddress);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 1) == 255) value[AlarmMinuteIndex] = 0; else value[AlarmMinuteIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 1);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 2) == 255) value[AlarmSecondIndex] = 0; else value[AlarmSecondIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 2);
  if (EEPROM.read(AlarmArmedEEPROMAddress) == 255) value[Alarm01] = 0; else value[Alarm01] = EEPROM.read(AlarmArmedEEPROMAddress);
  if (EEPROM.read(LEDsLockEEPROMAddress) == 255) LEDsLock = false; else LEDsLock = EEPROM.read(LEDsLockEEPROMAddress);
  if (EEPROM.read(DegreesFormatEEPROMAddress) == 255) value[DegreesFormatIndex] = CELSIUS; else value[DegreesFormatIndex] = EEPROM.read(DegreesFormatEEPROMAddress);
  if (EEPROM.read(HoursOffsetEEPROMAddress) == 255) value[HoursOffsetIndex] = value[HoursOffsetIndex]; else value[HoursOffsetIndex] = EEPROM.read(HoursOffsetEEPROMAddress) + minValue[HoursOffsetIndex];
  if (EEPROM.read(DateFormatEEPROMAddress) == 255) value[DateFormatIndex] = value[DateFormatIndex]; else value[DateFormatIndex] = EEPROM.read(DateFormatEEPROMAddress);
  

  pinMode(RedLedPin, OUTPUT);
  pinMode(GreenLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);

  pinMode(LEpin, OUTPUT);

  // SPI setup

  SPI.begin(); //
  SPI.setDataMode (SPI_MODE2); // Mode 3 SPI
  SPI.setClockDivider(SPI_CLOCK_DIV8); // SCK = 16MHz/128= 125kHz

  ////////////////////////////
  pinMode(pinBuzzer, OUTPUT);

//  LEDsSetup();
  gpsSerial.begin(9600);
//  byte settingsArray[] = {0x03, 0xE8, 0x03, 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00}; 
//  configureUblox(settingsArray);
  
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  doTest();
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  
  getRTCTime();
  byte prevSeconds = RTC_seconds;
  unsigned long RTC_ReadingStartTime = millis();
  RTC_present = true;
  while (prevSeconds == RTC_seconds)
  {
    doIndication();
    getRTCTime();
    if (abs(millis() - RTC_ReadingStartTime) > 3000){
      RTC_present = false;
      break;
    }
  }
  
  setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);

  for (int i = 0; i < 5; i++) {
    colors[i] = CRGB(0, 0, 255);
  }

  for (int i = 5; i < 9; i++) {
    colors[i] = CRGB(0, 160, 0);
  }

  for (int i = 9; i < 18; i++) {
    colors[i] = CRGB(255, 255, 255);
  }

  for (int i = 18; i < 21; i++) {
    colors[i] = CRGB(220, 100, 0);
  }

  for (int i = 21; i < 24; i++) {
    colors[i] = CRGB(140, 0, 0);
  }

//  for (int i = 0; i < 24; i++) {
//    colors[i] = CRGB(0, 0, 0);
//  }
}

/***************************************************************************************************************
  MAIN Programm
***************************************************************************************************************/
void loop() {

  static unsigned long lastTimeRTCSync = 0;
  if ((millis() - lastTimeRTCSync) > 60000 && (RTC_present)) //synchronize with RTC every 60 seconds
  {
    getRTCTime();
    setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);
    lastTimeRTCSync = millis();
  }

  GetDataFromSerial1();
  SyncWithGPS();

  if ((menuPosition == TimeIndex) || (modeChangedByUser == false) ) modesChanger();
  doIndication();
  
  blinkMask = B00000000;
  BlinkDown = true;
  BlinkUp = true;

  changeLedColor();

  static bool updateDateTime = false;
  switch (menuPosition)
  {
    case TimeIndex: //time mode
      if (!transactionInProgress) stringToDisplay = updateDisplayString();
      doDotBlink();
      blankMask = B00000000;
      break;
    case DateIndex: //date mode
      if (!transactionInProgress) stringToDisplay = updateDateString();
      dotPattern = B01000000; //turn on lower dots
      blankMask = B00000000;
      break;
    case AlarmIndex: //alarm mode
      stringToDisplay = PreZero(value[AlarmHourIndex]) + PreZero(value[AlarmMinuteIndex]) + PreZero(value[AlarmSecondIndex]);
      blankMask = B00000000;
      if (value[Alarm01] == 1) /*digitalWrite(pinUpperDots, HIGH);*/ dotPattern = B10000000; //turn on upper dots
      else
      {
        /*digitalWrite(pinUpperDots, LOW);
          digitalWrite(pinLowerDots, LOW);*/
        dotPattern = B00000000; //turn off upper dots
      }
      break;
    case hModeIndex: //12/24 hours mode
      stringToDisplay = "00" + String(value[hModeValueIndex]) + "00";
      blankMask = B00110011;
      dotPattern = B00000000; //turn off all dots
      break;
    case TemperatureIndex: //missed break
    case DegreesFormatIndex:
      if (!transactionInProgress)
      {
        stringToDisplay = updateTemperatureString(getTemperature(value[DegreesFormatIndex]));
        if (value[DegreesFormatIndex] == CELSIUS)
        {
          blankMask = B00110001;
          dotPattern = B01000000;
        }
        else
        {
          blankMask = B00100011;
          dotPattern = B00000000;
        }
      }

      if (getTemperature(value[DegreesFormatIndex]) < 0) dotPattern |= B10000000;
      else dotPattern &= B01111111;
      break;
     case TimeZoneIndex:
     case HoursOffsetIndex:
      stringToDisplay = String(PreZero(value[HoursOffsetIndex])) + "0000";
      blankMask = B00001111;
      if (value[HoursOffsetIndex]>=0) dotPattern = B00000000; //turn off all dots  
        else dotPattern = B10000000; //turn on upper dots  
      break;
     case DateFormatIndex:
      if (value[DateFormatIndex] == EU_DateFormat) 
      {
        stringToDisplay="311299";
        blinkPattern[DateDayIndex]=B00000011;
        blinkPattern[DateMonthIndex]=B00001100;
      }
        else 
        {
          stringToDisplay="123199";
          blinkPattern[DateDayIndex]=B00001100;
          blinkPattern[DateMonthIndex]=B00000011;
        }
     break; 
     case DateDayIndex:
     case DateMonthIndex:
     case DateYearIndex:
      if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay=PreZero(value[DateDayIndex])+PreZero(value[DateMonthIndex])+PreZero(value[DateYearIndex]);
        else stringToDisplay=PreZero(value[DateMonthIndex])+PreZero(value[DateDayIndex])+PreZero(value[DateYearIndex]);
     break;
  }
}

void changeLedColor() {
  static int lastCorrectedHour = -1;
  if (hour() == lastCorrectedHour) {
    return ;
  }

  lastCorrectedHour = hour();
  time_t adjustedTime = ukraineTime();
  CRGB color = colors[hour(adjustedTime)];
  analogWrite(RedLedPin, color.r);
  analogWrite(GreenLedPin, color.g);
  analogWrite(BlueLedPin, color.b);    
}

// Slightly different, this makes the rainbow equally distributed throughout
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
CRGB Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

String PreZero(int digit)
{
  if (digit > 9) {
    return String(digit);
  } else {
    return "0" + String(digit);
  }
}

String updateDisplayString()
{
  static unsigned long lastTimeStringWasUpdated;
  if (abs(millis() - lastTimeStringWasUpdated) > 1000)
  {
    lastTimeStringWasUpdated = millis();
    return getTimeNow();
  }
  return stringToDisplay;
}

String getTimeNow()
{
  time_t adjustedTime = ukraineTime();
  return PreZero(hour(adjustedTime)) + PreZero(minute(adjustedTime)) + PreZero(second(adjustedTime));
}

time_t ukraineTime() {
  static TimeChangeRule uaSummer = {"EDT", Last, Sun, Mar, 3, 180};  //UTC + 3 hours
  static TimeChangeRule uaWinter = {"EST", Last, Sun, Oct, 4, 120};  //UTC + 2 hours
  static Timezone uaUkraine(uaSummer, uaWinter);
  static int lastCorrectedHour = -1;
  static time_t deltaTime = 0;
  
  time_t utc = now();

  if (lastCorrectedHour != hour()) {
    TimeChangeRule *tcr;
    lastCorrectedHour = hour();
    time_t tmpTime = uaUkraine.toLocal(utc, &tcr);
    deltaTime = tcr -> offset * 60;
  }
  
  return utc + deltaTime;
}

void doTest()
{ 
//  LEDsTest();
  delay(3000);
  
  String testStringArray[10]={"000000","111111","222222","333333","444444","555555","666666","777777","888888","999999"};
  
  int dlay=500;
  bool test=true;
  byte strIndex=-1;
  unsigned long startOfTest=millis()+1000; 
  while (test) {
    if ((millis()-startOfTest)>dlay) 
     {
       startOfTest=millis();
       strIndex=strIndex+1;
       if (strIndex==20) test=false;
       stringToDisplay=testStringArray[strIndex % 10];
      }
    doIndication();  
  }
}

void doDotBlink()
{
  static unsigned long lastTimeBlink = millis();
  static bool dotState = 0;
  if (abs(millis() - lastTimeBlink) > 1000)
  {
    lastTimeBlink = millis();
    dotState = !dotState;
    if (dotState)
    {
      dotPattern = B11000000;
      /*digitalWrite(pinUpperDots, HIGH);GetDataFromSerial1
        digitalWrite(pinLowerDots, HIGH);*/
    }
    else
    {
      dotPattern = B00000000;
      /*digitalWrite(pinUpperDots, LOW);
        digitalWrite(pinLowerDots, LOW);*/
    }
  }
}

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w)
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //stop Oscillator

  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.write(decToBcd(w));
  Wire.write(decToBcd(d));
  Wire.write(decToBcd(mon));
  Wire.write(decToBcd(y));

  Wire.write(zero); //start

  Wire.endTransmission();

}

byte decToBcd(byte val) {
  // Convert normal decimal numbers to binary coded decimal
  return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val)  {
  // Convert binary coded decimal to normal decimal numbers
  return ( (val / 16 * 10) + (val % 16) );
}

void getRTCTime()
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  RTC_seconds = bcdToDec(Wire.read());
  RTC_minutes = bcdToDec(Wire.read());
  RTC_hours = bcdToDec(Wire.read() & 0b111111); //24 hour time
  RTC_day_of_week = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  RTC_day = bcdToDec(Wire.read());
  RTC_month = bcdToDec(Wire.read());
  RTC_year = bcdToDec(Wire.read());
}

int extractDigits(byte b)
{
  String tmp = "1";

  if (b == B00000011)
  {
    tmp = stringToDisplay.substring(0, 2);
  }
  if (b == B00001100)
  {
    tmp = stringToDisplay.substring(2, 4);
  }
  if (b == B00110000)
  {
    tmp = stringToDisplay.substring(4);
  }
  return tmp.toInt();
}

void injectDigits(byte b, int value)
{
  if (b == B00000011) stringToDisplay = PreZero(value) + stringToDisplay.substring(2);
  if (b == B00001100) stringToDisplay = stringToDisplay.substring(0, 2) + PreZero(value) + stringToDisplay.substring(4);
  if (b == B00110000) stringToDisplay = stringToDisplay.substring(0, 4) + PreZero(value);
}

bool isValidDate()
{
  int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (value[DateYearIndex] % 4 == 0) days[1] = 29;
  if (value[DateDayIndex] > days[value[DateMonthIndex] - 1]) return false;
  else return true;

}

void incrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex) // 12/24 hour mode menu position
      value[menuPosition] = value[menuPosition] + 1; else value[menuPosition] = value[menuPosition] + 12;
    if (value[menuPosition] > maxValue[menuPosition])  value[menuPosition] = minValue[menuPosition];
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1) /*digitalWrite(pinUpperDots, HIGH);*/dotPattern = B10000000; //turn on upper dots
      /*else digitalWrite(pinUpperDots, LOW); */ dotPattern = B00000000; //turn off all dots
    }
    if (menuPosition!=DateFormatIndex) injectDigits(blinkMask, value[menuPosition]);
  }
}

void dicrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex) value[menuPosition] = value[menuPosition] - 1; else value[menuPosition] = value[menuPosition] - 12;
    if (value[menuPosition] < minValue[menuPosition]) value[menuPosition] = maxValue[menuPosition];
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1) /*digitalWrite(pinUpperDots, HIGH);*/ dotPattern = B10000000; //turn on upper dots
      else /*digitalWrite(pinUpperDots, LOW);*/ dotPattern = B00000000; //turn off all dots
    }
    if (menuPosition!=DateFormatIndex) injectDigits(blinkMask, value[menuPosition]);
  }
}

void modesChanger()
{
  if (editMode == true) return;
  static unsigned long lastTimeModeChanged = millis();
  static unsigned long lastTimeAntiPoisoningIterate = millis();
  static int transnumber = 0;
  if (abs(millis() - lastTimeModeChanged) > modesChangePeriod)
  {
    lastTimeModeChanged = millis();
    if (transnumber == 0) {
      menuPosition = DateIndex;
      modesChangePeriod = dateModePeriod;
    }
    if (transnumber == 1) {
      menuPosition = TemperatureIndex;
      modesChangePeriod = dateModePeriod;
      if (!TempPresent) transnumber = 2;
    }
    if (transnumber == 2) {
      menuPosition = TimeIndex;
      modesChangePeriod = timeModePeriod;
    }
    transnumber++;
    if (transnumber > 2) transnumber = 0;

    if (modeChangedByUser == true)
    {
      menuPosition = TimeIndex;
    }
    modeChangedByUser = false;
  }
  if (abs(millis() - lastTimeModeChanged) < 2000)
  {
    if (abs(millis() - lastTimeAntiPoisoningIterate) > 100)
    {
      lastTimeAntiPoisoningIterate = millis();
      time_t adjustedTime = ukraineTime();
      if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(PreZero(day(adjustedTime)) + PreZero(month()) + PreZero(year() % 1000), getTimeNow());
      if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day(adjustedTime)) + PreZero(month()) + PreZero(year() % 1000) );
    }
  } else
  {
    transactionInProgress = false;
  }
}

String antiPoisoning2(String fromStr, String toStr)
{
  //static bool transactionInProgress=false;
  //byte fromDigits[6];
  static byte toDigits[6];
  static byte currentDigits[6];
  static byte iterationCounter = 0;
  if (!transactionInProgress)
  {
    transactionInProgress = true;
    blankMask = B00000000;
    for (int i = 0; i < 6; i++)
    {
      currentDigits[i] = fromStr.substring(i, i + 1).toInt();
      toDigits[i] = toStr.substring(i, i + 1).toInt();
    }
  }
  for (int i = 0; i < 6; i++)
  {
    if (iterationCounter < 10) currentDigits[i]++;
    else if (currentDigits[i] != toDigits[i]) currentDigits[i]++;
    if (currentDigits[i] == 10) currentDigits[i] = 0;
  }
  iterationCounter++;
  if (iterationCounter == 20)
  {
    iterationCounter = 0;
    transactionInProgress = false;
  }
  String tmpStr;
  for (int i = 0; i < 6; i++)
    tmpStr += currentDigits[i];
  return tmpStr;
}

String updateDateString()
{
  static unsigned long lastTimeDateUpdate = millis()+1001;
  static String DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
  static byte prevoiusDateFormatWas=value[DateFormatIndex];
  if ((abs(millis() - lastTimeDateUpdate) > 1000) || (prevoiusDateFormatWas != value[DateFormatIndex]))
  {
    lastTimeDateUpdate = millis();
    time_t adjustedTime = ukraineTime();
    if (value[DateFormatIndex]==EU_DateFormat) DateString = PreZero(day(adjustedTime)) + PreZero(month(adjustedTime)) + PreZero(year(adjustedTime) % 1000);
      else DateString = PreZero(month(adjustedTime)) + PreZero(day(adjustedTime)) + PreZero(year(adjustedTime) % 1000);
  }
  return DateString;
}

float getTemperature (boolean bTempFormat)
{
  byte TempRawData[2];
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0x44); //send make convert to all devices
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0xBE); //send request to all devices

  TempRawData[0] = ds.read();
  TempRawData[1] = ds.read();
  int16_t raw = (TempRawData[1] << 8) | TempRawData[0];
  if (raw == -1) raw = 0;
  float celsius = (float)raw / 16.0;
  float fDegrees;
  if (!bTempFormat) fDegrees = celsius * 10;
  else fDegrees = (celsius * 1.8 + 32.0) * 10;
  return fDegrees;
}

static unsigned long Last_Time_GPS_Sync=0;
void SyncWithGPS()
{
  
  static bool GPS_Sync_Flag=0;
  if (GPS_Sync_Flag == 0) 
  {
    unsigned long delta = millis() - GPS_Date_Time.GPS_Data_Parsed_time;
    if (delta > 3000) { return; }
    int seconds = delta / 1000 + 1;
    setRTCDateTime(GPS_Date_Time.GPS_hours, GPS_Date_Time.GPS_minutes, GPS_Date_Time.GPS_seconds + seconds, GPS_Date_Time.GPS_day, GPS_Date_Time.GPS_mounth, GPS_Date_Time.GPS_year % 1000, 1);
    GPS_Sync_Flag = 1;
    Last_Time_GPS_Sync=millis();
  }
  else
  {
     if (abs(millis()-Last_Time_GPS_Sync) > 10000) GPS_Sync_Flag=0;
        else GPS_Sync_Flag=1;
  }
}

void GetDataFromSerial1()
{
  if (gpsSerial.available()) {     // If anything comes in Serial1 (pins 0 & 1)
    byte GPS_incoming_byte;
    GPS_incoming_byte=gpsSerial.read();
    GPS_Package[GPS_position]=GPS_incoming_byte;
    GPS_position++;
    if (GPS_position == GPS_BUFFER_LENGTH-1)
    {
      GPS_position=0;
    }
    if (GPS_incoming_byte == 0x0A || GPS_incoming_byte == 0x24) 
    {
      GPS_Package[GPS_position]=0;
      if (ControlCheckSum()) { GPS_Parse_DateTime();}
      if (GPS_incoming_byte == 0x24) {
        GPS_Package[0]=0x24;
        GPS_position=1;
      } else {
        GPS_position=0;
      }
    }   
  }
}

bool GPS_Parse_DateTime()
{
  bool GPSsignal=false;
  if (!((GPS_Package[0] == '$')
       &&(GPS_Package[3] == 'R')
       &&(GPS_Package[4] == 'M')
       &&(GPS_Package[5] == 'C'))) {return false;}
  
  int hh=(GPS_Package[7]-48)*10+GPS_Package[8]-48;
  int mm=(GPS_Package[9]-48)*10+GPS_Package[10]-48;
  int ss=(GPS_Package[11]-48)*10+GPS_Package[12]-48;

  byte GPSDatePos=0;
  int CommasCounter=0;
  for (int i = 12; i < GPS_BUFFER_LENGTH ; i++)  
  {
    if (GPS_Package[i] == ',')
    {
      CommasCounter++; 
      if (CommasCounter==8) 
        {
          GPSDatePos=i+1;
          break;
        }
    }
  }
  
  int dd=(GPS_Package[GPSDatePos]-48)*10+GPS_Package[GPSDatePos+1]-48;
  
  int MM=(GPS_Package[GPSDatePos+2]-48)*10+GPS_Package[GPSDatePos+3]-48;
  int yyyy=2000+(GPS_Package[GPSDatePos+4]-48)*10+GPS_Package[GPSDatePos+5]-48;
  if ( !inRange( yyyy, 2020, 2100 ) ||
    !inRange( MM, 1, 12 ) ||
    !inRange( dd, 1, 31 ) ||
    !inRange( hh, 0, 23 ) ||
    !inRange( mm, 0, 59 ) ||
    !inRange( ss, 0, 59 ) ) {
      return false;
    } else {
      GPS_Date_Time.GPS_hours=hh;
      GPS_Date_Time.GPS_minutes=mm;
      GPS_Date_Time.GPS_seconds=ss;
      GPS_Date_Time.GPS_day=dd;
      GPS_Date_Time.GPS_mounth=MM;
      GPS_Date_Time.GPS_year=yyyy;
      GPS_Date_Time.GPS_Data_Parsed_time=millis();
      return true;
    }
}

uint8_t ControlCheckSum()
{
  uint8_t  CheckSum = 0, MessageCheckSum = 0;   // check sum
  uint16_t i = 1;                // 1 sybol left from '$'

  while (GPS_Package[i]!='*')
  {
    CheckSum^=GPS_Package[i];
    if (++i == GPS_BUFFER_LENGTH) { return 0;} // end of line not found
  }

  if ((i < 84) && (GPS_Package[i + 1] == 'W' && GPS_Package[i + 2] == 'E' && GPS_Package[i + 3] == 'B'))
    return 1;

  if (GPS_Package[++i]>0x40) MessageCheckSum=(GPS_Package[i]-0x37)<<4;  // ASCII codes to DEC convertation 
  else                  MessageCheckSum=(GPS_Package[i]-0x30)<<4;  
  if (GPS_Package[++i]>0x40) MessageCheckSum+=(GPS_Package[i]-0x37);
  else                  MessageCheckSum+=(GPS_Package[i]-0x30);
  
  if (MessageCheckSum != CheckSum) {return 0;} // wrong checksum
  return 1; // all ok!
}

String updateTemperatureString(float fDegrees)
{
  static  unsigned long lastTimeTemperatureString=millis()+1100;
  static String strTemp ="000000";
  /*int delayTempUpdate;
  if (displayNow) delayTempUpdate=0;
    else delayTempUpdate = 1000;*/
  if (abs(millis() - lastTimeTemperatureString) > 1000)
  {
    lastTimeTemperatureString = millis();
    int iDegrees = round(fDegrees);
    if (value[DegreesFormatIndex] == CELSIUS)
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees)) + "0";
    }else
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees)/10) + "00";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees)/10) + "00";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees)/10) + "00";
    }

    return strTemp;
  }
  return strTemp;
}


boolean inRange( int no, int low, int high )
{
if ( no < low || no > high ) 
{
  return false;
}
return true;
}

bool configureUblox(byte *settingsArrayPointer) {
  byte gpsSetSuccess = 0;
//  Serial.println(F("Configuring u-Blox GPS initial state..."));

  //Generate the configuration string for Navigation Mode
  byte setNav[] = {0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, *settingsArrayPointer, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  calcChecksum(&setNav[2], sizeof(setNav) - 4);

  //Generate the configuration string for Data Rate
  byte setDataRate[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, settingsArrayPointer[1], settingsArrayPointer[2], 0x01, 0x00, 0x01, 0x00, 0x00, 0x00};
  calcChecksum(&setDataRate[2], sizeof(setDataRate) - 4);

  //Generate the configuration string for Baud Rate
  byte setPortRate[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, settingsArrayPointer[3], settingsArrayPointer[4], settingsArrayPointer[5], 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  calcChecksum(&setPortRate[2], sizeof(setPortRate) - 4);

  byte setGGA[] = {11, 0xB5 ,0x62 ,0x06 ,0x01 ,0x03 ,0x00 ,0xF0 ,0x00 ,0x00 ,0xFA ,0x0F};
  byte setGLL[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x2B};
  byte setGSA[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x32};
  byte setGSV[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x39};
  byte setRMC[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x40};
  byte setVTG[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x46};

  gpsSerial.begin(9600);

  while(gpsSetSuccess < 3) {
//    Serial.print(F("Setting Navigation Mode... "));
    sendUBX(&setNav[0], sizeof(setNav));  //Send UBX Packet
    gpsSetSuccess += getUBX_ACK(&setNav[2]); //Passes Class ID and Message ID to the ACK Receive function
    if (gpsSetSuccess == 5) {
      gpsSetSuccess -= 4;
      delay(1500);
      byte lowerPortRate[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x80, 0x25, 0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0xB5};
      sendUBX(lowerPortRate, sizeof(lowerPortRate));
      gpsSerial.begin(4800);
      delay(2000);
    }
    if(gpsSetSuccess == 6) gpsSetSuccess -= 4;
    if (gpsSetSuccess == 10) { gpsStatus[0] = true; }
  }
//  if (gpsSetSuccess == 3) Serial.println(F("Navigation mode configuration failed."));
  gpsSetSuccess = 0;
  while(gpsSetSuccess < 3) {
    Serial.print(F("Setting Data Update Rate... "));
    sendUBX(&setDataRate[0], sizeof(setDataRate));  //Send UBX Packet
    gpsSetSuccess += getUBX_ACK(&setDataRate[2]); //Passes Class ID and Message ID to the ACK Receive function
    if (gpsSetSuccess == 10) gpsStatus[1] = true;
    if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
  }
//  if (gpsSetSuccess == 3) Serial.println(F("Data update mode configuration failed."));
  gpsSetSuccess = 0;


  while(gpsSetSuccess < 3 && settingsArrayPointer[6] == 0x00) {
//    Serial.print(F("Deactivating NMEA GLL Messages "));
    sendUBX(setGLL, sizeof(setGLL));
    gpsSetSuccess += getUBX_ACK(&setGLL[2]);
    if (gpsSetSuccess == 10) gpsStatus[2] = true;
    if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
  }
//  if (gpsSetSuccess == 3) Serial.println(F("NMEA GLL Message Deactivation Failed!"));
  gpsSetSuccess = 0;

  while(gpsSetSuccess < 3 && settingsArrayPointer[7] == 0x00) {
//    Serial.print(F("Deactivating NMEA GSA Messages "));
    sendUBX(setGSA, sizeof(setGSA));
    gpsSetSuccess += getUBX_ACK(&setGSA[2]);
    if (gpsSetSuccess == 10) gpsStatus[3] = true;
    if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
  }
//  if (gpsSetSuccess == 3) Serial.println(F("NMEA GSA Message Deactivation Failed!"));
  gpsSetSuccess = 0;

  while(gpsSetSuccess < 3 && settingsArrayPointer[8] == 0x00) {
//    Serial.print(F("Deactivating NMEA GSV Messages "));
    sendUBX(setGSV, sizeof(setGSV));
    gpsSetSuccess += getUBX_ACK(&setGSV[2]);
    if (gpsSetSuccess == 10) gpsStatus[4] = true;
    if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
  }
  if (gpsSetSuccess == 3) Serial.println(F("NMEA GSV Message Deactivation Failed!"));
  gpsSetSuccess = 0;

  while(gpsSetSuccess < 3 && settingsArrayPointer[9] == 0x00) {
//    Serial.print(F("Deactivating NMEA RMC Messages "));
    sendUBX(setRMC, sizeof(setRMC));
    gpsSetSuccess += getUBX_ACK(&setRMC[2]);
    if (gpsSetSuccess == 10) gpsStatus[5] = true;
    if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
  }
//  if (gpsSetSuccess == 3) Serial.println(F("NMEA RMC Message Deactivation Failed!"));
  gpsSetSuccess = 0;

  while(gpsSetSuccess < 3 && settingsArrayPointer[10] == 0x00) {
//    Serial.print(F("Deactivating NMEA VTG Messages "));
    sendUBX(setVTG, sizeof(setVTG));
    gpsSetSuccess += getUBX_ACK(&setVTG[2]);
    if (gpsSetSuccess == 10) gpsStatus[6] = true;
    if (gpsSetSuccess == 5 | gpsSetSuccess == 6) gpsSetSuccess -= 4;
  }
//  if (gpsSetSuccess == 3) Serial.println(F("NMEA VTG Message Deactivation Failed!"));
  gpsSetSuccess = 0;


//  Serial.print(F("Deactivating NMEA GGA Messages "));
  sendUBX(setGGA, sizeof(setGGA));  
  
  gpsSetSuccess = 0;
  if (settingsArrayPointer[4] != 0x25) {
//    Serial.print(F("Setting Port Baud Rate... "));
    sendUBX(&setPortRate[0], sizeof(setPortRate));
//    Serial.println(F("Success!"));
    delay(500);
  } 

  bool result = true;
  for (int i=0; i<=6; i++) {
    result = result && gpsStatus[i];
  }
  gpsSerial.begin(115200);
  return result;
}

void calcChecksum(byte *checksumPayload, byte payloadSize) {
  byte CK_A = 0, CK_B = 0;
  for (int i = 0; i < payloadSize ;i++) {
    CK_A = CK_A + *checksumPayload;
    CK_B = CK_B + CK_A;
    checksumPayload++;
  }
  *checksumPayload = CK_A;
  checksumPayload++;
  *checksumPayload = CK_B;
}

void sendUBX(byte *UBXmsg, byte msgLength) {
  for(int i = 0; i < msgLength; i++) {
    gpsSerial.write(UBXmsg[i]);
    gpsSerial.flush();
  }
  gpsSerial.println();
  gpsSerial.flush();
}


byte getUBX_ACK(byte *msgID) {
  byte CK_A = 0, CK_B = 0;
  byte incoming_char;
  boolean headerReceived = false;
  unsigned long ackWait = millis();
  byte ackPacket[10] = {0xB5, 0x62, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  int i = 0;
  while (1) {
    if (gpsSerial.available()) {
      incoming_char = gpsSerial.read();
      if (incoming_char == ackPacket[i]) {
        i++;
      }
      else if (i > 2) {
        ackPacket[i] = incoming_char;
        i++;
      }
    }
    if (i > 9) break;
    if ((millis() - ackWait) > 1500) {
//      Serial.println(F("ACK Timeout"));
      return 5;
    }
    if (i == 4 && ackPacket[3] == 0x00) {
//      Serial.println(F("NAK Received"));
      return 1;
    }
  }

  for (i = 2; i < 8 ;i++) {
  CK_A = CK_A + ackPacket[i];
  CK_B = CK_B + CK_A;
  }
  if (msgID[0] == ackPacket[6] && msgID[1] == ackPacket[7] && CK_A == ackPacket[8] && CK_B == ackPacket[9]) {
//    Serial.println(F("Success!"));
//    Serial.print(F("ACK Received! "));
    printHex(ackPacket, sizeof(ackPacket));
    return 10;
        }
  else {
//    Serial.print(F("ACK Checksum Failure: "));
    printHex(ackPacket, sizeof(ackPacket));
    delay(1000);
    return 1;
  }
}

void printHex(uint8_t *data, uint8_t length) // prints 8-bit data in hex
{
  char tmp[length*2+1];
  byte first ;
  int j=0;
  for (byte i = 0; i < length; i++)
  {
    first = (data[i] >> 4) | 48;
    if (first > 57) tmp[j] = first + (byte)7;
    else tmp[j] = first ;
    j++;

    first = (data[i] & 0x0F) | 48;
    if (first > 57) tmp[j] = first + (byte)7;
    else tmp[j] = first;
    j++;
  }
  tmp[length*2] = 0;
  for (byte i = 0, j = 0; i < sizeof(tmp); i++) {
    if (j == 1) {
      j = 0;
    }
    else j++;
  }
}
