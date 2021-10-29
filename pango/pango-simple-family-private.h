#pragma once

#include "pango-font.h"

#define PANGO_TYPE_SIMPLE_FAMILY (pango_simple_family_get_type ())

G_DECLARE_FINAL_TYPE (PangoSimpleFamily, pango_simple_family, PANGO, SIMPLE_FAMILY, PangoFontFamily)

struct _PangoSimpleFamily
{
  PangoFontFamily parent_instance;

  char *name;
  GPtrArray *faces;
  gboolean monospace;
  gboolean variable;
};

PangoSimpleFamily *     pango_simple_family_new         (const char             *name,
                                                         gboolean                monospace,
                                                         gboolean                variable);

void                    pango_simple_family_add_face    (PangoSimpleFamily      *self,
                                                         PangoFontFace          *face);


