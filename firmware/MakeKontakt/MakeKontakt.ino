#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <WiFiClientSecure.h>

#include "mySparkFun_APDS9960.h"

#include "kungFuHue.h"
#include "driver_Hue.h"
#include "driver_Wiz.h"
#include "driver_Yeelight.h"
#include "driver_Lifx.h"

//#define DEBUG

#define PIXELPIN          13
#define BUTTONPIN         12

#define AP_SSID "kungFuHue"
#define AP_PASS "myHuePassword"


#define EEPROM_SSID_IDX           0
#define EEPROM_PASS_IDX           (EEPROM_SSID_IDX + 64)
#define EEPROM_COLORMAP_IDX       (EEPROM_PASS_IDX + 64)
#define EEPROM_LEDBRIGHTNESS_IDX  (EEPROM_COLORMAP_IDX + 3*NDEVICES)
#define EEPROM_ORIENTATION_IDX    (EEPROM_LEDBRIGHTNESS_IDX + 1)
#define EEPROM_DRIVER_IDX         (EEPROM_ORIENTATION_IDX + 1)

// Hue data structure space in EEPROM
#define EEPROM_HUEDATA_IDX        (EEPROM_DRIVER_IDX + 1)

// WiZ data structure space in EEPROM
#define EEPROM_WIZDATA_IDX        (EEPROM_HUEDATA_IDX + sizeof(hueDriverData_t))

// Yeelight data structure space in EEPROM
#define EEPROM_YEELIGHTDATA_IDX   (EEPROM_WIZDATA_IDX + sizeof(wizDriverData_t))

// LIFX data structure space in EEPROM
#define EEPROM_LIFXDATA_IDX       (EEPROM_YEELIGHTDATA_IDX + sizeof(yeelightDriverData_t))

// Know where useful data ends, just in case we want to add stuff later on
#define EEPROM_END                (EEPROM_LIFXDATA_IDX + sizeof(lifxDriverData_t))


#define MALLOCBUF_SIZE            512

#define DISPLAY_MODE_MENU             0
#define DISPLAY_MODE_IP               1
#define DISPLAY_MODE_IP_PAUSE         2
#define DISPLAY_MODE_RSSI             3
#define DISPLAY_MODE_GAME             4


// we will do math on these to figure out what
// each gesture means taking the orientation
// of the device into consideration. So for
// orientation = 2 (upside down) we will use
// "gesture - 2" to figure out which action
// should be taken.
#define SWIPE_UP      0
#define SWIPE_RIGHT   (SWIPE_UP + 1)
#define SWIPE_DOWN    (SWIPE_UP + 2)
#define SWIPE_LEFT    (SWIPE_UP + 3)
#define SWIPE_IN      100
#define SWIPE_OUT     101

// Global Variables
SparkFun_APDS9960 apds = SparkFun_APDS9960();

ESP8266WebServer server(80);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, PIXELPIN, NEO_GRB + NEO_KHZ800);

uint8_t driver;

uint8_t displayMode;

uint32_t colorMap[NDEVICES];
uint8_t ledBrightness;

// Orientation: n*90 degrees, n in [0..3]
uint8_t orientation;

const uint8_t msDelay = 35;

uint8_t activeParam;

discoverParams_t allLights[NDISCOVERLIGHTS];
uint8_t loadAllParams;


unsigned long lastActionTime;
unsigned long currTime;
unsigned long displayModeTime;
char showIpString[32];
uint8_t showIpStringPos;


// Hue variables
uint8_t discoveryState;
uint8_t newUserState;
int newHueUserReturnValue;
char newHueUser[64];
char discoveryUrl[64];


int mod(int a, int b)
{
    int r;
    r = a % b;
    return r < 0 ? r + b : r;
}

void roll(int8_t ledStart)
{
  int8_t i;
  int8_t ledA, ledB;
  uint32_t colorA, colorB;
  uint8_t device;
  
  device = getCurrentDevice();

  for (i = 0; i < 9; i++)
  {
    ledA = ledStart + i, 16;
    ledB = ledStart - i, 16;
    colorA = getColor(ledA);
    colorB = getColor(ledB);
    setColor(ledA, colorMap[device]);
    setColor(ledB, colorMap[device]);
    strip.show();
    delay(msDelay);
    setColor(ledA, colorA);
    setColor(ledB, colorB);
    strip.show();
  }
}

// dir = 1 --> right
// dir = -1 --> left
void twist(int8_t dir)
{
  int8_t ledA, ledB;
  uint32_t colorA, colorB;
  uint8_t device;
  
  device = getCurrentDevice();
  
  uint8_t i;
  for (i = 0; i < 17; i++)
  {
    ledA = i*dir;
    ledB = i*dir + 8;
    colorA = getColor(ledA);
    colorB = getColor(ledB);
    setColor(ledA, colorMap[device]);
    setColor(ledB, colorMap[device]);
    strip.show();
    delay(msDelay);
    setColor(ledA, colorA);
    setColor(ledB, colorB);
    strip.show();
  }
}

void swipe(uint8_t dir)
{
  // We give the roll function the LED number where the
  // animation begins. dir parameter is one of the
  // SWIPE_x constants that are defined in order, UP = 0,
  // RIGHT, DOWN, LEFT. Thus mod(dir+2, 4) is the opposite
  // direction, where we want to start the animation.
  // Multiply by 4 because we have 16 LEDs for 4 directions.
  roll(mod(dir + 2, 4) * 4);
}


void changeDevice(int8_t dir)
{
  int8_t oldDevice, newDevice;
  uint8_t ledStart;
  int8_t i;

  oldDevice = getCurrentDevice();
  newDevice = oldDevice;

  // Figure out if change is possible and set newDevice accordingly
  if (dir == 1)
  {
    for (i = oldDevice + 1; i < NDEVICES; i++)
    {
      if (getDeviceEnabled(i) != 0)
        break;
    }
    if (i < NDEVICES)
    {
      // Change possible
      newDevice = i;
    }
  }
  else if (dir == -1)
  {
    for (i = oldDevice - 1; i >= 0; i--)
    {
      if (getDeviceEnabled(i) != 0)
        break;
    }
    if (i >= 0)
    {
      // Change possible
      newDevice = i;
    }
  }

  // Actually make the change in the driver
  setCurrentDevice(newDevice);
  
  if (dir == 1)
  {
    ledStart = 12;
  }
  else if (dir == -1)
  {
    ledStart = 4;
  }

  // First part of the animation
  for (i = 0; i < 9; i++)
  {
    setColor(ledStart + i, colorMap[oldDevice]);
    setColor(ledStart - i, colorMap[oldDevice]);
    strip.show();
    delay(msDelay);
  }

  if (newDevice != oldDevice)
  {
    // Continue animation to completion
    for (i = 0; i < 9; i++)
    {
      setColor(ledStart + i, colorMap[newDevice]);
      setColor(ledStart - i, colorMap[newDevice]);
      strip.show();
      delay(msDelay);
    }
    for (i = 0; i < 9; i++)
    {
      setColor(ledStart + i, 0);
      setColor(ledStart - i, 0);
      strip.show();
      delay(msDelay);
    }
  }
  else
  {
    // Need to bounce back
    for (i = 0; i < 9; i++)
    {
      setColor(ledStart + i + 8, 0);
      setColor(ledStart - i + 8, 0);
      strip.show();
      delay(msDelay);
    }
  }
}

void splitPostValues(char *post, char** keys, char** values, int *nKeys)
{
  char ampersand[] = "&";
  char equal[] = "=";
  char *aKey, *aValue;
  int n;
  n = 0;

  aKey = strtok(post, equal);
  if (aKey != NULL) aValue = strtok(NULL, ampersand);
  while (aKey != NULL && aValue != NULL)
  {
    keys[n] = aKey;
    values[n] = aValue;
    n++;
    aKey = strtok(NULL, equal);
    if (aKey != NULL) aValue = strtok(NULL, ampersand);
  }
  *nKeys = n;
}

void getKeyValue(char** keys, char** values, int nKeys, char* key, char** value)
{
  int i;
  for (i = 0; i < nKeys; i++)
  {
    if (strcmp(keys[i], key) == 0)
    {
      *value = values[i];
      return;
    }
  }
  *value = NULL;
}

void serverSendBegin()
{
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "<html>");
}

void serverSendChunk(char * str)
{
  server.sendContent(str);
}

void serverSendChunk(const __FlashStringHelper * str)
{
  server.sendContent(str);
}

void serverSendEnd()
{
  server.sendContent(F("</html>"));
  server.sendContent(F(""));
}

