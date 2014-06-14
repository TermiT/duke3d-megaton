//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>

#ifdef __cplusplus

template<typename t>
t clamp(t v, t min, t max);

template<typename t1, typename t2>
t2 lerp(t1 a, t1 b, t2 k);

struct rgb {
    double r;       // percent
    double g;       // percent
    double b;       // percent
	rgb():r(0),g(0),b(0){}
	rgb(double r, double g, double b):r(r),g(g),b(b){}
	rgb(int r, int g, int b):r(r/255.0),g(g/255.0),b(b/255.0){}
};

rgb rgb_interp(rgb a, rgb b, float k);
rgb rgb_lerp(rgb a, rgb b, float k);

extern "C" {
#endif

int clampi(int v, int min, int max);
unsigned int clampui(unsigned int v, unsigned int min, unsigned int max);
float clampf(float v, float min, float max);
double clampd(double v, double min, double max);
const char* va(const char *format, ...);
void crc32file(FILE *f, unsigned long *result);
char *str_replace (const char *string, const char *substr, const char *replacement);
long get_modified_time(const char * path);

#ifdef __cplusplus
}
#endif

#endif /* HELPERS_H */
