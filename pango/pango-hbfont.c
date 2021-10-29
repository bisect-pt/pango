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

#include "pango-hbfont-private.h"

#include "pango-font-private.h"
#include "pango-coverage-private.h"
#include "pango-hbface-private.h"
#include "pango-impl-utils.h"

#include <hb-ot.h>

/* {{{ Utilities */
static int
get_average_char_width (PangoFont  *font,
                        const char *text)
{
  hb_font_t *hb_font = pango_font_get_hb_font (font);
  int width = 0;

  for (const char *p = text; *p; p = g_utf8_next_char (p))
    {
      gunichar wc;
      hb_codepoint_t glyph;
      PangoRectangle extents;

      wc = g_utf8_get_char (p);
      if (!hb_font_get_nominal_glyph (hb_font, wc, &glyph))
        continue;

      pango_font_get_glyph_extents (font, glyph, &extents, NULL);

      width += extents.x + extents.width;
    }

  return width / pango_utf8_strwidth (text);
}

static void
get_max_char_size (PangoFont  *font,
                   const char *text,
                   int        *width,
                   int        *height)
{
  hb_font_t *hb_font = pango_font_get_hb_font (font);
  int w = 0;
  int h = 0;

  for (const char *p = text; *p; p = g_utf8_next_char (p))
    {
      gunichar wc;
      hb_codepoint_t glyph;
      PangoRectangle extents;

      wc = g_utf8_get_char (p);
      if (!hb_font_get_nominal_glyph (hb_font, wc, &glyph))
        continue;

      pango_font_get_glyph_extents (font, glyph, &extents, NULL);

      w = MAX (w, extents.x + extents.width);
      h = MAX (h, extents.height);
    }

  if (width)
    *width = w;

  if (height)
    *height = h;
}

static void
parse_variations (const char            *variations,
                  hb_ot_var_axis_info_t *axes,
                  int                    n_axes,
                  float                 *coords)
{
  const char *p;
  const char *end;
  hb_variation_t var;
  int i;

  p = variations;
  while (p && *p)
    {
      end = strchr (p, ',');
      if (hb_variation_from_string (p, end ? end - p: -1, &var))
        {
          for (i = 0; i < n_axes; i++)
            {
              if (axes[i].tag == var.tag)
                {
                  coords[axes[i].axis_index] = var.value;
                  break;
                }
            }
        }

      p = end ? end + 1 : NULL;
    }
}
/* }}} */
/* {{{ hex box sizing */

/* This code needs to stay in sync with the hexbox rendering
 * code in pangocairo-render.c.
 */
