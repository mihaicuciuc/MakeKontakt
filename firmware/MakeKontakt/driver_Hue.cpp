#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "driver_Hue.h"

static hueDriverData_t hueDriverData;


// Set the configuration for dev according to light number light in lights[] array
// Disable dev if light == -1
void hue_configSlot(uint8_t dev, int light, discoverParams_t *lights)
{
  if (light == -1)
  {
    hueDriverData.deviceType[dev] = 0;
    hueDriverData.deviceIndex[dev] = 0;
  }
  else if (lights[light].ok != -1)
  {
    if (lights[light].group == 0)
    {
      hueDriverData.deviceType[dev] = 1;
    }
    else
    {
      hueDriverData.deviceType[dev] = 2;
    }
    hueDriverData.deviceIndex[dev] = lights[light].index;
  }
}


uint8_t hue_getCurrentDevice()
{
  return hueDriverData.device;
}

uint8_t hue_getDeviceEnabled(uint8_t dev)
{
  if (hueDriverData.deviceType[dev] == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

void hue_setCurrentDevice(uint8_t dev)
{
  hueDriverData.device = dev;
}


int hue_getDescriptionXml(char* location, char* url)
{
  HTTPClient http;
  int i, j, retVal;
  char *urlBase, *endUrlBase;
  char data;
  char *buf;

  buf = (char*)malloc(HUE_MALLOCBUF_SIZE);

  http.begin(location);
  http.addHeader("Content-Type", "text/plain");
  http.GET();

  http.setTimeout(500);

  i = 0;
  data = http.getStream().read();
  while (data != 0 && i < HUE_MALLOCBUF_SIZE - 1)
  {
    buf[i] = data;
    data = http.getStream().read();
    i++;
    if (i % 16 == 0)
    {
      delay(1);
      yield();
    }
  }
  buf[i] = 0;
  http.end();

#ifdef DEBUG
  Serial.println("Received XML data:");
  Serial.print(buf);
  Serial.println("------------------");
#endif

  retVal = -1;
  if ((strstr(buf, "<modelName>Philips hue") != NULL))
  {
    // This is not a great way of checking the XML but it'll do for now
    urlBase = strstr(buf, "<URLBase>");
    endUrlBase = strstr(buf, "</URLBase>");
    if (urlBase != NULL && endUrlBase != NULL)
    {
      *endUrlBase = 0;
      // Find first digit in IP address..
      i = 0;
      while ((urlBase[i] < '0') || (urlBase[i] > '9'))
      {
        i++;
      }
      // Put a terminator either before the port number, or on ending /
      j = i;
      while (urlBase[j] != ':' && urlBase[j] != '/' && urlBase[j] != 0)
      {
        j++;
      }
      urlBase[j] = 0;
      strcpy(url, &urlBase[i]);
      retVal = 0;
    }
  }

  free(buf);
  return retVal;
}

int hue_getNewHueUser(char *userName, uint8_t *mac)
{
  DynamicJsonBuffer jsonBuffer(2048);
  HTTPClient http;
  int n;
  String s;
  char *buf;

  buf = (char*)malloc(HUE_MALLOCBUF_SIZE);

  userName[0] = 0;

  sprintf_P(buf, (PGM_P)F("http://%s/api"), hueDriverData.hueIP);
  http.begin(buf);
  http.addHeader("Content-Type", "text/plain");

  sprintf_P(buf, (PGM_P)F("{\"devicetype\":\"kungFuHue#id%02X%02X%02X%02X%02X%02X\"}"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  http.POST(buf);
  http.setTimeout(100);

  free(buf);
  
  JsonVariant variant = jsonBuffer.parse(http.getStream());
  http.end();

  if (!variant.is<JsonArray>())
  {
    return -2;
  }
  
  JsonArray& arr = variant;
  variant = arr[0];
  if (!variant.is<JsonObject>())
  {
    return -3;
  }
  
  JsonObject& root = variant;

  if (!root.success())
  {
    return -4;
  }

  variant = root["success"];
  if (variant.success())
  {
    variant = root["success"]["username"];
    if (variant.success())
    {
      strcpy(userName, variant.as<String>().c_str());
    }
    return 0;
  }

  variant = root["error"];
  if (variant.success())
  {
    variant = root["error"]["description"];
    if (variant.success())
    {
      strcpy(userName, variant.as<String>().c_str());
    }
    variant = root["error"]["type"];
    if (variant.success())
    {
      n = variant.as<int>();
      return n;
    }
    else
    {
      return -5;
    }
  }
}

void hue_getLightName(JsonVariant *variant, char *lightName)
{
  lightName[0] = 0;
  if (variant->is<JsonObject>())
  {
    JsonObject& root = *variant;
    if (root.success())
    {
      JsonVariant jsonVar;
      jsonVar = root["name"];
      if (jsonVar.success())
      {
        strncpy(lightName, jsonVar.as<String>().c_str(), DISCOVERPARAMS_NAME_SIZE - 1);
      }
    }
  }
}

void hue_getParam(deviceParams_t *deviceParams)
{
  DynamicJsonBuffer jsonBuffer(2048);
  HTTPClient http;
  char *buf;
  String s;

  buf = (char*)malloc(HUE_MALLOCBUF_SIZE);
  
  if (hueDriverData.deviceType[hueDriverData.device] == 1)
  {
    sprintf_P(buf, (PGM_P)F("http://%s/api/%s/lights/%d"), hueDriverData.hueIP, hueDriverData.hueUser, hueDriverData.deviceIndex[hueDriverData.device]);
  }
  else if (hueDriverData.deviceType[hueDriverData.device] == 2)
  {
    sprintf_P(buf, (PGM_P)F("http://%s/api/%s/groups/%d"), hueDriverData.hueIP, hueDriverData.hueUser, hueDriverData.deviceIndex[hueDriverData.device]);
  }

  http.begin(buf);
  http.addHeader("Content-Type", "text/plain");
  http.GET();

  http.setTimeout(100);

  JsonVariant variant = jsonBuffer.parse(http.getStream());
  http.end();

  deviceParams->on = -1;
  if (variant.is<JsonObject>())
  {
    JsonObject& root = variant;

    if (root.success())
    {
      JsonVariant jsonVar;
    
      if (hueDriverData.deviceType[hueDriverData.device] == 1) jsonVar = root["state"]["on"];
      else if (hueDriverData.deviceType[hueDriverData.device] == 2) jsonVar = root["action"]["on"];
      if (jsonVar.success())
      {
        s = jsonVar.as<String>();
        if (s == "false") deviceParams->on = 0;
        else deviceParams->on = 1;
      }
      else
      {
        deviceParams->on = -1;
      }

      if (hueDriverData.deviceType[hueDriverData.device] == 1) jsonVar = root["state"]["bri"];
      else if (hueDriverData.deviceType[hueDriverData.device] == 2) jsonVar = root["action"]["bri"];
      if (jsonVar.success()) deviceParams->bri = jsonVar.as<int>();
      else deviceParams->bri = -1;
    
      if (hueDriverData.deviceType[hueDriverData.device] == 1) jsonVar = root["state"]["ct"];
      else if (hueDriverData.deviceType[hueDriverData.device] == 2) jsonVar = root["action"]["ct"];
      if (jsonVar.success()) deviceParams->ct = jsonVar.as<int>();
      else deviceParams->ct = -1;
    
      if (hueDriverData.deviceType[hueDriverData.device] == 1) jsonVar = root["state"]["hue"];
      else if (hueDriverData.deviceType[hueDriverData.device] == 2) jsonVar = root["action"]["hue"];
      if (jsonVar.success()) deviceParams->hue = jsonVar.as<int>();
      else deviceParams->hue = -1;
    
      if (hueDriverData.deviceType[hueDriverData.device] == 1) jsonVar = root["state"]["sat"];
      else if (hueDriverData.deviceType[hueDriverData.device] == 2) jsonVar = root["action"]["sat"];
      if (jsonVar.success()) deviceParams->sat = jsonVar.as<int>();
      else deviceParams->sat = -1;
    }
  }

  free(buf);
}


void hue_setParam(int param, deviceParams_t *deviceParams)
{
  HTTPClient http;
  String s;
  char *buf;

  buf = (char*)malloc(HUE_MALLOCBUF_SIZE);
  
  if (hueDriverData.deviceType[hueDriverData.device] == 1)
  {
    sprintf_P(buf, (PGM_P)F("http://%s/api/%s/lights/%d/state"), hueDriverData.hueIP, hueDriverData.hueUser, hueDriverData.deviceIndex[hueDriverData.device]);
  }
  else if (hueDriverData.deviceType[hueDriverData.device] == 2)
  {
    sprintf_P(buf, (PGM_P)F("http://%s/api/%s/groups/%d/action"), hueDriverData.hueIP, hueDriverData.hueUser, hueDriverData.deviceIndex[hueDriverData.device]);
  }

  http.begin(buf);
  http.addHeader("Content-Type", "text/plain");

#ifdef DEBUG
  Serial.println(buf);
#endif

  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) sprintf_P(buf, (PGM_P)F("{\"on\":false}"));
      else sprintf_P(buf, (PGM_P)F("{\"on\":true}"));
      break;
    case PARAM_BRI:
      sprintf_P(buf, (PGM_P)F("{\"bri\":%d}"), deviceParams->bri);
      break;
    case PARAM_CT:
      sprintf_P(buf, (PGM_P)F("{\"ct\":%d}"), deviceParams->ct);
      break;
    case PARAM_HUE:
      sprintf_P(buf, (PGM_P)F("{\"hue\":%d}"), deviceParams->hue);
      break;
    case PARAM_SAT:
      sprintf_P(buf, (PGM_P)F("{\"sat\":%d}"), deviceParams->sat);
      break;
    default:
      sprintf_P(buf, "{}");
      break;
  }
  http.PUT(buf);
  http.end();
#ifdef DEBUG
  Serial.println(buf);
#endif
  free(buf);
}

int hue_hasParam(int param)
{
  deviceParams_t deviceParams;
  hue_getParam(&deviceParams);
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams.on == -1) return -1;
      break;
    case PARAM_BRI:
      if (deviceParams.bri == -1) return -1;
      break;
    case PARAM_CT:
      if (deviceParams.ct == -1) return -1;
      break;
    case PARAM_HUE:
      if (deviceParams.hue == -1) return -1;
      break;
    case PARAM_SAT:
      if (deviceParams.sat == -1) return -1;
      break;
    default:
      return -1;
      break;
  }
  return 1;
}

