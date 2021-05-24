#include "driver/rtc_io.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SPIFFS.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <rom/rtc.h>
#include "Adafruit_VEML6075.h"
#include "ThingSpeak.h"

#define BUTTON_PIN_BITMASK 0x8000

#define toSecondFactor 1000
hw_timer_t *timer = NULL;

#define TIMEOUT 5000

#define ENABLE 12
#define SDA 23
#define SCL 22
#define ONE_WIRE_BUS 16
#define windDir 34
#define GAL 32
#define GAU 33
#define GBL 25
#define GBU 26
#define RCLK 27
#define CCLR 13
#define CLK_INH 18
#define CNTR 35
#define SH_LD 2
#define CLK 4
#define BAT_V 36

#define BME280_SENSOR_ADDRESS (0x76)
Adafruit_BME280 bme;

Adafruit_VEML6075 uv = Adafruit_VEML6075();


OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);
uint8_t sensor1[8] = {0x28, 0xDC, 0xA2, 0x63, 0x2D, 0x19, 0x01, 0x8B};
uint8_t sensor2[8] = {0x28, 0x31, 0x58, 0x56, 0x2D, 0x19, 0x01, 0x3A};
uint8_t sensor3[8] = {0x28, 0xA5, 0xB2, 0x65, 0x2D, 0x19, 0x01, 0xF0};

SoftwareSerial softSerial(21, 19);

WiFiClient client;
WebServer server(80);

const char *badRequest = "<!DOCTYPE html><html lang='cs-cz'><head><meta http-equiv='refresh' content='3; url=http://192.168.4.1/settings' /><link rel='stylesheet' href='bootstrap.min.css'><meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'><title>Meteostanice</title></head><body><div class='container'><p><div class='row'><div class='col-sm-12'><h1>Meteostanice</h1><h3>Nevyplnili jste prislusna pole</h3><h6>Budete presmerovani</h6></div></div></p></div></body></html>";
const char *successfulRequest = "<!DOCTYPE html><html lang='cs-cz'><head><meta http-equiv=\'content-type\' content=\'text/html\' ; charset=\'utf-8\'><link rel='stylesheet' href='bootstrap.min.css'><meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'><title>Meteostanice</title></head><body><div class='container'><p><div class='row'><div class='col-sm-12'><h1>Meteostanice</h1><h3>Ulozeni uspesne</h3></div></div></p></div></body></html>";

//Default SSID and password to start AP
const char *ssid = "IoT system";
const char *password = "IoTsystem32";

bool bmeInitialized = false;
bool uvInitialized = false;

//Variables to store all data
float temp1;
float temp2;
float temp3;
float temperature;
float humidity;
double pressure;
float windSpd;
float rainAmmount;
float angle = 0;
String dir;
float moisture1;
float moisture2;
float moisture3;
float batteryVoltage;
float uvIndex;

bool configSaved = false;

//Variable which is set in ISR to true if reset button is pressed during the on time
volatile bool eraseConf = false;

volatile uint32_t count = 0;

//Set default mode of operation to 2, 1 means periodical wakeup to send data, 2 continuous mode
uint8_t mode = 2;
//If mode is set to 1,
uint8_t sendMode;
//Arrays to store credentials for HTTP Basic auth
char www_username[40];
char www_password[40];
//Arrays to store credentials to connect to WiFi or create AP
char ssid_in[40];
char password_in[40];
//Varible to store time to sleep in minutes
uint8_t sleepTime;
//Varibles to store Thingspeak channel IDs and API keys
unsigned long channelId1;
unsigned long channelId2;
char APIkey[20];
char APIkey2[20];
//Array to store URL or IP address of custom server to send data
char url_ip[50];
//Arrays to store depth of sensors to be shown on web page
char depth_Temperature_1[40] = "0";
char depth_Temperature_2[40] = "0";
char depth_Temperature_3[40] = "0";
char depth_Moisture_1[40] = "0";
char depth_Moisture_2[40] = "0";
char depth_Moisture_3[40] = "0";

void IRAM_ATTR timerISR()
{
  count++;
}

void IRAM_ATTR eraseConfISR()
{
  eraseConf = true;
}