static HexBoxInfo *
create_hex_box_info (PangoHbFont *self)
{
  const char hexdigits[] = "0123456789ABCDEF";
  hb_font_t *hb_font;
  PangoFont *mini_font;
  HexBoxInfo *hbi;
  int rows;
  double pad;
  int width = 0;
  int height = 0;
  hb_font_extents_t font_extents;
  double font_ascent, font_descent;
  double mini_size;
  PangoFontDescription *desc;
  PangoContext *context;

  if (!self->face->map)
    return NULL;

  desc = pango_font_describe_with_absolute_size (PANGO_FONT (self));
  hb_font = pango_font_get_hb_font (PANGO_FONT (self));

  /* Create mini_font description */

  /* We inherit most font properties for the mini font.
   * Just change family and size, so you get bold
   * hex digits in the hexbox for a bold font.
   */

  /* We should rotate the box, not glyphs */
  pango_font_description_unset_fields (desc, PANGO_FONT_MASK_GRAVITY);

  pango_font_description_set_family_static (desc, "monospace");

  rows = 2;
  mini_size = self->size / 2.2;

  if (mini_size < 6.0)
    {
      rows = 1;
      mini_size = MIN (MAX (self->size - 1, 0), 6.0);
    }

  pango_font_description_set_absolute_size (desc, mini_size);

  /* Load mini_font */
  context = pango_font_map_create_context (self->face->map);
  pango_context_set_matrix (context, &self->matrix);
  pango_context_set_language (context, pango_script_get_sample_language (PANGO_SCRIPT_LATIN));

  mini_font = pango_font_map_load_font (self->face->map, context, desc);

  g_object_unref (context);

  pango_font_description_free (desc);

  get_max_char_size (mini_font, hexdigits, &width, &height);

  hb_font_get_extents_for_direction (hb_font, HB_DIRECTION_LTR, &font_extents);
  font_ascent = font_extents.ascender / (double) PANGO_SCALE;
  font_descent = - font_extents.descender / (double) PANGO_SCALE;

  pad = (font_ascent + font_descent) / 43.;
  pad = MIN (pad, mini_size / (double) PANGO_SCALE);

  hbi = g_new (HexBoxInfo, 1);
  hbi->font = mini_font;
  hbi->rows = rows;

  hbi->digit_width  = width / (double) PANGO_SCALE;
  hbi->digit_height = height / (double) PANGO_SCALE;

  hbi->pad_x = pad;
  hbi->pad_y = pad;

  hbi->line_width = MIN (hbi->pad_x, hbi->pad_y);

  hbi->box_height = 3 * hbi->pad_y + rows * (hbi->pad_y + hbi->digit_height);

  if (rows == 1 || hbi->box_height <= font_ascent)
    hbi->box_descent = 2 * hbi->pad_y;
  else if (hbi->box_height <= font_ascent + font_descent - 2 * hbi->pad_y)
    hbi->box_descent = 2 * hbi->pad_y + hbi->box_height - font_ascent;
  else
    hbi->box_descent = font_descent * hbi->box_height / (font_ascent + font_descent);

  return hbi;
}

static void
get_space_extents (PangoHbFont    *self,
                   PangoRectangle *ink_rect,
                   PangoRectangle *logical_rect)
{
  hb_font_t *hb_font = pango_font_get_hb_font (PANGO_FONT (self));
  const char hexdigits[] = "0123456789ABCDEF";
  int width;
  hb_direction_t direction;
  hb_font_extents_t font_extents;

  direction = PANGO_GRAVITY_IS_VERTICAL (self->gravity)
              ? HB_DIRECTION_TTB
              : HB_DIRECTION_LTR;

  hb_font_get_extents_for_direction (hb_font, direction, &font_extents);

  /* We don't render missing spaces as hex boxes, so come up with some
   * width to use. For lack of anything better, use average hex digit width.
   */
  width = get_average_char_width (PANGO_FONT (self), hexdigits);

  if (ink_rect)
    {
      ink_rect->x = ink_rect->y = ink_rect->height = 0;
      ink_rect->width = width;
    }

  if (logical_rect)
    {
      logical_rect->x = 0;
      logical_rect->y = - font_extents.ascender;
      logical_rect->height = font_extents.ascender - font_extents.descender;
      logical_rect->width = width;
    }
}

static void
get_glyph_extents_missing (PangoHbFont    *self,
                           PangoGlyph      glyph,
                           PangoRectangle *ink_rect,
                           PangoRectangle *logical_rect)
{
  gunichar ch;
  int rows, cols;
  HexBoxInfo *hbi;

  ch = glyph & ~PANGO_GLYPH_UNKNOWN_FLAG;

  if (!self->hex_box_info)
    self->hex_box_info = create_hex_box_info (self);

  if (ch == 0x20 || ch == 0x2423)
    {
      get_space_extents (self, ink_rect, logical_rect);
      return;
    }

  hbi = self->hex_box_info;

  if (G_UNLIKELY (glyph == PANGO_GLYPH_INVALID_INPUT || ch > 0x10FFFF))
    {
      rows = hbi->rows;
      cols = 1;
    }
  else if (pango_get_ignorable_size (ch, &rows, &cols))
    {
      /* We special-case ignorables when rendering hex boxes */
    }
  else
    {
      rows = hbi->rows;
      cols = (ch > 0xffff ? 6 : 4) / rows;
    }

  if (ink_rect)
    {
      ink_rect->x = PANGO_SCALE * hbi->pad_x;
      ink_rect->y = PANGO_SCALE * (hbi->box_descent - hbi->box_height);
      ink_rect->width = PANGO_SCALE * (3 * hbi->pad_x + cols * (hbi->digit_width + hbi->pad_x));
      ink_rect->height = PANGO_SCALE * hbi->box_height;
    }

  if (logical_rect)
    {
      logical_rect->x = 0;
      logical_rect->y = PANGO_SCALE * (hbi->box_descent - (hbi->box_height + hbi->pad_y));
      logical_rect->width = PANGO_SCALE * (5 * hbi->pad_x + cols * (hbi->digit_width + hbi->pad_x));
      logical_rect->height = PANGO_SCALE * (hbi->box_height + 2 * hbi->pad_y);
    }
}

