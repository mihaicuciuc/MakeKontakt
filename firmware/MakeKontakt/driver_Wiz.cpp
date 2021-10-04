#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "driver_Wiz.h"
#include "color_transform.h"

static wizDriverData_t wizDriverData;


// Set the configuration for dev according to light number light in lights[] array
// Disable dev if light == -1
void wiz_configSlot(uint8_t dev, int light, discoverParams_t *lights)
{
  if (light == -1)
  {
    wizDriverData.deviceIP[dev] = IPAddress(0, 0, 0, 0);
    wizDriverData.deviceFlags[dev] = 0;
  }
  else if (lights[light].ok != -1)
  {
    wizDriverData.deviceIP[dev].fromString(lights[light].name);
    wizDriverData.deviceFlags[dev] = lights[light].deviceFlags;
  }
}


uint8_t wiz_getCurrentDevice()
{
  return wizDriverData.device;
}

uint8_t wiz_getDeviceEnabled(uint8_t dev)
{
  if (wizDriverData.deviceIP[dev] == IPAddress(0, 0, 0, 0))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

void wiz_setCurrentDevice(uint8_t dev)
{
  wizDriverData.device = dev;
}


int wiz_discoverLights(discoverParams_t *lights)
{
  WiFiUDP UDP;
  DynamicJsonBuffer jsonBuffer(2048);
  String s;
  unsigned long startTime, resendTime;
  int nLights, i;
  char *buf;

  UDP.begin(WIZ_UDP_PORT);

  buf = (char*)malloc(WIZ_MALLOCBUF_SIZE);

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
      UDP.beginPacket(IPAddress(255,255,255,255), 38899);
      sprintf_P(buf, (PGM_P)F("{\"method\":\"getSystemConfig\"}"));
      UDP.write(buf);
      UDP.endPacket();
      resendTime = millis();
    }

    yield();
    
    if (UDP.parsePacket() != 0)
    {
      int udpPacketSize;
      udpPacketSize = UDP.read(buf, WIZ_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println(buf);
#endif
      jsonBuffer.clear();
      JsonVariant variant = jsonBuffer.parse(buf);
    
      if (variant.is<JsonObject>())
      {
        JsonObject& root = variant;
    
        if (!root.success())
        {
          // Try another packet, we can't grok this one
          continue;
        }
    
        JsonVariant jsonVar;
 
        jsonVar = root["method"];
        if (jsonVar.success())
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
          lights[nLights].ok = -1;
          lights[nLights].group = 0;
          lights[nLights].deviceFlags = 0;
          strncpy(lights[nLights].name, UDP.remoteIP().toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1);
          
          jsonVar = root["result"]["moduleName"];
          if (jsonVar.success())
          {
            lights[nLights].ok = 1;
            if (strstr(jsonVar.as<String>().c_str(), "SHDW") != NULL)
            {
              lights[nLights].deviceFlags = WIZ_FLAG_BRI_ENABLED;
            }
            else if (strstr(jsonVar.as<String>().c_str(), "SHTW") != NULL)
            {
              lights[nLights].deviceFlags = WIZ_FLAG_BRI_ENABLED | WIZ_FLAG_CT_ENABLED;
            }
            else if (strstr(jsonVar.as<String>().c_str(), "SHRGB") != NULL)
            {
              lights[nLights].deviceFlags = WIZ_FLAG_BRI_ENABLED | WIZ_FLAG_CT_ENABLED | WIZ_FLAG_COLOR_ENABLED;
            }
          }
          
          nLights++;
          if (nLights == NDISCOVERLIGHTS)
          {
            // Stop now, ran out of free slots.
            break;
          }
        }
      }
    }
  }

  free(buf);
  UDP.stop();
  return nLights;
}


