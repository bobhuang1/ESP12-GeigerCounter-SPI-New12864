#include <ESP8266WiFi.h>
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

#define CURRENT_VERSION 4
#define DEBUG
//#define USE_WIFI_MANAGER     // disable to NOT use WiFi manager, enable to use
#define LANGUAGE_CN  // LANGUAGE_CN or LANGUAGE_EN, enable for 600 Chinese, disable for 601 English

// Serial 600 and 602 in Chinese, 601 in English
#define BUTTONPIN   4
#define GEIGERPIN   2
#define ALARMPIN 5
#define BACKLIGHTPIN 0

const unsigned char iconNuclear[] = {
  0xF0, 0x00, 0xFC, 0x03, 0xF2, 0x06, 0xF2, 0x04, 0x61, 0x08, 0x61, 0x08,
  0xFF, 0x0F, 0x9F, 0x0F, 0x9E, 0x07, 0x0E, 0x07, 0x0C, 0x03, 0xF0, 0x00
};

const unsigned char iconSpeaker[] = {
  0x60, 0x00, 0x70, 0x02, 0x58, 0x04, 0x4C, 0x09, 0x47, 0x0A, 0x45, 0x0A,
  0x45, 0x0A, 0x47, 0x0B, 0x4C, 0x09, 0x58, 0x04, 0x70, 0x02, 0x60, 0x00
};

const unsigned char iconMute[] = {
  0x60, 0x00, 0x70, 0x00, 0x58, 0x00, 0xCC, 0x08, 0x47, 0x05, 0x45, 0x02,
  0x45, 0x05, 0xC7, 0x08, 0x4C, 0x00, 0x58, 0x00, 0x70, 0x00, 0x60, 0x00
};

#ifdef LANGUAGE_CN
const String WDAY_NAMES[] = { "日", "一", "二", "三", "四", "五", "六" };
#else
const String WDAY_NAMES[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
#endif

#define LOG_PERIOD 20000 //Logging period in milliseconds
#define LOG_1_PERIOD 60000 //Logging period in milliseconds
#define LOG_10_PERIOD 60000 //Logging period in milliseconds
#define MINUTE_PERIOD 60000


// Serial 600, 601
bool backlightOffMode = false;
int displayContrast = 110;
int displayMultiplier = 200;
int displayBias = 15;
int displayMinimumLevel = 1;
int displayMaximumLevel = 1023;

U8G2_ST7565_64128N_F_4W_SW_SPI display(U8G2_R0, /* clock=*/ 14, /* data=*/ 12, /* cs=*/ 13, /* dc=*/ 15, /* reset=*/ 16); // U8G2_ST7565_64128N_F_4W_SW_SPI

volatile unsigned long counts = 0;                       // Tube events
volatile unsigned long counts1 = 0;                       // Tube events
volatile unsigned long counts10 = 0;                       // Tube events
unsigned long cpm = 0;                                   // CPM
unsigned long cpm1 = 0;                                   // CPM
unsigned long cpm10 = 0;                                   // CPM
unsigned long previousMillis;                            // Time measurement
unsigned long previous1Millis;                            // Time measurement
unsigned long previous10Millis;                            // Time measurement


time_t nowTime;
const String degree = String((char)176);
const String microSymbol = String((char)181);

int lightLevel[10];
int draw_state = 1;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
const unsigned long debounceDelay = 30;    // the debounce time; increase if the output flickers
bool geigerBeep = true;

void ICACHE_RAM_ATTR geigerHandler ();


void geigerHandler() { // Captures count of events from Geiger counter board
  counts ++;
  counts1 ++;
  counts10 ++;
  if (geigerBeep)
  {
    shortGeigerBeep(ALARMPIN, true);
  }
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
  noBeep(ALARMPIN, true);
  listSPIFFSFiles(); // Lists the files so you can see what is in the SPIFFS

  display.begin();
  display.setFontPosTop();
  setContrastSub();

  display.clearBuffer();
  display.drawXBM(31, 0, 66, 64, garfield);
  display.sendBuffer();
  shortBeep(ALARMPIN, true);
  delay(1000);

  drawProgress(String(CompileDate), String(CURRENT_VERSION));
  delay(1000);

  drawProgress("Backlight Level", "Test");

  selfTestBacklight(BACKLIGHTPIN);

#ifdef USE_WIFI_MANAGER
#ifdef LANGUAGE_CN
  drawProgress("连接WIFI:", "IBECloc12864-HW");
#else
  drawProgress("Connect to WIFI:", "IBECloc12864-HW");
#endif
#else
#ifdef LANGUAGE_CN
  drawProgress("连接WIFI中,", "请稍等...");
#else
  drawProgress("Connecting WIFI,", "Please Wait...");
#endif
#endif

  connectWIFI(
#ifdef USE_WIFI_MANAGER
    true
#else
    false
#endif
  );

  if (WiFi.status() == WL_CONNECTED)
  {
    // Get time from network time service
#ifdef DEBUG
    Serial.println("WIFI Connected");
#endif

#ifdef LANGUAGE_CN
    drawProgress("连接WIFI成功,", "正在同步时间...");
#else
    drawProgress("WIFI Connected,", "NTP Time Sync...");
#endif
    configTime(TZ_SEC, DST_SEC, NTP_SERVER);

#ifdef LANGUAGE_CN
    drawProgress("同步时间成功,", "正在启动中...");
#else
    drawProgress("Time Sync Success,", "Booting...");
#endif
  }
  interrupts();                                                            // Enable interrupts
  attachInterrupt(digitalPinToInterrupt(GEIGERPIN), geigerHandler, FALLING); // Define interrupt on falling edge
}

void adjustBackLightSub() {
  adjustBacklight(lightLevel, BACKLIGHTPIN, displayBias, displayMultiplier);
}

void detectButtonPush() {
  int reading;
  reading = digitalRead(BUTTONPIN);
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
        if (geigerBeep)
        {
          geigerBeep = false;
        }
        else
        {
          geigerBeep = true;
          shortGeigerBeep(ALARMPIN, true);
        }
      }
    }
  }
  lastButtonState = reading;
}

