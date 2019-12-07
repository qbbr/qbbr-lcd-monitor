/**
   qbbr-lcd-monitor

   Arduino LCD helper/notifier/station.

   @licence MIT
   @author Sokolov Innokenty <imqbbr@gmail.com>
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>

//#define QBBR_DEBUG 1 // comment for disable
#define QBBR_PRINT_JSON_DATA_DELAY 5000
#define QBBR_DEVICE_NAME "qbbr-lcd-monitor"
#define QBBR_DEVICE_VERSION 1.2

// LCD
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F, 16, 2); // 0x27|0x20|0x3F
const int buttonPin = 3; // mode change btn
unsigned long buttonPrevMillis = 0;
boolean backlightFlag = true;
typedef void (*FnMode)(void);
FnMode lcdModes[] = {&setModeHello, &setModeLine, &setModeTemp};
int lcdModeIndex = 0;
String line1 = "";
String line2 = "";

// BMP
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// DHT
#include "DHT.h"
#define DHT_PIN 2
DHT dht(DHT_PIN, DHT11); // DHT11|DHT22

// tone fn
#include "pitches.h"
const int buzzerPin = 4;
int melody[] = {NOTE_C4, NOTE_A3/*, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4*/};
int noteDurations[] = {8, 4}; // 4 = quarter note, 8 = eighth note, etc.
const int noteCount = 2;

String request;
unsigned long printPrevMillis = 0;

void setup()
{
  Serial.begin(9600);

  dht.begin();

  lcd.init(); // инициализация дисплея
  lcd.setBacklight(backlightFlag); // подсветка
  setModeByIndex(lcdModeIndex);

  if (!bmp.begin()) { // BMP сенсор
    Serial.print("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }

  pinMode(buttonPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
}

void loop()
{
  if (Serial.available() > 0) {
    delay(10); // small delay to allow input buffer to fill
    char c = Serial.read();

    if (c == ';') { // end packet
      parseRequest();
    } else if (c == '.') { // start packet
      clearRequest();
    } else {
      request += c;
    }
  }

  if (digitalRead(buttonPin) == HIGH) {
    if (millis() - buttonPrevMillis > 300) {
      debug("event - btn clicked");
      lcd.clear();
      lcdModeChange();

      buttonPrevMillis = millis();
    }
  }

  // json data
  if (millis() - printPrevMillis > QBBR_PRINT_JSON_DATA_DELAY) {
    print2SerialJsonData();

    printPrevMillis = millis();
  }
}

void print2SerialJsonData()
{
  StaticJsonDocument<200> doc;

  JsonObject root = doc.to<JsonObject>();
  root["device"] = QBBR_DEVICE_NAME;
  root["version"] = QBBR_DEVICE_VERSION;

  JsonObject nodeBMP = root.createNestedObject("bmp");
  float *data = getBmpData();
  nodeBMP["pressure"] = data[0]; // hPa
  nodeBMP["temperature"] = data[1]; // C
  nodeBMP["altitude"] = data[2]; // m

  JsonObject nodeDHT = root.createNestedObject("dht");
  nodeDHT["temperature"] = getDHTTemperature(); // C
  nodeDHT["humidity"] = getDHTHumidity(); // %

#ifdef QBBR_DEBUG
  serializeJsonPretty(root, Serial);
#else
  serializeJson(root, Serial);
#endif

  Serial.println();
}

void lcdModeChange()
{
  lcdModeIndex++;

  if (lcdModeIndex >= (sizeof(lcdModes) / sizeof(lcdModes[0]))) {
    lcdModeIndex = 0;
  }

  setModeByIndex(lcdModeIndex);
}

void setModeByIndex(int index)
{
  debug("set mode by index: " + String(index));
  lcdModes[index]();
}

void setModeHello()
{
  setText(QBBR_DEVICE_NAME, 0);
  setText("version " + String(QBBR_DEVICE_VERSION), 1);
}

void setModeLine()
{
  setText(line1, 0);
  setText(line2, 1);
}

void setModeTemp()
{
  float temp = getDHTTemperature();
  setText("Temp: " + String(temp) + " C", 0);
  float hum = getDHTHumidity();
  setText("Hum: " + String(hum) + " %", 1);
}

void parseRequest()
{
  if (request.length() == 0) {
    return;
  }

  if (request.indexOf(":") != -1) {
    /* Set request */

    String method = request.substring(0, request.indexOf(":"));
    String args = request.substring(request.indexOf(":") + 1);
    //debug("Method: " + method);
    //debug("Args: " + args);

    if (method == "l1") {
      line1 = args;
      setText(args, 0);
    } else if (method == "l2") {
      line2 = args;
      setText(args, 1);
    } else if (method == "fn") {
      if (args == "clear") {
        lcd.clear();
      } else if (args == "blink") {
        blinkBacklight();
      } else if (args == "blink-long") {
        blinkLongBacklight();
      } else if (args == "tone") {
        tonePlay();
      }
    }
  } else {
    /* Get request */

    if (request == "get-temp") {
      float temp = getDHTTemperature();
      debug(String(temp));
    } else if (request == "get-hum") {
      float hum = getDHTHumidity();
      debug(String(hum));
    }
  }

  clearRequest();
}

void clearRequest()
{
  request = "";
}

void debug(String msg)
{
#ifdef QBBR_DEBUG
  Serial.println("[D] " + msg);
#endif
}


/*
   LCD
*/

void setText(String text, int line)
{
  lcd.setCursor(0, line);
  lcd.print(text);
}

void blinkBacklight()
{
  boolean backlightFlagBefore = backlightFlag;

  lcd.setBacklight(true);
  delay(50);
  lcd.setBacklight(false);
  delay(50);

  backlightFlag = backlightFlagBefore;
  lcd.setBacklight(backlightFlag);
}

const int delays[] = {20, 40, 10, 40, 30, 60};

void blinkLongBacklight()
{
  boolean backlightFlagBefore = backlightFlag;

  for (int i = 0; i < sizeof(delays); i++) {
    backlightFlag = !backlightFlag;
    lcd.setBacklight(backlightFlag);
    delay(delays[i]);
  }

  backlightFlag = backlightFlagBefore;
  lcd.setBacklight(backlightFlag);
}


/*
   BMP Sensor
*/

float *getBmpData()
{
  static float data[3];
  sensors_event_t event;
  bmp.getEvent(&event);

  if (event.pressure) {
    data[0] = event.pressure;

    float temperature;
    bmp.getTemperature(&temperature);
    data[1] = temperature;

    /* Then convert the atmospheric pressure, SLP and temp to altitude    */
    /* Update this next line with the current SLP for better results      */
    float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
    data[2] = bmp.pressureToAltitude(seaLevelPressure, event.pressure, temperature);
  } else {
    debug("BMP180 Sensor error");
  }

  return data;
}


/*
   DHT Sensor
*/

float getDHTTemperature()
{
  float t = dht.readTemperature();

  if (isnan(t)) {
    printDHTError();
  }

  return t;
}

float getDHTHumidity()
{
  float h = dht.readHumidity();

  if (isnan(h)) {
    printDHTError();
  }

  return h;
}

void printDHTError()
{
  debug("something wrong with DHT sensor.");
}


/*
   tone fn
*/

void tonePlay()
{
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < noteCount; thisNote++) {
    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(buzzerPin, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(buzzerPin);
  }
}