void wiz_getParam(deviceParams_t *deviceParams)
{
  WiFiUDP UDP;
  DynamicJsonBuffer jsonBuffer(2048);
  String s;
  unsigned long startTime, resendTime;
  int r, g, b;
  int udpPacketSize;
  char *buf;

  UDP.begin(WIZ_UDP_PORT);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  buf = (char*)malloc(WIZ_MALLOCBUF_SIZE);

  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < WIZ_TIMEOUT)
  {
    if (millis() - resendTime > WIZ_RESEND_TIME)
    {
      UDP.beginPacket(wizDriverData.deviceIP[wizDriverData.device], 38899);
      UDP.write("{\"method\":\"getPilot\"}");
      UDP.endPacket();
      resendTime = millis();
    }
    
    yield();
    
    if (UDP.parsePacket() != 0)
    {
      udpPacketSize = UDP.read(buf, WIZ_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println(buf);
#endif
      
      deviceParams->on = -1;

      jsonBuffer.clear();
      JsonVariant variant = jsonBuffer.parse(buf);
    
      if (variant.is<JsonObject>())
      {
        JsonObject& root = variant;
        if (root.success())
        {
          JsonVariant jsonVar;
        
          jsonVar = root["result"]["state"];
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

          deviceParams->bri = -1;
          jsonVar = root["result"]["dimming"];
          if (jsonVar.success()) deviceParams->bri = jsonVar.as<int>();
        
          deviceParams->ct = -1;
          deviceParams->hue = -1;
          deviceParams->sat = -1;

          jsonVar = root["result"]["temp"];
          if (jsonVar.success())
          {
            deviceParams->ct = jsonVar.as<int>();
            
            // Compute hue and sat from color temperature jsonVar.as<int>
            float r, g, b, h, s;
            ct_to_rgb(jsonVar.as<int>(), &r, &g, &b);
            rgb_to_hs(r, g, b, &h, &s);
            deviceParams->hue = int((h * 65535) / 360);
            deviceParams->sat = int((s * 65535) / 100);
          }

          r = -1;
          g = -1;
          b = -1;
          jsonVar = root["result"]["r"];
          if (jsonVar.success()) r = jsonVar.as<int>();
          jsonVar = root["result"]["g"];
          if (jsonVar.success()) g = jsonVar.as<int>();
          jsonVar = root["result"]["b"];
          if (jsonVar.success()) b = jsonVar.as<int>();

          if (r != -1 && g != -1 && b != -1)
          {
            // Compute color temperature from RGB
            rgb_to_ct(r, g, b, &deviceParams->ct);
            
            // Compute hue and sat from RGB
            float h, s;
            rgb_to_hs(r, g, b, &h, &s);
            deviceParams->hue = int((h * 65535) / 360);
            deviceParams->sat = int((s * 65535) / 100);
          }

#ifdef DEBUG
          Serial.println(UDP.remoteIP());
          Serial.println(deviceParams->on);
          Serial.println(deviceParams->bri);
          Serial.println(deviceParams->ct);
          Serial.println(deviceParams->hue);
          Serial.println(deviceParams->sat);
#endif
          if (deviceParams->on != -1)
          {
            // Successful packet receive
            break;
          }
        }
      }
    }
  }
  free(buf);
  UDP.stop();
}

