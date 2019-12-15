/**
   qbbr-lcd-monitor

   Arduino LCD helper/notifier/station.

   @licence MIT
   @author Sokolov Innokenty <imqbbr@gmail.com>
*/

#include <math.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>

//#define QBBR_DEBUG 1 // comment for disable
#define QBBR_LCD_SENSORS_DATA_UPDATE_PERIOD 5000
#define QBBR_PRINT_JSON_DATA_DELAY 5000
#define QBBR_DEVICE_NAME "qbbr-lcd-monitor"
#define QBBR_DEVICE_VERSION 1.7

// LCD
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F, 16, 2); // 0x27|0x20|0x3F
const int buttonPin = 3; // screen next btn
unsigned long buttonClickPrevMillis = 0;
boolean backlightFlag = true;
typedef void (*ScreenList)(void);
ScreenList screenList[] = {&setScreen01_Hello, &setScreen02_SerialInputData, &setScreen03_DHTData, &setScreen04_BMPData};
int screenIndex = 0; // setScreen01_Hello
String serialInputLine1 = "";
String serialInputLine2 = "";
unsigned long lcdSensorsDataUpdatePrevMillis = 0;

// BMP
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// DHT
#include "DHT.h"
#define DHT_PIN 2
DHT dht(DHT_PIN, DHT11); // DHT11|DHT22

// Buzzer
#include "pitches.h"
const int buzzerPin = 8;
int melody[] = {NOTE_C4, NOTE_A3/*, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4*/};
int noteDurations[] = {8, 4}; // 4 = quarter note, 8 = eighth note, etc.
const int noteCount = 2;

// Relay
const int relayPin = 7;
const bool relayRevertLogic = true;
const bool relayDefaultSwitchOn = false;
const int button2Pin = 4;
unsigned long button2ClickPrevMillis = 0;

// Termistor for outside temp
const int termistorPin = 3; // A3

// other
unsigned long printJsonDataPrevMillis = 0;


void debug(String msg)
{
#ifdef QBBR_DEBUG
  Serial.println("[D] " + msg);
#endif
}

void setup()
{
  Serial.begin(9600);

  dht.begin();

  lcd.init();
  lcd.setBacklight(backlightFlag);
  setScreenByIndex(screenIndex);

  if (!bmp.begin()) {
    Serial.print("Ooops, no BMP085 detected ... Check your I2C/IIC ADDR!");
    while (1);
  }

  pinMode(buttonPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);

  if (relayRevertLogic) {
    digitalWrite(relayPin, HIGH); // HIGH for default switch off
  }

  if (relayDefaultSwitchOn) {
    relayOn();
  }
}

