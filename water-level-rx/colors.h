#ifndef _COLORS_H
#define _COLORS_H

struct RgbColor
{
  unsigned char r;
  unsigned char g;
  unsigned char b;
};

struct HsvColor
{
  unsigned char h;
  unsigned char s;
  unsigned char v;
};

RgbColor hsvToRgb(HsvColor hsv);

#endif