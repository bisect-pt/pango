/* Pango
 *
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <math.h>

#include <gio/gio.h>

#include "pango-simple-fontmap.h"
#include "pango-simple-family-private.h"
#include "pango-hbface-private.h"
#include "pango-hbfont-private.h"
#include "pango-context.h"
#include "pango-impl-utils.h"

#include <hb-ot.h>

/* {{{ PangoFontMap implementation */

struct _PangoSimpleFontMap
{
  PangoFontMap parent_instance;

  GPtrArray *families;

  double dpi;
};

struct _PangoSimpleFontMapClass
{
  PangoFontMapClass parent_class;
};

G_DEFINE_TYPE (PangoSimpleFontMap, pango_simple_font_map, PANGO_TYPE_FONT_MAP)

static void
pango_simple_font_map_init (PangoSimpleFontMap *self)
{
  self->families = g_ptr_array_new_with_free_func (g_object_unref);
  self->dpi = 96.;
}

static void
pango_simple_font_map_finalize (GObject *object)
{
  PangoSimpleFontMap *self = PANGO_SIMPLE_FONT_MAP (object);

  g_ptr_array_unref (self->families);

  G_OBJECT_CLASS (pango_simple_font_map_parent_class)->finalize (object);
}

static PangoSimpleFamily *
find_family (PangoSimpleFontMap *self,
             const char         *family_name)
{
  for (int i = 0; i < self->families->len; i++)
    {
      PangoSimpleFamily *family = g_ptr_array_index (self->families, i);

      if (g_ascii_strcasecmp (family->name, family_name) == 0)
        return family;
    }

  return NULL;
}

static PangoHbFace *
find_face (PangoSimpleFamily    *family,
           PangoFontDescription *desc)
{
  PangoHbFace *face = NULL;
  PangoFontDescription *best = NULL;

  for (int i = 0; i < family->faces->len; i++)
    {
      PangoHbFace *face2 = g_ptr_array_index (family->faces, i);

      if (pango_font_description_better_match (desc, best, face2->description))
        {
          face = face2;
          best = face2->description;
        }
    }

  return face;
}

/* Load a font from the first matching family */
static PangoFont *
pango_simple_font_map_load_font (PangoFontMap               *map,
                                 PangoContext               *context,
                                 const PangoFontDescription *desc)
{
  PangoSimpleFontMap *self = PANGO_SIMPLE_FONT_MAP (map);
  int size;
  const char *variations;
  PangoGravity gravity;
  const PangoMatrix *matrix;
  const char *family_name;
  char **families;
  PangoFontDescription *copy;
  PangoSimpleFamily *family;
  PangoHbFace *face;

  if (self->families->len == 0)
    return NULL;

  if (pango_font_description_get_size_is_absolute (desc))
    size = pango_font_description_get_size (desc);
  else
    size = pango_font_description_get_size (desc) * self->dpi / 72.;

  variations = pango_font_description_get_variations (desc);

  gravity = pango_font_description_get_gravity (desc);
  matrix = pango_context_get_matrix (context);

  family_name = pango_font_description_get_family (desc);
  families = g_strsplit (family_name, ",", 0);

  /* Unset gravity since PangoHbFace has no gravity */
  copy = pango_font_description_copy_static (desc);
  pango_font_description_unset_fields (copy, PANGO_FONT_MASK_GRAVITY);

  family = NULL;
  for (int i = 0; families[i]; i++)
    {
      family = find_family (self, families[i]);
      if (family)
        break;
    }

  if (!family)
    family = g_ptr_array_index (self->families, 0);

  face = find_face (family, copy);

  if (!face)
    {
      char *s = pango_font_description_to_string (desc);
      g_warning ("No match for pattern '%s', falling back to default face\n", s);
      g_free (s);

      face = g_ptr_array_index (family->faces, 0);
    }

  g_strfreev (families);

  pango_font_description_free (copy);

  return PANGO_FONT (pango_hb_font_new (face, size, variations, gravity, matrix));
}

