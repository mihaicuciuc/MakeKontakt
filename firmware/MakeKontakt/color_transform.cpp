#include <math.h>
#include "color_transform.h"


float max(float a, float b, float c)
{
  return ((a > b)? (a > c ? a : c) : (b > c ? b : c));
}

float min(float a, float b, float c)
{
  return ((a < b)? (a < c ? a : c) : (b < c ? b : c));
}

// Color temperature to RGB, algorithm copied from
// https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
// ct input value is in K
//inverse megakelvin
void ct_to_rgb(int ct, float *R, float *G, float *B)
{
  float r, g, b;
  float K;
  K = ct/100.0;
  if (K <= 66)
  {
    r = 255.0;
    g = K;
    g = 99.4708025861 * log(g) - 161.1195681661;
    if(K <= 19)
    {
      b = 0;
    }
    else
    {
      b = K - 10;
      b = 138.5177312231 * log(b) - 305.0447927307;
    }
  }
  else
  {
    r = K - 60;
    r = 329.698727446 * pow(r, -0.1332047592);
    g = K - 60;
    g = 288.1221695283 * pow(g, -0.0755148492 );
    b = 255.0;
  }
  if (r < 0) r = 0;
  if (g < 0) g = 0;
  if (b < 0) b = 0;
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  *R = r;
  *G = g;
  *B = b;
}


// RGB to HS(V), algorithm copied from
// https://www.tutorialspoint.com/c-program-to-change-rgb-color-model-to-hsv-color-model
void rgb_to_hs(float r, float g, float b, float *h, float *s)
{
  // R, G, B values are divided by 255
  // to change the range from 0..255 to 0..1:
  //float h, s, v;
  r /= 255.0;
  g /= 255.0;
  b /= 255.0;
  float cmax = max(r, g, b); // maximum of r, g, b
  float cmin = min(r, g, b); // minimum of r, g, b
  float diff = cmax-cmin; // diff of cmax and cmin.
  if (cmax == cmin)
    *h = 0;
  else if (cmax == r)
    *h = fmod((60 * ((g - b) / diff) + 360), 360.0);
  else if (cmax == g)
    *h = fmod((60 * ((b - r) / diff) + 120), 360.0);
  else if (cmax == b)
    *h = fmod((60 * ((r - g) / diff) + 240), 360.0);
  // if cmax equal zero
    if (cmax == 0)
       *s = 0;
    else
       *s = (diff / cmax) * 100;
  //v = cmax * 100;
#ifdef DEBUG
  Serial.printf("h s=(%f, %f)\r\n", h, s);
#endif
}

// HSV to RGB, algorithm copied from
// https://www.codespeedy.com/hsv-to-rgb-in-cpp/
void hs_to_rgb(float H, float S, float *R, float *G, float *B)
{
  float s = S/100;
  float v = 1;
  float C = s*v;
  float X = C*(1-fabs(fmod(H/60.0, 2)-1));
  float m = v-C;
  float r,g,b;
  if(H >= 0 && H < 60){
      r = C,g = X,b = 0;
  }
  else if(H >= 60 && H < 120){
      r = X,g = C,b = 0;
  }
  else if(H >= 120 && H < 180){
      r = 0,g = C,b = X;
  }
  else if(H >= 180 && H < 240){
      r = 0,g = X,b = C;
  }
  else if(H >= 240 && H < 300){
      r = X,g = 0,b = C;
  }
  else{
      r = C,g = 0,b = X;
  }
  *R = (r+m)*255.0;
  *G = (g+m)*255.0;
  *B = (b+m)*255.0;
  if (*R < 0) *R = 0;
  if (*G < 0) *G = 0;
  if (*B < 0) *B = 0;
  if (*R > 255) *R = 255;
  if (*G > 255) *G = 255;
  if (*B > 255) *B = 255;
}


// Do RGB -> CIE xy, from https://stackoverflow.com/questions/54663997/convert-rgb-color-to-xy
// Then CIE xy to CCT, from https://www.waveformlighting.com/tech/calculate-color-temperature-cct-from-cie-1931-xy-coordinates
void rgb_to_ct(float r, float g, float b, int *ct)
{
  float X, Y, Z, x, y;
  float n;

  r /= 255.0;
  g /= 255.0;
  b /= 255.0;

  X = r * 0.664511 + g * 0.154324 + b * 0.162028;
  Y = r * 0.283881 + g * 0.668433 + b * 0.047685;
  Z = r * 0.000088 + g * 0.072310 + b * 0.986039;

  if (X + Y + Z != 0)
  {
    x = X / (X + Y + Z);
    y = Y / (X + Y + Z);
    n = (x-0.3320)/(0.1858-y);
    *ct = (int)(437*n*n*n + 3601*n*n + 6861*n + 5517);
  }
  else
  {
    // Some safe value, whatever
    *ct = 3000;
  }
  // Some sane limits for our use case
  if (*ct < 1000) *ct = 1000;
  if (*ct > 10000) *ct = 10000;
}