void hue_incParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) deviceParams->on = 1;
      break;
    case PARAM_BRI:
      deviceParams->bri += 32;
      if (deviceParams->bri > 255) deviceParams->bri = 255;
      break;
    case PARAM_CT:
      deviceParams->ct += 32;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue + 4096, 65536);
      break;
    case PARAM_SAT:
      deviceParams->sat += 32;
      if (deviceParams->sat > 255) deviceParams->sat = 255;
      break;
  }
}

void hue_decParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 1) deviceParams->on = 0;
      break;
    case PARAM_BRI:
      deviceParams->bri -= 32;
      if (deviceParams->bri < 1) deviceParams->bri = 1;
      break;
    case PARAM_CT:
      deviceParams->ct -= 32;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue - 4096, 65536);
      break;
    case PARAM_SAT:
      deviceParams->sat -= 32;
      if (deviceParams->sat < 0) deviceParams->sat = 0;
      break;
  }
}


int hue_discoverBridge(char* url)
{
  WiFiUDP UDP;
  char xmlUrl[64];
  unsigned long startTime, resendTime;
  int udpPacketSize;
  char *buf;
  char *location;
  uint8_t done;

  UDP.begin(HUE_UDP_PORT);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  done = 0;
  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < 2000 && done == 0)
  {
    if (millis() - resendTime > 500)
    {
      // First step in Hue bridge discovery
      IPAddress multicastIpAddress(239,255,255,250);
      UDP.beginPacketMulticast(multicastIpAddress, 1900, UDP.destinationIP());
      UDP.write("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: ssdp:discover\r\nMX: 10\r\nST: ssdp:all\r\n");
      UDP.endPacket();
      resendTime = millis();
    }

    yield();
    
    if (UDP.parsePacket() != 0)
    {
      // Second step in Hue bridge discovery, receiving UDP reply
      buf = (char*)malloc(HUE_MALLOCBUF_SIZE);
      udpPacketSize = UDP.read(buf, HUE_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println(buf);
#endif

      // Look for URL in reply
      location = strstr(buf, "http://");
      // Now place a string terminator after URL, reusing udpPacketSize
      udpPacketSize = 0;
      while (location[udpPacketSize] != ' ' && location[udpPacketSize] != '\r' && location[udpPacketSize] != '\n' && location[udpPacketSize] != 0)
      {
        udpPacketSize++;
      }
      location[udpPacketSize] = 0;
      strncpy(xmlUrl, location, 64);
      free(buf);
      if (hue_getDescriptionXml(xmlUrl, xmlUrl) == 0)   // Reuse same buffer for input and output
      {
        done = 1;
      }
    }
  }
  
  UDP.stop();
  if (done == 1)
  {
    strncpy(url, xmlUrl, 64);
    return 0;
  }
  else
  {
    return -1;
  }
}

