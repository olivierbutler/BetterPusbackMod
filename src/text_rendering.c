/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license in the file COPYING
 * or http://www.opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file COPYING.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2017 Saso Kiselkov. All rights reserved.
 */

#include <acfutils/assert.h>
#include <acfutils/helpers.h>

#include "text_rendering.h"

/*
 * This weird macro construct is needed to implement freetype error code
 * to string translation. It defines a static ft_errors table that we can
 * traverse to translate an error code into a string.
 */
#undef FTERRORS_H_
#define FT_ERRORDEF(e, v, s) {e, s},
#define FT_ERROR_START_LIST {
#define FT_ERROR_END_LIST                                                      \
  {                                                                            \
    0, NULL                                                                    \
  }                                                                            \
  }                                                                            \
  ;

static const struct {
  int err_code;
  const char *err_msg;
} ft_errors[] =
#include FT_ERRORS_H

#define LINE_MARGIN_MULTIPLIER 1.2

    bool_t get_text_block_size(const char *text, FT_Face face, int font_size,
                               int *width, int *height) {
  FT_Error err;
  size_t h = font_size * LINE_MARGIN_MULTIPLIER, w = 0, line_w = 0;

  if ((err = FT_Set_Pixel_Sizes(face, 0, font_size)) != 0) {
    logMsg("Error setting font size to %d: %d", font_size, err);
    return (B_FALSE);
  }

  for (int i = 0, n = strlen(text); i < n; i++) {
    if (text[i] == '\n') {
      h += font_size * LINE_MARGIN_MULTIPLIER;
      w = MAX(w, line_w);
      line_w = 0;
    } else {
      FT_UInt glyph_index = FT_Get_Char_Index(face, text[i]);

      if ((err = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) != 0) {
        logMsg("Error loading glyph for '%c': %d", text[i], err);
        return (B_FALSE);
      }
      if ((err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) != 0) {
        logMsg("Error rendering glyph for '%c': %d", text[i], err);
        return (B_FALSE);
      }
      line_w += face->glyph->advance.x >> 6;
    }
  }
  w = MAX(w, line_w);

  *width = w;
  *height = h;

  return (B_TRUE);
}

static void blit_glyph(const FT_Bitmap *bitmap, uint8_t *rgba_texture,
                       unsigned tex_width, unsigned tex_height, unsigned x,
                       unsigned y, uint8_t r, uint8_t g, uint8_t b) {
  for (unsigned i = 0; i < bitmap->rows * bitmap->width; i++) {
    unsigned gx = i % bitmap->width, gy = i / bitmap->width;
    unsigned tex_coord = (((gy + y) * tex_width) + (gx + x)) * 4;

    ASSERT(tex_coord < (tex_width * tex_height * 4));

#define BLEND_LIN(x, y, r) (x) = (x) + ((y - x) * ((r) / 255.0))
    BLEND_LIN(rgba_texture[tex_coord + 0], r, bitmap->buffer[i]);
    BLEND_LIN(rgba_texture[tex_coord + 1], g, bitmap->buffer[i]);
    BLEND_LIN(rgba_texture[tex_coord + 2], b, bitmap->buffer[i]);
    BLEND_LIN(rgba_texture[tex_coord + 3], 255, bitmap->buffer[i]);
#undef BLEND_LIN
  }
}

bool_t render_text_block(const char *text, FT_Face face, int font_size, int x,
                         int y, uint8_t r, uint8_t g, uint8_t b,
                         uint8_t *rgba_texture, int texture_width,
                         int texture_height) {
  FT_Error err;
  int start_x = x;

  if ((err = FT_Set_Pixel_Sizes(face, 0, font_size)) != 0) {
    logMsg("Error setting font size to %d: %d", font_size, err);
    return (B_FALSE);
  }

  for (int i = 0, n = strlen(text); i < n; i++) {
    if (text[i] == '\n') {
      y += font_size * LINE_MARGIN_MULTIPLIER;
      x = start_x;
    } else {
      FT_GlyphSlot slot;

      if ((err = FT_Load_Char(face, text[i], FT_LOAD_RENDER)) != 0) {
        logMsg("Error rendering glyph for '%c': %d", text[i], err);
        return (B_FALSE);
      }
      slot = face->glyph;
      blit_glyph(&slot->bitmap, rgba_texture, texture_width, texture_height,
                 x + slot->bitmap_left, y - slot->bitmap_top, r, g, b);
      x += face->glyph->advance.x >> 6;
    }
  }

  return (B_TRUE);
}

const char *ft_err2str(FT_Error err) {
  for (int i = 0; ft_errors[i].err_msg != NULL; i++)
    if (ft_errors[i].err_code == err)
      return (ft_errors[i].err_msg);
  return (NULL);
}
