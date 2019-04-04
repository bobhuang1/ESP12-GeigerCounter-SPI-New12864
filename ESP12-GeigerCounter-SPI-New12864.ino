#include <ESP8266WiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>
#include <stdio.h>
#include <time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <Timezone.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <Wire.h>
#include "FS.h"
#include "GarfieldCommon.h"

#define DEBUG
#define SERIAL_NUMBER 601
//#define USE_WIFI_MANAGER     // disable to NOT use WiFi manager, enable to use
#define USE_HIGH_ALARM       // disable - LOW alarm sounds, enable - HIGH alarm sounds

#define BUTTONPIN   4
#define GEIGERPIN   2
#define ALARMPIN 5

#if SERIAL_NUMBER == 601
#define DISPLAY_TYPE 2   // 1-BIG 12864, 2-MINI 12864, 3-New Big BLUE 12864, to use 3, you must change u8x8_d_st7565.c as well!!!, 4- New BLUE 12864-ST7920
#endif

const String WDAY_NAMES[] = { "星期天", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六" };
#define LOG_PERIOD 20000 //Logging period in milliseconds
#define MINUTE_PERIOD 60000


String Location = SERIAL_NUMBER + " Default";
String Token = "Token";
int Resistor = 80000;
bool dummyMode = false;
bool backlightOffMode = false;
bool sendAlarmEmail = false;
String alarmEmailAddress = "Email";
int displayContrast = 128;
int displayMultiplier = 100;
int displayBias = 0;
int displayMinimumLevel = 1;
int displayMaximumLevel = 1023;
int temperatureMultiplier = 100;
int temperatureBias = 0;
int humidityMultiplier = 100;
int humidityBias = 0;

#if DISPLAY_TYPE == 3
#define BIGBLUE12864
#endif

#if DISPLAY_TYPE == 1
U8G2_ST7565_LM6059_F_4W_SW_SPI display(U8G2_R2, /* clock=*/ 14, /* data=*/ 12, /* cs=*/ 13, /* dc=*/ 15, /* reset=*/ 16); // U8G2_ST7565_LM6059_F_4W_SW_SPI
#define BACKLIGHTPIN 0
#endif

#if DISPLAY_TYPE == 2
U8G2_ST7565_64128N_F_4W_SW_SPI display(U8G2_R0, /* clock=*/ 14, /* data=*/ 12, /* cs=*/ 13, /* dc=*/ 15, /* reset=*/ 16); // U8G2_ST7565_64128N_F_4W_SW_SPI
#define BACKLIGHTPIN 0
#endif

#if DISPLAY_TYPE == 3
U8G2_ST7565_64128N_F_4W_SW_SPI display(U8G2_R2, /* clock=*/ 14, /* data=*/ 12, /* cs=*/ 13, /* dc=*/ 15, /* reset=*/ 16); // U8G2_ST7565_64128N_F_4W_SW_SPI
#define BACKLIGHTPIN 0
#endif

#if DISPLAY_TYPE == 4
U8G2_ST7920_128X64_F_SW_SPI display(U8G2_R2, /* clo  ck=*/ 14 /* A4 */ , /* data=*/ 12 /* A2 */, /* CS=*/ 16 /* A3 */, /* reset=*/ U8X8_PIN_NONE); // 16, U8X8_PIN_NONE
#define BACKLIGHTPIN 15
#endif

volatile unsigned long counts = 0;                       // Tube events
unsigned long cpm = 0;                                   // CPM
unsigned long previousMillis;                            // Time measurement

time_t nowTime;
const String degree = String((char)176);
int lightLevel[10];
int draw_state = 1;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
const unsigned long debounceDelay = 30;    // the debounce time; increase if the output flickers

#if DISPLAY_TYPE == 1
#define LONGBUTTONPUSH 30
#endif

#if DISPLAY_TYPE == 2
#define LONGBUTTONPUSH 80
#endif

#if DISPLAY_TYPE == 3
#define LONGBUTTONPUSH 40
#endif

#if DISPLAY_TYPE == 4
#define LONGBUTTONPUSH 40
#endif

int buttonPushCounter = 0;
int majorMode = 0; // 0-Clock Mode, 1-Math Mode, 2-80 Poems, 3-300Poems-not possible out of memory

int questionCount = 0;
const int questionTotal = 100;
int currentMode = 0; // 0 - show question, 1 - show answer
String currentQuestion = "";
String currentAnswer = "";

int lineCount = 0;
int lineTotal = 1;
int poemCount = 1;
const int poemTotal = 10;
#define TOTAL_POEMS  96 // Total number of poems
#define MAXIMUM_POEM_SIZE 11 //11 for 80 poems, 121 for 300 poems

String poemText[MAXIMUM_POEM_SIZE];
int currentPoem = 1;
bool readPoem = false;


void geigerHandler() { // Captures count of events from Geiger counter board
  counts++;
}

void setup() {
  delay(100);
  Serial.begin(115200);
#ifdef DEBUG
  Serial.println("Begin");
#endif
  initializeBackLightArray(lightLevel, BACKLIGHTPIN);
  adjustBackLightSub();

  pinMode(BUTTONPIN, INPUT);
  pinMode(GEIGERPIN, INPUT);
  pinMode(ALARMPIN, OUTPUT);
  noBeep(ALARMPIN,
#ifdef USE_HIGH_ALARM
         true
#else
         false
#endif
        );
  listSPIFFSFiles(); // Lists the files so you can see what is in the SPIFFS

  display.begin();
  display.setFontPosTop();
  setContrastSub();

  display.clearBuffer();
  display.drawXBM(31, 0, 66, 64, garfield);
  display.sendBuffer();
  shortBeep(ALARMPIN,
#ifdef USE_HIGH_ALARM
            true
#else
            false
#endif
           );
  delay(1000);

  drawProgress("Backlight Level", "Test");

  selfTestBacklight(BACKLIGHTPIN);

  drawProgress("连接WIFI中,", "请稍等...");

  connectWIFI(
#ifdef USE_WIFI_MANAGER
    true
#else
    false
#endif
  );

  if (WiFi.status() != WL_CONNECTED) ESP.restart();

  // Get time from network time service
#ifdef DEBUG
  Serial.println("WIFI Connected");
#endif
  drawProgress("连接WIFI成功,", "正在同步时间...");
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  writeBootWebSite(SERIAL_NUMBER);
  readValueWebSite(SERIAL_NUMBER, Location, Token, Resistor, dummyMode, backlightOffMode, sendAlarmEmail, alarmEmailAddress, displayContrast, displayMultiplier, displayBias, displayMinimumLevel, displayMaximumLevel, temperatureMultiplier, temperatureBias, humidityMultiplier, humidityBias);
  setContrastSub();
  Serial.print("Location: ");
  Serial.println(Location);
  Serial.print("Token: ");
  Serial.println(Token);
  Serial.print("Resistor: ");
  Serial.println(Resistor);
  Serial.print("dummyMode: ");
  Serial.println(dummyMode);
  Serial.print("backlightOffMode: ");
  Serial.println(backlightOffMode);
  Serial.print("sendAlarmEmail: ");
  Serial.println(sendAlarmEmail);
  Serial.print("alarmEmailAddress: ");
  Serial.println(alarmEmailAddress);
  Serial.print("displayContrast: ");
  Serial.println(displayContrast);
  Serial.print("displayMultiplier: ");
  Serial.println(displayMultiplier);
  Serial.print("displayBias: ");
  Serial.println(displayBias);
  Serial.print("displayMinimumLevel: ");
  Serial.println(displayMinimumLevel);
  Serial.print("displayMaximumLevel: ");
  Serial.println(displayMaximumLevel);
  Serial.print("temperatureMultiplier: ");
  Serial.println(temperatureMultiplier);
  Serial.print("temperatureBias: ");
  Serial.println(temperatureBias);
  Serial.print("humidityMultiplier: ");
  Serial.println(humidityMultiplier);
  Serial.print("humidityBias: ");
  Serial.println(humidityBias);
  Serial.println("");
  drawProgress("同步时间成功,", "正在启动中...");
  interrupts();                                                            // Enable interrupts
  attachInterrupt(digitalPinToInterrupt(GEIGERPIN), geigerHandler, FALLING); // Define interrupt on falling edge
}

void adjustBackLightSub() {
  adjustBacklight(lightLevel, BACKLIGHTPIN, displayBias, displayMultiplier);
}

void detectButtonPush() {
  int reading;
  reading = digitalRead(BUTTONPIN);
  if (reading == HIGH)
  {
    buttonPushCounter++;
  }
  else
  {
    buttonPushCounter = 0;
  }
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    if (reading != buttonState)
    {
      buttonState = reading;
      if (buttonState == HIGH)
      {
        shortBeep(ALARMPIN,
#ifdef USE_HIGH_ALARM
                  true
#else
                  false
#endif
                 );
        if (majorMode == 0)
        {
        }
        else if (majorMode == 1)
        {
          if (currentMode == 0)
          {
            currentMode = 1;
          }
          else
          {
            currentMode = 0;
            currentQuestion = generateMathQuestion(currentAnswer);
            questionCount++;
            if (questionCount >= questionTotal + 1)
            {
              questionCount = 1;
            }
          }
        }
        else if (majorMode == 2)
        {
          lineCount++;
          if (lineCount >= lineTotal)
          {
            lineCount = 0;
            currentPoem = random(1, TOTAL_POEMS + 1);
            readPoem = true;
            poemCount++;
            if (poemCount >= poemTotal + 1)
            {
              poemCount = 0;
            }
          }
        }
      }
      else
      {
        buttonPushCounter = 0;
      }
    }
  }
  lastButtonState = reading;
}

