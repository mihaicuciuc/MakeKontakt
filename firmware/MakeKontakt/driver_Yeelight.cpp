#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include "driver_Yeelight.h"

static yeelightDriverData_t yeelightDriverData;


// Set the configuration for dev according to light number light in lights[] array
// Disable dev if light == -1
void yeelight_configSlot(uint8_t dev, int light, discoverParams_t *lights)
{
  if (light == -1)
  {
    yeelightDriverData.deviceIP[dev] = IPAddress(0, 0, 0, 0);
    yeelightDriverData.deviceFlags[dev] = 0;
  }
  else if (lights[light].ok != -1)
  {
    yeelightDriverData.deviceIP[dev].fromString(lights[light].name);
    yeelightDriverData.deviceFlags[dev] = lights[light].deviceFlags;
  }
}


uint8_t yeelight_getCurrentDevice()
{
  return yeelightDriverData.device;
}

uint8_t yeelight_getDeviceEnabled(uint8_t dev)
{
  if (yeelightDriverData.deviceIP[dev] == IPAddress(0, 0, 0, 0))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

void yeelight_setCurrentDevice(uint8_t dev)
{
  yeelightDriverData.device = dev;
}


int yeelight_discoverLights(discoverParams_t *lights)
{
  WiFiUDP UDP;
  String s;
  unsigned long startTime, resendTime;
  int nLights, i;
  char *buf;
  char *location;

  UDP.begin(YEELIGHT_UDP_PORT);

  buf = (char*)malloc(YEELIGHT_MALLOCBUF_SIZE);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  nLights = 0;
  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < 2000)
  {
    if (millis() - resendTime > 500)
    {
      UDP.beginPacketMulticast(IPAddress(239,255,255,250), 1982, UDP.destinationIP());
      sprintf_P(buf, (PGM_P)F("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb\r\n"));
      UDP.write(buf);
      UDP.endPacket();
      resendTime = millis();
    }

    yield();
    
    if (UDP.parsePacket() != 0)
    {
      int udpPacketSize;
      udpPacketSize = UDP.read(buf, YEELIGHT_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println("-------------------------------------------");
      Serial.println(UDP.remoteIP());
      Serial.println(buf);
#endif

      // Check if proper light replied
      location = strstr(buf, "yeelight://");
      if (location != NULL)
      {
        for (i = 0; i < nLights; i++)
        {
          if (strncmp(lights[i].name, UDP.remoteIP().toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1) == 0)
          {
            // Let's look at another UDP packet. Will continue out of while loop with the line after this for.
            break;
          }
        }
        if (i != nLights)
        {
          continue;
        }
        // Good, new light. Add it.
        strncpy(lights[nLights].name, UDP.remoteIP().toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1);
        lights[nLights].group = 0;
        lights[nLights].ok = 0;
        lights[nLights].deviceFlags = 0;
        if (strstr(buf, "set_bright") != NULL) lights[nLights].deviceFlags |= YEELIGHT_FLAG_BRI_ENABLED;
        if (strstr(buf, "set_ct_abx") != NULL) lights[nLights].deviceFlags |= YEELIGHT_FLAG_CT_ENABLED;
        if (strstr(buf, "set_hsv") != NULL) lights[nLights].deviceFlags |= YEELIGHT_FLAG_COLOR_ENABLED;
        
        nLights++;
        if (nLights == NDISCOVERLIGHTS)
        {
          // Stop now, ran out of free slots.
          break;
        }
      }
    }
  }

  free(buf);
  UDP.stop();
  return nLights;
}



void yeelight_getParam(deviceParams_t *deviceParams)
{
  WiFiClient TCP;
  DynamicJsonBuffer jsonBuffer(2048);
  String s;
  unsigned long timeout;
  char *buf;
  int i;

  if (TCP.connect(yeelightDriverData.deviceIP[yeelightDriverData.device], 55443))
  {
    TCP.print("{\"id\":1,\"method\":\"get_prop\",\"params\":[\"power\",\"bright\",\"ct\",\"hue\",\"sat\"]}");
    TCP.print("\r\n");

    timeout = millis();
    while (TCP.available() == 0)
    {
      if (millis() - timeout > 500)
      {
        deviceParams->on = -1;
        TCP.stop();
        return;
      }
      delay(1);
    }

    buf = (char*)malloc(YEELIGHT_MALLOCBUF_SIZE);

    i = 0;
    while (TCP.available() && i < YEELIGHT_MALLOCBUF_SIZE - 1)
    {
      buf[i] = TCP.read();
      i++;
      if (i % 16 == 0)
      {
        delay(1);
        yield();
      }
    }
    buf[i] = 0;

#ifdef DEBUG
    Serial.println(buf);
#endif

    deviceParams->on = -1;
    
    JsonVariant variant = jsonBuffer.parse(buf);
    
    if (variant.is<JsonObject>())
    {
      JsonObject& root = variant;
      if (root.success())
      {
        JsonVariant jsonVar;
        jsonVar = root["result"];
        if (jsonVar.is<JsonArray>())
        {
          deviceParams->on = -1;
          s = jsonVar[0].as<String>();
          if (s == "off") deviceParams->on = 0;
          else if (s == "on") deviceParams->on = 1;

          deviceParams->bri = -1;
          s = jsonVar[1].as<String>();
          if (s != "") deviceParams->bri = jsonVar[1].as<int>();

          deviceParams->ct = -1;
          s = jsonVar[2].as<String>();
          if (s != "") deviceParams->ct = jsonVar[2].as<int>();

          deviceParams->hue = -1;
          s = jsonVar[3].as<String>();
          if (s != "") deviceParams->hue = jsonVar[3].as<int>();
            
          deviceParams->sat = -1;
          s = jsonVar[4].as<String>();
          if (s != "") deviceParams->sat = jsonVar[4].as<int>();

#ifdef DEBUG
          Serial.println(TCP.remoteIP());
          Serial.println(deviceParams->on);
          Serial.println(deviceParams->bri);
          Serial.println(deviceParams->ct);
          Serial.println(deviceParams->hue);
          Serial.println(deviceParams->sat);
#endif

        }
      }
    }

    free(buf);
  }
  TCP.stop();
}


void yeelight_setParam(int param, deviceParams_t *deviceParams)
{
  WiFiClient TCP;
  char *buf;

  buf = (char*)malloc(YEELIGHT_MALLOCBUF_SIZE);
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) sprintf_P(buf, (PGM_P)F("{\"id\":1,\"method\":\"set_power\",\"params\":[\"off\",\"smooth\",500]}"));
      else sprintf_P(buf, (PGM_P)F("{\"id\":1,\"method\":\"set_power\",\"params\":[\"on\",\"smooth\",500]}"));
      break;
    case PARAM_BRI:
      sprintf_P(buf, (PGM_P)F("{\"id\":1,\"method\":\"set_bright\",\"params\":[%d,\"smooth\",500]}"), deviceParams->bri);
      break;
    case PARAM_CT:
      sprintf_P(buf, (PGM_P)F("{\"id\":1,\"method\":\"set_ct_abx\",\"params\":[%d,\"smooth\",500]}"), deviceParams->ct);
      break;
    case PARAM_HUE:
      sprintf_P(buf, (PGM_P)F("{\"id\":1,\"method\":\"set_hsv\",\"params\":[%d,%d,\"smooth\",500]}"), deviceParams->hue, deviceParams->sat);
      break;
    case PARAM_SAT:
      sprintf_P(buf, (PGM_P)F("{\"id\":1,\"method\":\"set_hsv\",\"params\":[%d,%d,\"smooth\",500]}"), deviceParams->hue, deviceParams->sat);
      break;
    default:
      sprintf_P(buf, "{}");
      break;
  }

  if (TCP.connect(yeelightDriverData.deviceIP[yeelightDriverData.device], 55443))
  {
    TCP.print(buf);
    TCP.print("\r\n");
  }

