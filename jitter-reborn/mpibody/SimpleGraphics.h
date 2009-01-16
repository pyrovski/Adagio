#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef SIMPLEGRAPHICS_H
#define SIMPLEGRAPHICS_H

#define TRUE 1
#define FALSE 0

typedef unsigned char bool;

typedef unsigned char Color1b;

typedef struct {
    Color1b b;
	Color1b g;
	Color1b r;
} Color3b;

typedef struct {
    float r;
    float g;
    float b;
} Color3f;

typedef struct {
    Color1b r;
    Color1b g;
    Color1b b;
    Color1b a;
} Color4b;

Color1b* createImage8bit(int sx, int sy);
Color3b* createImage24bit(int sx, int sy);
Color4b* createImage32bit(int sx, int sy);

bool writeTGA(const char* filename, int sx, int sy, Color3b* image);

bool writeBMP1b(const char* filename, int sx, int sy, Color1b* image);
bool writeBMP3b(const char* filename, int sx, int sy, Color3b* image);
bool writeBMP4b(const char* filename, int sx, int sy, Color4b* image);

Color3b HSBtoRGB(float hue, float saturation, float brightness);

#endif
