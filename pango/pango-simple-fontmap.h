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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#pragma once

#include <pango/pango-font.h>
#include <pango/pango-fontmap.h>
#include <pango/pango-hbface.h>
#include <hb.h>

G_BEGIN_DECLS

#define PANGO_TYPE_SIMPLE_FONT_MAP   (pango_simple_font_map_get_type ())

PANGO_AVAILABLE_IN_1_50
G_DECLARE_FINAL_TYPE (PangoSimpleFontMap, pango_simple_font_map, PANGO, SIMPLE_FONT_MAP, PangoFontMap)

PANGO_AVAILABLE_IN_1_50
PangoSimpleFontMap *    pango_simple_font_map_new       (void);

PANGO_AVAILABLE_IN_1_50
void                    pango_simple_font_map_add_file  (PangoSimpleFontMap   *self,
                                                         const char           *file,
                                                         unsigned int          index);

PANGO_AVAILABLE_IN_1_50
void                    pango_simple_font_map_add_face  (PangoSimpleFontMap   *self,
                                                         PangoHbFace          *face);

PANGO_AVAILABLE_IN_1_50
void                    pango_simple_font_map_set_resolution (PangoSimpleFontMap *self,
                                                              double              dpi);

G_END_DECLS
