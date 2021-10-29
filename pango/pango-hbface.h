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

#include <hb.h>

G_BEGIN_DECLS

#define PANGO_TYPE_HB_FACE      (pango_hb_face_get_type ())
PANGO_AVAILABLE_IN_1_50
G_DECLARE_FINAL_TYPE (PangoHbFace, pango_hb_face, PANGO, HB_FACE, PangoFontFace)

PANGO_AVAILABLE_IN_1_50
PangoHbFace *   pango_hb_face_new_from_file     (const char             *file,
                                                 unsigned int            index);

PANGO_AVAILABLE_IN_1_50
PangoHbFace *   pango_hb_face_new_from_bytes    (GBytes                 *bytes,
                                                 unsigned int            index);

PANGO_AVAILABLE_IN_1_50
PangoHbFace *   pango_hb_face_new_transformed   (PangoHbFace            *face,
                                                 const char             *name,
                                                 const PangoMatrix      *transform);

PANGO_AVAILABLE_IN_1_50
PangoHbFace *   pango_hb_face_new_alias         (PangoHbFace            *face,
                                                 const char             *fullname);

G_END_DECLS
