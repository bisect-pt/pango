#pragma once

#include "pango-hbfont.h"
#include "pango-hbface.h"
#include <hb.h>

typedef struct _HexBoxInfo HexBoxInfo;
struct _HexBoxInfo
{
  PangoFont *font;
  int rows;
  double digit_width;
  double digit_height;
  double pad_x;
  double pad_y;
  double line_width;
  double box_descent;
  double box_height;
};

struct _PangoHbFont
{
  PangoFont parent_instance;

  PangoHbFace *face;
  int size; /* pixel size, scaled by PANGO_SCALE */
  char *variations;
  PangoGravity gravity;
  PangoMatrix matrix;

  HexBoxInfo *hex_box_info;
};