void setContrastSub() {
  if (displayContrast > 0)
  {
    display.setContrast(displayContrast);
#ifdef DEBUG
    Serial.print("Set displayContrast to: ");
    Serial.println(displayContrast);
    Serial.println();
#endif
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > LOG_PERIOD)
  {
    previousMillis = currentMillis;
    cpm = counts * MINUTE_PERIOD / LOG_PERIOD;
#ifdef DEBUG
    Serial.print("CPM: ");
    Serial.println(cpm);
    Serial.println();
#endif
    counts = 0;
  }

  if (currentMillis - previous1Millis > LOG_1_PERIOD)
  {
    previous1Millis = currentMillis;
    cpm1 = counts1 * MINUTE_PERIOD / LOG_1_PERIOD;
#ifdef DEBUG
    Serial.print("CPM 1: ");
    Serial.println(cpm1);
    Serial.println();
#endif
    counts1 = 0;
  }

  if (currentMillis - previous10Millis > LOG_10_PERIOD)
  {
    previous10Millis = currentMillis;
    cpm10 = counts10 * MINUTE_PERIOD / LOG_10_PERIOD;
#ifdef DEBUG
    Serial.print("CPM 10: ");
    Serial.println(cpm10);
    Serial.println();
#endif
    counts10 = 0;
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

  if (draw_state >= 10)
  {
    draw_state = 1;
  }
}

void draw(void) {
  detectButtonPush();
  drawLocal();
  detectButtonPush();
}

void drawProgress(String labelLine1, String labelLine2) {
  display.clearBuffer();

#ifdef LANGUAGE_CN
  display.enableUTF8Print();
  display.setFont(u8g2_font_wqy12_t_gb2312); // u8g2_font_wqy12_t_gb2312, u8g2_font_helvB08_tf
#else
  display.setFont(u8g2_font_helvR08_tf);
#endif

  int stringWidth = 1;
  if (labelLine1 != "")
  {
#ifdef LANGUAGE_CN
    stringWidth = display.getUTF8Width(string2char(labelLine1));
#else
    stringWidth = display.getStrWidth(string2char(labelLine1));
#endif
    display.setCursor((128 - stringWidth) / 2, 13);
    display.print(labelLine1);
  }

  if (labelLine2 != "")
  {
#ifdef LANGUAGE_CN
    stringWidth = display.getUTF8Width(string2char(labelLine2));
#else
    stringWidth = display.getStrWidth(string2char(labelLine2));
#endif
    display.setCursor((128 - stringWidth) / 2, 36);
    display.print(labelLine2);
  }

#ifdef LANGUAGE_CN
  display.disableUTF8Print();
#endif

  display.sendBuffer();
}

void drawLocal() {
  nowTime = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&nowTime);
  char buff[20];

  float radioActivity = cpm * 0.0057;
  float radioActivity1 = cpm1 * 0.0057;
  float radioActivity10 = cpm10 * 0.0057;

#ifdef LANGUAGE_CN
  display.enableUTF8Print();
  display.setFont(u8g2_font_wqy12_t_gb2312); // u8g2_font_wqy12_t_gb2312, u8g2_font_helvB08_tf
#else
  display.setFont(u8g2_font_helvR08_tf);
#endif

#ifdef LANGUAGE_CN
  String stringText = String(timeInfo->tm_year + 1900) + "-" + intToTwoDigitString(timeInfo->tm_mon + 1) + "-" + intToTwoDigitString(timeInfo->tm_mday) + " " + WDAY_NAMES[timeInfo->tm_wday].c_str();
#else
  String stringText = String(WDAY_NAMES[timeInfo->tm_wday].c_str()) + "  " + String(timeInfo->tm_year + 1900) + "-" + intToTwoDigitString(timeInfo->tm_mon + 1) + "-" + intToTwoDigitString(timeInfo->tm_mday);
#endif
  int stringWidth = display.getUTF8Width(string2char(stringText));
  if (WiFi.status() == WL_CONNECTED && timeInfo->tm_year != 70)
  {
    display.setCursor((128 - stringWidth) / 2, 1);
    display.print(stringText);
  }

#ifdef LANGUAGE_CN
  String WindDirectionAndSpeed = "核辐射检测仪";
#else
  String WindDirectionAndSpeed = "Nuclear Radiation";
#endif

#ifdef LANGUAGE_CN
  stringWidth = display.getUTF8Width(string2char(WindDirectionAndSpeed));
#else
  stringWidth = display.getStrWidth(string2char(WindDirectionAndSpeed));
#endif

  display.setCursor(127 - stringWidth, 54);
  display.print(WindDirectionAndSpeed);

#ifdef LANGUAGE_CN
#else
#endif
  String safetyLevel = "背景辐射，非常安全";

  String converted2 = "< 3.42";

  if (radioActivity < 3.42)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "背景辐射，非常安全";
#else
    safetyLevel = "Background Radiation";
#endif
    converted2 = "< 3.42";
  }
  else if (radioActivity < 5.7)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "有辐射，基本安全";
