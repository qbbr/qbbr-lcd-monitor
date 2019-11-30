/**
 * qbbr-lcd-monitor
 * 
 * Arduino LCD helper/notifier.
 * 
 * @licence MIT
 * @author Sokolov Innokenty <imqbbr@gmail.com>
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include "pitches.h"

#define DEBUG 1

LiquidCrystal_I2C lcd(0x3F, 16, 2); // 0x27|0x20|0x3F

#define DHT_PIN 2
DHT dht(DHT_PIN, DHT11); // DHT11|DHT22

const int buttonPin = 3;
unsigned long buttonPrevMillis = 0;

boolean backlightFlag = true;
String request;

typedef void (*FnMode)(void);
FnMode lcdModes[] = {&setModeHello, &setModeLine, &setModeTemp};
int lcdModeIndex = 0;

String line1 = "";
String line2 = "";

const int buzzerPin = 4;

unsigned long tempPrevMillis = 0;

void setup()
{
  Serial.begin(9600);

  dht.begin();

  lcd.init(); // инициализация дисплея
  lcd.setBacklight(backlightFlag); // подсветка
  setModeByIndex(lcdModeIndex);

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
  setText("      QBBR", 0);
  setText("LCD Monitor v1.1", 1);
}

void setModeLine()
{
  setText(line1, 0);
  setText(line2, 1);
}

void setModeTemp()
{
  float temp = getTemperature();
  setText("Temp: " + String(temp) + " C", 0);
  float hum = getHumidity();
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
      float temp = getTemperature();
      debug(String(temp));
    } else if (request == "get-hum") {
      float hum = getHumidity();
      debug(String(hum));
    }
  }

  clearRequest();
}

void clearRequest()
{
  request = "";
}

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

int delays[] = {20, 40, 10, 40, 30, 60};

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

int melody[] = {NOTE_C4, NOTE_A3/*, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4*/};
int noteDurations[] = {8, 4}; // 4 = quarter note, 8 = eighth note, etc.
int noteCount = 2;

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

float getTemperature()
{
  float t = dht.readTemperature();

  if (isnan(t)) {
    printDHTError();
  }

  return t;
}

float getHumidity()
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

void debug(String msg)
{
#ifdef DEBUG
  Serial.println("[D] " + msg);
#endif
}
