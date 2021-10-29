/* Pango
 *
 * Copyright (C) 2021 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "pango-hbface-private.h"

#include <hb-ot.h>

/* {{{ PangoFontFace implementation */

struct _PangoHbFaceClass
{
  PangoFontFaceClass parent_class;
};

G_DEFINE_TYPE (PangoHbFace, pango_hb_face, PANGO_TYPE_FONT_FACE)

static void
pango_hb_face_init (PangoHbFace *self)
{
}

static void
pango_hb_face_finalize (GObject *object)
{
  PangoHbFace *self = PANGO_HB_FACE (object);

  hb_face_destroy (self->face);
  pango_font_description_free (self->description);
  if (self->map)
    g_object_remove_weak_pointer (G_OBJECT (self->map), (gpointer *)&self->map);
  g_free (self->name);
  g_free (self->languages);

  G_OBJECT_CLASS (pango_hb_face_parent_class)->finalize (object);
}

static const char *
pango_hb_face_get_face_name (PangoFontFace *face)
{
  PangoHbFace *self = PANGO_HB_FACE (face);

  return self->name;
}

static PangoFontDescription *
pango_hb_face_describe (PangoFontFace *face)
{
  PangoHbFace *self = PANGO_HB_FACE (face);

  return pango_font_description_copy (self->description);
}

static PangoFontFamily *
pango_hb_face_get_family (PangoFontFace *face)
{
  PangoHbFace *self = PANGO_HB_FACE (face);

  return self->family;
}

static gboolean
pango_hb_face_is_synthesized (PangoFontFace *face)
{
  PangoHbFace *self = PANGO_HB_FACE (face);

  return self->synthetic;
}

static void
pango_hb_face_class_init (PangoHbFaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontFaceClass *face_class = PANGO_FONT_FACE_CLASS (class);

  object_class->finalize = pango_hb_face_finalize;

  face_class->get_face_name = pango_hb_face_get_face_name;
  face_class->describe = pango_hb_face_describe;
  face_class->list_sizes = NULL;
  face_class->is_synthesized = pango_hb_face_is_synthesized;
  face_class->get_family = pango_hb_face_get_family;
}

/* }}} */
/* {{{ Utilities */

static char *
strip_family_name (const char *fullname)
{
  PangoFontDescription *desc;
  char *name;

  desc = pango_font_description_from_string (fullname);

  pango_font_description_unset_fields (desc, PANGO_FONT_MASK_FAMILY);

  name = pango_font_description_to_string (desc);
  if (strcmp (name, "Normal") == 0)
    {
      g_free (name);
      name = g_strdup ("Regular");
    }

  pango_font_description_free (desc);

  return name;
}

static char *
get_name_from_hb_face (hb_face_t       *face,
                       hb_ot_name_id_t  name_id,
                       hb_ot_name_id_t  fallback_id)
{
  char buf[256];
  unsigned int len;
  hb_language_t language;

  language = hb_language_from_string ("en", -1);

  len = sizeof (buf);
  if (hb_ot_name_get_utf8 (face, name_id, language, &len, buf) > 0)
    return g_strdup (buf);

  if (fallback_id != HB_OT_NAME_ID_INVALID)
    {
      len = sizeof (buf);
      if (hb_ot_name_get_utf8 (face, fallback_id, language, &len, buf) > 0)
        return g_strdup (buf);
    }

  return NULL;
}

static PangoHbFace *
pango_hb_face_new_from_blob (hb_blob_t    *blob,
                             unsigned int  index)
{
  PangoHbFace *self;
  char *family_name;
  char *fullname;

  self = g_object_new (PANGO_TYPE_HB_FACE, NULL);

  self->face = hb_face_create (blob, index);

  family_name = get_name_from_hb_face (self->face,
                                       HB_OT_NAME_ID_TYPOGRAPHIC_FAMILY,
                                       HB_OT_NAME_ID_FONT_FAMILY);

  self->name = get_name_from_hb_face (self->face,
                                      HB_OT_NAME_ID_TYPOGRAPHIC_SUBFAMILY,
                                      HB_OT_NAME_ID_FONT_SUBFAMILY);

  fullname = g_strconcat (family_name, " ", self->name, NULL);
  self->description = pango_font_description_from_string (fullname);
  g_free (fullname);
  g_free (family_name);

  self->matrix = (PangoMatrix) PANGO_MATRIX_INIT;
  self->x_scale = self->y_scale = 1.;

  return self;
}

