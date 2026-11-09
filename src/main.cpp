#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char *ssid = "";
const char *password = "";

const short DHT11_pin = 2; // D4 / GPIO2
#define DHT11_type DHT11

DHT dht(DHT11_pin, DHT11_type);

float temperature = 0;
float humidity = 0;
unsigned long last_sensor_time = 0;

enum States
{
  CONNECTING,
  SUCCESS,
  FAILED
};

States cur_state = CONNECTING;

ESP8266WebServer server(80);

unsigned long last_time_connected = 0;
unsigned long last_time_failed = 0;
unsigned long start_attempt_time = 0;

void setup()
{
  Serial.begin(115200);
  Serial.print("Connecting to WiFi");

  // Ініціалізація LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("\nПомилка монтування LittleFS, спроба форматування...");
    if (LittleFS.format())
    {
      Serial.println("Файлову систему успішно відформатовано. Монтуємо знову...");
      LittleFS.begin();
    }
    else
    {
      Serial.println("Помилка форматування LittleFS!");
    }
  }
  else
  {
    Serial.println("\nLittleFS замонтовано успішно.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  dht.begin();
  start_attempt_time = millis();
}

void loop()
{
  unsigned long cur_time = millis();
  yield(); // Годуємо сторожовий таймер ESP8266

  switch (cur_state)
  {
  case CONNECTING:
    if (WiFi.status() == WL_CONNECTED)
    {
      cur_state = SUCCESS;
      Serial.println("\nWiFi connected success!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      // 1. Головна сторінка SPA (надійне відкриття безпосередньо через стрімінг файлу)
      server.on("/", HTTP_GET, []()
                {
        if (LittleFS.exists("/index.html")) {
          File file = LittleFS.open("/index.html", "r");
          server.streamFile(file, "text/html");
          file.close();
        } else {
          server.send(404, "text/plain", "Error: index.html not found!");
        } });

      // 2. GET API: Телеметрія метеостанції
      server.on("/api/data", HTTP_GET, []()
                {
        JsonDocument doc;
        doc["uptime"] = millis() / 1000;
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        doc["status"] = "OK";
        
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json); });

      // 3. API для правил (заглушка)
      server.on("/api/rules", HTTP_GET, []()
                {
        JsonDocument doc;
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json); });

      server.on("/api/rules", HTTP_POST, []()
                {
        if (!server.hasArg("plain")) {
          server.send(400, "application/json", "{\"status\":\"error\"}");
          return;
        }
        server.send(200, "application/json", "{\"status\":\"success\"}"); });

      // 4. Автоматична роздача інших статичних файлів (style.css, app.js тощо)
      server.serveStatic("/", LittleFS, "/");

      // SPA Fallback (на випадок інших запитів)
      server.onNotFound([]()
                        {
        File file = LittleFS.open("/index.html", "r");
        if (file) {
          server.streamFile(file, "text/html");
          file.close();
        } else {
          server.send(404, "text/plain", "404: Not Found");
        } });

      server.begin();
      Serial.println("HTTP server started.");
    }
    else
    {
      if (cur_time - last_time_connected >= 300)
      {
        last_time_connected = cur_time;
        Serial.print(".");
      }
      if (cur_time - start_attempt_time >= 30000)
      {
        cur_state = FAILED;
        last_time_failed = cur_time;
      }
    }
    break;

  case SUCCESS:
    server.handleClient();

    // Зчитуємо дані з DHT11 кожні 2 секунди
    if (cur_time - last_sensor_time >= 2000)
    {
      last_sensor_time = cur_time;
      float h = dht.readHumidity();
      float t = dht.readTemperature();

      if (!isnan(h) && !isnan(t))
      {
        humidity = h;
        temperature = t;
      }
    }
    break;

  case FAILED:
    if (cur_time - last_time_failed >= 10000)
    {
      ESP.restart();
    }
    break;

  default:
    break;
  }
}