void setup()
{
  pinMode(15, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(15), eraseConfISR, FALLING);
  // initialize serial port
  while (!Serial)
  {
  }
  getResetReason(rtc_get_reset_reason(0));
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  if (!SPIFFS.begin(true))
  {
    return;
  }

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0:
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      eraseConfig();
      esp_restart();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      break;
    default:
      {
        break;
      }
  }

  if (SPIFFS.exists("/mode.txt"))
  {
    configSaved = true;
    loadConfig();
  }

  if (mode == 2)
  {
    timer = timerBegin(0, 80, true);       
    timerAttachInterrupt(timer, &timerISR, true);
    timerAlarmWrite(timer, 1000, true);
    timerAlarmEnable(timer);
    createAP();
    prepareForRead();
  }
}

void loop()
{
  if (eraseConf)
  {
    eraseConfig();
    esp_restart();
  }

  if (mode == 1)
  {

    prepareForRead();
    delay(3000); //Wait a while to stabilize value returned by moisture sensors
    getAllData();
    sendData();
    if (eraseConf)
    {
      eraseConfig();
      esp_restart();
    }
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
    rtc_gpio_pullup_en(GPIO_NUM_15);
    rtc_gpio_pulldown_dis(GPIO_NUM_15);
    rtc_gpio_hold_en(GPIO_NUM_15);
    esp_sleep_enable_timer_wakeup(sleepTime * 60 * 1e6 - (millis() * 1000));
    esp_deep_sleep_start();
  }
  else
  {
    server.handleClient();
  }
}

void resetCounter()
{
  pinMode(CCLR, OUTPUT);
  digitalWrite(CCLR, LOW);
  delayMicroseconds(10);
  digitalWrite(CCLR, HIGH);
}

void createAP()
{
  WiFi.mode(WIFI_AP_STA);

  if (configSaved)
  {
    WiFi.softAP(ssid_in, password_in, 13, false, 4);
  }
  else
  {
    WiFi.softAP(ssid, password, 13, false, 4);
  }

  server.on("/", handleIndex);
  server.on("/settings", handleSettings);
  server.on("/json", handleJson);
  server.on("/getXML", sendXMLData);
  server.on("/getXMLWifi", sendXMLWifi);
  server.on("/uloz", saveConfig);
  server.on("/javascript.js", handleJavascript);
  server.on("/pure-min.css", handleCss);
  server.begin();
}

void sendData()
{
  uint16_t i = 0;
  char floatArray[20];
  snprintf(floatArray, sizeof floatArray, "%.2f", temperature);
  temperature = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", humidity);
  humidity = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2lf", pressure);
  pressure = atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", windSpd);
  windSpd = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", angle);
  angle = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", rainAmmount);
  rainAmmount = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", uvIndex);
  uvIndex = (float)atof(floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", temp1);
  temp1 = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", temp2);
  temp2 = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", temp3);
  temp3 = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", moisture1);
  moisture1 = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", moisture2);
  moisture2 = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", moisture3);
  moisture3 = (float)atof(floatArray);
  snprintf(floatArray, sizeof floatArray, "%.2f", batteryVoltage);
  batteryVoltage = (float)atof(floatArray);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_in, password_in);

  while ((WiFi.status() != WL_CONNECTED) && (i < 20000))
  {
    delay(1);
    i++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (sendMode == 1)
    {
      ThingSpeak.begin(client);
      ThingSpeak.setField(1, temperature);
      ThingSpeak.setField(2, humidity);
      ThingSpeak.setField(3, (float)pressure);
      ThingSpeak.setField(4, windSpd);
      ThingSpeak.setField(5, angle);
      ThingSpeak.setField(6, rainAmmount);
      ThingSpeak.setField(7, uvIndex);
      int x = ThingSpeak.writeFields(channelId1, APIkey);

      ThingSpeak.setField(1, temp1);
      ThingSpeak.setField(2, temp2);
      ThingSpeak.setField(3, temp3);
      ThingSpeak.setField(4, moisture1);
      ThingSpeak.setField(5, moisture2);
      ThingSpeak.setField(6, moisture3);
      ThingSpeak.setField(7, batteryVoltage);

      x = ThingSpeak.writeFields(channelId2, APIkey2);
    }
    else
    {
      String message;
      message = "teplota=";
      message += String(temperature, 1);
      message += "&vlhkost=";
      message += String(humidity, 1);
      message += "&tlak=";
      message += String(pressure, 1);
      message += "&rychlost_vetru=";
      message += String(windSpd, 1);
      message += "&smer_vetru=";
      message += String(angle, 1);

      message += "&srazky=";
      message += String(rainAmmount, 2);
      message += "&uvindex=";
      message += String(uvIndex, 1);
      message += "&teplota_pudy_";
      message += String(depth_Temperature_1);
      message += "cm=";
      message += String(temp1, 1);
      message += "&teplota_pudy_";
      message += String(depth_Temperature_2);
      message += "cm=";
      message += String(temp2, 1);
      message += "&teplota_pudy_";
      message += String(depth_Temperature_3);
      message += "cm=";
      message += String(temp3, 1);

      message += "&vlhkost_pudy_";
      message += String(depth_Moisture_1);
      message += "cm=";
      message += String(moisture1, 1);
      message += "&vlhkost_pudy_";
      message += String(depth_Moisture_2);
      message += "cm=";
      message += String(moisture2, 1);
      message += "&vlhkost_pudy_";
      message += String(depth_Moisture_3);
      message += "cm=";
      message += String(moisture3, 1);
      message += "&napeti_baterie=";
      message += String(batteryVoltage, 2);

      if (client.connect(url_ip, 80))
      {
        client.print("POST /update HTTP/1.1\n");
        client.print("Connection: close\n");
        client.print("Content-Type: application/x-www-form-urlencoded\n");
        client.print("Content-Length: ");
        client.print(message.length());
        client.print("\n\n");
        client.print(message);

        String answer = getResponse();
      }
      else
      {
        //Error connecting to server
      }
    }
  }
  else
  {
    //Error connecting to WiFi
  }
}