/* }}} */
/* {{{ Private API */

/*< private >
 * pango_hb_face_set_font_map:
 * @self: a `PangoHbFace`
 * @map: a `PangoFontMap`
 *
 * Sets the font map of a `PangoHbFace`.
 *
 * This should only be called by fontmap implementations.
 */
void
pango_hb_face_set_font_map (PangoHbFace  *self,
                            PangoFontMap *map)
{
  self->map = map;
  g_object_add_weak_pointer (G_OBJECT (map), (gpointer *)&self->map);
}

/*< private >
 * pango_hb_face_set_family:
 * @self: a `PangoHbFace`
 * @family: a `PangoFontFamily`
 *
 * Sets the font family of a `PangoHbFace`.
 *
 * This should only be called by fontmap implementations.
 */
void
pango_hb_face_set_family (PangoHbFace     *self,
                          PangoFontFamily *family)
{
  g_return_if_fail (self->family == NULL);
  self->family = family;
}

typedef struct {
  guint16 major;
  guint16 minor;
  gint32 italicAngle;
  gint16 underlinePosition;
  gint16 underlineThickness;
  guint8 isFixedPitch[4];
} PostTable;

/*< private >
 * pango_hb_face_is_monospace:
 * @face: a `PangoHbFace`
 *
 * Returns whether @face should be considered monospace.
 *
 * Returns: `TRUE` if @face is monospace
 */
gboolean
pango_hb_face_is_monospace (PangoHbFace *face)
{
  hb_blob_t *post_blob;
  guint8 *raw_post;
  unsigned int post_len;
  gboolean res = FALSE;

  post_blob = hb_face_reference_table (face->face, HB_TAG ('p', 'o', 's', 't'));
  raw_post = (guint8 *) hb_blob_get_data (post_blob, &post_len);

  if (post_len >= sizeof (PostTable))
    {
      PostTable *post = (PostTable *)raw_post;

      res = post->isFixedPitch[0] != 0 ||
            post->isFixedPitch[1] != 0 ||
            post->isFixedPitch[2] != 0 ||
            post->isFixedPitch[3] != 0;
    }

  hb_blob_destroy (post_blob);

  return res;
}

/*< private >
 * pango_hb_face_is_variable:
 * @face: a `PangoHbFace`
 *
 * Returns whether @face should be considered a variable font.
 *
 * Returns: `TRUE` if @face is variable
 */
gboolean
pango_hb_face_is_variable (PangoHbFace *face)
{
  return hb_ot_var_get_axis_count (face->face) > 0;
}

/*< private >
 * pango_hb_face_get_languages:
 * @face: a `PangoHbFace`
 *
 * Returns the languages supported by @face.
 *
 * Returns: (transfer one) (nullable): a NULL-terminated
 *   array of `PangoLanguage *`
 */
PangoLanguage **
pango_hb_face_get_languages (PangoHbFace *face)
{
  return face->languages;
}

/*< private >
 * pango_hb_face_set_languages:
 * @face: a `PangoHbFace`
 * @languages: (array length=n_languages): the languages supported by @face
 * @n_languages: the number of languages
 *
 * Sets the languages that are supported by @face.
 *
 * This should only be called by fontmap implementations.
 */
void
pango_hb_face_set_languages (PangoHbFace          *face,
                             const PangoLanguage **languages,
                             gsize                 n_languages)
{
  // It would be nicer if HB_OT_META_TAG_SUPPORTED_LANGUAGES was useful
  g_free (face->languages);
  face->languages = g_new (PangoLanguage *, n_languages + 1);
  memcpy (face->languages, languages, n_languages * sizeof (PangoLanguage *));
  face->languages[n_languages] = NULL;
}

 /* }}} */