void handle_config_home()
{
  char *buf;
  buf = (char*)malloc(MALLOCBUF_SIZE);
  serverSendBegin();
  serverSendChunk(F("<body><h1>kungFuHue config</h1><a href='/wifi'>Config WiFi</a><br><br><a href='/driver'>Select driver</a><br><br><a href='/hue'>Config Hue Bridge connection</a><br><br><a href='/devices'>Config devices</a><br><br><a href='/bri'>Config screen brightness</a><br><br><a href='/orientation'>Config orientation</a><br><br><a href='/rssi'>Show WiFi signal strength</a><br><br><a href='/findme'>Find me!</a><br><br><a href='/game'>Toggle game mode</a><br><br><a href='/restart'>Reboot device</a><br><br>"));
  sprintf_P(buf, (PGM_P)F("FW Timestamp = %s<br><br>ESP.getFreeHeap() = %d<br><br>ESP.getFreeContStack() = %d (watermark)<br><br>uptime = %d s</body>"), __TIMESTAMP__, ESP.getFreeHeap(), ESP.getFreeContStack(), millis()/1000);
  serverSendChunk(buf);
  serverSendEnd();
  free(buf);
}

void handle_rssi()
{
  serverSendBegin();
  serverSendChunk(F("<head><meta http-equiv=\"refresh\" content=\"3; url='/rssi'\"/></head><body><p>Device now indicates WiFi signal strength.</p><br><br><a href='/'>Back to main</a></body>"));
  serverSendEnd();
  displayMode = DISPLAY_MODE_RSSI;
  displayModeTime = millis();
}

void handle_game()
{
  serverSendBegin();
  if (displayMode != DISPLAY_MODE_GAME)
  {
    serverSendChunk(F("<body><p>Game mode on!</p><br><br><a href='/'>Back to main</a></body>"));
    displayMode = DISPLAY_MODE_GAME;
  }
  else
  {
    serverSendChunk(F("<body><p>Game mode off</p><br><br><a href='/'>Back to main</a></body>"));
    displayMode = DISPLAY_MODE_MENU;
  }
  serverSendEnd();
  displayModeTime = millis();
  lastActionTime = displayModeTime;
}

void handle_find_me()
{
  handle_config_home();
  showWink();
}

void handle_restart()
{
  serverSendBegin();
  serverSendChunk(F("<head><meta http-equiv=\"refresh\" content=\"10; url='/'\"/></head><body><p>Please wait, device is restarting.</p></body>"));
  serverSendEnd();
  delay(500);
  ESP.restart();
}

void handle_config_orientation()
{
  char *orient;
  char *keys[32];
  char *values[32];
  int nKeys, i, n;
  char *buf;

  buf = (char*)malloc(MALLOCBUF_SIZE);

  serverSendBegin();
  
  if (server.hasArg("plain") == false)
  {
    serverSendChunk(F("<body><h1>kungFuHue orientation config</h1><form action='' method='post'> <label for='orientation'>Orientation:</label><select id='orientation' name='orientation'>"));
    for (i = 0; i < 4; i++)
    {
      if (orientation == i)
      {
        sprintf_P(buf, (PGM_P)F("<option value='%d' selected>%d</option>"), i + 100, i*90);
      }
      else
      {
        sprintf_P(buf, (PGM_P)F("<option value='%d'>%d</option>"), i + 100, i*90);
      }
      serverSendChunk(buf);
    }
    serverSendChunk(F("<br><br><input type='submit' value='Submit'></form><br><br><a href='/'>Back to main</a></body>"));
  }
  else
  {
    strncpy(buf, server.arg("plain").c_str(), MALLOCBUF_SIZE);
    splitPostValues(buf, keys, values, &nKeys);

    orient = NULL;
    getKeyValue(keys, values, nKeys, "orientation", &orient);

    if (orient != NULL)
    {
      n = atoi(orient);
      if (n >= 100 && n < 104)
      {
        orientation = n - 100;
        EEPROM.write(EEPROM_ORIENTATION_IDX, orientation);
      }

      if (EEPROM.commit())
      {
        serverSendChunk(F("<body><p>Updated!</p><br><br><a href='/'>Back to main</a></body>"));
      }
      else
      {
        serverSendChunk(F("<body><p>EEPROM write failed!</p><br><br><a href='/'>Back to main</a></body>"));
      }
    }
    else
    {
      serverSendChunk(F("<body><p>Error!</p><br><br><a href='/'>Back to main</a></body>"));
    }
  }
  serverSendEnd();
  free(buf);
}

void handle_config_driver()
{
  char *drv;
  char *keys[32];
  char *values[32];
  char drvNames[NDRIVERS][20];
  int nKeys, i, n;
  char *buf;
  
  buf = (char*)malloc(MALLOCBUF_SIZE);

  serverSendBegin();
  
  if (server.hasArg("plain") == false)
  {
    serverSendChunk(F("<body><h1>kungFuHue driver selection</h1><form action='' method='post'> <label for='driver'>Driver:</label><select id='driver' name='driver'>"));
    
    strcpy_P(drvNames[0], (PGM_P)F("Philips Hue"));
    strcpy_P(drvNames[1], (PGM_P)F("WiZ"));
    strcpy_P(drvNames[2], (PGM_P)F("Yeelight"));
    strcpy_P(drvNames[3], (PGM_P)F("LiFx"));
    
    for (i = 0; i < NDRIVERS; i++)
    {
      if (driver == i)
      {
        sprintf_P(buf, (PGM_P)F("<option value='%d' selected>%s</option>"), i + 100, drvNames[i]);
      }
      else
      {
        sprintf_P(buf, (PGM_P)F("<option value='%d'>%s</option>"), i + 100, drvNames[i]);
      }
      serverSendChunk(buf);
    }
    serverSendChunk(F("<br><br><input type='submit' value='Submit'></form><br><br><a href='/'>Back to main</a></body>"));
  }
  else
  {
    strncpy(buf, server.arg("plain").c_str(), MALLOCBUF_SIZE);
    splitPostValues(buf, keys, values, &nKeys);

    drv = NULL;
    getKeyValue(keys, values, nKeys, "driver", &drv);

    if (drv != NULL)
    {
      n = atoi(drv);
      if (n >= 100 && n < 100 + NDRIVERS)
      {
        driver = n - 100;
        EEPROM.write(EEPROM_DRIVER_IDX, driver);
      }

      if (getDeviceEnabled(getCurrentDevice()) == 0)
      {
        // Driver not properly initialized
        for (i = 0; i < NDEVICES; i++)
        {
          if (getDeviceEnabled(i) != 0)
          {
            setCurrentDevice(i);
            break;
          }
        }
      }

      if (EEPROM.commit())
      {
        serverSendChunk(F("<body><p>Updated!</p><br><br><a href='/'>Back to main</a></body>"));
      }
      else
      {
        serverSendChunk(F("<body><p>EEPROM write failed!</p><br><br><a href='/'>Back to main</a></body>"));
      }
    }
    else
    {
      serverSendChunk(F("<body><p>Error!</p><br><br><a href='/'>Back to main</a></body>"));
    }
  }
  serverSendEnd();
  free(buf);
}