String getResponse()
{
  String response;
  long startTime = millis();

  delay(200);
  while (client.available() < 1 && ((millis() - startTime) < TIMEOUT))
  {
    delay(5);
  }

  if (client.available() > 0)
  {
    char symbol;
    do
    {
      symbol = client.read();
      response += symbol;
    } while (client.available() > 0);
  }
  client.stop();

  return response;
}

void handleIndex()
{
  File htmlFile = SPIFFS.open("/index.html", "r");
  server.streamFile(htmlFile, "text/html");
  htmlFile.close();
}

void handleJavascript()
{
  File javascriptFile = SPIFFS.open("/javascript.js", "r");
  server.streamFile(javascriptFile, "text/javascript");
  javascriptFile.close();
}

void handleCss()
{
  File cssFile = SPIFFS.open("/pure-min.css", "r");
  server.streamFile(cssFile, "text/css");
  cssFile.close();
}

void handleSettings()
{
  const char *www_username_default = "admin";
  const char *www_password_default = "esp32";
  if (!configSaved)
  {
    if (!server.authenticate(www_username_default, www_password_default))
    {
      return server.requestAuthentication();
    }
    File settings = SPIFFS.open("/settings.html", "r");
    server.streamFile(settings, "text/html");
    settings.close();
  }
  else
  {
    if (!server.authenticate(www_username, www_password))
    {
      return server.requestAuthentication();
    }
    File settings = SPIFFS.open("/settings.html", "r");
    server.streamFile(settings, "text/html");
    settings.close();
  }
}

void handleJson()
{
  getAllData();
  char jsonBuffer[500];
  char floatArray[10];
  snprintf(floatArray, sizeof floatArray, "%.2f", temperature);
  strcpy(jsonBuffer, "{\"teplota:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", humidity);
  strcat(jsonBuffer, ", \"vlhkost:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", pressure);
  strcat(jsonBuffer, ", \"tlak:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", windSpd);
  strcat(jsonBuffer, ", \"rychlost vetru:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", angle);
  strcat(jsonBuffer, ", \"smer vetru:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", rainAmmount);
  strcat(jsonBuffer, ", \"mnozstvi srazek:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.1f", uvIndex);
  strcat(jsonBuffer, ", \"uv index:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", temp1);
  strcat(jsonBuffer, ", \"teplota pudy ");
  strcat(jsonBuffer, depth_Temperature_1);
  strcat(jsonBuffer, " cm:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", temp2);
  strcat(jsonBuffer, ", \"teplota pudy ");
  strcat(jsonBuffer, depth_Temperature_2);
  strcat(jsonBuffer, " cm:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", temp3);
  strcat(jsonBuffer, ", \"teplota pudy ");
  strcat(jsonBuffer, depth_Temperature_3);
  strcat(jsonBuffer, " cm:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", moisture1);
  strcat(jsonBuffer, ", \"vlhkost pudy ");
  strcat(jsonBuffer, depth_Moisture_1);
  strcat(jsonBuffer, " cm:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", moisture2);
  strcat(jsonBuffer, ", \"vlhkost pudy ");
  strcat(jsonBuffer, depth_Moisture_2);
  strcat(jsonBuffer, " cm:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", moisture3);
  strcat(jsonBuffer, ", \"vlhkost pudy ");
  strcat(jsonBuffer, depth_Moisture_3);
  strcat(jsonBuffer, " cm:\" ");
  strcat(jsonBuffer, floatArray);

  snprintf(floatArray, sizeof floatArray, "%.2f", batteryVoltage);
  strcat(jsonBuffer, ", \"napeti baterie:\" ");
  strcat(jsonBuffer, floatArray);
  strcat(jsonBuffer, "}");
  server.send(200, "application/json; charset=utf-8", jsonBuffer);
}

