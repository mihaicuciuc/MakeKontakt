#ifndef DRIVER_YEELIGHT_H
#define DRIVER_YEELIGHT_H

#include "kungFuHue.h"

//#define DEBUG

#define YEELIGHT_UDP_PORT                   41526

#define YEELIGHT_MALLOCBUF_SIZE             1024

#define YEELIGHT_TIMEOUT                    300


#define YEELIGHT_FLAG_BRI_ENABLED           1
#define YEELIGHT_FLAG_CT_ENABLED            2
#define YEELIGHT_FLAG_COLOR_ENABLED         4


/*
bri: 1..100
ct: kelvin
hue: 0..359
sat: 0..100
*/


typedef struct YeelightDriverData
{
  uint8_t device;                   // Needs to be first byte in the structure for getting default device from EEPROM
  IPAddress deviceIP[NDEVICES];
  uint8_t deviceFlags[NDEVICES];
} yeelightDriverData_t;


// Generic interface
uint8_t yeelight_getCurrentDevice();
uint8_t yeelight_getDeviceEnabled(uint8_t dev);
void yeelight_setCurrentDevice(uint8_t dev);

void yeelight_getParam(deviceParams_t *deviceParams);
void yeelight_setParam(int param, deviceParams_t *deviceParams);
int yeelight_hasParam(int param);

void yeelight_incParam(int param, deviceParams_t *deviceParams);
void yeelight_decParam(int param, deviceParams_t *deviceParams);


int yeelight_discoverLights(discoverParams_t *lights);
int yeelight_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights);

void yeelight_configSlot(uint8_t dev, int light, discoverParams_t *lights);

yeelightDriverData_t* yeelight_getDriverDataStruct();


#endif	/*DRIVER_YEELIGHT_H*/