uint8_t readHex(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

void decodeHtmlPost(char *str, int len)
{
  int i, j;
  for (i = 0; i < len; i++)
  {
    if (str[i] == 0) break;
    
    if (str[i] == '+') str[i] = ' ';

    if (str[i] == '%')
    {
      str[i] = readHex(str[i+1]) << 4;
      str[i] |= readHex(str[i+2]);
      j = i+1;
      while (j < len - 2)
      {
        str[j] = str[j+2];
        if (str[j] == 0) break;
        j++;
      }
    }
  }
}

void handle_config_wifi()
{
  char *ssid, *pass;
  char *keys[32];
  char *values[32];
  int nKeys, i;
  char current_ssid[64];
  char *buf;

  buf = (char*)malloc(MALLOCBUF_SIZE);

  serverSendBegin();
  if (server.hasArg("plain") == false)
  {
    for (i = 0; i < 64; i++)
    {
      current_ssid[i] = EEPROM.read(EEPROM_SSID_IDX + i);
    }

    serverSendChunk(F("<body><h1>kungFuHue WiFi config</h1><form action='' method='post'> <label for='ssid'>WiFi SSID:</label>"));
    sprintf_P(buf, (PGM_P)F("<input type='text' id='ssid' name='ssid' value='%s'>"), current_ssid);
    serverSendChunk(buf);
    serverSendChunk(F("<br><br> <label for='pass'>WiFi password:</label> <input type='password' id='pass' name='pass'><br><br> <input type='submit' value='Submit'></form><br><br><a href='/'>Back to main</a></body>"));
  }
  else
  {
    strncpy(buf, server.arg("plain").c_str(), MALLOCBUF_SIZE);
    splitPostValues(buf, keys, values, &nKeys);

    ssid = NULL;
    pass = NULL;
    getKeyValue(keys, values, nKeys, "ssid", &ssid);
    getKeyValue(keys, values, nKeys, "pass", &pass);

#ifdef DEBUG
    Serial.print("ssid: '");
    Serial.print(ssid);
    Serial.println("'");
    Serial.print("pass: '");
    Serial.print(pass);
    Serial.println("'");
#endif

    decodeHtmlPost(ssid, 64);
    decodeHtmlPost(pass, 64);

#ifdef DEBUG
    Serial.print("decoded ssid: '");
    Serial.print(ssid);
    Serial.println("'");
    Serial.print("decoded pass: '");
    Serial.print(pass);
    Serial.println("'");
#endif


    if (ssid != NULL && pass != NULL)
    {
      i = 0;
      while (ssid[i] != 0)
      {
        EEPROM.write(EEPROM_SSID_IDX + i, ssid[i]);
        i++;
      }
      EEPROM.write(EEPROM_SSID_IDX + i, 0);
      
      i = 0;
      while (pass[i] != 0)
      {
        EEPROM.write(EEPROM_PASS_IDX + i, pass[i]);
        i++;
      }
      EEPROM.write(EEPROM_PASS_IDX + i, 0);
      if (EEPROM.commit())
      {
        serverSendChunk(F("<body><p>Updated!</p><br><br><a href='/'>Back to main</a></body>"));
      }
      else
      {
        serverSendChunk(F("<body><p>EEPROM write failed!</p><br><br><a href='/'>Back to main</a></body>"));
      }
    }
    else
    {
      serverSendChunk(F("<body><p>Error!</p><br><br><a href='/'>Back to main</a></body>"));
    }
  }
  serverSendEnd();
  free(buf);
}


void handle_config_brightness()
{
  char *brightness;
  char *keys[32];
  char *values[32];
  int nKeys, i;
  char *buf;

  buf = (char*)malloc(MALLOCBUF_SIZE);

  i = 0;
  serverSendBegin();
  if (server.hasArg("plain") == false)
  {
    serverSendChunk(F("<body><h1>kungFuHue Screen brightness config</h1><form action='' method='post'> <label for='bri'>Screen brightness [1..255]:</label>"));
    sprintf_P(buf, (PGM_P)F("<input type='number' id='bri' name='bri' min='1' max='255' value='%d'>"), ledBrightness);
    serverSendChunk(buf);
    serverSendChunk(F("<input type='submit' value='Submit'></form><br><br><a href='/'>Back to main</a></body>"));
  }
  else
  {
    strncpy(buf, server.arg("plain").c_str(), MALLOCBUF_SIZE - 1);
    splitPostValues(buf, keys, values, &nKeys);

    brightness = NULL;
    getKeyValue(keys, values, nKeys, "bri", &brightness);

    if (brightness != NULL)
    {
      i = atoi(brightness);
    }
    
    if (i >= 1 && i <= 255)
    {
      // Update brightness
      EEPROM.write(EEPROM_LEDBRIGHTNESS_IDX, i);
      if (EEPROM.commit())
      {
        ledBrightness = i;
        loadColorMap(ledBrightness);
        serverSendChunk(F("<body><p>Updated!</p><br><br><a href='/'>Back to main</a></body>"));
      }
      else
      {
        serverSendChunk(F("<body><p>EEPROM write failed!</p><br><br><a href='/'>Back to main</a></body>"));
      }
    }
    else
    {
      serverSendChunk(F("<body><p>Error!</p><br><br><a href='/'>Back to main</a></body>"));
    }
  }
  serverSendEnd();

  free(buf);

  // Flash LEDs to show off new brightness level
  if (i >= 1 && i <= 255)
  {
    strip.clear();
    for (i = 0; i < 16; i++)
    {
      setColor(i, colorMap[hue_getCurrentDevice()]);
    }
    strip.show();
    delay(750);
    strip.clear();
    strip.show();    
  }
}

void handle_config_hue()
{
  char *hue_ip, *hue_user, *autodetect, *pairbridge;
  char *keys[32];
  char *values[32];
  int nKeys, i;
  char *buf;
  hueDriverData_t *p_hueData;

  p_hueData = hue_getDriverDataStruct();
  
  buf = (char*)malloc(MALLOCBUF_SIZE);

  serverSendBegin();
  if (server.hasArg("plain") == false)
  {
    serverSendChunk(F("<body><h1>kungFuHue Hue Bridge config</h1><br><br><form action='' method='post'><button name='autodetect' value='1'>Detect bridge</button></form><br><br><form action='' method='post'><button name='pairbridge' value='1'>Pair with bridge</button></form><br><br><form action='' method='post'><label for='hue_ip'>Hue Bridge IP address:</label><input type='text' id='hue_ip' name='hue_ip'"));
    if (discoveryState == HUE_DISCOVERY_DISCOVERED)
    {
      sprintf_P(buf, (PGM_P)F(" value='%s'> [Autodetected]"), discoveryUrl);
      serverSendChunk(buf);
      discoveryState = HUE_DISCOVERY_IDLE;
    }
    else
    {
      sprintf_P(buf, (PGM_P)F(" value='%s'>"), p_hueData->hueIP);
      serverSendChunk(buf);
    }

    serverSendChunk(F("<br><br><label for='hue_user'>Hue Bridge user:</label> <input type='text' id='hue_user' name='hue_user'"));
    if (newUserState == HUE_NEWUSER_EXECUTED)
    {
      if (newHueUserReturnValue == 0)
      {
        sprintf_P(buf, (PGM_P)F(" value='%s'> [Paired]"), newHueUser);
        serverSendChunk(buf);
      }
      else
      {
        sprintf_P(buf, (PGM_P)F(" value='%s'> [error: %s]"), p_hueData->hueUser, newHueUser);
        serverSendChunk(buf);
      }
      newUserState = HUE_NEWUSER_IDLE;
    }
    else
    {
      sprintf_P(buf, (PGM_P)F(" value='%s'>"), p_hueData->hueUser);
      serverSendChunk(buf);
    }
    
    serverSendChunk(F("<br><br> <input type='submit' value='Submit'></form><br><br><a href='/'>Back to main</a></body></html>"));
  }
  else
  {
    strncpy(buf, server.arg("plain").c_str(), MALLOCBUF_SIZE);
    splitPostValues(buf, keys, values, &nKeys);

    autodetect = NULL;
    getKeyValue(keys, values, nKeys, "autodetect", &autodetect);

    pairbridge = NULL;
    getKeyValue(keys, values, nKeys, "pairbridge", &pairbridge);
    
    if ((autodetect != NULL) && (*autodetect == '1'))
    {
      discoveryState = HUE_DISCOVERY_REQUEST;
      serverSendChunk(F("<head><meta http-equiv=\"refresh\" content=\"3; url='/hue'\"/><body><p>Please wait.. If page does not load automatically in a few seconds please follow <a href=\"/hue\">this link</a></p></body>"));
    }
    else if ((pairbridge != NULL) && (*pairbridge == '1'))
    {
      newUserState = HUE_NEWUSER_REQUEST;
      serverSendChunk(F("<head><meta http-equiv=\"refresh\" content=\"3; url='/hue'\"/><body><p>Please wait.. If page does not load automatically in a few seconds please follow <a href=\"/hue\">this link</a></p></body>"));
    }
    else
    {
      hue_ip = NULL;
      hue_user = NULL;
      getKeyValue(keys, values, nKeys, "hue_ip", &hue_ip);
      getKeyValue(keys, values, nKeys, "hue_user", &hue_user);
  
      if (hue_ip != NULL && hue_user != NULL)
      {
        strncpy(p_hueData->hueIP, hue_ip, 32);
        strncpy(p_hueData->hueUser, hue_user, 64);

        // Skip first byte -- default device. We can overwrite the rest
        for (i = 1; i < sizeof(hueDriverData_t); i++)
        {
          EEPROM.write(EEPROM_HUEDATA_IDX + i, ((char*)p_hueData)[i]);
        }
        
        if (EEPROM.commit())
        {
          serverSendChunk(F("<body><p>Updated!</p><br><br><a href='/'>Back to main</a></body>"));
        }
        else
        {
          serverSendChunk(F("<body><p>EEPROM write failed!</p><br><br><a href='/'>Back to main</a></body>"));
        }
      }
      else
      {
        serverSendChunk(F("<body><p>Error!</p><br><br><a href='/'>Back to main</a></body>"));
      }
    }
  }
  serverSendEnd();
  free(buf);
}

void handle_config_devices()
{
  serverSendBegin();
  serverSendChunk(F("<head><meta http-equiv=\"refresh\" content=\"1; url='/devices_loaded'\"/></head><body><p>Please wait, gathering data from the Hue Bridge. If page does not load automatically in a few seconds please follow <a href=\"/devices_loaded\">this link</a>.</p></body>"));
  serverSendEnd();
  // Request main loop to load parameters for us. Hope it does it until we get to the next page
  if (loadAllParams == 0)
  {
    loadAllParams = 1;
  }
}

void handle_config_devices_loaded()
{
  int i, j, nKeys;
  char *devs[NDEVICES];
  char *keys[32];
  char *values[32];
  char smallBuf[64];
  char *buf;
  int foundLight;

  uint32_t newColorMap[NDEVICES];

  if (loadAllParams != 0)
  {
    handle_config_devices();
    return;
  }
  
  buf = (char*)malloc(MALLOCBUF_SIZE);

  serverSendBegin();
  if (server.hasArg("plain") == false)
  {
    serverSendChunk(F("<body><h1>kungFuHue config controlled devices</h1><form action='' method='post'><table><tr><th>Device</th><th>Color</th><th>Default</th></tr><tr><td>"));

    // Leftmost table column: Assign devices to slots
    for (j = 0; j < NDEVICES; j++)
    {
      sprintf_P(buf, (PGM_P)F("<label for='dev%d'>Device %d:</label><select id='dev%d' name='dev%d'><option value='-1'>None</option>"), j, j, j, j);
      serverSendChunk(buf);

      // Add valid lights/groups to list
      for (i = 0; i < NDISCOVERLIGHTS; i++)
      {
        if (allLights[i].ok != -1)
        {
          foundLight = findLight(j, i, allLights);
          if (foundLight == 0)
          {
            // Light not configured for this slot
            if (allLights[i].group == 0)
            {
              sprintf_P(buf, (PGM_P)F("<option value='%d'>Light '%s'</option>"), i + 100, allLights[i].name);
            }
            else
            {
              sprintf_P(buf, (PGM_P)F("<option value='%d'>Group '%s'</option>"), i + 100, allLights[i].name);
            }
          }
          else
          {
            // Light configured for this slot
            if (allLights[i].group == 0)
            {
              sprintf_P(buf, (PGM_P)F("<option value='%d' selected>Light '%s'</option>"), i + 100, allLights[i].name);
            }
            else
            {
              sprintf_P(buf, (PGM_P)F("<option value='%d' selected>Group '%s'</option>"), i + 100, allLights[i].name);
            }
          }
          serverSendChunk(buf);
        }
      }
      serverSendChunk(F("</select><br><br>"));
    }
    
    serverSendChunk(F("</td><td>"));
  
    // Middle column: Device color
    for (j = 0; j < NDEVICES; j++)
    {
      sprintf_P(buf, (PGM_P)F("<select id='color%d' name='color%d'>"), j, j);
      serverSendChunk(buf);
      
      if ((colorMap[j] & 0xFF0000) != 0) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='R' style='background:#FF0000' %s>Red</option>"), smallBuf);
      serverSendChunk(buf);

      if ((colorMap[j] & 0x00FF00) != 0) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='G' style='background:#00FF00' %s>Green</option>"), smallBuf);
      serverSendChunk(buf);

      if ((colorMap[j] & 0x0000FF) != 0) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='B' style='background:#0000FF' %s>Blue</option>"), smallBuf);
      serverSendChunk(buf);

      if (((colorMap[j] & 0xFF0000) != 0) && ((colorMap[j] & 0x00FF00) != 0)) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='RG' style='background:#FFFF00' %s>Yellow</option>"), smallBuf);
      serverSendChunk(buf);

      if (((colorMap[j] & 0xFF0000) != 0) && ((colorMap[j] & 0x0000FF) != 0)) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='RB' style='background:#FF00FF' %s>Magenta</option>"), smallBuf);
      serverSendChunk(buf);

      if (((colorMap[j] & 0x00FF00) != 0) && ((colorMap[j] & 0x0000FF) != 0)) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='GB' style='background:#00FFFF' %s>Cyan</option>"), smallBuf);
      serverSendChunk(buf);

      if (((colorMap[j] & 0xFF0000) != 0) && ((colorMap[j] & 0x00FF00) != 0) && ((colorMap[j] & 0x0000FF) != 0)) strcpy(smallBuf, "selected");
      else strcpy(smallBuf, "");
      sprintf_P(buf, (PGM_P)F("<option value='RGB' style='background:#FFFFFF' %s>White</option>"), smallBuf);
      serverSendChunk(buf);

      serverSendChunk(F("</select><br><br>"));
    }

    serverSendChunk(F("</td><td>"));
  
    // Right column: Is device default?
    i = getDefaultDevice();
    for (j = 0; j < NDEVICES; j++)
    {
      if (i == j)
      {
        strcpy(smallBuf, "checked='checked'");
      }
      else
      {
        strcpy(smallBuf, "");
      }
      sprintf_P(buf, (PGM_P)F("<input type='radio' id='default%d' name='default' value='%d' %s><br><br>"), j, j + 1000, smallBuf);
      serverSendChunk(buf);
    }

    serverSendChunk(F("</td></tr></table><input type='submit' value='Submit'></form>"));
  }
  else
  {
    strncpy(buf, server.arg("plain").c_str(), MALLOCBUF_SIZE);
    splitPostValues(buf, keys, values, &nKeys);

    for (j = 0; j < NDEVICES; j++)
    {
      devs[j] = NULL;
      sprintf(smallBuf, "dev%d", j);
      getKeyValue(keys, values, nKeys, smallBuf, &devs[j]);
    }

    serverSendChunk(F("<body>"));

    for (j = 0; j < NDEVICES; j++)
    {
      if (devs[j] != NULL)
      {
        // Check if it is valid according to our loaded devices
        i = atoi(devs[j]);
        if (i == -1)
        {
          // Disable slot
          configSlot(j, -1, allLights);
        }
        if (i >= 100 && i < 164)
        {
          // This is supposed to be a light/group
          i -= 100;
          configSlot(j, i, allLights);
        }
        sprintf_P(smallBuf, (PGM_P)F("<p>devs[%d] = %s <br><br></p>"), j, devs[j]);
        serverSendChunk(smallBuf);
      }
    }

    for (j = 0; j < NDEVICES; j++)
    {
      devs[j] = NULL;
      sprintf(smallBuf, "color%d", j);
      getKeyValue(keys, values, nKeys, smallBuf, &devs[j]);
    }
    for (j = 0; j < NDEVICES; j++)
    {
      if (devs[j] != NULL)
      {
        newColorMap[j] = 0;
        if (strstr(devs[j], "R") != NULL) newColorMap[j] |= 0x010000;
        if (strstr(devs[j], "G") != NULL) newColorMap[j] |= 0x000100;
        if (strstr(devs[j], "B") != NULL) newColorMap[j] |= 0x000001;
        sprintf_P(smallBuf, (PGM_P)F("<p>color[%d] = %s <br><br></p>"), j, devs[j]);
        serverSendChunk(smallBuf);
      }
    }

    devs[0] = NULL;
    getKeyValue(keys, values, nKeys, "default", &devs[0]);
    if (devs[0] != NULL)
    {
      i = atoi(devs[0]);
      if (i >= 1000 && i < 1000 + NDEVICES)
      {
        // This should be the new default device. Let's check it for consistency
        i -= 1000;
        if (getDeviceEnabled(i) != 0)
        {
          // It is ok, use it
          setCurrentDevice(i);
        }
        else
        {
          // Not ok, we disabled this slot. Use first enabled one..
          for (j = 0; j < NDEVICES; j++)
          {
            if (getDeviceEnabled(j) != 0)
            {
              setCurrentDevice(j);
              break;
            }
          }
        }
      }
      sprintf_P(smallBuf, (PGM_P)F("<p>default = %s <br><br></p>"), devs[0]);
      serverSendChunk(smallBuf);
    }

    for (j = 0; j < NDEVICES; j++)
    {
      EEPROM.write(EEPROM_COLORMAP_IDX + j*3 + 0, (newColorMap[j] >> 16) & 0xFF);
      EEPROM.write(EEPROM_COLORMAP_IDX + j*3 + 1, (newColorMap[j] >> 8) & 0xFF);
      EEPROM.write(EEPROM_COLORMAP_IDX + j*3 + 2, newColorMap[j] & 0xFF);
    }
    loadColorMap(ledBrightness);

    if (eepromWriteCommitFullConfig())
    {
      serverSendChunk(F("<p>Updated!</p><br><br>"));
      loadColorMap(ledBrightness);
    }
    else
    {
      serverSendChunk(F("<p>EEPROM write failed!</p>"));
    }

  }
  serverSendChunk(F("<br><br><a href='/'>Back to main</a></body>"));
  serverSendEnd();
  free(buf);
}

void displayMenuBri()
{
  uint8_t device;
  device = getCurrentDevice();

  strip.clear();
  setColor(0, colorMap[device]);
  setColor(8, colorMap[device]);
  strip.show();
}

void displayMenuCt()
{
  uint8_t device;
  device = getCurrentDevice();

  strip.clear();
  setColor(3, colorMap[device]);
  setColor(13, colorMap[device]);
  setColor(8, colorMap[device]);
  strip.show();
}

void displayMenuHue()
{
  uint8_t device;
  device = getCurrentDevice();

  strip.clear();
  setColor(2, colorMap[device]);
  setColor(6, colorMap[device]);
  setColor(10, colorMap[device]);
  setColor(14, colorMap[device]);
  strip.show();
}

void displayMenuSat()
{
  uint8_t device;
  device = getCurrentDevice();

  strip.clear();
  setColor(0, colorMap[device]);
  setColor(3, colorMap[device]);
  setColor(6, colorMap[device]);
  setColor(10, colorMap[device]);
  setColor(13, colorMap[device]);
  strip.show();
}

void displayRssi()
{
  int rssi, i;
  rssi = WiFi.RSSI();
  rssi = (rssi + 100) / 6;
  if (rssi < 0) rssi = 0;
  if (rssi > 9) rssi = 9;
  strip.clear();
  for (i = 0; i < rssi; i++)
  {
    setColor(8 + i, 0x060606);
    setColor(8 - i, 0x060606);
  }
  strip.show();
}

void showWink()
{
  // draw eyes
  uint8_t i;
  for (i = 0; i < 3; i++)
  {
    strip.clear();
    strip.show();
    delay(50);
    
    setColor(14, 0x000006);
    setColor(2, 0x000006);
    strip.show();
    delay(500);
  }

  // draw smile
  for (i = 11; i >= 5; i--)
  {
    setColor(i, 0x060000);   
    strip.show();
    delay(50);
  }

  delay(750);

  // wink
  for (i = 0; i < 2; i++)
  {
    setColor(2, 0);
    strip.show();
    delay(300);
    setColor(2, 0x000006);
    strip.show();
    delay(500);
  }
  
}

void displayDigit(uint8_t d)
{
  uint8_t i;
  strip.clear();
  switch(d)
  {
    case ' ':
      break;
    case '.':
      setColor(8, 0x060606);
      break;
    case '0':
      for (i = 0; i <= 15; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '1':
      setColor(0, 0x060606);
      setColor(8, 0x060606);
      break;
    case '2':
      for (i = 2; i <= 6; i++)
      {
        setColor(i, 0x060606);
      }
      for (i = 10; i <= 14; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '3':
      setColor(15, 0x060606);
      setColor(0, 0x060606);
      setColor(1, 0x060606);
      setColor(2, 0x060606);
      setColor(4, 0x060606);
      setColor(6, 0x060606);
      setColor(7, 0x060606);
      setColor(8, 0x060606);
      setColor(9, 0x060606);
      break;
    case '4':
      for (i = 12; i <= 15; i++)
      {
        setColor(i, 0x060606);
      }
      for (i = 4; i <= 7; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '5':
      setColor(12, 0x060606);
      setColor(13, 0x060606);
      setColor(15, 0x060606);
      setColor(0, 0x060606);
      setColor(1, 0x060606);
      for (i = 4; i <= 8; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '6':
      setColor(0, 0x060606);
      for (i = 4; i <= 15; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '7':
      setColor(14, 0x060606);
      setColor(15, 0x060606);
      setColor(0, 0x060606);
      setColor(1, 0x060606);
      for (i = 3; i <= 6; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '8':
      setColor(14, 0x060606);
      setColor(15, 0x060606);
      setColor(0, 0x060606);
      setColor(1, 0x060606);
      setColor(2, 0x060606);
      for (i = 6; i <= 10; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    case '9':
      for (i = 12; i <= 15; i++)
      {
        setColor(i, 0x060606);
      }
      for (i = 0; i <= 8; i++)
      {
        setColor(i, 0x060606);
      }
      break;
    default:
      break;
  }
  strip.show();
}

void setColor(int pixel, uint32_t color)
{
  strip.setPixelColor(mod(pixel + 4*orientation, 16), color);
}

uint32_t getColor(int pixel)
{
  return strip.getPixelColor(mod(pixel + 4*orientation, 16));
}


// Common interface functions

int eepromWriteCommitFullConfig()
{
  int i;
  switch(driver)
  {
    case DRIVER_HUE:
      hueDriverData_t *p_hueData;
      p_hueData = hue_getDriverDataStruct();
      for (i = 0; i < sizeof(hueDriverData_t); i++)
      {
        EEPROM.write(EEPROM_HUEDATA_IDX + i, ((char*)p_hueData)[i]);
      }
      break;
    case DRIVER_WIZ:
      wizDriverData_t *p_wizData;
      p_wizData = wiz_getDriverDataStruct();
      for (i = 0; i < sizeof(wizDriverData_t); i++)
      {
        EEPROM.write(EEPROM_WIZDATA_IDX + i, ((char*)p_wizData)[i]);
      }
      break;
    case DRIVER_YEELIGHT:
      yeelightDriverData_t *p_yeelightData;
      p_yeelightData = yeelight_getDriverDataStruct();
      for (i = 0; i < sizeof(yeelightDriverData_t); i++)
      {
        EEPROM.write(EEPROM_YEELIGHTDATA_IDX + i, ((char*)p_yeelightData)[i]);
      }
      break;
    case DRIVER_LIFX:
      lifxDriverData_t *p_lifxData;
      p_lifxData = lifx_getDriverDataStruct();
      for (i = 0; i < sizeof(lifxDriverData_t); i++)
      {
        EEPROM.write(EEPROM_LIFXDATA_IDX + i, ((char*)p_lifxData)[i]);
      }
      break;
  }
  return EEPROM.commit();
}

void configSlot(uint8_t dev, int light, discoverParams_t *lights)
{
  switch(driver)
  {
    case DRIVER_HUE:
      hue_configSlot(dev, light, lights);
      break;
    case DRIVER_WIZ:
      wiz_configSlot(dev, light, lights);
      break;
    case DRIVER_YEELIGHT:
      yeelight_configSlot(dev, light, lights);
      break;
    case DRIVER_LIFX:
      lifx_configSlot(dev, light, lights);
      break;
  }
}


uint8_t getDefaultDevice()
{
  switch(driver)
  {
    case DRIVER_HUE:
      return EEPROM.read(EEPROM_HUEDATA_IDX);
      break;
    case DRIVER_WIZ:
      return EEPROM.read(EEPROM_WIZDATA_IDX);
      break;
    case DRIVER_YEELIGHT:
      return EEPROM.read(EEPROM_YEELIGHTDATA_IDX);
      break;
    case DRIVER_LIFX:
      return EEPROM.read(EEPROM_LIFXDATA_IDX);
      break;
  }
}

// Returns
// 0: not found
// 1: found
int findLight(uint8_t lightNumber, uint8_t nLights, discoverParams_t *lights)
{
  switch(driver)
  {
    case DRIVER_HUE:
      return hue_findLight(lightNumber, nLights, lights);
      break;
    case DRIVER_WIZ:
      return wiz_findLight(lightNumber, nLights, lights);
      break;
    case DRIVER_YEELIGHT:
      return yeelight_findLight(lightNumber, nLights, lights);
      break;
    case DRIVER_LIFX:
      return lifx_findLight(lightNumber, nLights, lights);
      break;
  }
}

int discoverLights(discoverParams_t *lights)
{
  int retVal;
  switch(driver)
  {
    case DRIVER_HUE:
      retVal = hue_discoverLights(lights);
      break;
    case DRIVER_WIZ:
      retVal = wiz_discoverLights(lights);
      break;
    case DRIVER_YEELIGHT:
      retVal = yeelight_discoverLights(lights);
      break;
    case DRIVER_LIFX:
      retVal = lifx_discoverLights(lights);
      break;
  }
#ifdef DEBUG
  int i;
  Serial.println("Discovered lights/groups:");
  for (i = 0; i < NDISCOVERLIGHTS; i++)
  {
    if (allLights[i].ok != -1)
    {
      Serial.print(i);
      Serial.print(": '");
      Serial.print(allLights[i].name);
      Serial.print("', group = ");
      Serial.print(allLights[i].group);
      Serial.print(", index = ");
      Serial.print(allLights[i].index);
      Serial.print(", flags = ");
      Serial.println(allLights[i].deviceFlags);
    }
  }
#endif
  return retVal;
}

uint8_t getCurrentDevice()
{
  switch(driver)
  {
    case DRIVER_HUE:
      return hue_getCurrentDevice();
      break;
    case DRIVER_WIZ:
      return wiz_getCurrentDevice();
      break;
    case DRIVER_YEELIGHT:
      return yeelight_getCurrentDevice();
      break;
    case DRIVER_LIFX:
      return lifx_getCurrentDevice();
      break;
  }
}

void setCurrentDevice(uint8_t dev)
{
  switch(driver)
  {
    case DRIVER_HUE:
      hue_setCurrentDevice(dev);
      break;
    case DRIVER_WIZ:
      wiz_setCurrentDevice(dev);
      break;
    case DRIVER_YEELIGHT:
      yeelight_setCurrentDevice(dev);
      break;
    case DRIVER_LIFX:
      lifx_setCurrentDevice(dev);
      break;
  }
}


// 1: enabled
// 0: disabled
uint8_t getDeviceEnabled(uint8_t dev)
{
  switch(driver)
  {
    case DRIVER_HUE:
      return hue_getDeviceEnabled(dev);
      break;
    case DRIVER_WIZ:
      return wiz_getDeviceEnabled(dev);
      break;
    case DRIVER_YEELIGHT:
      return yeelight_getDeviceEnabled(dev);
      break;
    case DRIVER_LIFX:
      return lifx_getDeviceEnabled(dev);
      break;
  }
}


void getParam(deviceParams_t *deviceParams)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  switch(driver)
  {
    case DRIVER_HUE:
      hue_getParam(deviceParams);
      break;
    case DRIVER_WIZ:
      wiz_getParam(deviceParams);
      break;
    case DRIVER_YEELIGHT:
      yeelight_getParam(deviceParams);
      break;
    case DRIVER_LIFX:
      lifx_getParam(deviceParams);
      break;
  }
}

void setParam(int param, deviceParams_t *deviceParams)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  switch(driver)
  {
    case DRIVER_HUE:
      hue_setParam(param, deviceParams);
      break;
    case DRIVER_WIZ:
      wiz_setParam(param, deviceParams);
      break;
    case DRIVER_YEELIGHT:
      yeelight_setParam(param, deviceParams);
      break;
    case DRIVER_LIFX:
      lifx_setParam(param, deviceParams);
      break;
  }
}

int hasParam(int param)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return -1;
  }

  switch(driver)
  {
    case DRIVER_HUE:
      return hue_hasParam(param);
      break;
    case DRIVER_WIZ:
      return wiz_hasParam(param);
      break;
    case DRIVER_YEELIGHT:
      return yeelight_hasParam(param);
      break;
    case DRIVER_LIFX:
      return lifx_hasParam(param);
      break;
  }
}

void incParam(int param, deviceParams_t *deviceParams)
{
  switch(driver)
  {
    case DRIVER_HUE:
      hue_incParam(param, deviceParams);
      break;
    case DRIVER_WIZ:
      wiz_incParam(param, deviceParams);
      break;
    case DRIVER_YEELIGHT:
      yeelight_incParam(param, deviceParams);
      break;
    case DRIVER_LIFX:
      lifx_incParam(param, deviceParams);
      break;
  }
}

void decParam(int param, deviceParams_t *deviceParams)
{
  switch(driver)
  {
    case DRIVER_HUE:
      hue_decParam(param, deviceParams);
      break;
    case DRIVER_WIZ:
      wiz_decParam(param, deviceParams);
      break;
    case DRIVER_YEELIGHT:
      yeelight_decParam(param, deviceParams);
      break;
    case DRIVER_LIFX:
      lifx_decParam(param, deviceParams);
      break;
  }
}


int changeMenu(uint8_t newParam)
{
  if (hasParam(newParam) != -1)
  {
    activeParam = newParam;
    return 1;
  }
  else
  {
    return -1;
  }
}


void menuActionRoot(uint8_t action)
{
  deviceParams_t deviceParams;
  switch (action)
  {
    case SWIPE_UP:
      swipe(SWIPE_UP);
      incParam(PARAM_ON, &deviceParams);
      setParam(PARAM_ON, &deviceParams);
      break;
    case SWIPE_DOWN:
      swipe(SWIPE_DOWN);
      decParam(PARAM_ON, &deviceParams);
      setParam(PARAM_ON, &deviceParams);
      break;
    case SWIPE_LEFT:
      changeDevice(-1);
      break;
    case SWIPE_RIGHT:
      changeDevice(1);
      break;
    case SWIPE_IN:
      twist(1);
      twist(-1);
      break;
    case SWIPE_OUT:
      twist(-1);
      if (changeMenu(PARAM_BRI) == 1) displayMenuBri();
      else twist(1);
      break;
  }
}

void menuActionBri(uint8_t action)
{
  deviceParams_t deviceParams;
  switch (action)
  {
    case SWIPE_UP:
      swipe(SWIPE_UP);
      getParam(&deviceParams);
      incParam(PARAM_BRI, &deviceParams);
      setParam(PARAM_BRI, &deviceParams);
      displayMenuBri();
      break;
    case SWIPE_DOWN:
      swipe(SWIPE_DOWN);
      getParam(&deviceParams);
      decParam(PARAM_BRI, &deviceParams);
      setParam(PARAM_BRI, &deviceParams);
      displayMenuBri();
      break;
    case SWIPE_LEFT:
      swipe(SWIPE_LEFT);
      if (changeMenu(PARAM_CT) == 1) displayMenuCt();
      else swipe(SWIPE_RIGHT);
      break;
    case SWIPE_RIGHT:
      swipe(SWIPE_RIGHT);
      if (changeMenu(PARAM_HUE) == 1) displayMenuHue();
      else swipe(SWIPE_LEFT);
      break;
    case SWIPE_IN:
      twist(1);
      changeMenu(PARAM_ON);
      break;
    case SWIPE_OUT:
      twist(-1);
      twist(1);
      break;
  }
}

void menuActionCt(uint8_t action)
{
  deviceParams_t deviceParams;
  switch (action)
  {
    case SWIPE_UP:
      swipe(SWIPE_UP);
      getParam(&deviceParams);
      incParam(PARAM_CT, &deviceParams);
      setParam(PARAM_CT, &deviceParams);
      displayMenuCt();
      break;
    case SWIPE_DOWN:
      swipe(SWIPE_DOWN);
      getParam(&deviceParams);
      decParam(PARAM_CT, &deviceParams);
      setParam(PARAM_CT, &deviceParams);
      displayMenuCt();
      break;
    case SWIPE_LEFT:
      swipe(SWIPE_LEFT);
      swipe(SWIPE_RIGHT);
      break;
    case SWIPE_RIGHT:
      swipe(SWIPE_RIGHT);
      if (changeMenu(PARAM_BRI) == 1) displayMenuBri();
      else swipe(SWIPE_LEFT);
      break;
    case SWIPE_IN:
      twist(1);
      changeMenu(PARAM_ON);
      break;
    case SWIPE_OUT:
      twist(-1);
      twist(1);
      break;
  }
}

void menuActionHue(uint8_t action)
{
  deviceParams_t deviceParams;
  switch (action)
  {
    case SWIPE_UP:
      swipe(SWIPE_UP);
      getParam(&deviceParams);
      incParam(PARAM_HUE, &deviceParams);
      setParam(PARAM_HUE, &deviceParams);
      displayMenuHue();
      break;
    case SWIPE_DOWN:
      swipe(SWIPE_DOWN);
      getParam(&deviceParams);
      decParam(PARAM_HUE, &deviceParams);
      setParam(PARAM_HUE, &deviceParams);
      displayMenuHue();
      break;
    case SWIPE_LEFT:
      swipe(SWIPE_LEFT);
      if (changeMenu(PARAM_BRI) == 1) displayMenuBri();
      else swipe(SWIPE_RIGHT);
      break;
    case SWIPE_RIGHT:
      swipe(SWIPE_RIGHT);
      if (changeMenu(PARAM_SAT) == 1) displayMenuSat();
      else swipe(SWIPE_LEFT);
      break;
    case SWIPE_IN:
      twist(1);
      changeMenu(PARAM_ON);
      break;
    case SWIPE_OUT:
      twist(-1);
      twist(1);
      break;
  }
}

void menuActionSat(uint8_t action)
{
  deviceParams_t deviceParams;
  switch (action)
  {
    case SWIPE_UP:
      swipe(SWIPE_UP);
      getParam(&deviceParams);
      incParam(PARAM_SAT, &deviceParams);
      setParam(PARAM_SAT, &deviceParams);
      displayMenuSat();
      break;
    case SWIPE_DOWN:
      swipe(SWIPE_DOWN);
      getParam(&deviceParams);
      decParam(PARAM_SAT, &deviceParams);
      setParam(PARAM_SAT, &deviceParams);
      displayMenuSat();
      break;
    case SWIPE_LEFT:
      swipe(SWIPE_LEFT);
      if (changeMenu(PARAM_HUE) == 1) displayMenuHue();
      else swipe(SWIPE_RIGHT);
      break;
    case SWIPE_RIGHT:
      swipe(SWIPE_RIGHT);
      swipe(SWIPE_LEFT);
      break;
    case SWIPE_IN:
      twist(1);
      changeMenu(PARAM_ON);
      break;
    case SWIPE_OUT:
      twist(-1);
      twist(1);
      break;
  }
}


void menuAction(uint8_t action)
{
  switch (activeParam)
  {
    case PARAM_ON:
      menuActionRoot(action);
      break;
    case PARAM_BRI:
      menuActionBri(action);
      break;
    case PARAM_CT:
      menuActionCt(action);
      break;
    case PARAM_HUE:
      menuActionHue(action);
      break;
    case PARAM_SAT:
      menuActionSat(action);
      break;
    default:
      break;
  }
}

void loadColorMap(uint8_t bri)
{
  uint8_t i;
  for (i = 0; i < NDEVICES; i++)
  {
    colorMap[i] =  (uint32_t)(EEPROM.read(EEPROM_COLORMAP_IDX + i*3 + 0) * bri) << 16;
    colorMap[i] |= (uint32_t)(EEPROM.read(EEPROM_COLORMAP_IDX + i*3 + 1) * bri) << 8;
    colorMap[i] |= (uint32_t)(EEPROM.read(EEPROM_COLORMAP_IDX + i*3 + 2) * bri);
  }
}


void init_eeprom_default_values()
{
  uint8_t i;

  char ssid[64] = "";
  char pass[64] = "";

  uint8_t ledBrightness = 0x0C;
  uint32_t colorMap[NDEVICES] = {0x010100, 0x010001, 0x010101, 0x000100, 0x000001};
  uint8_t orientation = 0;

  uint8_t driver = DRIVER_HUE;
  //uint8_t driver = DRIVER_WIZ;
  //uint8_t driver = DRIVER_YEELIGHT;
  //uint8_t driver = DRIVER_LIFX;
  
  hueDriverData_t hueData;
  wizDriverData_t wizData;
  yeelightDriverData_t yeelightData;
  lifxDriverData_t lifxData;
  
  i = 0;
  while (ssid[i] != 0)
  {
    EEPROM.write(EEPROM_SSID_IDX + i, ssid[i]);
    i++;
  }
  EEPROM.write(EEPROM_SSID_IDX + i, 0);
  
  i = 0;
  while (pass[i] != 0)
  {
    EEPROM.write(EEPROM_PASS_IDX + i, pass[i]);
    i++;
  }
  EEPROM.write(EEPROM_PASS_IDX + i, 0);

  for (i = 0; i < NDEVICES; i++)
  {
    EEPROM.write(EEPROM_COLORMAP_IDX + i*3 + 0, (colorMap[i] >> 16) & 0xFF);
    EEPROM.write(EEPROM_COLORMAP_IDX + i*3 + 1, (colorMap[i] >> 8) & 0xFF);
    EEPROM.write(EEPROM_COLORMAP_IDX + i*3 + 2, colorMap[i] & 0xFF);
  }

  EEPROM.write(EEPROM_LEDBRIGHTNESS_IDX, ledBrightness);

  EEPROM.write(EEPROM_DRIVER_IDX, driver);

  EEPROM.write(EEPROM_ORIENTATION_IDX, orientation);

  // Hue structure
  for (i = 0; i < sizeof(hueDriverData_t); i++)
  {
    EEPROM.write(EEPROM_HUEDATA_IDX + i, ((char*)&hueData)[i]);
  }

  // WiZ structure
  for (i = 0; i < sizeof(wizDriverData_t); i++)
  {
    EEPROM.write(EEPROM_WIZDATA_IDX + i, ((char*)&wizData)[i]);
  }

  // Yeelight structure
  for (i = 0; i < sizeof(yeelightDriverData_t); i++)
  {
    EEPROM.write(EEPROM_YEELIGHTDATA_IDX + i, ((char*)&yeelightData)[i]);
  }

  // Lifx structure
  for (i = 0; i < sizeof(lifxDriverData_t); i++)
  {
    EEPROM.write(EEPROM_LIFXDATA_IDX + i, ((char*)&lifxData)[i]);
  }

  EEPROM.commit();
}

void setup()
{
  uint8_t i;
  uint8_t mac[6];
  char hostName[32];
  char ssid[64], pass[64];
  hueDriverData_t *p_hueData;
  wizDriverData_t *p_wizData;
  yeelightDriverData_t *p_yeelightData;
  lifxDriverData_t *p_lifxData;
  
  pinMode(BUTTONPIN, INPUT);

  pinMode(15, OUTPUT);
  pinMode(2, OUTPUT);
  digitalWrite(15, LOW);
  digitalWrite(2, LOW);
 

  strip.begin();
  strip.clear();
  strip.show();

#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("kungFuHue booting..");
  Serial.print("FW Timestamp: ");
  Serial.println(__TIMESTAMP__);
  Serial.print("EEPROM Hue data index: ");
  Serial.println(EEPROM_HUEDATA_IDX);
  Serial.print("EEPROM WiZ data index: ");
  Serial.println(EEPROM_WIZDATA_IDX);
  Serial.print("EEPROM Yeelight data index: ");
  Serial.println(EEPROM_YEELIGHTDATA_IDX);
  Serial.print("EEPROM LiFx data index: ");
  Serial.println(EEPROM_LIFXDATA_IDX);
  Serial.print("EEPROM END: ");
  Serial.println(EEPROM_END);
#endif DEBUG

  EEPROM.begin(512);

  // FOR DEVELOPMENT PURPOSES ONLY!!!
  //if (EEPROM.read(EEPROM_SSID_IDX) == 0xFF) init_eeprom_default_values();
  //init_eeprom_default_values();
  // FOR DEVELOPMENT PURPOSES ONLY!!!

  driver = EEPROM.read(EEPROM_DRIVER_IDX);

  // Read EEPROM stored configuration
  for (i = 0; i < 64; i++)
  {
    ssid[i] = EEPROM.read(EEPROM_SSID_IDX + i);
    pass[i] = EEPROM.read(EEPROM_PASS_IDX + i);
  }

  ledBrightness = EEPROM.read(EEPROM_LEDBRIGHTNESS_IDX);
  loadColorMap(ledBrightness);
  
  orientation = EEPROM.read(EEPROM_ORIENTATION_IDX);

  p_hueData = hue_getDriverDataStruct();
  for (i = 0; i < sizeof(hueDriverData_t); i++)
  {
    ((char*)p_hueData)[i] = EEPROM.read(EEPROM_HUEDATA_IDX + i);
  }

  p_wizData = wiz_getDriverDataStruct();
  for (i = 0; i < sizeof(wizDriverData_t); i++)
  {
    ((char*)p_wizData)[i] = EEPROM.read(EEPROM_WIZDATA_IDX + i);
  }

  p_yeelightData = yeelight_getDriverDataStruct();
  for (i = 0; i < sizeof(yeelightDriverData_t); i++)
  {
    ((char*)p_yeelightData)[i] = EEPROM.read(EEPROM_YEELIGHTDATA_IDX + i);
  }

  p_lifxData = lifx_getDriverDataStruct();
  for (i = 0; i < sizeof(lifxDriverData_t); i++)
  {
    ((char*)p_lifxData)[i] = EEPROM.read(EEPROM_LIFXDATA_IDX + i);
  }

  
  if (digitalRead(BUTTONPIN) == 0)
  {
    // Button pressed
    IPAddress local_IP(192,168,100,1);
    IPAddress gateway(192,168,100,254);
    IPAddress subnet(255,255,255,0);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASS);
  }
  else
  {
    i = 0;
    WiFi.macAddress(mac);
    sprintf_P(hostName, (PGM_P)F("kungFuHue-%02X%02X%02X%02X%02X%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
      if (i < 16)
      {
        setColor(i, ((uint32_t)ledBrightness << 16) | ((uint32_t)ledBrightness << 8) | ledBrightness);
        strip.show();
      }
      else
      {
        setColor(i - 16, 0);
        strip.show();
      }
      i++;
      if (i == 32) i = 0;
      delay(msDelay);
    }
    strip.clear();
    strip.show();
  }

#ifdef DEBUG
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
#endif DEBUG
  
  Wire.begin(4, 5);

  // Initialize APDS-9960 (configure I2C and initial values)
  if ( !apds.init() )
  {
    // Handle error somehow
    setColor(0, 0x300000);
    strip.show();
    while(1);
  }
  
  // Start running the APDS-9960 gesture sensor engine
  if ( !apds.enableGestureSensor(true) )
  {
    // Handle error
    setColor(1, 0x300000);
    strip.show();
    while(1);
  }

  server.on("/", handle_config_home);
  server.on("/wifi", handle_config_wifi);
  server.on("/driver", handle_config_driver);
  server.on("/hue", handle_config_hue);
  server.on("/devices", handle_config_devices);
  server.on("/devices_loaded", handle_config_devices_loaded);
  server.on("/restart", handle_restart);
  server.on("/rssi", handle_rssi);
  server.on("/findme", handle_find_me);
  server.on("/game", handle_game);
  server.on("/orientation", handle_config_orientation);
  server.on("/bri", handle_config_brightness);
  server.begin();

  loadAllParams = 0;
  displayMode = DISPLAY_MODE_MENU;
  discoveryState = HUE_DISCOVERY_IDLE;
  newUserState = HUE_NEWUSER_IDLE;

  activeParam = PARAM_ON;

  lastActionTime = 0;
}


void ifttt(char* trig, char* value1, char* value2, char* value3)
{
  char *buf;
  char *smallBuf;
  
  WiFiClientSecure client;
  
  client.setInsecure();
  if (client.connect("maker.ifttt.com", 443))
  {
    buf = (char*)malloc(MALLOCBUF_SIZE);
    smallBuf = (char*)malloc(256);
    
    sprintf_P(smallBuf, (PGM_P)F("{\"value1\":\"%s\",\"value2\":\"%s\",\"value3\":\"%s\"}"), value1, value2, value3);
    sprintf_P(buf, (PGM_P)F("POST https://maker.ifttt.com/trigger/%s/with/key/k9_NQnvOgKygfpOFmGvtR HTTP/1.1\r\nHost: maker.ifttt.com\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s\r\n\r\n"), trig, strlen(smallBuf), smallBuf);
    client.print(buf);

    free(smallBuf);
    free(buf);

    while (client.available())
    {
      client.read();
    }
    client.stop();
  }
}



void loop_hue()
{
  // Hue bridge discovery
  if (discoveryState == HUE_DISCOVERY_REQUEST)
  {
    if (hue_discoverBridge(discoveryUrl) == 0)
    {
      discoveryState = HUE_DISCOVERY_DISCOVERED;
    }
    else
    {
      discoveryState = HUE_DISCOVERY_IDLE;
    }
  }

  // Pairing with hue bridge
  if (newUserState == HUE_NEWUSER_REQUEST)
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    newHueUserReturnValue = hue_getNewHueUser(newHueUser, mac);
    newUserState = HUE_NEWUSER_EXECUTED;
  }

}


void loop_wiz()
{
  // Nothing to do.
}

void loop_yeelight()
{
  // Nothing to do.
}

void loop_lifx()
{
  // Nothing to do.  
}

void loop()
{
  currTime = millis();

  if (displayMode == DISPLAY_MODE_MENU)
  {
    // Normal mode
    if ( apds.isGestureAvailable() )
    {
      switch ( apds.readGesture() )
      {
        case DIR_UP:
          menuAction(mod(SWIPE_UP - orientation, 4));
          break;
        case DIR_DOWN:
          menuAction(mod(SWIPE_DOWN - orientation, 4));
          break;
        case DIR_LEFT:
          menuAction(mod(SWIPE_LEFT - orientation, 4));
          break;
        case DIR_RIGHT:
          menuAction(mod(SWIPE_RIGHT - orientation, 4));
          break;
        case DIR_NEAR:
          menuAction(SWIPE_IN);
          break;
        case DIR_FAR:
          menuAction(SWIPE_OUT);
          break;
        default:
          break;
      }
      lastActionTime = currTime;
    }

    if (activeParam == PARAM_ON)
    {
      strip.clear();
      strip.show();
    }
    else
    {
      if (currTime - lastActionTime > 30000)
      {
        menuAction(SWIPE_IN);
      }
    }

    if (digitalRead(BUTTONPIN) == 0)
    {
      // Switch to showing IP address. This will take a while
      displayMode = DISPLAY_MODE_IP;
      displayModeTime = millis();
      showIpStringPos = 0;
      sprintf(showIpString, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    }
  }
  else if ((displayMode == DISPLAY_MODE_IP) || (displayMode == DISPLAY_MODE_IP_PAUSE))
  {
    // Button clicked. We need to display IP address on LED ring
    if (currTime - displayModeTime > 2000)
    {
      displayModeTime = currTime;
      if (displayMode == DISPLAY_MODE_IP)
      {
        displayMode = DISPLAY_MODE_IP_PAUSE;
        showIpStringPos++;
      }
      else
      {
        displayMode = DISPLAY_MODE_IP;
      }
    }
    
    if (showIpString[showIpStringPos] != 0)
    {
      if (displayMode == DISPLAY_MODE_IP) displayDigit(showIpString[showIpStringPos]);
      else displayDigit(' ');
    }
    else
    {
      displayMode = DISPLAY_MODE_MENU;
    }
  }
  else if (displayMode == DISPLAY_MODE_RSSI)
  {
    if (currTime - displayModeTime > 5000)
    {
      displayMode = DISPLAY_MODE_MENU;
    }
    displayRssi();
  }
  else if (displayMode == DISPLAY_MODE_GAME)
  {
    static int shipPosition = -1;
    if (currTime - lastActionTime > 4000)
    {
      // If ship still alive
      if (shipPosition != -1)
      {
          strip.clear();
          setColor(2, 0x200000);
          setColor(6, 0x200000);
          setColor(10, 0x200000);
          setColor(14, 0x200000);
          strip.show();
          delay(500);
      }
      strip.clear();
      strip.show();
      delay(500);
      // new ship
      shipPosition = random(16);
      strip.clear();
      setColor(shipPosition, 0x200000);
      strip.show();
      lastActionTime = currTime;
    }
    
    if ( apds.isGestureAvailable() )
    {
      apds.readGesture();
      if (apds.getDeltasValid() == 1)
      {
        int x, y, led, i;
        x = apds.getLRdelta();
        y = -apds.getUDdelta();
        apds.resetDeltas();
        /*
        Serial.print(F("x: "));
        Serial.print(x);
        Serial.print(F(" y: "));
        Serial.print(y);
        Serial.print(F(" atan2(in degrees): "));
        Serial.println(atan2(y, x) * 180 / PI);
        */
        led = -((atan2(y, x) + PI/2) * 8 / PI);
        roll(led);
        /*
        strip.clear();
        strip.show();
        setColor(led, 0x060606);
        strip.show();
        */
        if (mod(led+8, 16) == shipPosition)
        {
          // Blow ship up
          for (i = 0; i < 3; i++)
          {
            strip.clear();
            setColor(shipPosition, 0x200000 << i );
            strip.show();
            delay(50);
          }
          for (i = 0; i < 4; i++)
          {
            strip.clear();
            setColor(shipPosition + i, 0x800000 >> (2 * i + 1));
            setColor(shipPosition - i, 0x800000 >> (2 * i + 1));
            strip.show();
            delay(100);
          }
          strip.clear();
          strip.show();
          delay(500);
          strip.clear();
          setColor(0, 0x002000);
          setColor(4, 0x002000);
          setColor(8, 0x002000);
          setColor(12, 0x002000);
          strip.show();
          delay(500);
          strip.clear();
          strip.show();
          shipPosition = -1;
        }

        // Clear any gestures queued up while showing animations
        while (apds.isGestureAvailable())
        {
          apds.readGesture();
        }
      }
    }
  }

  if (loadAllParams != 0)
  {
    uint8_t i;
    for (i = 0; i < NDISCOVERLIGHTS; i++)
    {
      allLights[i].ok = -1;
      allLights[i].name[0] = 0;
    }
    discoverLights(allLights);
    loadAllParams = 0;
  }
  
  server.handleClient();

  // Handle driver-specific tasks, e.g. Hue bridge discovery
  switch(driver)
  {
    case DRIVER_HUE:
      // loop_hue() will catch UDP packets to handle discovery process
      loop_hue();
      break;
    case DRIVER_WIZ:
      loop_wiz();
      break;
    case DRIVER_YEELIGHT:
      loop_yeelight();
      break;
    case DRIVER_LIFX:
      loop_lifx();
      break;
  }

  delay(10);
}
