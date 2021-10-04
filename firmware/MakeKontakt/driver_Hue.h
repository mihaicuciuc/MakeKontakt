#ifndef DRIVER_HUE_H
#define DRIVER_HUE_H

#include "kungFuHue.h"

//#define DEBUG

#define HUE_UDP_PORT                      42267

#define HUE_MALLOCBUF_SIZE                1024
#define HUE_MALLOCDISCOVERBUF_SIZE        2048

#define HUE_DISCOVERY_IDLE                0
#define HUE_DISCOVERY_REQUEST             1
#define HUE_DISCOVERY_DISCOVERED          2

#define HUE_NEWUSER_IDLE                  0
#define HUE_NEWUSER_REQUEST               1
#define HUE_NEWUSER_EXECUTED              2



typedef struct HueDriverData
{
  uint8_t device;                   // Needs to be first byte in the structure for getting default device from EEPROM
  char hueIP[32];
  char hueUser[64];
  uint8_t deviceType[NDEVICES];     // 0: disabled; 1: light; 2: group
  uint8_t deviceIndex[NDEVICES];
} hueDriverData_t;


// Generic interface
uint8_t hue_getCurrentDevice();
uint8_t hue_getDeviceEnabled(uint8_t dev);
void hue_setCurrentDevice(uint8_t dev);

void hue_getParam(deviceParams_t *deviceParams);
void hue_setParam(int param, deviceParams_t *deviceParams);
int hue_hasParam(int param);

void hue_incParam(int param, deviceParams_t *deviceParams);
void hue_decParam(int param, deviceParams_t *deviceParams);

int hue_discoverLights(discoverParams_t *lights);
int hue_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights);

void hue_configSlot(uint8_t dev, int light, discoverParams_t *lights);

hueDriverData_t* hue_getDriverDataStruct();


// Specific for Hue
int hue_discoverBridge(char* url);
int hue_getNewHueUser(char *userName, uint8_t *mac);


#endif	/*DRIVER_HUE_H*/
