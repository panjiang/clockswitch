#include <Arduino.h>
#include "RTClib.h"
#include "ESP8266WiFi.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char *ssid = "hello world";
const char *password = "hello1102";
String host = "https://nettools.wuyabiji.com";
String serverName = "https://nettools.wuyabiji.com/v1/time";

RTC_DS1307 rtc;

class Trigger
{
public:
  uint32_t seconds; // Seconds since zero o'clock.
  bool isopen;      // Open or close.

  Trigger(String hmtime, bool isopen_)
  {

    auto i = hmtime.indexOf(":");
    auto j = hmtime.lastIndexOf(":");
    auto shour = hmtime.substring(0, i);
    auto sminute = hmtime.substring(i + 1);
    auto ssecond = hmtime.substring(j + 1);

    auto h = atoi(shour.c_str());
    auto m = atoi(sminute.c_str());
    auto s = atoi(ssecond.c_str());

    seconds = h * 3600 + m * 60 + s;
    Serial.printf("Trigger seconds: %d\n", seconds);
    isopen = isopen_;
  }
};

String formatTime(DateTime now)
{
  char buff[100];
  snprintf(buff, sizeof(buff), "%d-%d-%d %d:%d:%d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return buff;
}

String formatRTCNow()
{
  if (!rtc.isrunning())
  {
    return "";
  }
  return formatTime(rtc.now());
}

uint32_t getRemoteunixtime()
{
  // Sync time with server API
  WiFiClientSecure client;
  client.setInsecure(); //the magic line, use with caution
  client.connect(host, 443);
  Serial.printf("Connected: %s\n", host.c_str());

  HTTPClient http;
  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    String payload = http.getString();
    http.end();

    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    JsonObject obj = doc.as<JsonObject>();
    uint32_t unixtime = obj["unix"];
    uint32_t offset = obj["offset"];
    // RTC unixtime is non-UTC.
    return unixtime + offset;
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    http.end();
  }

  return 0;
}

void setupWiFi()
{
  // WiFi
  Serial.printf("WiFi connect: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("*");
  }

  Serial.println("");
  Serial.println("WiFi connection Successful");
  Serial.printf("The IP Address of ESP8266 Module is: ");
  Serial.print(WiFi.localIP());
  Serial.print("\n");
}

void setupRTC(uint32_t unixtime)
{
  // RTC
  while (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    delay(5000);
  }
  Serial.println("Found RTC");

  if (unixtime > 0)
  {
    auto now = DateTime(unixtime);
    rtc.adjust(now);

    Serial.println("RTC sync with remote time");
  }
  else
  {
    if (!rtc.isrunning())
    {
      Serial.println("RTC is NOT running, let's set the time!");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  auto now = DateTime(unixtime);
  Serial.printf("%d-%d-%d %d:%d:%d\n", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
}

const uint8_t switchInPort = D8;
const uint8_t lightInPort = D6;

void setupPorts()
{
  // PIN PORT
  pinMode(switchInPort, OUTPUT);
  pinMode(lightInPort, OUTPUT);
  digitalWrite(lightInPort, HIGH);
}

// The current seconds of today.
uint32_t secondsOfToday()
{
  auto ts = rtc.now();
  return ts.unixtime() % 86400;
}

// Control switch open or close.
void controlSwitch(bool open)
{
  if (open)
  {
    Serial.printf("Open at: %s\n", formatRTCNow().c_str());
    digitalWrite(switchInPort, HIGH);
  }
  else
  {
    Serial.printf("Close at: %s\n", formatRTCNow().c_str());
    digitalWrite(switchInPort, LOW);
  }
}

uint32_t prevSeconds = 0;

void setup()
{
  Serial.begin(57600);
  setupWiFi();
  uint32_t unixtime = 0;
  unixtime = getRemoteunixtime();
  setupRTC(unixtime);
  setupPorts();
}

const static Trigger triggers[] = {
    Trigger("12:59:00", true),
    Trigger("12:59:05", false),
    Trigger("12:59:10", true),
    Trigger("12:59:15", false),
    Trigger("12:60:00", true),
    Trigger("12:60:05", false),
    Trigger("12:60:10", true),
    Trigger("12:60:15", false),
};

void loop()
{

  auto nowSeconds = secondsOfToday();
  if (nowSeconds == prevSeconds)
  {
    return;
  }

  prevSeconds = nowSeconds;

  for (auto &t : triggers)
  {
    if (t.seconds == nowSeconds)
    {
      controlSwitch(t.isopen);
    }
  }
}