void sendXMLData()
{
  getAllData();
  char xmlBuffer[800];
  char floatArray[10];

  strcpy(xmlBuffer, "<?xml version = \"1.0\" ?><inputs>");

  snprintf(floatArray, sizeof floatArray, "%.2f", temperature);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", humidity);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", pressure);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");

  snprintf(floatArray, sizeof floatArray, "%.2f", windSpd);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");

  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, dir.c_str());
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", rainAmmount);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");

  snprintf(floatArray, sizeof floatArray, "%.2f", uvIndex);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");

  snprintf(floatArray, sizeof floatArray, "%.2f", temp1);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", temp2);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", temp3);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", moisture1);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", moisture2);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");
  snprintf(floatArray, sizeof floatArray, "%.2f", moisture3);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");

  snprintf(floatArray, sizeof floatArray, "%.2f", batteryVoltage);
  strcat(xmlBuffer, "<reading>");
  strcat(xmlBuffer, floatArray);
  strcat(xmlBuffer, "</reading>\n");

  strcat(xmlBuffer, "<placeholder>");
  strcat(xmlBuffer, depth_Temperature_1);
  strcat(xmlBuffer, "</placeholder>\n");
  strcat(xmlBuffer, "<placeholder>");
  strcat(xmlBuffer, depth_Temperature_2);
  strcat(xmlBuffer, "</placeholder>\n");
  strcat(xmlBuffer, "<placeholder>");
  strcat(xmlBuffer, depth_Temperature_3);
  strcat(xmlBuffer, "</placeholder>\n");
  strcat(xmlBuffer, "<placeholder>");
  strcat(xmlBuffer, depth_Moisture_1);
  strcat(xmlBuffer, "</placeholder>\n");
  strcat(xmlBuffer, "<placeholder>");
  strcat(xmlBuffer, depth_Moisture_2);
  strcat(xmlBuffer, "</placeholder>\n");
  strcat(xmlBuffer, "<placeholder>");
  strcat(xmlBuffer, depth_Moisture_3);
  strcat(xmlBuffer, "</placeholder>\n");

  strcat(xmlBuffer, "</inputs>");
  server.send(200, "text/xml", xmlBuffer);
}

void sendXMLWifi()
{
  char charBuffer[800] = "\0";
  char xmlBuffer[850];
  char RSSIbuffer[10];

  int n = WiFi.scanComplete();
  if (n == -2)
  {
    WiFi.scanNetworks(true, true);
    strcpy(xmlBuffer, "<?xml version = \"1.0\" ?><inputs>");
    strcat(xmlBuffer, "<reading>");
    strcat(xmlBuffer, "Scanning");
    strcat(xmlBuffer, "</reading>\n");
    strcat(xmlBuffer, "</inputs>");
    server.send(200, "text/xml", xmlBuffer);
  }
  else if (n == -1)
  {
    strcpy(xmlBuffer, "<?xml version = \"1.0\" ?><inputs>");
    strcat(xmlBuffer, "<reading>");
    strcat(xmlBuffer, "Scanning...");
    strcat(xmlBuffer, "</reading>\n");
    strcat(xmlBuffer, "</inputs>");
    server.send(200, "text/xml", xmlBuffer);
  }
  else if (n)
  {
    for (int i = 0; i < n; i++)
    {
      if (WiFi.SSID(i) == "")
      {
        strcat(charBuffer, "*HIDDEN_SSID*");
      }
      else
      {
        strcat(charBuffer, WiFi.SSID(i).c_str());
      }
      strcat(charBuffer, "  Sila signalu: ");
      itoa(WiFi.RSSI(i), RSSIbuffer, 10);
      strcat(charBuffer, RSSIbuffer);
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
      {
        strcat(charBuffer, " dBm&lt;br&gt;");
      }
      else
      {
        strcat(charBuffer, " dBm &#x1F512;&lt;br&gt;");
      }
    }
    strcpy(xmlBuffer, "<?xml version = \"1.0\" ?><inputs>");
    strcat(xmlBuffer, "<reading>");
    strcat(xmlBuffer, charBuffer);
    strcat(xmlBuffer, "</reading>\n");
    strcat(xmlBuffer, "</inputs>");
    server.send(200, "text/xml", xmlBuffer);
    WiFi.scanDelete();
    if (WiFi.scanComplete() == -2)
    {
      WiFi.scanNetworks(true, true);
    }
  }
}

