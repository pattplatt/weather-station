#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "ESPAsyncWebServer.h"
#include <AALeC.h>
#include <Ticker.h>
#include <elapsedMillis.h>

elapsedMillis timeElapsedSinceLastDBWrite;
elapsedMillis timeElapsedSinceLastTelegramMessage;

//insert WIFI credentials here
const char* ssid = "G2-R024-Labor";
const char* password = "pYxQocnzceD48Fdw";

//here the AALeC webserver credentials can be set
const char* ssidAP = "ESP8266-Access-Point";
const char* passwordAP = "123456789";

int temp = 0;
int hum=0;

String res="";
String resultArray[2];
AsyncWebServer server(80);

// Initialize Telegram BOT
#define BOTtoken "<your bot token>"
//Use @myidbot (IDBot) on Telegram to find out your Chat ID 
#define CHAT_ID "<insert chat ID>"

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif
c_AALeC aalec;
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    int message_id = bot.messages[i].message_id;
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    //assign the received message to variable
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (bot.messages[i].type == "callback_query") {
      Serial.print("Call back button pressed by: ");
      Serial.println(bot.messages[i].from_id);
      Serial.print("Data on the button: ");
      Serial.println(bot.messages[i].text);
    }
    //check for text variable and send respond accordingly 
    if (text == "/start") {
     startBot(chat_id,message_id,text);
    }

    if (text == "/getData") {
        char msg[] = "Select measurements\n";
        String keyboardJson = "[";
          keyboardJson += "[{ \"text\" : \"Temperature\", \"callback_data\" : \"temperature\" }";
          keyboardJson += ",{ \"text\" : \"Humidity\", \"callback_data\" : \"humidity\" }]";
          keyboardJson += ",[{ \"text\" : \"Restart\", \"callback_data\" : \"/start\" }]";
          keyboardJson += "]";
        Serial.println("keyboard " + keyboardJson);
        bot.sendMessageWithInlineKeyboard(chat_id, msg, "Markdown", keyboardJson,message_id);
    }

    if (text == "humidity" | text == "temperature") {
      char msg[] = "Choose an interval\n";
      String keyboardJson = "[";
        keyboardJson += "[{ \"text\" : \"Last 7 days\", \"callback_data\" : \"7\" }]";
        keyboardJson += ",[{ \"text\" : \"Last 14 days\", \"callback_data\" : \"14\" }]";
        keyboardJson += ",[{ \"text\" : \"Last 21 days\", \"callback_data\" : \"21\" }]";
        keyboardJson += ",[{ \"text\" : \"Last 30 days\", \"callback_data\" : \"30\" }]";
        keyboardJson += ",[{ \"text\" : \"Restart\", \"callback_data\" : \"/getData\" }]";
        keyboardJson += "]";
      resultArray[1] = text;
      bot.sendMessageWithInlineKeyboard(chat_id, msg, "Markdown", keyboardJson,message_id);
    }

    if (text == "30" | text == "21"| text == "14"| text == "7") {
      resultArray[0] = text;
      res = queryInflux(&resultArray[0],&resultArray[1]);
      bot.sendMessage(chat_id, res);
      //listen to the /queryString URL and send query if it gets requested
      server.on("/queryString", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Sending temp");
        request->send_P(200, "text/plain", res.c_str());
      });
    }
  }
}

void setup() {
  Serial.begin(115200);
  client.setBufferSizes(512, 512);
  configTime(3600, 3600, "pool.ntp.org");
  client.setTrustAnchors(&cert);      
  aalec.init();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    aalec.set_rgb_strip(4,255,165,0);
  }
  Serial.println(WiFi.localIP());
  aalec.set_rgb_strip(4,0,128,0);
  bot.sendMessage(CHAT_ID, "Bot started", "");
  startBot(CHAT_ID,0, "/start");
  startInflux();
  WiFi.softAP(ssidAP, passwordAP);
  //start webserver on AALeC that	AZ-Touch ESP can receive messages
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Sending temp");
    std::string tempString = std::to_string(temp);
    request->send_P(200, "text/plain", tempString.c_str());
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Sending hum");
    std::string humString = std::to_string(hum);
    request->send_P(200, "text/plain", humString.c_str());
  });
  server.begin();
}

// Checks for new messages every 1 second.
const int botRequestDelay = 1000;
unsigned long lastTimeBotRan;
//write to influx every minute
const uint32_t influxWriteInterval = 60000;
const uint32_t intervalMessageLED = 10000;

void loop() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) {
      aalec.set_rgb_strip(3,0,0,128);
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  lastTimeBotRan = millis();
  }
  if (timeElapsedSinceLastTelegramMessage > intervalMessageLED) { 
    aalec.set_rgb_strip(3,0,0,0);
    timeElapsedSinceLastTelegramMessage = 0;
  }
  // check if it's time
  if (timeElapsedSinceLastDBWrite > influxWriteInterval) { 
    Serial.println("writing to influx");
    temp = aalec.get_temp();
    hum = aalec.get_humidity();
    writeData(&temp,&hum);
    timeElapsedSinceLastDBWrite = 0;
  }
}

void startBot(String chat_id, int message_id, String text){
   char welcomeMessage[] = "Hi!\nIm your weather station bot!\n\nHere you can request the maximum temperature and humidity from the last 30 days and update it to the weather station display.\n\n";
      String keyboardJson = "[";
      keyboardJson += "[{ \"text\" : \"Get data\", \"callback_data\" : \"/getData\" }]";
      keyboardJson += "]";
      if (text == "/start") {
        bot.sendMessageWithInlineKeyboard(chat_id, welcomeMessage, "Markdown", keyboardJson);
      }
}