int hue_discoverLightsOrGroups(uint8_t dType, discoverParams_t *lights, int nLightsMax)
{
  DynamicJsonBuffer jsonBuffer(2048);
  HTTPClient http;
  int i, nBraces, nQuotes, n;
  char *buf, *jsonBegin, *idxBegin;
  char data;
  char lightName[DISCOVERPARAMS_NAME_SIZE];

  buf = (char*)malloc(HUE_MALLOCDISCOVERBUF_SIZE);
  
  if (dType == 1)
  {
    sprintf_P(buf, (PGM_P)F("http://%s/api/%s/lights"), hueDriverData.hueIP, hueDriverData.hueUser);
  }
  else if (dType == 2)
  {
    sprintf_P(buf, (PGM_P)F("http://%s/api/%s/groups"), hueDriverData.hueIP, hueDriverData.hueUser);
  }

  http.begin(buf);
  http.addHeader("Content-Type", "text/plain");
  http.GET();

  n = 0;  // Number of lights/groups found
  nQuotes = 0;
  nBraces = 0;
  i = 0;
  data = http.getStream().read();
  while (data != 0 && i < HUE_MALLOCDISCOVERBUF_SIZE - 1)
  {
    buf[i] = data;

    if (data == '"')
    {
      if (nBraces == 1)
      {
        if (nQuotes == 0)
        {
          // This is the first quote so on the next char starts the index for our next light, lights[n]
          nQuotes = 1;
          idxBegin = &buf[i + 1];
        }
        else
        {
          // Second quote. Parse number.
          nQuotes = 0;
          buf[i] = 0;
          lights[n].index = atoi(idxBegin);
#ifdef DEBUG
          Serial.print("found index: ");
          Serial.println(lights[n].index);
#endif
        }
      }
    }

    if (data == '{')
    {
      nBraces++;
      if (nBraces == 2)
      {
        // This is the beginning of a light/group that we need to properly parse. Save location
        jsonBegin = &buf[i];
      }
    }

    if (data == '}')
    {
      nBraces--;
      if (nBraces == 1)
      {
        // This is the position of the last brace in a light/group that we need to properly parse.
        buf[i+1] = 0;
#ifdef DEBUG
        Serial.print("dType = ");
        Serial.print(dType);
        Serial.print(", found json: ");
        Serial.println(jsonBegin);
#endif
        jsonBuffer.clear();
        JsonVariant variant = jsonBuffer.parse(jsonBegin);
        hue_getLightName(&variant, lightName);
        strncpy(lights[n].name, lightName, DISCOVERPARAMS_NAME_SIZE);
        if (lightName[0] != 0) lights[n].ok = 1;
        else lights[n].ok = -1;
        
        if (dType == 1)
        {
          lights[n].group = 0;
        }
        else if (dType == 2)
        {
          lights[n].group = 1;
        }
        n++;

        if (n == nLightsMax)
        {
          // Stop now, ran out of free slots.
          break;
        }
        
        // Reset i, to start reusing same buf[] for next light. Use -1 to compensate for i++ further down.
        i = -1;
      }
    }
    
    data = http.getStream().read();
    i++;
    if (i % 16 == 0)
    {
      delay(1);
      yield();
    }
  }
  http.end();

  free(buf);
  return n;
}


int hue_discoverLights(discoverParams_t *lights)
{
  int n;
  // Do groups discovery first
  n = hue_discoverLightsOrGroups(2, lights, NDISCOVERLIGHTS);
  n += hue_discoverLightsOrGroups(1, &lights[n], NDISCOVERLIGHTS - n);
  return n;
}


int hue_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights)
{
  if (lights[lightsIndex].ok != -1)
  {
    if (hueDriverData.deviceIndex[lightNumber] == lights[lightsIndex].index)
    {
      if (hueDriverData.deviceType[lightNumber] == 1 && lights[lightsIndex].group == 0)
      {
        return 1;
      }

      if (hueDriverData.deviceType[lightNumber] == 2 && lights[lightsIndex].group != 0)
      {
        return 1;
      }
    }
  }

  return 0;
}

hueDriverData_t* hue_getDriverDataStruct()
{
  return &hueDriverData;
}