/* }}} */
/* {{{ PangoFont implementation */

struct _PangoHbFontClass
{
  PangoFontClass parent_class;
};

G_DEFINE_TYPE (PangoHbFont, pango_hb_font, PANGO_TYPE_FONT)

static void
pango_hb_font_init (PangoHbFont *self)
{
  self->gravity = PANGO_GRAVITY_SOUTH;
  self->matrix = (PangoMatrix) PANGO_MATRIX_INIT;
}

static void
hex_box_info_destroy (HexBoxInfo *hex_box_info)
{
  g_object_unref (hex_box_info->font);
  g_free (hex_box_info);
}

static void
pango_hb_font_finalize (GObject *object)
{
  PangoHbFont *self = PANGO_HB_FONT (object);

  g_object_unref (self->face);
  g_free (self->variations);
  g_clear_pointer (&self->hex_box_info, hex_box_info_destroy);

  G_OBJECT_CLASS (pango_hb_font_parent_class)->finalize (object);
}

static PangoFontDescription *
pango_hb_font_describe (PangoFont *font)
{
  PangoHbFont *self = PANGO_HB_FONT (font);
  PangoFontDescription *desc;

  desc = pango_font_face_describe (PANGO_FONT_FACE (self->face));
  pango_font_description_set_absolute_size (desc, self->size);

  return desc;
}

static PangoCoverage *
pango_hb_font_get_coverage (PangoFont     *font,
                            PangoLanguage *language G_GNUC_UNUSED)
{
  PangoHbFont *self = PANGO_HB_FONT (font);

  return pango_coverage_new_for_hb_face (self->face->face);
}