void prepareForRead()
{

  pinMode(windDir, INPUT);
  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, HIGH);
  delay(10);
  softSerial.begin(9600);
  Wire.begin(SDA, SCL);
  delay(10);
  sensors.begin();
  if (!bme.begin(BME280_SENSOR_ADDRESS))
  {
  }
  else
  {
    bme.setSampling(Adafruit_BME280::MODE_FORCED);
    bmeInitialized = true;
  }

  if (!uv.begin())
  {
  }
  else
  {
    uvInitialized = true;
  }

  while (softSerial.available() > 0)
  {
    softSerial.read();
  }

  digitalWrite(GAL, HIGH);
  digitalWrite(GAU, HIGH);
  digitalWrite(GBL, HIGH);
  digitalWrite(GBU, HIGH);
  pinMode(GAL, OUTPUT);
  pinMode(GAU, OUTPUT);
  pinMode(GBL, OUTPUT);
  pinMode(GBU, OUTPUT);

  digitalWrite(GAL, HIGH);
  digitalWrite(GAU, HIGH);
  digitalWrite(GBL, HIGH);
  digitalWrite(GBU, HIGH);

  pinMode(RCLK, OUTPUT);
  digitalWrite(RCLK, LOW);
  digitalWrite(CCLR, HIGH);
  pinMode(CCLR, OUTPUT);
  digitalWrite(CCLR, HIGH);

  pinMode(CLK, OUTPUT);
  pinMode(CLK_INH, OUTPUT);
  digitalWrite(CLK_INH, LOW);
  digitalWrite(SH_LD, HIGH);
  pinMode(SH_LD, OUTPUT);
  digitalWrite(SH_LD, HIGH);
  pinMode(CNTR, INPUT);
}

void getAllData()
{
  sensors.requestTemperatures();
  sensors.requestTemperatures();
  temp1 = getTemperature(sensor1);
  temp2 = getTemperature(sensor2);
  temp3 = getTemperature(sensor3);
  getAtmosphericData();
  getWindDirection();

  if (uvInitialized)
  {
    uvIndex = uv.readUVI();
  }
  else
  {
    uvIndex = 9999.99;
  }

  getWindSpd_Rain();
  batteryVoltage = getBatteryVoltage();

  moisture1 = getSoilMoisture(1);
  moisture2 = getSoilMoisture(2);
  moisture3 = getSoilMoisture(3);
}

void getAtmosphericData()
{
  if (bmeInitialized)
  {
    bme.takeForcedMeasurement();
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = (bme.readPressure() / 100.0F);
  }
  else
  {
    temperature = 9999.99;
    humidity = 9999.99;
    pressure = 9999.99;
  }
}

void getWindSpd_Rain()
{
  byte incoming;
  uint16_t windPulses;
  uint16_t rainPulses;

  digitalWrite(RCLK, HIGH);
  delayMicroseconds(10);
  digitalWrite(RCLK, LOW);

  delayMicroseconds(10);
  digitalWrite(GAU, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, HIGH);
  delayMicroseconds(10);
  digitalWrite(CLK, LOW);
  delayMicroseconds(10);
  incoming = shiftIn(CNTR, CLK, MSBFIRST);
  windPulses = incoming << 8;
  digitalWrite(GAU, HIGH);
  delayMicroseconds(10);

  digitalWrite(GAL, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, HIGH);
  delayMicroseconds(10);
  digitalWrite(CLK, LOW);
  delayMicroseconds(10);
  incoming = shiftIn(CNTR, CLK, MSBFIRST);
  windPulses = windPulses + incoming;
  digitalWrite(GAL, HIGH);
  delayMicroseconds(10);

  digitalWrite(GBU, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, HIGH);
  delayMicroseconds(10);
  digitalWrite(CLK, LOW);
  delayMicroseconds(10);
  incoming = shiftIn(CNTR, CLK, MSBFIRST);
  rainPulses = incoming << 8;
  digitalWrite(GBU, HIGH);
  delayMicroseconds(10);

  digitalWrite(GBL, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, LOW);
  delayMicroseconds(10);
  digitalWrite(SH_LD, HIGH);
  delayMicroseconds(10);
  digitalWrite(CLK, LOW);
  delayMicroseconds(10);
  incoming = shiftIn(CNTR, CLK, MSBFIRST);
  rainPulses = rainPulses + incoming;
  digitalWrite(GBL, HIGH);
  delayMicroseconds(10);

  digitalWrite(CCLR, LOW);
  delayMicroseconds(10);
  digitalWrite(CCLR, HIGH);
  digitalWrite(CLK, LOW);

  getWindSpd(windPulses);
  getRainAmmount(rainPulses);
}

