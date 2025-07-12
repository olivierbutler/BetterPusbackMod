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

#ifndef _XRAAS_TEXT_RENDERING_H_
#define _XRAAS_TEXT_RENDERING_H_

#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <acfutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool_t get_text_block_size(const char *text, FT_Face face, int font_size,
                           int *width, int *height);
bool_t render_text_block(const char *text, FT_Face face, int font_size, int x,
                         int y, uint8_t r, uint8_t g, uint8_t b,
                         uint8_t *rgba_texture, int texture_width,
                         int texture_height);
const char *ft_err2str(FT_Error err);

#ifdef __cplusplus
}
#endif

#endif /* _XRAAS_TEXT_RENDERING_H_ */