void setContrastSub() {
  if (displayContrast > 0)
  {
    display.setContrast(displayContrast);
    Serial.print("Set displayContrast to: ");
    Serial.println(displayContrast);
    Serial.println();
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > LOG_PERIOD)
  {
    previousMillis = currentMillis;
    cpm = counts * MINUTE_PERIOD / LOG_PERIOD;
    counts = 0;
  }

  if (buttonPushCounter >= LONGBUTTONPUSH)
  {
    buttonPushCounter = 0;
    draw_state = 1;
    if (majorMode == 0)
    {
      majorMode = 1; // we are in Math mode, need to initialize all variables
      questionCount = 0;
      currentMode = 0; // 0 - show question, 1 - show answer
      currentQuestion = generateMathQuestion(currentAnswer);
    }
    else if (majorMode == 1)
    {
      majorMode = 2; // we are in 80 Poem mode
      currentPoem = random(1, TOTAL_POEMS + 1);
      lineCount = 0;
      poemCount = 1;
      readPoem = true;
    }
    else
    {
      majorMode = 0; // we are in Clock mode
    }
    longBeep(ALARMPIN,
#ifdef USE_HIGH_ALARM
             true
#else
             false
#endif
            );
  }

  if (majorMode == 2 && readPoem)
  {
    String strFileName = convertPoemNumberToFileName(currentPoem, TOTAL_POEMS);
    readPoemFromSPIFFS(strFileName, poemText, lineTotal);
    readPoem = false;
  }

  if (backlightOffMode)
  {
    nowTime = time(nullptr);
    struct tm* timeInfo;
    timeInfo = localtime(&nowTime);
    if (timeInfo->tm_hour >= 0 && timeInfo->tm_hour < 7)
    {
      turnOffBacklight(BACKLIGHTPIN, displayMinimumLevel);
    }
    else
    {
      adjustBackLightSub();
    }
  }
  else
  {
    adjustBackLightSub();
  }
  detectButtonPush();

  display.firstPage();
  do {
    draw();

    draw_state++;
  } while ( display.nextPage() );

  if (majorMode == 0)
  {
    if (draw_state >= 10)
    {
      draw_state = 1;
    }
  }
  else if (majorMode == 1)
  {
    if (draw_state >= 10)
    {
      draw_state = 1;
    }
  }
  else if (majorMode == 2)
  {
    if (draw_state >= 121)
    {
      draw_state = 1;
    }
  }
}

