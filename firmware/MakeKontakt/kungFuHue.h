#ifndef KUNGFUHUE_H
#define KUNGFUHUE_H



#define DRIVER_HUE        0
#define DRIVER_WIZ        1
#define DRIVER_YEELIGHT   2
#define DRIVER_LIFX       3

#define NDRIVERS          4


#define NDEVICES          5

#define DISCOVERPARAMS_NAME_SIZE      36

#define PARAM_ON    0
#define PARAM_BRI   1
#define PARAM_CT    2
#define PARAM_HUE   3
#define PARAM_SAT   4

#define NDISCOVERLIGHTS    64

typedef struct DeviceParams
{
  int on;
  int bri;
  int ct;
  int hue;
  int sat;
} deviceParams_t;


typedef struct DiscoverParams
{
  int ok;
  uint8_t index;
  uint8_t group;
  uint8_t deviceFlags;
  char name[DISCOVERPARAMS_NAME_SIZE];
} discoverParams_t;


int mod(int a, int b);


#endif	/*KUNGFUHUE_H*/
