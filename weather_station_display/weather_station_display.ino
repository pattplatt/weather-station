#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "ArialRounded.h"
#include "settings.h"
#include <XPT2046_Touchscreen.h>
#include "TouchControllerWS.h"
#include <JsonListener.h>
#include <MiniGrafx.h>
#include <ILI9341_SPI.h>
#include <simpleDSTadjust.h>
#include <ESP8266WiFiMulti.h>

ESP8266WiFiMulti WiFiMulti;

const char* ssidAP = "ESP8266-Access-Point";
const char* passwordAP = "123456789";

//IP address or domain name with URL path where we listen for messages
const char* serverNameTemp = "http://192.168.4.1/temperature";
const char* serverNameHumi = "http://192.168.4.1/humidity";
const char* serverQueryString = "http://192.168.4.1/queryString";

#define MINI_BLACK 0
#define MINI_WHITE 1
#define MINI_YELLOW 2
#define MINI_BLUE 3

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

//defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                      ILI9341_YELLOW, // 2
                      0x7E3C
                      }; //3

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
// Limited to 4 colors due to memory constraints
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors
bool showQueryScreen = false;

ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);
//Carousel carousel(&gfx, 0, 0, 240, 100);

void drawWifiQuality();

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TouchControllerWS touchController(&ts);

void calibrationCallback(int16_t x, int16_t y);
CalibrationCallback calibration = &calibrationCallback;

void calibrationCallback(int16_t x, int16_t y) {
  gfx.setColor(1);
  gfx.fillCircle(x, y, 10);
}

void drawProgress(uint8_t percentage, String text);

String temperature;
String humidity;
String queryResult;

unsigned long previousMillis = 0;
const long interval = 5000; 

void setup() {
  Serial.begin(115200);
  Serial.println();

  Serial.println(TFT_LED);
  pinMode(TFT_LED, OUTPUT);
  //digitalWrite(TFT_LED, LOW);
  digitalWrite(TFT_LED, HIGH);
  
  gfx.init();
  gfx.fillBuffer(MINI_BLACK);
  gfx.commit();
 
  Serial.print("Connecting to ");
  Serial.println(ssidAP);
  WiFi.begin(ssidAP, passwordAP);
  int i = 0;
  Serial.print("connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (i>80){
      i=0;
      drawProgress(50,"Connecting to AALeC server");
      i+=10;
      Serial.print(".");
  }}

  drawProgress(100,"Connected to WiFi");
  Serial.println("");
  Serial.println("Connected to WiFi");
  Serial.println("Initializing touch screen...");
  ts.begin();
  
  Serial.println("Mounting file system...");
  bool isFSMounted = SPIFFS.begin();
  if (!isFSMounted) {
    Serial.println("Formatting file system...");
    drawProgress(50,"Formatting file system");
    SPIFFS.format();
  }
  drawProgress(100,"Formatting done");
  SPIFFS.remove("/calibration.txt");

  boolean isCalibrationAvailable = touchController.loadCalibration();
  if (!isCalibrationAvailable) {
    Serial.println("Calibration not available");
    touchController.startCalibration(&calibration);
    while (!touchController.isCalibrationFinished()) {
      gfx.fillBuffer(0);
      gfx.setColor(MINI_YELLOW);
      gfx.setTextAlignment(TEXT_ALIGN_CENTER);
      gfx.drawString(120, 160, "Please calibrate\ntouch screen by\ntouch point");
      touchController.continueCalibration();
      gfx.commit();
      yield();
    }
    touchController.saveCalibration();
  }
}
long lastDrew = 0;
bool btnClick;
uint8_t MAX_TOUCHPOINTS = 10;
TS_Point points[10];
uint8_t currentTouchPoint = 0;

void loop() {
  if (touchController.isTouched(0)) {
    TS_Point p = touchController.getPoint();
    if (p.y > 0){
      Serial.println("touched screen");
      showQueryScreen = !showQueryScreen;
    } 
  }
  unsigned long currentMillis = millis();

  if(currentMillis - previousMillis >= interval) {
     // Check WiFi connection status
    if ((WiFiMulti.run() == WL_CONNECTED)) {
      temperature = httpGETRequest(serverNameTemp);
      humidity = httpGETRequest(serverNameHumi);
      queryResult = httpGETRequest(serverQueryString);
      Serial.println("Temperature: " + temperature + " *C - Humidity: " + humidity);
      previousMillis = currentMillis;
    }
    else {
      Serial.println("WiFi Disconnected");
      WiFi.disconnect();
      WiFi.begin(ssidAP, passwordAP);
      delay(10000);
    }
  }
  
  if (showQueryScreen == false){
    showData("Temperature: "+ temperature + "°C\n"+"Humidity: " +humidity + "%\n","Last query: \n" + queryResult);
    drawWifiQuality(9,0);
    }
  else{
    showQuery(queryResult);
    drawWifiQuality(0,8);
    }
  gfx.commit();
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path 
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "--"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  return payload;
}
// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 90, "Starting wheater station");
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(120, 146, text);
  gfx.setColor(MINI_WHITE);
  gfx.drawRect(10, 168, 240 - 20, 15);
  gfx.setColor(MINI_BLUE);
  gfx.fillRect(12, 170, 216 * percentage / 100, 11);
  gfx.commit();
}
//show the measurement and the query on main screen
void showData(String liveData, String Query) {
  gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 10, "Data:");
  gfx.setColor(MINI_BLUE);
  gfx.drawString(120, 40, liveData);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawLine(0, 80, 500, 80);
  gfx.drawString(0, 80, Query);
}
//show only query on second screen
void showQuery(String Query){
  gfx.setFont(ArialMT_Plain_10);
  gfx.fillBuffer(MINI_BLACK);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(0, 0, "Full Query:");
  gfx.setColor(MINI_BLUE);
  gfx.drawLine(0, 15, 500, 15);
  gfx.drawString(0, 16, Query);
}

void drawWifiQuality(int y, int y_draw) {
  int8_t quality = getWifiQuality();
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);  
  gfx.drawString(228, y, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(230 + 2 * i, 18 - j - y_draw);
      }
    }
  }
}

int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}