void draw(void) {
  detectButtonPush();
  if (majorMode == 0) // Clock mode
  {
    drawLocal();
    detectButtonPush();
  }
  else if (majorMode == 1) // Math mode
  {
    drawMath();
  }
  else if (majorMode == 2) // Poem modes
  {
    drawPoem();
    detectButtonPush();
  }
}

void drawProgress(String labelLine1, String labelLine2) {
  display.clearBuffer();
  display.enableUTF8Print();
  display.setFont(u8g2_font_wqy12_t_gb2312); // u8g2_font_wqy12_t_gb2312, u8g2_font_helvB08_tf
  int stringWidth = 1;
  if (labelLine1 != "")
  {
    stringWidth = display.getUTF8Width(string2char(labelLine1));
    display.setCursor((128 - stringWidth) / 2, 13);
    display.print(labelLine1);
  }
  if (labelLine2 != "")
  {
    stringWidth = display.getUTF8Width(string2char(labelLine2));
    display.setCursor((128 - stringWidth) / 2, 36);
    display.print(labelLine2);
  }
  display.disableUTF8Print();
  display.sendBuffer();
}

void drawLocal() {
  nowTime = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&nowTime);
  char buff[20];

  float radioActivity = cpm * 0.0057;
  float radioActivityRem = radioActivity * 10;

  display.enableUTF8Print();
  display.setFont(u8g2_font_wqy12_t_gb2312); // u8g2_font_wqy12_t_gb2312, u8g2_font_helvB08_tf
  String stringText = String(timeInfo->tm_year + 1900) + "年" + String(timeInfo->tm_mon + 1) + "月" + String(timeInfo->tm_mday) + "日 " + WDAY_NAMES[timeInfo->tm_wday].c_str();
  int stringWidth = display.getUTF8Width(string2char(stringText));
  display.setCursor((128 - stringWidth) / 2, 1);
  display.print(stringText);

  String WindDirectionAndSpeed = "核辐射检测仪";
  stringWidth = display.getUTF8Width(string2char(WindDirectionAndSpeed));
  display.setCursor(127 - stringWidth, 54);
  display.print(WindDirectionAndSpeed);

  String safetyLevel = "安全";
  String converted2 = "< 3.42";

  if (radioActivity < 3.42)
  {
    safetyLevel = "背景辐射，非常安全";
    converted2 = "< 3.42";
  }
  else if (radioActivity < 5.7)
  {
    safetyLevel = "有辐射，基本安全";
    converted2 = "< 5.7";
  }
  else if (radioActivity < 10)
  {
    safetyLevel = "中辐射，长期能患癌";
    converted2 = "< 10";
  }
  else if (radioActivity < 1000)
  {
    safetyLevel = "强辐射，长期能患癌";
    converted2 = "< 1000";
  }
  else if (radioActivity < 3500)
  {
    safetyLevel = "很强辐射，长期患癌";
    converted2 = "< 3500";
  }
  else if (radioActivity < 10000)
  {
    safetyLevel = "超强辐射，明显症状";
    converted2 = "< 10000";
  }
  else if (radioActivity < 41000)
  {
    safetyLevel = "极强辐射，5%死亡";
    converted2 = "< 41000";
  }
  else if (radioActivity < 83000)
  {
    safetyLevel = "极强辐射，50%死亡";
    converted2 = "< 83000";
  }
  else if (radioActivity < 333000)
  {
    safetyLevel = "致死辐射，100%死亡";
    converted2 = "< 333000";
  }
  stringWidth = display.getUTF8Width(string2char(safetyLevel));
  display.setCursor((127 - stringWidth) / 2, 39);
  display.print(safetyLevel);
  display.disableUTF8Print();

  //  display.setFont(u8g2_font_helvR24_tn); // u8g2_font_inb21_ mf, u8g2_font_helvR24_tn

  display.setFont(u8g2_font_helvB08_tf);
  String temp = "CPM: " + String(cpm);
  display.drawStr(0, 13, string2char(temp));

  char outstr1[20];
  dtostrf(radioActivityRem, 18, 3, outstr1);
  String converted1 = String(outstr1);
  converted1.trim();
  converted1 = "mR: " + converted1;
  stringWidth = display.getStrWidth(string2char(converted1));
  display.drawStr(127 - stringWidth, 13, string2char((converted1)));

  char outstr[20];
  dtostrf(radioActivity, 18, 4, outstr);
  String converted = String(outstr);
  converted.trim();
  converted = "uSv: " + converted;
  stringWidth = display.getStrWidth(string2char(converted));
  display.drawStr(0, 25, string2char((converted)));

  converted2.trim();
  stringWidth = display.getStrWidth(string2char(converted2));
  display.drawStr(127 - stringWidth, 25, string2char((converted2)));

  display.setFont(u8g2_font_helvB08_tf);
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  display.drawStr(0, 53, buff);

  display.drawHLine(0, 51, 128);
}

