#include <Arduino.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include "driver_Lifx.h"

static lifxDriverData_t lifxDriverData;

const uint8_t productFlags[] PROGMEM = {
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 1
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 3
  0,
  0,
  0,
  0,
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 10
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 11
  0,
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 15
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 18
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 19
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 20
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 22
  0,
  0,
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 27
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 28
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 29
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 30
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 31
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 32
  0,
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 36
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 37
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 38
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 39
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 40
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 43
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 44
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 45
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 46
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 49
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 50
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 51
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 52
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 53
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 55
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 57
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 59
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 60
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 61
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 62
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 63
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 64
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 65
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 66
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 68
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 81  
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 82
  0,
  0,
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 85
  0,
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 87
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 88
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 90
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 91
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 92
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 93
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 94
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 96
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 97
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 98
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 99
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 100
  LIFX_FLAG_BRI_ENABLED,                                                    // Product 101
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 109
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 110
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 111
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED | LIFX_FLAG_COLOR_ENABLED,   // Product 112
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 113
  LIFX_FLAG_BRI_ENABLED | LIFX_FLAG_CT_ENABLED,                             // Product 114
};

void lifx_setHeader(lx_protocol_header_t *h, int payloadSize, int addr, uint8_t *mac, uint16_t packetType)
{
  memset(h, 0, sizeof(lx_protocol_header_t));
  
  h->size = 36 + payloadSize;
  h->protocol = 1024;
  if (addr != 0)
  {
    h->addressable = 1;
    h->tagged = 0;
    h->target[0] = mac[0];
    h->target[1] = mac[1];
    h->target[2] = mac[2];
    h->target[3] = mac[3];
    h->target[4] = mac[4];
    h->target[5] = mac[5];
  }
  else
  {
    h->addressable = 1;
    h->tagged = 1;
    h->target[0] = 0;
    h->target[1] = 0;
    h->target[2] = 0;
    h->target[3] = 0;
    h->target[4] = 0;
    h->target[5] = 0;
  }
  h->origin = 0;
  h->source = 2;
  h->res_required = 1;
  h->ack_required = 0;
  h->sequence = 1;
  h->type = packetType;

#ifdef DEBUG
  int i;
  Serial.println("-------------------------");
  for (i = 0; i < sizeof(lx_protocol_header_t); i++)
  {
    Serial.print(((char*)h)[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
#endif
}


// Set the configuration for dev according to light number light in lights[] array
// Disable dev if light == -1
void lifx_configSlot(uint8_t dev, int light, discoverParams_t *lights)
{
  int n, i;
  if (light == -1)
  {
    lifxDriverData.deviceIP[dev] = IPAddress(0, 0, 0, 0);
    lifxDriverData.devicePort[dev] = 0;
    lifxDriverData.deviceFlags[dev] = 0;
    lifxDriverData.deviceMac[dev][0] = 0;
    lifxDriverData.deviceMac[dev][1] = 0;
    lifxDriverData.deviceMac[dev][2] = 0;
    lifxDriverData.deviceMac[dev][3] = 0;
    lifxDriverData.deviceMac[dev][4] = 0;
    lifxDriverData.deviceMac[dev][5] = 0;
  }
  else if (lights[light].ok != -1)
  {
    lifxDriverData.deviceIP[dev].fromString(lights[light].name);
    lifxDriverData.deviceFlags[dev] = lights[light].deviceFlags;
    n = strlen(lights[light].name);
    n++;
    lifxDriverData.devicePort[dev] = lights[light].name[n + 1];
    lifxDriverData.devicePort[dev] <<= 8;
    lifxDriverData.devicePort[dev] += lights[light].name[n];
    n += 2;
    for (i = 0; i < 6; i++)
    {
      lifxDriverData.deviceMac[dev][i] = lights[light].name[n + i];
    }
  }
}


uint8_t lifx_getCurrentDevice()
{
  return lifxDriverData.device;
}

uint8_t lifx_getDeviceEnabled(uint8_t dev)
{
  if (lifxDriverData.deviceIP[dev] == IPAddress(0, 0, 0, 0))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

void lifx_setCurrentDevice(uint8_t dev)
{
  lifxDriverData.device = dev;
}



int lifx_discoverLightsBasic(discoverParams_t *lights)
{
  WiFiUDP UDP;
  unsigned long startTime, resendTime;
  int nLights, i, n;
  char *buf;
  int ok;
  lx_protocol_header_t header;
  lx_protocol_header_t *p_header;
  uint32_t port;
  uint8_t service;
  char* p_payload;

  UDP.begin(LIFX_UDP_PORT);

  buf = (char*)malloc(LIFX_MALLOCBUF_SIZE);

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
      lifx_setHeader(&header, 0, 0, NULL, 2);  // GetService, packet 2
      UDP.beginPacket(IPAddress(255,255,255,255), 56700);
      UDP.write(((char*)&header), sizeof(header));
      UDP.endPacket();
      resendTime = millis();
    }

    yield();
    
    if (UDP.parsePacket() != 0)
    {
      int udpPacketSize;
      udpPacketSize = UDP.read(buf, LIFX_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println("----------lifx_discoverLights reply------------------");
      Serial.println(UDP.remoteIP());
      for (i = 0; i < 64; i++)
      {
        if (i % 16 == 0) Serial.println();
        Serial.print(buf[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
#endif

      p_header = (lx_protocol_header_t*)buf;
      p_payload = &buf[sizeof(lx_protocol_header_t)];
      ok = 1;

      if (p_header->size != 41) ok = 0;
      if (p_header->protocol != 1024) ok = 0;
      if (p_header->addressable != 1) ok = 0;
      if (p_header->type != 3) ok = 0;
      if (((uint8_t)*p_payload) != 1) ok = 0;    // service must be UDP

#ifdef DEBUG
      Serial.print("size: ");
      Serial.println(p_header->size);
      Serial.print("protocol: ");
      Serial.println(p_header->protocol);
      Serial.print("addressable: ");
      Serial.println(p_header->addressable);
      Serial.print("type: ");
      Serial.println(p_header->type);
      Serial.print("p_payload: ");
      Serial.println((uint8_t)*p_payload);
      Serial.print("p_payload+1: ");
      port = *((uint8_t*)(p_payload + 2));
      port <<= 8;
      port += *((uint8_t*)(p_payload + 1));
      Serial.println(port);
#endif

      if (ok == 1)
      {
        strncpy(lights[nLights].name, UDP.remoteIP().toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1);
        n = strlen(lights[nLights].name);
        n++;
        lights[nLights].name[n] = *((uint8_t*)(p_payload + 1));
        n++;
        lights[nLights].name[n] = *((uint8_t*)(p_payload + 2));
        n++;
        for (i = 0; i < 6; i++)
        {
          lights[nLights].name[n + i] = p_header->target[i];
        }

        for (i = 0; i < nLights; i++)
        {
          if (strncmp(lights[i].name, lights[nLights].name, DISCOVERPARAMS_NAME_SIZE - 1) == 0)
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
        lights[nLights].group = 0;
        lights[nLights].ok = 0;
        lights[nLights].deviceFlags = 0;
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


void lifx_getFlags(int light, discoverParams_t *lights)
{
  WiFiUDP UDP;
  unsigned long startTime, resendTime;
  int i, n, ok;
  char *buf;
  lx_protocol_header_t header;
  lx_protocol_header_t *p_header;
  char* p_payload;

  uint32_t vendor, product;
  
  IPAddress ip;
  uint16_t port;
  uint8_t mac[6];

  // Get connection parameters encoded in light name
  ip.fromString(lights[light].name);
  n = strlen(lights[light].name);
  n++;
  port = lights[light].name[n + 1];
  port <<= 8;
  port += lights[light].name[n];
  n += 2;
  for (i = 0; i < 6; i++)
  {
    mac[i] = lights[light].name[n + i];
  }

  UDP.begin(LIFX_UDP_PORT);

  buf = (char*)malloc(LIFX_MALLOCBUF_SIZE);

/*
  Serial.println("----productFlags-----");
  for(i = 0; i < sizeof(productFlags); i++)
  {
    sprintf(buf, "[%d]: %d", i, pgm_read_byte(productFlags + i));
    Serial.println(buf);
  }
  Serial.println("---/productFlags-----");
*/

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }
  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < 2000)
  {
    if (millis() - resendTime > 500)
    {
      lifx_setHeader(&header, 0, 1, mac, 32);  // GetVersion
      UDP.beginPacket(ip, port);
      UDP.write(((char*)&header), sizeof(header));
      UDP.endPacket();
      resendTime = millis();
    }

    yield();
    
    if (UDP.parsePacket() != 0)
    {
      int udpPacketSize;
      udpPacketSize = UDP.read(buf, LIFX_MALLOCBUF_SIZE - 1);
      buf[udpPacketSize] = 0;
      UDP.flush();
#ifdef DEBUG
      Serial.println("----------lifx_getFlags reply------------------");
      Serial.println(UDP.remoteIP());
      for (i = 0; i < 64; i++)
      {
        if (i % 16 == 0) Serial.println();
        Serial.print(buf[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
#endif

      p_header = (lx_protocol_header_t*)buf;
      p_payload = &buf[sizeof(lx_protocol_header_t)];
      ok = 1;
      
      if (p_header->size != 48) ok = 0;
      if (p_header->protocol != 1024) ok = 0;
      if (p_header->addressable != 1) ok = 0;
      if (p_header->type != 33) ok = 0;

#ifdef DEBUG
      Serial.print("size: ");
      Serial.println(p_header->size);
      Serial.print("protocol: ");
      Serial.println(p_header->protocol);
      Serial.print("addressable: ");
      Serial.println(p_header->addressable);
      Serial.print("type: ");
      Serial.println(p_header->type);
#endif

      if (ok == 1)
      {
        vendor = *((uint8_t*)(p_payload + 3));
        vendor <<= 8;
        vendor |= *((uint8_t*)(p_payload + 2));
        vendor <<= 8;
        vendor |= *((uint8_t*)(p_payload + 1));
        vendor <<= 8;
        vendor |= *((uint8_t*)(p_payload));

        product = *((uint8_t*)(p_payload + 7));
        product <<= 8;
        product |= *((uint8_t*)(p_payload + 6));
        product <<= 8;
        product |= *((uint8_t*)(p_payload + 5));
        product <<= 8;
        product |= *((uint8_t*)(p_payload + 4));

        if ((vendor == 1) && (product < sizeof(productFlags)))
        {
          lights[light].deviceFlags = pgm_read_byte(productFlags + product);
          break;
        }
      }
    }
  }

  free(buf);
  UDP.stop();
}

int lifx_discoverLights(discoverParams_t *lights)
{
  int nLights, i;
  nLights = lifx_discoverLightsBasic(lights);
  for (i = 0; i < nLights; i++)
  {
    lifx_getFlags(i, lights);
  }
  return nLights;
}


void lifx_getParam(deviceParams_t *deviceParams)
{
  WiFiUDP UDP;
  unsigned long startTime, resendTime;
  int ok, power;
  lx_protocol_header_t header;
  lx_protocol_header_t* p_header;
  char *buf;
  char* p_payload;

  UDP.begin(LIFX_UDP_PORT);

  buf = (char*)malloc(LIFX_MALLOCBUF_SIZE);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < LIFX_TIMEOUT)
  {
    if (millis() - resendTime > LIFX_RESEND_TIME)
    {
      lifx_setHeader(&header, 0, 1, lifxDriverData.deviceMac[lifxDriverData.device], 101);    // GetColor
      UDP.beginPacket(lifxDriverData.deviceIP[lifxDriverData.device], lifxDriverData.devicePort[lifxDriverData.device]);
      UDP.write(((char*)&header), sizeof(header));
      UDP.endPacket();
      resendTime = millis();
    }
    
    yield();
    
    if (UDP.parsePacket() != 0)
    {
      UDP.read(buf, LIFX_MALLOCBUF_SIZE - 1);
      UDP.flush();
#ifdef DEBUG
      Serial.println("------------lifx_getParam reply--------------------");
      Serial.println(UDP.remoteIP());
      int i;
      for (i = 0; i < 64; i++)
      {
        if (i % 16 == 0) Serial.println();
        Serial.print(buf[i], HEX);
      }
      Serial.println();
#endif

      p_header = (lx_protocol_header_t*)buf;
      p_payload = &buf[sizeof(lx_protocol_header_t)];
      ok = 1;
      
      if (p_header->size != 88) ok = 0;
      if (p_header->protocol != 1024) ok = 0;
      if (p_header->addressable != 1) ok = 0;
      if (p_header->type != 107) ok = 0;

#ifdef DEBUG
      Serial.print("size: ");
      Serial.println(p_header->size);
      Serial.print("protocol: ");
      Serial.println(p_header->protocol);
      Serial.print("addressable: ");
      Serial.println(p_header->addressable);
      Serial.print("type: ");
      Serial.println(p_header->type);
#endif

      if (ok == 1)
      {
        deviceParams->hue = *((uint8_t*)(p_payload + 1));
        deviceParams->hue <<= 8;
        deviceParams->hue |= *((uint8_t*)(p_payload));

        deviceParams->sat = *((uint8_t*)(p_payload + 3));
        deviceParams->sat <<= 8;
        deviceParams->sat |= *((uint8_t*)(p_payload + 2));

        deviceParams->bri = *((uint8_t*)(p_payload + 5));
        deviceParams->bri <<= 8;
        deviceParams->bri |= *((uint8_t*)(p_payload + 4));

        deviceParams->ct = *((uint8_t*)(p_payload + 7));
        deviceParams->ct <<= 8;
        deviceParams->ct |= *((uint8_t*)(p_payload + 6));

        power = *((uint8_t*)(p_payload + 11));
        power <<= 8;
        power |= *((uint8_t*)(p_payload + 10));
                
        if (power != 0)
        {
          deviceParams->on = 1;
        }
        
#ifdef DEBUG
        Serial.println(UDP.remoteIP());
        Serial.println(deviceParams->on);
        Serial.println(deviceParams->bri);
        Serial.println(deviceParams->ct);
        Serial.println(deviceParams->hue);
        Serial.println(deviceParams->sat);
#endif

        break;
      }
    }
  }
  free(buf);
  UDP.stop();
}

void lifx_setPower(uint16_t power)
{
  WiFiUDP UDP;
  unsigned long startTime, resendTime;
  int ok;
  lx_protocol_header_t header;
  lx_protocol_header_t* p_header;
  char *buf;
  char* p_payload;

  UDP.begin(LIFX_UDP_PORT);

  buf = (char*)malloc(LIFX_MALLOCBUF_SIZE);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < LIFX_TIMEOUT)
  {
    if (millis() - resendTime > LIFX_RESEND_TIME)
    {
      lifx_setHeader(&header, 2, 1, lifxDriverData.deviceMac[lifxDriverData.device], 21);
      UDP.beginPacket(lifxDriverData.deviceIP[lifxDriverData.device], lifxDriverData.devicePort[lifxDriverData.device]);
      UDP.write(((char*)&header), sizeof(header));
      UDP.write((uint8_t)(power & 0xFF));
      UDP.write((uint8_t)((power >> 8) & 0xFF));
      UDP.endPacket();
      resendTime = millis();
    }
    
    yield();
    
    if (UDP.parsePacket() != 0)
    {
      UDP.read(buf, LIFX_MALLOCBUF_SIZE - 1);
      UDP.flush();
#ifdef DEBUG
      Serial.println("------------lifx_setPower reply--------------------");
      Serial.println(UDP.remoteIP());
      int i;
      for (i = 0; i < 64; i++)
      {
        if (i % 16 == 0) Serial.println();
        Serial.print(buf[i], HEX);
      }
      Serial.println();
#endif

      p_header = (lx_protocol_header_t*)buf;
      p_payload = &buf[sizeof(lx_protocol_header_t)];
      ok = 1;
      
      if (p_header->size != 38) ok = 0;
      if (p_header->protocol != 1024) ok = 0;
      if (p_header->addressable != 1) ok = 0;
      if (p_header->type != 22) ok = 0;

#ifdef DEBUG
      Serial.print("size: ");
      Serial.println(p_header->size);
      Serial.print("protocol: ");
      Serial.println(p_header->protocol);
      Serial.print("addressable: ");
      Serial.println(p_header->addressable);
      Serial.print("type: ");
      Serial.println(p_header->type);
#endif

      if (ok == 1)
      {
#ifdef DEBUG
      Serial.print("ok, got StatePower packet");
#endif
        break;
      }
    }
  }
  free(buf);
  UDP.stop();
}



void lifx_setColor(deviceParams_t *deviceParams)
{
  WiFiUDP UDP;
  unsigned long startTime, resendTime;
  int ok;
  lx_protocol_header_t header;
  lx_protocol_header_t* p_header;
  char *buf;
  char* p_payload;
  uint32_t duration;

  UDP.begin(LIFX_UDP_PORT);

  buf = (char*)malloc(LIFX_MALLOCBUF_SIZE);

  while (UDP.parsePacket() != 0)
  {
    UDP.flush();
  }

  startTime = millis();
  resendTime = 0;
  while (millis() - startTime < LIFX_TIMEOUT)
  {
    if (millis() - resendTime > LIFX_RESEND_TIME)
    {
      lifx_setHeader(&header, 13, 1, lifxDriverData.deviceMac[lifxDriverData.device], 102);
      UDP.beginPacket(lifxDriverData.deviceIP[lifxDriverData.device], lifxDriverData.devicePort[lifxDriverData.device]);
      UDP.write(((char*)&header), sizeof(header));
      UDP.write(0x00);  // Reserved
      UDP.write((uint8_t)(deviceParams->hue & 0xFF));
      UDP.write((uint8_t)((deviceParams->hue >> 8) & 0xFF));
      UDP.write((uint8_t)(deviceParams->sat & 0xFF));
      UDP.write((uint8_t)((deviceParams->sat >> 8) & 0xFF));
      UDP.write((uint8_t)(deviceParams->bri & 0xFF));
      UDP.write((uint8_t)((deviceParams->bri >> 8) & 0xFF));
      UDP.write((uint8_t)(deviceParams->ct & 0xFF));
      UDP.write((uint8_t)((deviceParams->ct >> 8) & 0xFF));
      duration = 500;   // 500ms transition time
      UDP.write((uint8_t)(duration & 0xFF));
      UDP.write((uint8_t)((duration >> 8) & 0xFF));
      UDP.write((uint8_t)((duration >> 16) & 0xFF));
      UDP.write((uint8_t)((duration >> 24) & 0xFF));
      UDP.endPacket();
      resendTime = millis();
    }
    
    yield();
    
    if (UDP.parsePacket() != 0)
    {
      UDP.read(buf, LIFX_MALLOCBUF_SIZE - 1);
      UDP.flush();
#ifdef DEBUG
      Serial.println("------------lifx_setColor reply--------------------");
      Serial.println(UDP.remoteIP());
      int i;
      for (i = 0; i < 64; i++)
      {
        if (i % 16 == 0) Serial.println();
        Serial.print(buf[i], HEX);
      }
      Serial.println();
#endif

      p_header = (lx_protocol_header_t*)buf;
      p_payload = &buf[sizeof(lx_protocol_header_t)];
      ok = 1;
      
      if (p_header->size != 88) ok = 0;
      if (p_header->protocol != 1024) ok = 0;
      if (p_header->addressable != 1) ok = 0;
      if (p_header->type != 107) ok = 0;

#ifdef DEBUG
      Serial.print("size: ");
      Serial.println(p_header->size);
      Serial.print("protocol: ");
      Serial.println(p_header->protocol);
      Serial.print("addressable: ");
      Serial.println(p_header->addressable);
      Serial.print("type: ");
      Serial.println(p_header->type);
#endif

      if (ok == 1)
      {
#ifdef DEBUG
      Serial.print("ok, got LightState packet");
#endif
        break;
      }
    }
  }
  free(buf);
  UDP.stop();
}


void lifx_setParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) lifx_setPower(0);
      else lifx_setPower(65535);
      break;
    case PARAM_BRI:
    case PARAM_CT:
    case PARAM_HUE:
    case PARAM_SAT:
      lifx_setColor(deviceParams);
      break;
    default:
      break;
  }
}

int lifx_hasParam(int param)
{
  switch (param)
  {
    case PARAM_ON:
      break;
    case PARAM_BRI:
      if ((lifxDriverData.deviceFlags[lifxDriverData.device] & LIFX_FLAG_BRI_ENABLED) == 0) return -1;
      break;
    case PARAM_CT:
      if ((lifxDriverData.deviceFlags[lifxDriverData.device] & LIFX_FLAG_CT_ENABLED) == 0) return -1;
      break;
    case PARAM_HUE:
      if ((lifxDriverData.deviceFlags[lifxDriverData.device] & LIFX_FLAG_COLOR_ENABLED) == 0) return -1;
      break;
    case PARAM_SAT:
      if ((lifxDriverData.deviceFlags[lifxDriverData.device] & LIFX_FLAG_COLOR_ENABLED) == 0) return -1;
      break;
    default:
      return -1;
      break;
  }
  return 1;
}

void lifx_incParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 0) deviceParams->on = 1;
      break;
    case PARAM_BRI:
      deviceParams->bri += 8192;
      if (deviceParams->bri > 65535) deviceParams->bri = 65535;
      break;
    case PARAM_CT:
      deviceParams->ct -= 500;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue + 4096, 65536);
      break;
    case PARAM_SAT:
      deviceParams->sat += 8192;
      if (deviceParams->sat > 65535) deviceParams->sat = 65535;
      break;
  }
}

void lifx_decParam(int param, deviceParams_t *deviceParams)
{
  switch (param)
  {
    case PARAM_ON:
      if (deviceParams->on == 1) deviceParams->on = 0;
      break;
    case PARAM_BRI:
      deviceParams->bri -= 8192;
      if (deviceParams->bri < 1024) deviceParams->bri = 1024;
      break;
    case PARAM_CT:
      deviceParams->ct += 500;
      break;
    case PARAM_HUE:
      deviceParams->hue = mod(deviceParams->hue - 4096, 65536);
      break;
    case PARAM_SAT:
      deviceParams->sat -= 8192;
      if (deviceParams->sat < 0) deviceParams->sat = 0;
      break;
  }
}

int lifx_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights)
{
  if (lights[lightsIndex].ok != -1)
  {
    if (strncmp(lights[lightsIndex].name, lifxDriverData.deviceIP[lightNumber].toString().c_str(), DISCOVERPARAMS_NAME_SIZE - 1) == 0)
    {
      return 1;
    }
  }
  return 0;
}


lifxDriverData_t* lifx_getDriverDataStruct()
{
  return &lifxDriverData;
}