void loop()
{
  // read serial data
  if (Serial.available() > 0) {
    delay(10); // small delay to allow input buffer to fill
    char c = Serial.read();

    if (c == ';') { // end packet
      parseRequest();
    } else if (c == '.') { // start packet
      clearRequest();
    } else {
      putToRequest(c);
    }
  }

  // btn click - screen next
  if (digitalRead(buttonPin) == HIGH) {
    if (millis() - buttonClickPrevMillis > 300) {
      debug("event - btn clicked");
      lcd.clear();
      screenNext();

      buttonClickPrevMillis = millis();
    }
  }

  // btn2 click - relay toggle
  if (digitalRead(button2Pin) == HIGH) {
    if (millis() - button2ClickPrevMillis > 300) {
      debug("event - btn2 clicked");
      relayToggle();

      button2ClickPrevMillis = millis();
    }
  }

  // print json data
  if (millis() - printJsonDataPrevMillis > QBBR_PRINT_JSON_DATA_DELAY || printJsonDataPrevMillis == 0) {
    print2SerialJsonData();

    printJsonDataPrevMillis = millis();
  }

  // update only on screen with sensors (setScreen03_DHTData, setScreen04_BMPData)
  if (screenIndex == 2 || screenIndex == 3) {
    if (millis() - lcdSensorsDataUpdatePrevMillis > QBBR_LCD_SENSORS_DATA_UPDATE_PERIOD) {
      debug("update data on screen");
      setScreenByIndex(screenIndex);

      lcdSensorsDataUpdatePrevMillis = millis();
    }
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

  JsonObject nodeOutside = root.createNestedObject("outside");
  nodeOutside["temperature"] = getTermistorTemperature(); // C

#ifdef QBBR_DEBUG
  serializeJsonPretty(root, Serial);
#else
  serializeJson(root, Serial);
#endif

  Serial.println();
}


/*
   Screen
*/

void screenNext()
{
  screenIndex++;
  if (screenIndex >= (sizeof(screenList) / sizeof(screenList[0]))) {
    screenIndex = 0;
  }
  setScreenByIndex(screenIndex);
}

void setScreenByIndex(int index)
{
  debug("set screen by index: " + String(index));
  lcd.clear();
  screenList[index]();
}

/* Screen 01 */
void setScreen01_Hello()
{
  setText(QBBR_DEVICE_NAME, 0);
  setText("version " + String(QBBR_DEVICE_VERSION), 1);
}

/* Screen 02 */
void setScreen02_SerialInputData()
{
  setText(serialInputLine1, 0);
  setText(serialInputLine2, 1);
}

/* Screen 03 */
void setScreen03_DHTData()
{
  float temp = getDHTTemperature();
  float hum = getDHTHumidity();
  setText(String(temp) + " 'C " + '(' + String(hum, 0) + " " + '%' + ')', 0);
  float outsideTemp = getTermistorTemperature();
  setText("Out: " + String(outsideTemp) + " 'C", 1);
}

/* Screen 04 */
void setScreen04_BMPData()
{
  float *data = getBmpData(); // [hPa, C, m]
  setText("Temp: " + String(data[1]) + " C", 0);
  setText(String((int) data[0]) + " hPa / " + String((int) data[2]) + " m", 1);
}


/*
   request handler
*/

String request;

void parseRequest()
{
  if (request.length() == 0) {
    return;
  }

  if (request.indexOf(":") != -1) { // Set request
    String method = request.substring(0, request.indexOf(":"));
    String args = request.substring(request.indexOf(":") + 1);
    debug("Method: " + method);
    debug("Args: " + args);

    if (method == "l1") {
      serialInputLine1 = args;
      setText(serialInputLine1, 0);
    } else if (method == "l2") {
      serialInputLine2 = args;
      setText(serialInputLine2, 1);
    } else if (method == "fn") {
      if (args == "screen-clear") {
        lcd.clear();
      } else if (args == "screen-blink") {
        screenBlinkBacklight();
      } else if (args == "screen-blink-long") {
        screenBlinkLongBacklight();
      } else if (args == "screen-next") {
        screenNext();
      } else if (args == "buzzer-tone") {
        buzzerTone();
      } else if (args == "i2c-scan") {
        i2cScan();
      } else if (args == "relay-toggle") {
        relayToggle();
      } else if (args == "relay-on") {
        relayOn();
      } else if (args == "relay-off") {
        relayOff();
      }
    }
  } else { // Get request
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

void putToRequest(char c)
{
  request += c;
}

void clearRequest()
{
  request = "";
}


/*
   LCD
*/

void setText(String text, int line)
{
  lcd.setCursor(0, line);
  lcd.print(text);
}

void screenBlinkBacklight()
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

void screenBlinkLongBacklight()
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
   Buzzer
*/

void buzzerTone()
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


/*
   I2C/IIC scanner
*/

void i2cScan()
{
  byte error, address;
  int devices;

  Serial.println("[I2C] Scanning...");

  devices = 0;
  for (address = 1; address < 127; address++) {
    // https://www.arduino.cc/en/Reference/WireEndTransmission
    // The i2c_scanner uses the return value of the Write.endTransmisstion
    // to see if a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0 || error == 4) {
      if (error == 0) { // success
        Serial.print("[I2C] device found at address 0x");
        devices++;
      } else if (error == 4) { // other error
        Serial.print("[I2C][E] Unknown error at address 0x");
      }

      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }

  if (devices == 0) {
    Serial.println("[I2C] No devices");
  } else {
    Serial.println("[I2C] done");
  }
}


/*
   Relay
*/

void relayToggle()
{
  digitalWrite(relayPin, !digitalRead(relayPin));
}

void relayOn()
{
  digitalWrite(relayPin, relayRevertLogic ? LOW : HIGH);
}

void relayOff()
{
  digitalWrite(relayPin, relayRevertLogic ? HIGH : LOW);
}


/**
   Analog termistor (NTC 10K 0.5%)
*/
float getTermistorTemperature()
{
  int input;
  float temp;

  input = analogRead(termistorPin);
  temp = log(((10240000 / input) - 10000));
  temp = 1 / (0.001129148 + (0.000234125 * temp) + (0.0000000876741 * temp * temp * temp));
  temp = temp - 273.15;

  return temp;
}