void getWindSpd(uint16_t windPulses)
{
  if (mode == 1)
  {
    windSpd = (windPulses / (double)(sleepTime * 60.0) * 2.4);
  }
  else
  {
    windSpd = (windPulses / (double)(count / toSecondFactor) * 2.4);
  }
  count = 0;
}

void getRainAmmount(uint16_t rainPulses)
{
  rainAmmount = (rainPulses * 0.28);
}

void getWindDirection()
{
  float directionDegrees[] = {112.5, 67.5, 90, 157.5, 135, 202.5, 180, 22.5, 45, 247.5, 225, 337.5, 0, 292.5, 315, 270};
  String directionString[] = {"ESE", "ENE", "E", "SSE", "SE", "SSW", "S", "NNE", "NE", "WSW", "SW", "NNW", "N", "WNW", "NW", "W"};
  int min[] = {200, 315, 371, 466, 652, 891, 1072, 1422, 1730, 2108, 2500, 2659, 2986, 3227, 3429, 3665};
  int max[] = {314, 369, 465, 651, 890, 1071, 1421, 1729, 2107, 2499, 2658, 2985, 3226, 3428, 3664, 4000};
  bool error = true;
  uint16_t incoming2 = analogRead(windDir);
  uint16_t incoming_dir = incoming2 + linearize(incoming2);
  for (int i = 0; i <= 15; i++)
  {
    if (incoming_dir >= min[i] && incoming_dir <= max[i])
    {
      dir = directionString[i];
      angle = directionDegrees[i];
      error = false;
      break;
    }
  }
  if (error == true)
  {
    dir = "CHYBA";
    angle = 99.9;
  };
}

double linearize(double x)
{
  return 209.97 - 2.002e-03 * x + 1.289e-04 * pow(x, 2) - 2.049e-07 * pow(x, 3) + 1.275e-10 * pow(x, 4) - 3.339e-14 * pow(x, 5) + 2.996e-18 * pow(x, 6);
}

float getTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  return tempC;
}

float getSoilMoisture(char address)
{
  float moisture;
  char start_symbol = '#';
  char incoming;
  bool received = false;
  bool success = false;
  int address_in;
  char start_symbol_in;
  char receivedData[8];
  byte n = 0;
  uint16_t adcvalue = 9999;
  uint8_t i = 0;

  while (i < 3 && !success)
  {
    n = 0;
    while (softSerial.available() > 0)
    {
      softSerial.read();
    }

    softSerial.print(start_symbol);
    softSerial.print(address);
    delay(30 + 5 * i);

    if (softSerial.available() > 0)
    {
      delay(5);
      while (softSerial.available() > 0)
      {
        incoming = softSerial.read();
        receivedData[n] = incoming;
        n++;
        received = true;
      }
    }
    if (received == true)
    {
      received = false;
      start_symbol_in = receivedData[0];
      address_in = receivedData[1];

      if (start_symbol_in == '#')
      {
        if (address_in == address)
        {
          adcvalue = ((receivedData[2] << 8) | receivedData[3]);
          if ((adcvalue >= 0) && (adcvalue <= 1023))
          {
            success = true;
          }
          else
          {
            //Error in received data
          }
        }
        else
        {
          //Wrong address
        }
      }
    }
    i++;
  }
  if (success == true)
  {
    moisture = calculateSoilMoisture(adcvalue, (uint8_t)address);
    return moisture;
  }
  else
  {
    return 9999.99;
  }
}

float calculateSoilMoisture(uint16_t adc, uint8_t sensor)
{

  float moisture;
  switch (sensor)
  {
    case 1:
      {
        float a = 1.72e+07;
        float b = 2.205732229;
        double value = pow(adc, -b);
        moisture = (double)(a * value);
        break;
      }

    case 2:
      {
        float a = 1.61e+07;
        float b = 2.19157717;
        double value = pow(adc, -b);
        moisture = (double)(a * value);
        break;
      }

    case 3:
      {
        float a = 3.03e+07;
        float b = 2.29062659;
        double value = pow(adc, -b);
        moisture = (double)(a * value);
        break;
      }
    default:

      moisture = 999;
      break;
  }

  if (moisture < 0)
  {
    moisture = 0;
  }
  else if ((moisture > 40) && (moisture != 999))
  {
    moisture = 40;
  }
  return moisture;
}

