#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <MQTTClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Time.h>
#include <Wire.h>
#include "SSD1306.h"
#include "OLEDDisplayUi.h"
#include "font.h"
#include "images.h"
#include "config.h"

#define OLED_SDA D2
#define OLED_SCL D1

const int tz = 1; //Central European Time CET
int defaultOffset = 3600;

SSD1306 display(0x3C, OLED_SDA, OLED_SCL);

long updateInterval = 600000;
long lastUpdate = -updateInterval;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", defaultOffset, updateInterval);
time_t currentTime;

String timeS = "";
String temp = "99";
String weather = "";

WiFiClient wifi;
MQTTClient mqtt;

const char* host = HOSTNAME;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void connect();

void setup() {
  Serial.begin(115200);
  Serial.println("Wifi-Clock");

  // put your setup code here, to run once:
  display.init();
  display.setContrast(30);
  display.clear();
  display.flipScreenVertically();
  display.drawXbm( 34, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.display();

  connect();

  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);

  mqtt.begin(MQTT_HOST, wifi);
  Serial.println("Start MQTT");

  Serial.println("start NTP");
  timeClient.begin();

  delay(1500);
}

void loop() {
  if (millis()-lastUpdate >= updateInterval) {
    if (WiFi.status() != WL_CONNECTED) connect();
    //handle Updateserver
    httpServer.handleClient();

    Serial.print("Update time....");
    timeClient.update();
    if (timeClient.getHours() == 2 && timeClient.getMinutes() == 0) timeClient.update();

    timeClient.setTimeOffset(offsetDstEurope());

    Serial.print(offsetDstEurope());
    Serial.print(" ");

    Serial.print(timeClient.getHours());
    Serial.print(":");
    Serial.println(timeClient.getMinutes());
    lastUpdate = millis();

    mqtt.loop();
    delay(10);
    if (!mqtt.connected()) {
      connectMQTT();


      mqtt.loop();
      delay(1000);
      mqtt.loop();
      delay(1000);
    }

    WiFi.disconnect();
    Serial.println("disconnect");
  }

  timeS = timeClient.getHours();
  timeS += ":";
  if (timeClient.getMinutes()<10) timeS += "0";
  timeS += timeClient.getMinutes();
  display.clear();
  //display.drawXbm( 35, 0,50, 50, sleet_bits);
  display.setFont(Monospaced_bold_22);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(128 - display.getStringWidth(temp+"°C"), 2, temp+"°C");
  display.setFont(Monospaced_bold_42);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 24, timeS);
  display.display();
  delay(1000);
}

void connect() {
  Serial.print("checking wifi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected to wifi!");
}

void connectMQTT() {
  Serial.print("checking mqtt...");
  while (!mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.print(".");
  }
  mqtt.subscribe("/+/clock/#");
  Serial.println("\nconnected to mqtt!");
}

void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  Serial.print("incoming: ");
  Serial.print(topic);
  Serial.print(" - ");
  Serial.print(payload);
  Serial.println();
  if (topic == "/finnja/clock/weather/temp") {
    temp = payload;
  }
  if (topic == "/finnja/clock/weather/weather") {
    weather = payload;
  }
}

int offsetDstEurope() {
  setTime(timeClient.getEpochTime() - 2208988800UL + tz * SECS_PER_HOUR);

  int beginDSTDate=  (31 - (5* year() /4 + 4) % 7); //  last sunday of march
  int beginDSTMonth=3;
  int endDSTDate= (31 - (5 * year() /4 + 1) % 7); //last sunday of october
  int endDSTMonth=10;

  if (((month() > beginDSTMonth) && (month() < endDSTMonth))
    || ((month() == beginDSTMonth) && (day() >= beginDSTDate))
    || ((month() == endDSTMonth) && (day() <= endDSTDate)))
  return 7200;  // DST europe = utc +2 hour
  else return 3600; // nonDST europe = utc +1 hour
}
