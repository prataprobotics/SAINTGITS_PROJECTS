#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>

#include <can.h>
#include <mcp2515.h>

#include <CanHacker.h>
#include <CanHackerLineReader.h>
#include <lib.h>

#include <SPI.h>

const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
tmElements_t tm;

static const int RXPin = 4, TXPin = 3; //gps pin
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

const int SPI_CS_PIN = 10;
const int INT_PIN = 2;

const int SS_RX_PIN = 12;
const int SS_TX_PIN = 11;

CanHackerLineReader *lineReader = NULL;
CanHacker *canHacker = NULL;

SoftwareSerial softwareSerial(SS_RX_PIN, SS_TX_PIN); //rtc

void setup()
{
  Serial.begin(9600);
  ss.begin(GPSBaud);

  bool parse = false;
  bool config = false;

  if (getDate(__DATE__) && getTime(__TIME__)) {
    parse = true;
    if (RTC.write(tm)) {
      config = true;
    }
  }

  while (!Serial) ; // wait for Arduino Serial Monitor
  delay(200);
  if (parse && config) {
    Serial.print("DS1307 configured Time = ");
    Serial.print(__TIME__);
    Serial.print(", Date = ");
    Serial.println(__DATE__);
  } else if (parse) {
    Serial.println("DS1307 Communication Error :-{");
    Serial.println("Please check your circuitry");
  } else {
    Serial.print("Could not parse info from the compiler, Time = \"");
    Serial.print(__TIME__);
    Serial.print("\", Date = \"");
    Serial.print(__DATE__);
    Serial.println("\"");
  }

  while (!Serial) ; // wait for serial
  delay(200);
  Serial.println("DS1307RTC Read Test");
  Serial.println("--------------");
  Serial.println();

  Serial.begin(9600);
  while (!Serial);
  SPI.begin();
  softwareSerial.begin(9600);

  Stream *interfaceStream = &Serial;
  Stream *debugStream = &softwareSerial;

  canHacker = new CanHacker(interfaceStream, debugStream, SPI_CS_PIN);
  //canHacker->enableLoopback(); // uncomment this for loopback
  lineReader = new CanHackerLineReader(canHacker);

  pinMode(INT_PIN, INPUT);
}

void loop()
{
  updateGPS();
  displayGPSLocation();
  displayRTCData();

  CanHacker::ERROR error;

  if (digitalRead(INT_PIN) == LOW) {
    error = canHacker->processInterrupt();
    handleError(error);
  }

  error = lineReader->process();
  handleError(error);

  if (canHacker->receiveCanFrame(NULL)) {
    struct can_frame frame;
    canHacker->receiveCanFrame(&frame);
    Serial.print("ID: ");
    Serial.print(frame.can_id, HEX);
    Serial.print(" Data: ");
    for (int i = 0; i < frame.can_dlc; i++) {
      Serial.print(frame.data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  delay(1000);
}

void updateGPS()
{
  while (ss.available() > 0)
  {
    if (gps.encode(ss.read()))
    {
      // GPS data is processed
    }
  }

  if (millis() > 300000 && gps.charsProcessed() < 10)
  {
    Serial.println("No GPS detected: check wiring.");
    while (true);
  }
}

void displayGPSLocation()
{
  Serial.println();
  Serial.print("Location: ");
  if (gps.location.isValid())
  {    
    Serial.print(gps.location.lat(), 6);
    Serial.print(",");
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print("INVALID");
  }

  Serial.print("  Date: ");
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print("/");
    Serial.print(gps.date.day());
    Serial.print("/");
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print("INVALID");
  }
  Serial.println();
}

void displayRTCData()
{
  tmElements_t rtcTime;
  if (RTC.read(rtcTime))
  {
    Serial.print("RTC Time: ");
    print2digits(rtcTime.Hour);
    Serial.write(':');
    print2digits(rtcTime.Minute);
    Serial.write(':');
    print2digits(rtcTime.Second);
    Serial.print(", Date (D/M/Y): ");
    Serial.print(rtcTime.Day);
    Serial.write('/');
    Serial.print(rtcTime.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(rtcTime.Year));
    Serial.println();
  }
  else
  {
    if (RTC.chipPresent())
    {
      Serial.println("The DS1307 is stopped. Please run the SetTime example to initialize the time and begin running.");
      Serial.println();
    }
    else
    {
      Serial.println("DS1307 read error! Please check the circuitry.");
      Serial.println();
    }
    delay(9000);
  }
}

void print2digits(int number)
{
  if (number >= 0 && number < 10)
  {
    Serial.write('0');
  }
  Serial.print(number);
}

bool getTime(const char *str)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  tm.Hour = Hour;
  tm.Minute = Min;
  tm.Second = Sec;
  return true;
}

bool getDate(const char *str)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3) return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++)
  {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  if (monthIndex >= 12) return false;
  tm.Day = Day;
  tm.Month = monthIndex + 1;
  tm.Year = CalendarYrToTm(Year);
  return true;
}

void handleError(const CanHacker::ERROR error) {
  switch (error) {
    case CanHacker::ERROR_OK:
    case CanHacker::ERROR_UNKNOWN_COMMAND:
    case CanHacker::ERROR_NOT_CONNECTED:
    case CanHacker::ERROR_MCP2515_ERRIF:
    case CanHacker::ERROR_INVALID_COMMAND:
      return;

    default:
      break;
  }

  softwareSerial.print("Failure (code ");
  softwareSerial.print((int)error);
  softwareSerial.println(")");

  digitalWrite(SPI_CS_PIN, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);

  while (1) {
    int c = (int)error;
    for (int i = 0; i < c; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
    }

    delay(2000);
  }
}