#else
    safetyLevel = "Light Radiation Safe";
#endif
    converted2 = "< 5.7";
  }
  else if (radioActivity < 10)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "中辐射，长期能患癌";
#else
    safetyLevel = "Low Radiation Cancer Risk";
#endif
    converted2 = "< 10";
  }
  else if (radioActivity < 1000)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "强辐射，长期能患癌";
#else
    safetyLevel = "Med Radiation Cancer Risk High";
#endif
    converted2 = "< 1000";
  }
  else if (radioActivity < 3500)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "很强辐射，长期患癌";
#else
    safetyLevel = "High Radiation Cancer Risk High";
#endif
    converted2 = "< 3500";
  }
  else if (radioActivity < 10000)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "超强辐射，明显症状";
#else
    safetyLevel = "V High Radiation Symptoms";
#endif
    converted2 = "< 10000";
  }
  else if (radioActivity < 41000)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "极强辐射，5%死亡";
#else
    safetyLevel = "U High Radiation 5% Death";
#endif
    converted2 = "< 41000";
  }
  else if (radioActivity < 83000)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "极强辐射，50%死亡";
#else
    safetyLevel = "Lethal Radiation 50% Death";
#endif
    converted2 = "< 83000";
  }
  else if (radioActivity < 333000)
  {
#ifdef LANGUAGE_CN
    safetyLevel = "致死辐射，100%死亡";
#else
    safetyLevel = "Lethal Radiation 100% Death";
#endif
    converted2 = "< 333000";
  }