/* Add one font for each family we find */
static PangoFontset *
pango_simple_font_map_load_fontset (PangoFontMap               *map,
                                    PangoContext               *context,
                                    const PangoFontDescription *description,
                                    PangoLanguage              *language)
{
  PangoSimpleFontMap *self = PANGO_SIMPLE_FONT_MAP (map);
  PangoFontsetSimple *fontset;
  int size;
  const char *variations;
  PangoGravity gravity;
  const PangoMatrix *matrix;
  const char *family_name;
  char **families;
  PangoFontDescription *copy;

  fontset = pango_fontset_simple_new (language);

  if (self->families->len == 0)
    return PANGO_FONTSET (fontset);

  if (pango_font_description_get_size_is_absolute (description))
    size = pango_font_description_get_size (description);
  else
    size = pango_font_description_get_size (description) * self->dpi / 72.;

  variations = pango_font_description_get_variations (description);

  gravity = pango_font_description_get_gravity (description);
  matrix = pango_context_get_matrix (context);

  family_name = pango_font_description_get_family (description);
  families = g_strsplit (family_name, ",", 0);

  /* Unset gravity since PangoHbFace has no gravity */
  copy = pango_font_description_copy_static (description);
  pango_font_description_unset_fields (copy, PANGO_FONT_MASK_GRAVITY);

  for (int i = 0; families[i]; i++)
    {
      PangoSimpleFamily *family;
      PangoHbFace *face;
      PangoHbFont *font;

      family = find_family (self, families[i]);
      if (!family)
        continue;

      face = find_face (family, copy);
      if (!face)
        continue;

      font = pango_hb_font_new (face, size, variations, gravity, matrix);

      pango_fontset_simple_append (fontset, PANGO_FONT (font));
    }

  g_strfreev (families);

  pango_font_description_free (copy);

  return PANGO_FONTSET (fontset);
}

static void
pango_simple_font_map_list_families (PangoFontMap      *map,
                                     PangoFontFamily ***families,
                                     int               *n_families)
{
  PangoSimpleFontMap *self = PANGO_SIMPLE_FONT_MAP (map);

  if (n_families)
    *n_families = self->families->len;

  if (families)
    *families = g_memdup2 (self->families->pdata, self->families->len * sizeof (PangoFontFamily *));
}

static PangoFontFamily *
pango_simple_font_map_get_family (PangoFontMap *map,
                                  const char   *name)
{
  PangoSimpleFontMap *self = PANGO_SIMPLE_FONT_MAP (map);

  return PANGO_FONT_FAMILY (find_family (self, name));
}

static void
pango_simple_font_map_class_init (PangoSimpleFontMapClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontMapClass *map_class = PANGO_FONT_MAP_CLASS (class);

  object_class->finalize = pango_simple_font_map_finalize;

  map_class->load_font = pango_simple_font_map_load_font;
  map_class->load_fontset = pango_simple_font_map_load_fontset;
  map_class->list_families = pango_simple_font_map_list_families;
  map_class->get_family = pango_simple_font_map_get_family;
}

/* }}} */
/* {{{ Public API */

/**
 * pango_simple_font_map_new:
 *
 * Creates a new `PangoSimpleFontmMap`.
 *
 * Returns: A newly created `PangoSimpleFontMap
 */
PangoSimpleFontMap *
pango_simple_font_map_new (void)
{
  return g_object_new (PANGO_TYPE_SIMPLE_FONT_MAP, NULL);
}

/**
 * pango_simple_font_map_add_face:
 * @self: a `PangoSimpleFontMap`
 * @face: a `PangoHbFace`
 *
 * Adds @face to the `PangoSimpleFontMap`.
 *
 * This is most useful for creating transformed
 * faces or aliases. See [method@Pango.HbFace.new_transformed]
 * or [method@Pango.HbFace.new_alias].
 */
void
pango_simple_font_map_add_face (PangoSimpleFontMap *self,
                                PangoHbFace        *face)
{
  PangoFontMap *map = PANGO_FONT_MAP (self);
  const char *family_name;
  PangoSimpleFamily *family;

  family_name = pango_font_description_get_family (face->description);
  family = PANGO_SIMPLE_FAMILY (pango_font_map_get_family (map, family_name));
  if (!family)
    {
      family = pango_simple_family_new (family_name,
                                        pango_hb_face_is_monospace (face),
                                        pango_hb_face_is_variable (face));
      g_ptr_array_add (self->families, family);
    }

  pango_hb_face_set_font_map (face, map);
  pango_simple_family_add_face (family, PANGO_FONT_FACE (face));
  pango_hb_face_set_family (face, PANGO_FONT_FAMILY (family));
}

/**
 * pango_simple_font_map_add_file:
 * @file: font filename
 * @index: face index
 *
 * Creates a new `PangoHbFace` and adds it.
 */
void
pango_simple_font_map_add_file (PangoSimpleFontMap *self,
                                const char         *file,
                                unsigned int        index)
{
  PangoHbFace *face;

  face = pango_hb_face_new_from_file (file, index);
  pango_simple_font_map_add_face (self, face);
  g_object_unref (face);
}

/**
 * pango_simple_font_map_set_resolution:
 * @self: a `PangoSimpleFontMap`
 * @dpi: the new resolution, in "dots per inch"
 *
 * Sets the resolution for the fontmap.
 *
 * This is a scale factor between points specified in a
 * `PangoFontDescription` and Cairo units. The default value
 * is 96, meaning that a 10 point font will be 13 units high.
 * (10 * 96. / 72. = 13.3).
 */
void
pango_simple_font_map_set_resolution (PangoSimpleFontMap *self,
                                      double              dpi)
{
  self->dpi = dpi;

  pango_font_map_changed (PANGO_FONT_MAP (self));
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
