#ifndef DRIVER_WIZ_H
#define DRIVER_WIZ_H

#include "kungFuHue.h"

//#define DEBUG

#define WIZ_UDP_PORT                    43576

#define WIZ_MALLOCBUF_SIZE              1024

#define WIZ_TIMEOUT                     300
#define WIZ_RESEND_TIME                 50

#define WIZ_FLAG_BRI_ENABLED            1
#define WIZ_FLAG_CT_ENABLED             2
#define WIZ_FLAG_COLOR_ENABLED          4

/*
bri: 1..100
ct: kelvin
hue: 0..65535
sat: 0..65535
*/


typedef struct WizDriverData
{
  uint8_t device;                   // Needs to be first byte in the structure for getting default device from EEPROM
  IPAddress deviceIP[NDEVICES];
  uint8_t deviceFlags[NDEVICES];
} wizDriverData_t;


// Generic interface
uint8_t wiz_getCurrentDevice();
uint8_t wiz_getDeviceEnabled(uint8_t dev);
void wiz_setCurrentDevice(uint8_t dev);

void wiz_getParam(deviceParams_t *deviceParams);
void wiz_setParam(int param, deviceParams_t *deviceParams);
int wiz_hasParam(int param);

void wiz_incParam(int param, deviceParams_t *deviceParams);
void wiz_decParam(int param, deviceParams_t *deviceParams);


int wiz_discoverLights(discoverParams_t *lights);
int wiz_findLight(uint8_t lightNumber, uint8_t lightsIndex, discoverParams_t *lights);

void wiz_configSlot(uint8_t dev, int light, discoverParams_t *lights);

wizDriverData_t* wiz_getDriverDataStruct();


#endif	/*DRIVER_WIZ_H*/