void wiz_setParam(int param, deviceParams_t *deviceParams)
{
  WiFiUDP UDP;
  DynamicJsonBuffer jsonBuffer(2048);
  unsigned long startTime, resendTime;
  int udpPacketSize;
  char *buf;

  UDP.begin(WIZ_UDP_PORT);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  buf = (char*)malloc(WIZ_MALLOCBUF_SIZE);

  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) sprintf_P(buf, (PGM_P)F("{\"method\":\"setPilot\",\"params\":{\"state\":false}}"));
      else sprintf_P(buf, (PGM_P)F("{\"method\":\"setPilot\",\"params\":{\"state\":true}}"));
      break;
    case PARAM_BRI:
      sprintf_P(buf, (PGM_P)F("{\"method\":\"setPilot\",\"params\":{\"dimming\":%d}}"), deviceParams->bri);
      break;
    case PARAM_CT:
      sprintf_P(buf, (PGM_P)F("{\"method\":\"setPilot\",\"params\":{\"temp\":%d}}"), deviceParams->ct);
      break;
    case PARAM_HUE:
    case PARAM_SAT:
      // Get rgb values from hs
      float r, g, b;
      hs_to_rgb((deviceParams->hue * 360.0) / 65535.0, (deviceParams->sat * 100.0) / 65535.0, &r, &g, &b);
      sprintf_P(buf, (PGM_P)F("{\"method\":\"setPilot\",\"params\":{\"r\":%d,\"g\":%d,\"b\":%d}}"), (int)r, (int)g, (int)b);
      break;
    default:
      sprintf_P(buf, "{}");
      break;
  }

  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < WIZ_TIMEOUT)
  {
    if (millis() - resendTime > WIZ_RESEND_TIME)
    {
      UDP.beginPacket(wizDriverData.deviceIP[wizDriverData.device], 38899);
      UDP.write(buf);
      UDP.endPacket();
#ifdef DEBUG
      Serial.println(wizDriverData.deviceIP[wizDriverData.device]);
      Serial.println(buf);
#endif
      resendTime = millis();
    }
    
    yield();
    
    if (UDP.parsePacket() != 0)
    {
      udpPacketSize = UDP.read(buf, WIZ_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println(buf);
#endif

      jsonBuffer.clear();
      JsonVariant variant = jsonBuffer.parse(buf);
    
      if (variant.is<JsonObject>())
      {
        JsonObject& root = variant;
        if (root.success())
        {
          JsonVariant jsonVar;
        
          jsonVar = root["method"];
          if (jsonVar.success())
          {
            break;
          }
        }
      }
    }
  }
  free(buf);
  UDP.stop();
}

int wiz_hasParam(int param)
{
  switch (param)
  {
    case PARAM_ON:
      break;
    case PARAM_BRI:
      if ((wizDriverData.deviceFlags[wizDriverData.device] & WIZ_FLAG_BRI_ENABLED) == 0) return -1;
      break;
    case PARAM_CT:
      if ((wizDriverData.deviceFlags[wizDriverData.device] & WIZ_FLAG_CT_ENABLED) == 0) return -1;
      break;
    case PARAM_HUE:
      if ((wizDriverData.deviceFlags[wizDriverData.device] & WIZ_FLAG_COLOR_ENABLED) == 0) return -1;
      break;
    case PARAM_SAT:
      if ((wizDriverData.deviceFlags[wizDriverData.device] & WIZ_FLAG_COLOR_ENABLED) == 0) return -1;
      break;
    default:
      return -1;
      break;
  }
  return 1;
}

void wiz_incParam(int param, deviceParams_t *deviceParams)
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
      deviceParams->hue = mod(deviceParams->hue + 4096, 65536);
      break;
    case PARAM_SAT:
      deviceParams->sat += 4096;
      if (deviceParams->sat > 65535) deviceParams->sat = 65535;
      break;
  }
}

void wiz_decParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 1) deviceParams->on = 0;
      break;
    case PARAM_BRI:
      deviceParams->bri -= 13;
      if (deviceParams->bri < 0) deviceParams->bri = 0;
      break;
    case PARAM_CT:
      deviceParams->ct += 500;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue - 4096, 65536);
      break;
    case PARAM_SAT:
      deviceParams->sat -= 4096;
      if (deviceParams->sat < 0) deviceParams->sat = 0;
      break;
  }
}

int wiz_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights)
{
  if (lights[lightsIndex].ok != -1)
  {
    if (strncmp(lights[lightsIndex].name, wizDriverData.deviceIP[lightNumber].toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1) == 0)
    {
      return 1;
    }
  }
  
  return 0;
}


wizDriverData_t* wiz_getDriverDataStruct()
{
  return &wizDriverData;
}