void drawMath(void) {
  nowTime = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&nowTime);
  char buff[20];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display.setFont(u8g2_font_helvB10_tf); // u8g2_font_helvB08_tf, u8g2_font_6x13_tn
  display.setCursor(1, 1);
  display.print(questionCount);
  display.print("/");
  display.print(questionTotal);

  display.setCursor(90, 1);
  display.print(buff);

  display.setFont(u8g2_font_helvB12_tf); // u8g2_font_helvB08_tf, u8g2_font_10x20_tf
  int stringWidth = display.getStrWidth(string2char(currentAnswer));
  display.setCursor((128 - stringWidth) / 2, 28);
  if (currentMode == 0)
  {
    display.print(currentQuestion);
  }
  else
  {
    display.print(currentAnswer);
  }
}

void drawPoem(void) {
  //    display.drawXBM(31, 0, 66, 64, garfield);
  nowTime = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&nowTime);
  char buff[20];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  int stringWidth = 0;

  display.enableUTF8Print();
  display.setFont(u8g2_font_wqy12_t_gb2312); // u8g2_font_wqy12_t_gb2312, u8g2_font_helvB08_tf

  int tempLineBegin = 0;
  int tempLineMultiply = 1;
  int tempLineEnd = 5;

  tempLineMultiply = lineCount / 5;
  tempLineBegin = tempLineMultiply * 5;
  tempLineEnd = tempLineBegin + 5;

  if (tempLineEnd > lineCount + 1)
  {
    tempLineEnd = lineCount + 1;
  }

  int intMaxY = 0;
  for (int i = tempLineBegin; i < tempLineEnd; ++i)
  {
    String strTemp = poemText[i];
    if (strTemp.length() > 30)
    {
      // each Chinese character's length is 3 in UTF-8
      strTemp = strTemp.substring(0, 30);
      strTemp.trim();
    }
    stringWidth = display.getUTF8Width(string2char(strTemp));
    intMaxY = (i % 5) * 13 + 1;
    display.setCursor((128 - stringWidth) / 2, intMaxY);
    display.print(strTemp);
    detectButtonPush();
  }
  display.disableUTF8Print();

  display.setFont(u8g2_font_helvB08_tf); // u8g2_font_helvB08_tf, u8g2_font_6x13_tn
  if (intMaxY < 52)
  {
    display.setCursor(28, 53);
    display.print(TOTAL_POEMS);
    display.setCursor(70, 53);
    display.print(buff);
  }

  display.setCursor(0, 41);
  display.print(lineCount);

  stringWidth = display.getStrWidth(string2char(String(lineTotal - 1)));
  display.setCursor(128 - stringWidth, 41);
  display.print(lineTotal - 1);

  display.setCursor(0, 53);
  display.print(poemCount);

  stringWidth = display.getStrWidth(string2char(String(poemTotal)));
  display.setCursor(128 - stringWidth, 53);
  display.print(poemTotal);
}