#ifdef LANGUAGE_CN
  stringWidth = display.getUTF8Width(string2char(safetyLevel));
#else
  stringWidth = display.getStrWidth(string2char(safetyLevel));
#endif
  display.setCursor((127 - stringWidth) / 2, 39);
  display.print(safetyLevel);
#ifdef LANGUAGE_CN
  display.disableUTF8Print();
#endif

#ifdef LANGUAGE_CN
  int secondLineY = 14;
  int thirdLineY = 26;
#else
  int secondLineY = 14;
  int thirdLineY = 27;
#endif

  display.setFont(u8g2_font_helvB08_tf);
  String temp = "CPM: " + String(cpm);
  display.drawStr(0, secondLineY, string2char(temp));

  stringWidth = display.getStrWidth(string2char(String(cpm1)));
  display.drawStr((127 - stringWidth) / 2 + 8, secondLineY, string2char(String(cpm1)));

  stringWidth = display.getStrWidth(string2char(String(cpm10)));
  display.drawStr(127 - stringWidth, secondLineY, string2char(String(cpm10)));

  char outstr[20];
  dtostrf(radioActivity, 18, 2, outstr);
  String converted = String(outstr);
  converted.trim();
  converted = microSymbol + "Sv: " + converted;
  stringWidth = display.getStrWidth(string2char(converted));
  display.drawStr(0, thirdLineY, string2char((converted)));

  char outstr0[20];
  dtostrf(radioActivity1, 18, 2, outstr0);
  String converted0 = String(outstr0);
  converted0.trim();
  stringWidth = display.getStrWidth(string2char(converted0));
  display.drawStr((127 - stringWidth) / 2 + 10, thirdLineY, string2char((converted0)));

  char outstr1[20];
  dtostrf(radioActivity10, 18, 2, outstr1);
  String converted1 = String(outstr1);
  converted1.trim();
  stringWidth = display.getStrWidth(string2char(converted1));
  display.drawStr(127 - stringWidth, thirdLineY, string2char((converted1)));

  /*
    converted2.trim();
    stringWidth = display.getStrWidth(string2char(converted2));
    display.drawStr(127 - stringWidth, 25, string2char((converted2)));
  */

  display.setFont(u8g2_font_helvR08_tf);
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  if (WiFi.status() == WL_CONNECTED && timeInfo->tm_year != 70)
  {
    display.drawStr(0, 53, buff);
  }
  display.drawHLine(0, 51, 128);
  if (geigerBeep)
  {
    display.drawXBM(113, 0, 12, 12, iconSpeaker);
    if (radioActivity >= 3.42)
    {
      longBeep(ALARMPIN, true);
    }
  }
  else
  {
    display.drawXBM(113, 0, 12, 12, iconMute);
  }
  display.drawXBM(0, 0, 12, 12, iconNuclear);
  if (radioActivity > 3)
  {
    display.drawStr(13, 1, "!!");
  }
}

void shortGeigerBeep(int alarmPIN, bool bolUseHighAlarm) {
  if (bolUseHighAlarm)
  {
    digitalWrite(alarmPIN, HIGH);
    delay(1);
    digitalWrite(alarmPIN, LOW);
  }
  else
  {
    digitalWrite(alarmPIN, LOW);
    delay(1);
    digitalWrite(alarmPIN, HIGH);
  }
}