static void
pango_hb_font_get_glyph_extents (PangoFont      *font,
                                 PangoGlyph      glyph,
                                 PangoRectangle *ink_rect,
                                 PangoRectangle *logical_rect)
{
  PangoHbFont *self = PANGO_HB_FONT (font);
  hb_font_t *hb_font = pango_font_get_hb_font (font);
  hb_glyph_extents_t extents;
  hb_direction_t direction;
  hb_font_extents_t font_extents;

  direction = PANGO_GRAVITY_IS_VERTICAL (self->gravity)
              ? HB_DIRECTION_TTB
              : HB_DIRECTION_LTR;

  hb_font_get_extents_for_direction (hb_font, direction, &font_extents);

  if (glyph == PANGO_GLYPH_EMPTY)
    {
      if (ink_rect)
        ink_rect->x = ink_rect->y = ink_rect->width = ink_rect->height = 0;

      if (logical_rect)
        {
          logical_rect->x = logical_rect->width = 0;
          logical_rect->y = - font_extents.ascender;
          logical_rect->height = font_extents.ascender - font_extents.descender;
        }

      return;
    }
  else if (glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    {
      get_glyph_extents_missing (self, glyph, ink_rect, logical_rect);

      return;
    }

  hb_font_get_glyph_extents (hb_font, glyph, &extents);

  if (ink_rect)
    {
      PangoRectangle r;

      r.x = extents.x_bearing;
      r.y = - extents.y_bearing;
      r.width = extents.width;
      r.height = - extents.height;

      pango_matrix_transform_rectangle (&self->face->matrix, &r);

      switch (self->gravity)
        {
        case PANGO_GRAVITY_SOUTH:
          ink_rect->x = r.x;
          ink_rect->y = r.y;
          ink_rect->width = r.width;
          ink_rect->height = r.height;
          break;
        case PANGO_GRAVITY_NORTH:
          ink_rect->x = - r.x;
          ink_rect->y = - r.y;
          ink_rect->width = - r.width;
          ink_rect->height = - r.height;
          break;
        case PANGO_GRAVITY_EAST:
          ink_rect->x = r.y;
          ink_rect->y = - r.x - r.width;
          ink_rect->width = r.height;
          ink_rect->height = r.width;
          break;
        case PANGO_GRAVITY_WEST:
          ink_rect->x = - r.y - r.height;
          ink_rect->y = r.x;
          ink_rect->width = r.height;
          ink_rect->height = r.width;
          break;
        case PANGO_GRAVITY_AUTO:
        default:
          g_assert_not_reached ();
        }

      if (PANGO_GRAVITY_IS_IMPROPER (self->gravity))
        {
          PangoMatrix matrix = (PangoMatrix) PANGO_MATRIX_INIT;
          pango_matrix_scale (&matrix, -1, -1);
          pango_matrix_transform_rectangle (&matrix, ink_rect);
        }
    }

  if (logical_rect)
    {
      hb_position_t h_advance;
      hb_font_extents_t extents;

      h_advance = hb_font_get_glyph_h_advance (hb_font, glyph);
      hb_font_get_h_extents (hb_font, &extents);

      logical_rect->x = 0;
      logical_rect->height = extents.ascender - extents.descender;

      switch (self->gravity)
        {
        case PANGO_GRAVITY_SOUTH:
          logical_rect->y = - extents.ascender;
          logical_rect->width = h_advance;
          break;
        case PANGO_GRAVITY_NORTH:
          logical_rect->y = extents.descender;
          logical_rect->width = h_advance;
          break;
        case PANGO_GRAVITY_EAST:
          logical_rect->y = - logical_rect->height / 2;
          logical_rect->width = logical_rect->height;
          break;
        case PANGO_GRAVITY_WEST:
          logical_rect->y = - logical_rect->height / 2;
          logical_rect->width = - logical_rect->height;
          break;
        case PANGO_GRAVITY_AUTO:
        default:
          g_assert_not_reached ();
        }

      if (PANGO_GRAVITY_IS_IMPROPER (self->gravity))
        {
          logical_rect->height = - logical_rect->height;
          logical_rect->y = - logical_rect->y;
        }
    }
}

static PangoFontMetrics *
pango_hb_font_get_metrics (PangoFont     *font,
                           PangoLanguage *language)
{
  hb_font_t *hb_font = pango_font_get_hb_font (font);
  PangoFontMetrics *metrics;
  hb_font_extents_t extents;
  hb_position_t position;

  metrics = pango_font_metrics_new ();

  hb_font_get_extents_for_direction (hb_font, HB_DIRECTION_LTR, &extents);

  metrics->descent = - extents.descender;
  metrics->ascent = extents.ascender;
  metrics->height = extents.ascender - extents.descender + extents.line_gap;

  metrics->underline_thickness = PANGO_SCALE;
  metrics->underline_position = - PANGO_SCALE;
  metrics->strikethrough_thickness = PANGO_SCALE;
  metrics->strikethrough_position = metrics->ascent / 2;

  if (hb_ot_metrics_get_position (hb_font, HB_OT_METRICS_TAG_UNDERLINE_SIZE, &position))
    metrics->underline_thickness = position;

  if (hb_ot_metrics_get_position (hb_font, HB_OT_METRICS_TAG_UNDERLINE_OFFSET, &position))
    metrics->underline_position = position;

  if (hb_ot_metrics_get_position (hb_font, HB_OT_METRICS_TAG_STRIKEOUT_SIZE, &position))
    metrics->strikethrough_thickness = position;

  if (hb_ot_metrics_get_position (hb_font, HB_OT_METRICS_TAG_STRIKEOUT_OFFSET, &position))
    metrics->strikethrough_position = position;

  metrics->approximate_char_width = get_average_char_width (font, pango_language_get_sample_string (language));
  get_max_char_size (font, "0123456789", &metrics->approximate_digit_width, NULL);

  return metrics;
}

static PangoFontMap *
pango_hb_font_get_font_map (PangoFont *font)
{
  PangoHbFont *self = PANGO_HB_FONT (font);

  return self->face->map;
}

static hb_font_t *
pango_hb_font_create_hb_font (PangoFont *font)
{
  PangoHbFont *self = PANGO_HB_FONT (font);
  hb_face_t *hb_face = self->face->face;
  hb_font_t *hb_font;
  double x_scale, y_scale;
  unsigned int n_axes;
  hb_ot_var_axis_info_t axes[32];
  float coords[32];

  hb_font = hb_font_create (hb_face);

  x_scale = self->face->x_scale;
  y_scale = self->face->y_scale;

  if (PANGO_GRAVITY_IS_IMPROPER (self->gravity))
    {
      x_scale = - x_scale;
      y_scale = - y_scale;
    }

  hb_font_set_scale (hb_font, self->size * x_scale, self->size * y_scale);

  n_axes = G_N_ELEMENTS (axes);
  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, axes);
  if (n_axes > 0 && self->variations)
    {
      for (int i = 0; i < n_axes; i++)
        coords[axes[i].axis_index] = axes[i].default_value;

      parse_variations (self->variations, axes, n_axes, coords);
      hb_font_set_var_coords_design (hb_font, coords, n_axes);
    }

  return hb_font;
}

