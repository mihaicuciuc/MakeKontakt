#ifndef COLOR_TRANSFORM_H
#define COLOR_TRANSFORM_H


// RGB values in 0..255 (float)
// Hue in 0..360 (float)
// Sat in 0..100 (float)
// Color temperature in K

void ct_to_rgb(int ct, float *R, float *G, float *B);
void rgb_to_ct(float r, float g, float b, int *ct);

void rgb_to_hs(float r, float g, float b, float *h, float *s);
void hs_to_rgb(float H, float S, float *R, float *G, float *B);


#endif  /*COLOR_TRANSFORM_H*/