float getBatteryVoltage()
{
  float batADC = analogRead(BAT_V) + 145.765;
  float batVoltage = (batADC / 4095.0 * 3.3 * 2.066116);
  return batVoltage;
}

void saveConfig()
{

  eraseConfig();
  bool success = false;

  String ssid_in = "";
  String password_in = "";
  String www_username = "";
  String www_password = "";

  char channelID[20];
  char channelID2[20];

  String depth_Temperature_1;
  String depth_Temperature_2;
  String depth_Temperature_3;
  String depth_Moisture_1;
  String depth_Moisture_2;
  String depth_Moisture_3;

  if ((server.arg("www_username") != "") && (server.arg("www_password") != "") && (server.arg("ssid_i") != "") && (server.arg("password_i") != ""))
  {
    mode = server.arg("rezim").toInt();
    if (mode == 1)
    {
      sendMode = server.arg("rezimZasilani").toInt();
      if (sendMode == 1)
      {
        if (server.arg("apikey") != "" && (server.arg("apikey2") != ""))
        {
          www_username = server.arg("www_username");
          www_password = server.arg("www_password");
          ssid_in = server.arg("ssid_i");
          password_in = server.arg("password_i");
          sleepTime = server.arg("intervalZasilani").toInt();
          strcpy(APIkey, server.arg("apikey").c_str());
          strcpy(APIkey2, server.arg("apikey2").c_str());
          strcpy(channelID, server.arg("channelID").c_str());
          strcpy(channelID2, server.arg("channelID2").c_str());
          success = true;
        }
        else
        {
          server.send(400, "text/html", badRequest);
        }
      }
      else
      {
        if ((server.arg("url_ip") != "") && (server.arg("teplota_p1") != "") && (server.arg("teplota_p2") != "") && (server.arg("teplota_p3") != "") && (server.arg("vlhkost_p1") != "") && (server.arg("vlhkost_p2") != "") && (server.arg("vlhkost_p3") != ""))
        {
          www_username = server.arg("www_username");
          www_password = server.arg("www_password");
          ssid_in = server.arg("ssid_i");
          password_in = server.arg("password_i");
          sleepTime = server.arg("intervalZasilani").toInt();
          strcpy(url_ip, server.arg("url_ip").c_str());
          depth_Temperature_1 = server.arg("teplota_p1");
          depth_Temperature_2 = server.arg("teplota_p2");
          depth_Temperature_3 = server.arg("teplota_p3");
          depth_Moisture_1 = server.arg("vlhkost_p1");
          depth_Moisture_2 = server.arg("vlhkost_p2");
          depth_Moisture_3 = server.arg("vlhkost_p3");
          success = true;
        }
        else
        {
          server.send(400, "text/html", badRequest);
        }
      }
    }
    else
    {
      if ((server.arg("teplota_p1") != "") && (server.arg("teplota_p2") != "") && (server.arg("teplota_p3") != "") && (server.arg("vlhkost_p1") != "") && (server.arg("vlhkost_p2") != "") && (server.arg("vlhkost_p3") != ""))
      {
        www_username = server.arg("www_username");
        www_password = server.arg("www_password");
        ssid_in = server.arg("ssid_i");
        password_in = server.arg("password_i");
        depth_Temperature_1 = server.arg("teplota_p1");
        depth_Temperature_2 = server.arg("teplota_p2");
        depth_Temperature_3 = server.arg("teplota_p3");
        depth_Moisture_1 = server.arg("vlhkost_p1");
        depth_Moisture_2 = server.arg("vlhkost_p2");
        depth_Moisture_3 = server.arg("vlhkost_p3");
        success = true;
      }
      else
      {
        server.send(400, "text/html", badRequest);
      }
    }
  }

  if (success)
  {
    server.send(200, "text/html", successfulRequest);
    File writeData = SPIFFS.open("/mode.txt", FILE_WRITE);
    if (writeData)
    {
      writeData.println(mode);
    }
    writeData.close();
    if (mode == 1)
    {
      File writeData = SPIFFS.open("/sendMode.txt", FILE_WRITE);
      if (writeData)
      {
        writeData.println(sendMode);
      }
      writeData.close();
      if (sendMode == 1)
      {
        File writeData = SPIFFS.open("/thingspeak.txt", FILE_WRITE);
        if (writeData)
        {
          writeData.println(www_username);
          writeData.println(www_password);
          writeData.println(ssid_in);
          writeData.println(password_in);
          writeData.println(sleepTime);
          writeData.println(APIkey);
          writeData.println(channelID);
          writeData.println(APIkey2);
          writeData.println(channelID2);
        }
        writeData.close();
      }
      else
      {
        File writeData = SPIFFS.open("/ownServer.txt", FILE_WRITE);
        if (writeData)
        {
          writeData.println(www_username);
          writeData.println(www_password);
          writeData.println(ssid_in);
          writeData.println(password_in);
          writeData.println(sleepTime);
          writeData.println(url_ip);
          writeData.println(depth_Temperature_1);
          writeData.println(depth_Temperature_2);
          writeData.println(depth_Temperature_3);
          writeData.println(depth_Moisture_1);
          writeData.println(depth_Moisture_2);
          writeData.println(depth_Moisture_3);
        }
        writeData.close();
      }
    }
    else
    {
      File writeData = SPIFFS.open("/serverMode.txt", FILE_WRITE);
      if (writeData)
      {
        writeData.println(www_username);
        writeData.println(www_password);
        writeData.println(ssid_in);
        writeData.println(password_in);
        writeData.println(depth_Temperature_1);
        writeData.println(depth_Temperature_2);
        writeData.println(depth_Temperature_3);
        writeData.println(depth_Moisture_1);
        writeData.println(depth_Moisture_2);
        writeData.println(depth_Moisture_3);
      }
      writeData.close();
    }
    esp_restart();
  }
}

