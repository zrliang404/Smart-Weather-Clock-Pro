/*
 * MIT License
 *
 * Copyright (c) 2023 梅花嵌入式
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ST_FONTS_H__
#define __ST_FONTS_H__


#include <stdint.h>


typedef struct
{
    const uint16_t width;
    const uint16_t height;
    const uint8_t *data;
    const uint32_t count;
} st_fonts_t;




extern st_fonts_t font_ascii_8x16;
extern st_fonts_t font_temper_16x32;
extern st_fonts_t font_time_24x48;
extern st_fonts_t font_zhaoqing_24x24;
extern st_fonts_t font_shanghai_24x24;
extern st_fonts_t font_guangzhou_24x24;
extern st_fonts_t font_beijing_24x24;
extern st_fonts_t font_heihe_24x24;
extern st_fonts_t font_tiantiankaixing_16x16;

#endif /* __ST_FONTS_H__ */