/* {{{ Public API */

/**
 * pango_hb_face_new_from_bytes:
 * @bytes: a `GBytes` containing font data
 * @index: face index
 *
 * Creates a new `PangoHbFace` from font data in memory.
 *
 * Returns: a newly created `PangoHbFace`
 */
PangoHbFace *
pango_hb_face_new_from_bytes (GBytes       *bytes,
                              unsigned int  index)
{
  hb_blob_t *blob;
  PangoHbFace *face;

  blob = hb_blob_create (g_bytes_get_data (bytes, NULL),
                         g_bytes_get_size (bytes),
                         HB_MEMORY_MODE_READONLY,
                         g_bytes_ref (bytes),
                         (hb_destroy_func_t) g_bytes_unref);

  face = pango_hb_face_new_from_blob (blob, index);

  hb_blob_destroy (blob);

  return face;
}

/**
 * pango_hb_face_new_from_file:
 * @file: font filename
 * @index: face index
 *
 * Creates a new `PangoHbFace` from a font file.
 *
 * Returns: a newly created `PangoHbFace`
 */
PangoHbFace *
pango_hb_face_new_from_file (const char   *file,
                             unsigned int  index)
{
  hb_blob_t *blob;
  PangoHbFace *face;

  blob = hb_blob_create_from_file (file);

  face = pango_hb_face_new_from_blob (blob, index);

  hb_blob_destroy (blob);

  return face;
}

/**
 * pango_hb_face_new_transformed:
 * @face: a `PangoHbFace`
 * @name: the name for the transformed face
 * @transform: the transform to apply
 *
 * Creates a new `PangoHbFace` that applies the
 * given @transform to the glyphs in @face.
 *
 * Note that the @name should not include the family
 * name. For example, when using this to create a
 * synthetic Italic, the name should just be "Italic".
 *
 * Returns: (transfer full): a newly created `PangoHbFace`
 */
PangoHbFace *
pango_hb_face_new_transformed (PangoHbFace       *face,
                               const char        *name,
                               const PangoMatrix *transform)
{
  PangoHbFace *self;
  const char *family_name;
  char *fullname;

  g_return_val_if_fail (PANGO_IS_HB_FACE (face), NULL);
  g_return_val_if_fail (name && *name, NULL);
  g_return_val_if_fail (transform != NULL, NULL);

  self = g_object_new (PANGO_TYPE_HB_FACE, NULL);

  self->face = hb_face_reference (face->face);

  family_name = pango_font_description_get_family (face->description);
  self->name = g_strdup (name);

  fullname = g_strconcat (family_name, " ", self->name, NULL);
  self->description = pango_font_description_from_string (fullname);
  g_free (fullname);

  self->matrix = *transform;
  pango_matrix_get_font_scale_factors (&self->matrix, &self->x_scale, &self->y_scale);
  pango_matrix_scale (&self->matrix, 1./self->x_scale, 1./self->y_scale);

  self->synthetic = TRUE;

  return self;
}

/**
 * pango_hb_face_new_alias:
 * @face: a `PangoHbFace`
 * @fullname: the full name of the new face
 *
 * Creates a new `PangoHbFace that acts as an alias
 * for the given @face.
 *
 * Note that @fullname should include the family name.
 * For example, when using this to create a monospace
 * italic font, @fullname should be "Monospace Italic".
 *
 * Returns: (transfer full): a newly created `PangoHbFace`
 */
PangoHbFace *
pango_hb_face_new_alias (PangoHbFace *face,
                         const char  *fullname)
{
  PangoHbFace *self;

  g_return_val_if_fail (PANGO_IS_HB_FACE (face), NULL);
  g_return_val_if_fail (fullname && *fullname, NULL);

  self = g_object_new (PANGO_TYPE_HB_FACE, NULL);

  self->face = hb_face_reference (face->face);

  self->name = strip_family_name (fullname);
  self->description = pango_font_description_from_string (fullname);

  self->matrix = (PangoMatrix) PANGO_MATRIX_INIT;
  self->x_scale = self->y_scale = 1.;

  self->synthetic = TRUE;

  return self;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
