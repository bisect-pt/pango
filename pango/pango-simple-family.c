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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gio/gio.h>

#include "pango-simple-family-private.h"
#include "pango-impl-utils.h"

/* {{{ PangoFontFamily implementation */

struct _PangoSimpleFamilyClass
{
  PangoFontFamilyClass parent_class;
};

G_DEFINE_TYPE (PangoSimpleFamily, pango_simple_family, PANGO_TYPE_FONT_FAMILY)

static void
pango_simple_family_init (PangoSimpleFamily *self)
{
  self->faces = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
pango_simple_family_finalize (GObject *object)
{
  PangoSimpleFamily *self = PANGO_SIMPLE_FAMILY (object);

  g_free (self->name);
  g_ptr_array_unref (self->faces);

  G_OBJECT_CLASS (pango_simple_family_parent_class)->finalize (object);
}

static const char *
pango_simple_family_get_name (PangoFontFamily *family)
{
  PangoSimpleFamily *self = PANGO_SIMPLE_FAMILY (family);

  return self->name;
}

static void
pango_simple_family_list_faces (PangoFontFamily  *family,
                                PangoFontFace  ***faces,
                                int              *n_faces)
{
  PangoSimpleFamily *self = PANGO_SIMPLE_FAMILY (family);

  if (faces)
    *faces = g_memdup2 (self->faces, self->faces->len * sizeof (PangoFontFace *));

  if (n_faces)
    *n_faces = self->faces->len;
}

static PangoFontFace *
pango_simple_family_get_face (PangoFontFamily *family,
                               const char     *name)
{
  PangoSimpleFamily *self = PANGO_SIMPLE_FAMILY (family);

  for (int i = 0; i < self->faces->len; i++)
    {
      PangoFontFace *face = g_ptr_array_index (self->faces, i);

        if (name == NULL || strcmp (name, pango_font_face_get_face_name (face)) == 0)
          return PANGO_FONT_FACE (face);
    }

  return NULL;
}

static gboolean
pango_simple_family_is_monospace (PangoFontFamily *family)
{
  PangoSimpleFamily *self = PANGO_SIMPLE_FAMILY (family);

  return self->monospace;
}

static gboolean
pango_simple_family_is_variable (PangoFontFamily *family)
{
  PangoSimpleFamily *self = PANGO_SIMPLE_FAMILY (family);

  return self->variable;
}

static void
pango_simple_family_class_init (PangoSimpleFamilyClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontFamilyClass *family_class = PANGO_FONT_FAMILY_CLASS (class);

  object_class->finalize = pango_simple_family_finalize;

  family_class->list_faces = pango_simple_family_list_faces;
  family_class->get_name = pango_simple_family_get_name;
  family_class->get_face = pango_simple_family_get_face;
  family_class->is_monospace = pango_simple_family_is_monospace;
  family_class->is_variable = pango_simple_family_is_variable;
}

/* }}} */
/* {{{ Private API */

/*< private >
 * pango_simple_family_new:
 * @name: the family name
 * @monospace: whether this family is monospace
 * @variable: whether this family is variable
 *
 * Creates a new `PangoSimpleFamily`.
 *
 * Returns: a newly created `PangoSimpleFamily`
 */
PangoSimpleFamily *
pango_simple_family_new (const char *name,
                         gboolean    monospace,
                         gboolean    variable)
{
  PangoSimpleFamily *self;

  self = g_object_new (PANGO_TYPE_SIMPLE_FAMILY, NULL);

  self->name = g_strdup (name);
  self->monospace = monospace;
  self->variable = variable;

  return self;
}

/*< private >
 * pango_simple_family_add_face:
 * @self: a `PangoSimpleFamily`
 * @face: a `PangoFontFace` to add
 *
 * Adds a `PangoFontFace` to a `PangoSimpleFamily`.
 *
 * It is an error to call this function more than
 * once for the same face.
 */
void
pango_simple_family_add_face (PangoSimpleFamily *self,
                              PangoFontFace     *face)
{
  g_ptr_array_add (self->faces, g_object_ref (face));
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
