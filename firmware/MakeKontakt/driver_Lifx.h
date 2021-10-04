#ifndef DRIVER_LIFX_H
#define DRIVER_LIFX_H

#include "kungFuHue.h"

//#define DEBUG

#define LIFX_UDP_PORT                       40326

#define LIFX_MALLOCBUF_SIZE                 1024

#define LIFX_TIMEOUT                        300
#define LIFX_RESEND_TIME                    50

#define LIFX_FLAG_BRI_ENABLED               1
#define LIFX_FLAG_CT_ENABLED                2
#define LIFX_FLAG_COLOR_ENABLED             4

/*
bri: 1024..65535
ct: kelvin
hue: 0..65535
sat: 0..65535
*/


typedef struct LifxDriverData
{
  uint8_t device;                   // Needs to be first byte in the structure for getting default device from EEPROM
  IPAddress deviceIP[NDEVICES];
  uint16_t devicePort[NDEVICES];
  uint8_t deviceMac[NDEVICES][6];
  uint8_t deviceFlags[NDEVICES];
} lifxDriverData_t;


#pragma pack(push, 1)
typedef struct {
  /* frame */
  uint16_t size;
  uint16_t protocol:12;
  uint8_t  addressable:1;
  uint8_t  tagged:1;
  uint8_t  origin:2;
  uint32_t source;
  /* frame address */
  uint8_t  target[8];
  uint8_t  reserved[6];
  uint8_t  res_required:1;
  uint8_t  ack_required:1;
  uint8_t  :6;
  uint8_t  sequence;
  /* protocol header */
  uint64_t :64;
  uint16_t type;
  uint16_t :16;
  /* variable length payload follows */
} lx_protocol_header_t;
#pragma pack(pop)


// Generic interface
uint8_t lifx_getCurrentDevice();
uint8_t lifx_getDeviceEnabled(uint8_t dev);
void lifx_setCurrentDevice(uint8_t dev);

void lifx_getParam(deviceParams_t *deviceParams);
void lifx_setParam(int param, deviceParams_t *deviceParams);
int lifx_hasParam(int param);

void lifx_incParam(int param, deviceParams_t *deviceParams);
void lifx_decParam(int param, deviceParams_t *deviceParams);


int lifx_discoverLights(discoverParams_t *lights);
int lifx_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights);

void lifx_configSlot(uint8_t dev, int light, discoverParams_t *lights);

lifxDriverData_t* lifx_getDriverDataStruct();


#endif	/*DRIVER_LIFX_H*/
