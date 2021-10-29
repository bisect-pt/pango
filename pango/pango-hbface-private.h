#pragma once

#include "pango-hbface.h"
#include "pango-fontmap.h"
#include <hb.h>

struct _PangoHbFace
{
  PangoFontFace parent_instance;

  hb_face_t *face;
  char *name;
  PangoFontFamily *family;
  PangoFontDescription *description;
  PangoMatrix matrix;
  double x_scale, y_scale;
  PangoLanguage **languages;
  gboolean synthetic;

  PangoFontMap *map;
};

void            pango_hb_face_set_font_map      (PangoHbFace          *self,
                                                 PangoFontMap         *map);
void            pango_hb_face_set_family        (PangoHbFace          *self,
                                                 PangoFontFamily      *family);

gboolean        pango_hb_face_is_monospace      (PangoHbFace          *self);
gboolean        pango_hb_face_is_variable       (PangoHbFace          *self);
PangoLanguage **pango_hb_face_get_languages     (PangoHbFace          *self);
void            pango_hb_face_set_languages     (PangoHbFace          *self,
                                                 const PangoLanguage **languages,
                                                 gsize                 n_languages);