#ifdef DEBUG
  Serial.println(yeelightDriverData.deviceIP[yeelightDriverData.device]);
  Serial.println(buf);
#endif
  free(buf);
  TCP.stop();
}

int yeelight_hasParam(int param)
{
  switch (param)
  {
    case PARAM_ON:
      break;
    case PARAM_BRI:
      if ((yeelightDriverData.deviceFlags[yeelightDriverData.device] & YEELIGHT_FLAG_BRI_ENABLED) == 0) return -1;
      break;
    case PARAM_CT:
      if ((yeelightDriverData.deviceFlags[yeelightDriverData.device] & YEELIGHT_FLAG_CT_ENABLED) == 0) return -1;
      break;
    case PARAM_HUE:
      if ((yeelightDriverData.deviceFlags[yeelightDriverData.device] & YEELIGHT_FLAG_COLOR_ENABLED) == 0) return -1;
      break;
    case PARAM_SAT:
      if ((yeelightDriverData.deviceFlags[yeelightDriverData.device] & YEELIGHT_FLAG_COLOR_ENABLED) == 0) return -1;
      break;
    default:
      return -1;
      break;
  }
  return 1;
}

void yeelight_incParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) deviceParams->on = 1;
      break;
    case PARAM_BRI:
      deviceParams->bri += 13;
      if (deviceParams->bri > 100) deviceParams->bri = 100;
      break;
    case PARAM_CT:
      deviceParams->ct -= 500;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue + 23, 360);
      break;
    case PARAM_SAT:
      deviceParams->sat += 13;
      if (deviceParams->sat > 100) deviceParams->sat = 100;
      break;
  }
}

void yeelight_decParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 1) deviceParams->on = 0;
      break;
    case PARAM_BRI:
      deviceParams->bri -= 13;
      if (deviceParams->bri < 1) deviceParams->bri = 1;
      break;
    case PARAM_CT:
      deviceParams->ct += 500;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue - 23, 360);
      break;
    case PARAM_SAT:
      deviceParams->sat -= 13;
      if (deviceParams->sat < 0) deviceParams->sat = 0;
      break;
  }
}

int yeelight_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights)
{
  if (lights[lightsIndex].ok != -1)
  {
    if (strncmp(lights[lightsIndex].name, yeelightDriverData.deviceIP[lightNumber].toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1) == 0)
    {
      return 1;
    }
  }
  
  return 0;
}


yeelightDriverData_t* yeelight_getDriverDataStruct()
{
  return &yeelightDriverData;
}