static PangoLanguage **
pango_hb_font_get_languages (PangoFont *font)
{
  PangoHbFont *self = PANGO_HB_FONT (font);

  return pango_hb_face_get_languages (self->face);
}

static gboolean
pango_hb_font_has_char (PangoFont *font,
                        gunichar   wc)
{
  hb_font_t *hb_font = pango_font_get_hb_font (font);
  hb_codepoint_t glyph;

  return hb_font_get_nominal_glyph (hb_font, wc, &glyph);
}

static PangoFontFace *
pango_hb_font_get_face (PangoFont *font)
{
  PangoHbFont *self = PANGO_HB_FONT (font);

  return PANGO_FONT_FACE (self->face);
}

static void
pango_hb_font_class_init (PangoHbFontClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PangoFontClass *font_class = PANGO_FONT_CLASS (class);
  PangoFontClassPrivate *pclass;

  object_class->finalize = pango_hb_font_finalize;

  font_class->describe = pango_hb_font_describe;
  font_class->describe_absolute = pango_hb_font_describe;
  font_class->get_coverage = pango_hb_font_get_coverage;
  font_class->get_glyph_extents = pango_hb_font_get_glyph_extents;
  font_class->get_metrics = pango_hb_font_get_metrics;
  font_class->get_font_map = pango_hb_font_get_font_map;
  font_class->create_hb_font = pango_hb_font_create_hb_font;

  pclass = g_type_class_get_private ((GTypeClass *) class, PANGO_TYPE_FONT);

  pclass->get_languages = pango_hb_font_get_languages;
  pclass->has_char = pango_hb_font_has_char;
  pclass->get_face = pango_hb_font_get_face;
}
/* }}} */
/* {{{ Public API */

/**
 * pango_hb_font_new:
 * @face: the `PangoHbFace` to use
 * @size: the desired size in pixels, scaled by `PANGO_SCALE`
 * @variations: (nullable): font variations to apply
 * @gravity: the gravity to use when rendering
 * @matrix: (nullable): transformation matrix to use when rendering
 *
 * Creates a new `PangoHbFont`.
 *
 * Returns: a newly created `PangoHbFont`
 */
PangoHbFont *
pango_hb_font_new (PangoHbFace       *face,
                   int                size,
                   const char        *variations,
                   PangoGravity       gravity,
                   const PangoMatrix *matrix)
{
  PangoHbFont *self;

  self = g_object_new (PANGO_TYPE_HB_FONT, NULL);

  self->face = g_object_ref (face);

  self->size = size;
  self->variations = g_strdup (variations);
  self->gravity = gravity;
  if (matrix)
    self->matrix = *matrix;

  return self;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