void loadConfig()
{
  String reader;
  File readData = SPIFFS.open("/mode.txt", FILE_READ);
  if (readData)
  {
    reader = readData.readStringUntil('n');
    reader.replace("\r", "");
  }
  readData.close();
  mode = reader.toInt();
  if (mode == 1)
  {
    File readData = SPIFFS.open("/sendMode.txt", FILE_READ);
    if (readData)
    {
      reader = readData.readStringUntil('n');
      reader.replace("\r", "");
    }
    readData.close();
    sendMode = reader.toInt();

    if (sendMode == 1)
    {
      File readData = SPIFFS.open("/thingspeak.txt", FILE_READ);
      if (readData)
      {
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(www_username, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(www_password, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(ssid_in, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(password_in, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        sleepTime = reader.toInt();
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(APIkey, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        channelId1 = reader.toInt();
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(APIkey2, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        channelId2 = reader.toInt();
      }
      readData.close();
    }
    else
    {
      File readData = SPIFFS.open("/ownServer.txt", FILE_READ);
      if (readData)
      {
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(www_username, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(www_password, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(ssid_in, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(password_in, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        sleepTime = reader.toInt();
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(url_ip, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(depth_Temperature_1, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(depth_Temperature_2, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(depth_Temperature_3, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(depth_Moisture_1, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(depth_Moisture_2, reader.c_str());
        reader = readData.readStringUntil('\n');
        reader.replace("\r", "");
        strcpy(depth_Moisture_3, reader.c_str());
      }
      readData.close();
    }
  }
  else
  {
    File readData = SPIFFS.open("/serverMode.txt", FILE_READ);
    if (readData)
    {
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(www_username, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(www_password, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(ssid_in, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(password_in, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(depth_Temperature_1, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(depth_Temperature_2, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(depth_Temperature_3, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(depth_Moisture_1, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(depth_Moisture_2, reader.c_str());
      reader = readData.readStringUntil('\n');
      reader.replace("\r", "");
      strcpy(depth_Moisture_3, reader.c_str());
    }
    readData.close();
  }
}

void eraseConfig()
{
  if (SPIFFS.exists("/mode.txt"))
  {
    SPIFFS.remove("/mode.txt");
  }
  if (SPIFFS.exists("/sendMode.txt"))
  {
    SPIFFS.remove("/sendMode.txt");
  }
  if (SPIFFS.exists("/thingspeak.txt"))
  {
    SPIFFS.remove("/thingspeak.txt");
  }
  if (SPIFFS.exists("/ownServer.txt"))
  {
    SPIFFS.remove("/ownServer.txt");
  }
  if (SPIFFS.exists("/serverMode.txt"))
  {
    SPIFFS.remove("/serverMode.txt");
  }
}

void getResetReason(RESET_REASON reason)
{
  switch (reason)
  {
    case 1:
      resetCounter();
      break;
    default:
      break;
  }
}
