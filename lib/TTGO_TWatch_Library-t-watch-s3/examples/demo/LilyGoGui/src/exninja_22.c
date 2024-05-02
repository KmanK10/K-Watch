#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

/*******************************************************************************
 * Size: 22 px
 * Bpp: 3
 * Opts: --font ../font/new/Exninja-Regular/Exninja-Regular/Exninja-2.ttf -r 0x20-0x7F --size 22 --format lvgl --bpp 3 --lv-include lvgl/lvgl.h  -o exninja_22.c
 ******************************************************************************/

#ifndef EXNINJA_22
#define EXNINJA_22 1
#endif

#if EXNINJA_22

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t gylph_bitmap[] = {
    /* U+20 " " */

    /* U+21 "!" */
    0x4, 0x2c, 0x7, 0xff, 0x65, 0x70, 0x70, 0x64,
    0x0,

    /* U+22 "\"" */
    0x1, 0xd2, 0x70, 0x8, 0x1, 0x2, 0xa8, 0x46,
    0x3, 0x55, 0xa0,

    /* U+23 "#" */
    0x2, 0x34, 0x1, 0xa0, 0x7c, 0xd0, 0x6, 0x80,
    0xff, 0xe5, 0x10, 0x40, 0xe5, 0xfa, 0x8d, 0xe0,
    0xfe, 0x2a, 0xd3, 0x12, 0xb1, 0x6c, 0x55, 0xa6,
    0x25, 0x62, 0xd8, 0xab, 0x50, 0x5a, 0x62, 0xd8,
    0x84, 0x84, 0x24, 0xc1, 0x28, 0xf, 0xfe, 0x10,

    /* U+24 "$" */
    0x3, 0x89, 0x1, 0xfe, 0xb4, 0x7, 0xaf, 0xf3,
    0x6f, 0xea, 0x12, 0x6d, 0x27, 0xb4, 0x40, 0x13,
    0xc0, 0x9a, 0x1, 0xff, 0xce, 0x20, 0x7f, 0x91,
    0xfe, 0x6d, 0xfd, 0x40, 0x6f, 0xe4, 0xff, 0x64,
    0x40, 0xe2, 0x4, 0x40, 0xff, 0xe7, 0xb0, 0x3f,
    0xeb, 0xfc, 0xdb, 0xf8, 0x90, 0xdb, 0x27, 0xb6,
    0x2, 0x26, 0xf8, 0x98, 0x0,

    /* U+25 "%" */
    0x1b, 0xfc, 0xc0, 0xf4, 0xe9, 0xa, 0x6c, 0x60,
    0x1c, 0x9c, 0x40, 0x3c, 0x86, 0x7, 0x1a, 0xd8,
    0xf, 0xfe, 0x7, 0x30, 0xf, 0xfe, 0x5, 0x38,
    0xf, 0xfe, 0x4, 0x58, 0xc, 0xc5, 0xfc, 0x88,
    0x4e, 0x10, 0x31, 0xad, 0xee, 0x6b, 0x60, 0x3c,
    0xa4, 0xc7, 0x18, 0x7, 0xff, 0x3, 0xc, 0x64,
    0xc8, 0xf, 0x43, 0x86, 0x6f, 0x52, 0x6, 0x6d,
    0x48, 0x7, 0xfa, 0x86, 0x4, 0x63, 0x40, 0x7f,
    0xf0, 0x32, 0x80, 0x7f, 0xf0, 0x31, 0xa0, 0x7f,
    0xf0, 0x29, 0xe0, 0x79, 0x48, 0xe1, 0x8b, 0xc8,
    0x1c, 0xcb, 0x64, 0x84,

    /* U+26 "&" */
    0x1f, 0xfd, 0x80, 0xc8, 0x4b, 0x73, 0x20, 0x79,
    0xa5, 0xb0, 0x20, 0x7f, 0xfb, 0x48, 0x1f, 0xfc,
    0xd, 0xfe, 0xa0, 0x88, 0x40, 0x10, 0x64, 0xb0,
    0x6c, 0x5, 0xa1, 0xed, 0x88, 0x8c, 0x81, 0x3a,
    0xf0, 0x6, 0x20, 0x1c, 0xb0, 0xea, 0x30, 0x3e,
    0x39, 0x51, 0x0, 0xcd, 0x2c, 0x40, 0x40, 0x8,
    0x4b, 0x73, 0x35, 0xd0, 0x1f, 0xfd, 0x96, 0x80,

    /* U+27 "'" */
    0x1, 0xd0, 0x40, 0x55, 0x20, 0xd4, 0x0,

    /* U+28 "(" */
    0x2, 0x44, 0xa, 0xb0, 0x8, 0x20, 0x19, 0x81,
    0x2c, 0x4, 0x50, 0x2, 0x7, 0xf3, 0x3, 0x30,
    0x3f, 0x88, 0x20, 0x45, 0x81, 0x2c, 0x5, 0x98,
    0x12, 0x4, 0x9, 0xa0, 0x28, 0xc0,

    /* U+29 ")" */
    0xc, 0xd, 0x10, 0x13, 0xe0, 0x55, 0x1, 0x10,
    0x40, 0x92, 0x1, 0x1e, 0x7, 0xee, 0x40, 0xb9,
    0x3, 0xf2, 0xe0, 0x45, 0x0, 0x20, 0x80, 0xa8,
    0x9, 0x70, 0x4, 0x90, 0x3, 0xc0, 0x80,

    /* U+2A "*" */
    0x3, 0x52, 0x6, 0x60, 0x13, 0xc, 0x4, 0xc4,
    0x4, 0x84, 0x31, 0xe2, 0x3a, 0x81, 0xc4, 0x1a,
    0x80, 0x8a, 0x0, 0x80, 0xd5, 0x0, 0xc0, 0x55,
    0x51, 0x7, 0x21, 0xd4, 0xe, 0x20, 0x32, 0x3,
    0x62,

    /* U+2B "+" */
    0x3, 0xba, 0x3, 0xff, 0xb2, 0xbf, 0xc2, 0xff,
    0x45, 0x6d, 0x3, 0xb7, 0x4, 0xa6, 0x32, 0x51,
    0x3, 0xff, 0x9e, 0x98, 0x18,

    /* U+2C "," */
    0x1, 0xd0, 0x40, 0x55, 0x20, 0xd4, 0x0,

    /* U+2D "-" */
    0x5f, 0xff, 0x45, 0x6f, 0xf8,

    /* U+2E "." */
    0x38, 0x32, 0x0,

    /* U+2F "/" */
    0x3, 0xff, 0x83, 0x78, 0x1f, 0xfc, 0x18, 0xb0,
    0x1f, 0xfc, 0x6, 0xd0, 0x81, 0xff, 0x18, 0xd8,
    0xf, 0xfe, 0x6, 0x50, 0xf, 0xfe, 0x6, 0x34,
    0xf, 0xfe, 0x4, 0x3c, 0xf, 0xfe, 0x3, 0x6a,
    0x40, 0xff, 0x8c, 0x68, 0xf, 0xfe, 0x6, 0x50,
    0xf, 0xfe, 0x6, 0x34, 0xf, 0xfe, 0x5, 0x3c,
    0xf, 0xfe, 0x3, 0x54, 0x81, 0xff, 0x29, 0x8,
    0xf, 0xf8, 0xd4, 0xc0, 0xff, 0xe0, 0x73, 0x40,
    0xff, 0xe0, 0x0,

    /* U+30 "0" */
    0xb, 0x7f, 0xfd, 0x0, 0x57, 0x6f, 0xe8, 0xd9,
    0xa, 0x97, 0xe6, 0xb0, 0x1f, 0xff, 0xf0, 0x3f,
    0xff, 0xe0, 0x64, 0x2a, 0x5f, 0x9a, 0xc2, 0xbb,
    0x7f, 0x46, 0xc0,

    /* U+31 "1" */
    0x2, 0x54, 0x9, 0xd4, 0x1, 0xc2, 0x8, 0x8e,
    0x1, 0x78, 0x7, 0xff, 0xfc,

    /* U+32 "2" */
    0x5f, 0xff, 0xd4, 0x29, 0x3f, 0xe4, 0xc3, 0x7f,
    0xf1, 0xe0, 0x7f, 0xfa, 0x8f, 0x13, 0xff, 0xf0,
    0x45, 0xcb, 0x7f, 0xb9, 0x61, 0xa5, 0xfc, 0x7,
    0xff, 0xb9, 0x88, 0x97, 0xf8, 0x93, 0x6b, 0x7f,
    0xd0,

    /* U+33 "3" */
    0x5f, 0xff, 0xd4, 0x29, 0x3f, 0xe4, 0xc3, 0x7f,
    0xf0, 0xe0, 0x7f, 0xfa, 0xf8, 0x7, 0xff, 0xf0,
    0x40, 0x1d, 0xbf, 0xc1, 0x1, 0x25, 0xfc, 0x38,
    0x1f, 0xfe, 0xd4, 0xbf, 0xcc, 0x75, 0x6f, 0xfa,
    0x26,

    /* U+34 "4" */
    0x5c, 0xf, 0xf6, 0x80, 0x7f, 0xff, 0x8, 0x1f,
    0xc4, 0x23, 0xbf, 0xfe, 0x2, 0xd9, 0x6f, 0xf0,
    0x18, 0xa5, 0xfc, 0x40, 0xff, 0xff, 0x81, 0xfe,

    /* U+35 "5" */
    0x13, 0xff, 0xfa, 0x97, 0x2d, 0xff, 0x46, 0x1a,
    0x5f, 0xe2, 0x7, 0xff, 0x81, 0x1, 0xff, 0x23,
    0x7f, 0xfe, 0xc0, 0x36, 0x5b, 0xfc, 0x58, 0x2,
    0x97, 0xf1, 0xe0, 0x7f, 0xfb, 0x52, 0xff, 0x33,
    0xd5, 0xbf, 0xe8, 0x98,

    /* U+36 "6" */
    0x13, 0xff, 0xfa, 0x96, 0xd6, 0xff, 0xa3, 0x11,
    0x2f, 0xf1, 0x3, 0xff, 0xc0, 0x40, 0xff, 0xe0,
    0xef, 0xff, 0xb0, 0x14, 0xb7, 0xf8, 0xb0, 0xd,
    0x2f, 0xe3, 0xc0, 0xff, 0xf6, 0x31, 0x12, 0xfc,
    0xcf, 0x2d, 0xad, 0xfd, 0x13,

    /* U+37 "7" */
    0x5f, 0xff, 0xf0, 0x30, 0x56, 0xff, 0xc0, 0x64,
    0xbf, 0xc4, 0x70, 0x3f, 0xe3, 0x19, 0x3, 0xfe,
    0x86, 0x1, 0xff, 0x35, 0x0, 0xff, 0xe0, 0x46,
    0x80, 0xff, 0xa8, 0x80, 0x7f, 0xc9, 0x28, 0x1f,
    0xfc, 0xa, 0x90, 0xf, 0xfa, 0xa, 0x7, 0xfc,
    0x9c, 0x3, 0xff, 0x81, 0x13, 0x3, 0xfe, 0x86,
    0x1, 0xff, 0x17, 0x8, 0x1f, 0xf7, 0x2c, 0xf,
    0xe0,

    /* U+38 "8" */
    0x13, 0xff, 0xf5, 0x5, 0xb5, 0xbf, 0xa2, 0x6c,
    0x22, 0x5f, 0x99, 0xe0, 0x7f, 0xf8, 0x8, 0x1f,
    0xc7, 0xa1, 0xbf, 0xfe, 0x8, 0x89, 0x6f, 0xf0,
    0x4c, 0x34, 0xbf, 0x8f, 0x3, 0xff, 0xd8, 0xc4,
    0x4b, 0xf3, 0x3c, 0xb6, 0xb7, 0xf4, 0x4c,

    /* U+39 "9" */
    0xf, 0xff, 0xf6, 0x1, 0x1d, 0xbf, 0xa1, 0x84,
    0x54, 0xbf, 0x34, 0x40, 0xff, 0xf0, 0x30, 0x3f,
    0x98, 0x2a, 0xff, 0xfc, 0x40, 0x5d, 0x6f, 0xf1,
    0x2, 0x29, 0x7f, 0x30, 0x3f, 0xfd, 0xc9, 0x7f,
    0x9a, 0x2a, 0xdf, 0xf4, 0x30,

    /* U+3A ":" */
    0x5c, 0x32, 0xe, 0x3, 0xff, 0x82, 0x70, 0x64,
    0x0,

    /* U+3B ";" */
    0x1, 0x50, 0x4, 0x40, 0x76, 0x7, 0xff, 0x2b,
    0xa0, 0x80, 0xaa, 0x41, 0xa8,

    /* U+3C "<" */
    0x3, 0xff, 0x81, 0x60, 0x1a, 0x2c, 0x5, 0x1d,
    0x40, 0x23, 0xa8, 0x4, 0x71, 0x1, 0x30, 0x80,
    0xdc, 0xe2, 0x6, 0xc7, 0x20, 0x36, 0x55, 0x81,
    0xab, 0x0,

    /* U+3D "=" */
    0x3, 0xfe, 0x5f, 0xff, 0x45, 0x6f, 0xf8, 0x25,
    0xfc, 0x57, 0xff, 0xd1, 0x5b, 0xfe,

    /* U+3E ">" */
    0x3, 0xf6, 0x40, 0x73, 0xa8, 0xd, 0x55, 0x20,
    0x6a, 0xb1, 0x3, 0x55, 0x88, 0x1b, 0xa, 0x5,
    0x21, 0xc0, 0x11, 0xc8, 0x1, 0x5d, 0x60, 0x6a,
    0x80, 0x80,

    /* U+3F "?" */
    0x3, 0xff, 0x84, 0xbf, 0xff, 0xb0, 0x56, 0xff,
    0xc5, 0x84, 0xbf, 0xe1, 0xc0, 0xff, 0xf5, 0xf0,
    0x3d, 0x3f, 0xe0, 0x80, 0xfd, 0x6e, 0xe4, 0xf,
    0x8a, 0x58, 0xf, 0xfe, 0xc7, 0x60, 0x7f, 0xf0,
    0x1d, 0x3, 0xff, 0x80, 0xe8, 0x1f, 0xfc, 0xe,
    0xc0, 0xf0,

    /* U+40 "@" */
    0x3, 0xff, 0x86, 0xbf, 0xff, 0x50, 0x15, 0xdb,
    0xfa, 0x26, 0x84, 0x4b, 0xf3, 0x58, 0xf, 0xfe,
    0x2c, 0xff, 0xcc, 0xf, 0x5b, 0xe7, 0x0, 0xf1,
    0x4b, 0xb0, 0x1f, 0x2f, 0xfa, 0x81, 0xf7, 0x32,
    0x66, 0x7, 0xe6, 0xfb, 0x1, 0xf9, 0x81, 0x88,
    0x1f, 0x63, 0xfe, 0x83, 0xac, 0x4, 0xf5, 0xbf,
    0x66, 0x6, 0x29, 0x7c, 0xd3, 0xc, 0xf, 0xc6,
    0xe2, 0xe7, 0xff, 0xb1, 0x82, 0x7b, 0x7f, 0x6c,
    0x0,

    /* U+41 "A" */
    0xb, 0x7f, 0xfd, 0x0, 0x57, 0x6f, 0xe8, 0xd9,
    0xa, 0x97, 0xe6, 0xb0, 0x1f, 0xfe, 0xef, 0xff,
    0xc8, 0x9, 0xbf, 0xf0, 0x1a, 0x4f, 0xf2, 0x3,
    0xff, 0xfe, 0x7, 0xf8,

    /* U+42 "B" */
    0x17, 0xff, 0xd8, 0x80, 0x2a, 0x5b, 0xf1, 0xa0,
    0x18, 0x69, 0x7c, 0x48, 0xf, 0xff, 0x11, 0x3,
    0xff, 0x83, 0xbf, 0xf8, 0xac, 0x40, 0x4b, 0x7f,
    0x8d, 0x0, 0xd2, 0xff, 0x10, 0x3f, 0xfd, 0x8c,
    0x44, 0xbf, 0xc4, 0x9b, 0x5b, 0xfc, 0x60,

    /* U+43 "C" */
    0xb, 0x7f, 0xfd, 0x0, 0x57, 0x6f, 0xe8, 0xd9,
    0xa, 0x97, 0xe6, 0xb0, 0x1f, 0xfc, 0x12, 0x7,
    0xff, 0x2, 0xc0, 0x3f, 0xff, 0xe0, 0x7f, 0xf5,
    0xed, 0x1, 0xff, 0xc2, 0x42, 0xa5, 0xf9, 0xac,
    0x2b, 0xb7, 0xf4, 0x6c,

    /* U+44 "D" */
    0xb, 0x7f, 0xf5, 0x20, 0x54, 0x49, 0xe6, 0xb2,
    0x8, 0x76, 0xfd, 0x39, 0xa0, 0x44, 0xf, 0xdc,
    0x30, 0x3f, 0xe2, 0x58, 0xf, 0xfe, 0x3, 0x3,
    0xff, 0xfe, 0x7, 0xe6, 0x7, 0xff, 0x0, 0x96,
    0x3, 0xfe, 0xe1, 0x80, 0x6f, 0xe9, 0xcd, 0x2,
    0x93, 0xe6, 0xb2, 0x0,

    /* U+45 "E" */
    0xb, 0x7f, 0xfa, 0x8a, 0xed, 0xfd, 0xd0, 0xa9,
    0x7f, 0x1, 0xff, 0xda, 0x20, 0x7f, 0xdb, 0xfe,
    0xa0, 0x75, 0xbe, 0x80, 0x72, 0x5e, 0x20, 0x7f,
    0xf9, 0xd0, 0xa9, 0x7f, 0xa, 0xed, 0xfd, 0xc0,

    /* U+46 "F" */
    0xb, 0x7f, 0xfa, 0x8a, 0xe4, 0xfd, 0xd0, 0xad,
    0xfe, 0x40, 0x7f, 0xf6, 0x88, 0x1f, 0xf6, 0xff,
    0xa8, 0x1d, 0x6f, 0xa0, 0x1c, 0x97, 0x88, 0x1f,
    0xff, 0xf0,

    /* U+47 "G" */
    0xb, 0x7f, 0xfd, 0x0, 0x57, 0x6f, 0xe8, 0xd9,
    0xa, 0x97, 0xe6, 0xb0, 0x1f, 0xfc, 0xd, 0x80,
    0x7f, 0xf0, 0x1b, 0x1, 0xff, 0xd6, 0x7f, 0xfa,
    0x81, 0xe7, 0x6f, 0x10, 0x3f, 0x25, 0xcc, 0xf,
    0xff, 0x61, 0x42, 0xa5, 0xf9, 0xac, 0x2b, 0xb7,
    0xf4, 0x68,

    /* U+48 "H" */
    0x5c, 0xf, 0xf5, 0xa0, 0x3f, 0xff, 0x84, 0xf,
    0xfe, 0xe, 0xff, 0xf9, 0x1, 0x5b, 0xfe, 0x20,
    0x49, 0x7f, 0x98, 0x1f, 0xff, 0xf0, 0x3f, 0xc0,

    /* U+49 "I" */
    0x5c, 0xf, 0xff, 0x10,

    /* U+4A "J" */
    0x3, 0xff, 0x81, 0x68, 0xf, 0xff, 0xf8, 0x1f,
    0xff, 0x55, 0xc0, 0xff, 0xee, 0x21, 0x52, 0xfc,
    0xd6, 0x15, 0xdb, 0xfa, 0x36,

    /* U+4B "K" */
    0x5c, 0xf, 0xd7, 0xa0, 0x3f, 0xaa, 0xc8, 0xf,
    0xd9, 0x62, 0x7, 0xc7, 0xe, 0x40, 0xf8, 0xe1,
    0x80, 0xfc, 0xb2, 0xc0, 0x7e, 0x55, 0xd0, 0x3f,
    0x4a, 0xe0, 0x1f, 0xcc, 0x3f, 0xfe, 0x20, 0x1c,
    0x9f, 0x8c, 0x1, 0x1b, 0xfc, 0x98, 0x1f, 0xff,
    0x50,

    /* U+4C "L" */
    0x5c, 0xf, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x3f,
    0xf8, 0x88, 0x54, 0xbf, 0x0, 0xae, 0xdf, 0xc8,

    /* U+4D "M" */
    0x3, 0xff, 0x89, 0x90, 0x1f, 0xca, 0x84, 0x69,
    0x3, 0xe3, 0x51, 0x2, 0xe0, 0x7d, 0xc0, 0xea,
    0x60, 0x1d, 0x4d, 0x3, 0x28, 0xd0, 0x13, 0x55,
    0x81, 0xcd, 0x52, 0xc, 0x68, 0xf, 0xa9, 0xe3,
    0x98, 0x7, 0xf7, 0x3c, 0xe0, 0x3f, 0xc6, 0x8a,
    0x7, 0xff, 0x1, 0x74, 0x7, 0xff, 0xfc, 0xf,
    0xfe, 0xc0,

    /* U+4E "N" */
    0x10, 0x81, 0xff, 0xc0, 0x4f, 0x81, 0xfc, 0x79,
    0x0, 0x60, 0x1f, 0xfc, 0x16, 0xc8, 0xf, 0xfe,
    0x4, 0x54, 0x81, 0xff, 0xc0, 0xa3, 0x81, 0xff,
    0xc1, 0xc6, 0x1, 0xff, 0xc0, 0x31, 0xa0, 0x3f,
    0xf8, 0xd, 0x50, 0x3f, 0xf8, 0x34, 0xd0, 0x3f,
    0xf8, 0x3d, 0x30, 0x3f, 0xf8, 0x6, 0xc2, 0x7,
    0xff, 0x1, 0x3c, 0x7, 0xff, 0x6, 0x2a, 0x7,
    0xff, 0x6, 0xa8, 0x7, 0xff, 0x7, 0x20, 0x3f,
    0xf8, 0x26, 0x2,

    /* U+4F "O" */
    0xb, 0x7f, 0xfd, 0x0, 0x57, 0x6f, 0xe8, 0xd9,
    0xa, 0x97, 0xe6, 0xb0, 0x1f, 0xff, 0xf0, 0x3f,
    0xff, 0xe0, 0x64, 0x2a, 0x5f, 0x9a, 0xc2, 0xbb,
    0x7f, 0x46, 0xc0,

    /* U+50 "P" */
    0x13, 0xff, 0xf5, 0x5, 0xb4, 0x9f, 0xc9, 0xb0,
    0x8d, 0xff, 0x1e, 0x7, 0xff, 0x80, 0x81, 0xfc,
    0x78, 0xd, 0xff, 0xf0, 0x40, 0x2d, 0xff, 0x72,
    0x1, 0x2f, 0xf0, 0x1f, 0xff, 0xf0, 0x3f, 0xe0,

    /* U+51 "Q" */
    0xb, 0x7f, 0xfd, 0x0, 0x52, 0xad, 0xfa, 0x36,
    0x41, 0xa5, 0xf9, 0xac, 0x2, 0x18, 0x7, 0xff,
    0x2, 0x30, 0x3f, 0xf8, 0x8, 0xd0, 0x3f, 0xf8,
    0x1d, 0x10, 0x3f, 0xe2, 0xb8, 0x1f, 0xfc, 0x8,
    0x50, 0x1f, 0xf1, 0x70, 0xf, 0xfe, 0x4, 0x2c,
    0xf, 0xfe, 0x4, 0x80, 0x3f, 0xf8, 0xc, 0x40,
    0x3f, 0xf8, 0x11, 0x90, 0x24, 0x2a, 0x5c, 0x47,
    0x6b, 0xa, 0xed, 0xf0, 0x31, 0xb0, 0x5b, 0xff,
    0x89, 0x40, 0x3f, 0xee, 0x50, 0x1f, 0xf1, 0xcc,
    0x0,

    /* U+52 "R" */
    0x13, 0xff, 0xf5, 0x0, 0x5b, 0x5b, 0xfa, 0x26,
    0x18, 0x89, 0x7e, 0x67, 0x81, 0xff, 0xe4, 0x20,
    0x7f, 0x1e, 0x5, 0xbf, 0xfe, 0x8, 0xa, 0x90,
    0xed, 0xf7, 0x20, 0x4b, 0x36, 0xc9, 0x70, 0x1f,
    0x48, 0x78, 0xf, 0xfe, 0x3, 0xc7, 0x90, 0x3f,
    0xe3, 0xd6, 0x60, 0x7f, 0xf0, 0x2b, 0x90, 0x7,
    0xff, 0x2, 0x57, 0x80, 0xff, 0xe0, 0x2c, 0x3a,

    /* U+53 "S" */
    0x13, 0xff, 0xf5, 0x5, 0xb5, 0xbf, 0xa2, 0x6c,
    0x22, 0x5f, 0x99, 0x3, 0xff, 0x83, 0x98, 0x1f,
    0xfd, 0x22, 0x7, 0xfc, 0x8e, 0xff, 0xfb, 0x0,
    0xd9, 0x6f, 0xf1, 0x60, 0xa, 0x5f, 0xc7, 0x81,
    0xff, 0xd1, 0x20, 0x7f, 0xf0, 0x16, 0x3, 0xff,
    0x80, 0xc4, 0x4b, 0xf3, 0x3c, 0xb6, 0xb7, 0xf4,
    0x4c,

    /* U+54 "T" */
    0x5f, 0xff, 0xf0, 0x2a, 0x93, 0x90, 0x93, 0xc1,
    0xbe, 0x21, 0xbe, 0x20, 0x7f, 0xff, 0xc0, 0xff,
    0xff, 0x81, 0xff, 0xd4,

    /* U+55 "U" */
    0x5c, 0xf, 0xf5, 0xa0, 0x3f, 0xff, 0xe0, 0x7f,
    0xff, 0xc0, 0xff, 0xeb, 0x37, 0xfe, 0x58, 0x4,
    0x9f, 0xe6, 0xc0,

    /* U+56 "V" */
    0x1d, 0x81, 0xfe, 0x7c, 0x2, 0xa0, 0x7f, 0xa3,
    0x1, 0xc9, 0x1, 0xf9, 0xa, 0x1, 0x18, 0x7,
    0xee, 0x48, 0xa, 0xa0, 0x3e, 0x2a, 0x1, 0x98,
    0x60, 0x7a, 0x84, 0x7, 0x38, 0x7, 0x9b, 0x1,
    0xea, 0x10, 0x19, 0x88, 0x7, 0x89, 0x70, 0x35,
    0x40, 0x7e, 0x88, 0x80, 0x25, 0xc0, 0xfc, 0x85,
    0x1, 0xa, 0x3, 0xf9, 0xb0, 0x5, 0x40, 0xff,
    0x40, 0xd8, 0x30, 0x3f, 0xe5, 0x69, 0x1, 0xff,
    0xc0, 0xe4, 0xa0, 0x1f, 0xfc, 0x4, 0x51, 0x3,
    0xc0,

    /* U+57 "W" */
    0x3c, 0x81, 0xff, 0x3e, 0x8, 0x60, 0x7f, 0xdc,
    0x81, 0x10, 0x3f, 0xe2, 0xc0, 0x1c, 0x7, 0x20,
    0x39, 0x90, 0xd, 0x80, 0xd6, 0x90, 0x18, 0xf0,
    0x18, 0x81, 0x90, 0xe0, 0x71, 0x0, 0x40, 0xcc,
    0x44, 0x40, 0x10, 0xc0, 0x30, 0x40, 0x54, 0xc5,
    0x0, 0xc1, 0x0, 0x42, 0x4, 0xb8, 0x6c, 0x0,
    0x81, 0xee, 0x28, 0x42, 0x86, 0x39, 0x3, 0x12,
    0xd, 0x80, 0x15, 0x41, 0x60, 0x64, 0xd0, 0xa0,
    0x5c, 0x93, 0x20, 0x6e, 0x61, 0x20, 0x24, 0x61,
    0xe0, 0x62, 0xd, 0x3, 0xaa, 0x4, 0xc, 0xc0,
    0x30, 0x39, 0x80, 0x60, 0x62, 0x10, 0x1f, 0x40,
    0x40, 0xef, 0x40, 0xf9, 0xf0, 0x20,

    /* U+58 "X" */
    0x1b, 0x0, 0xff, 0x5e, 0x3, 0x9a, 0x7, 0xe8,
    0xb0, 0x3, 0x53, 0x3, 0xc9, 0xc2, 0x4, 0xa4,
    0x20, 0x31, 0xad, 0x80, 0xe6, 0xa9, 0x2, 0xc6,
    0x1, 0xf5, 0x3c, 0x6, 0x18, 0xf, 0xec, 0x6c,
    0x38, 0xf, 0xfb, 0x27, 0x48, 0x1f, 0xfc, 0xfc,
    0x9d, 0x20, 0x7f, 0xb1, 0xb0, 0xe0, 0x3f, 0xa9,
    0xe0, 0x30, 0xc0, 0x7c, 0xd5, 0x20, 0x58, 0xc0,
    0x39, 0x48, 0x40, 0x63, 0x5b, 0x1, 0x1a, 0x98,
    0x1e, 0x4e, 0x10, 0x1c, 0xd0, 0x3f, 0x45, 0x80,

    /* U+59 "Y" */
    0x3c, 0x81, 0xfd, 0x68, 0xf, 0xff, 0xe2, 0x3,
    0xf9, 0x82, 0xa7, 0xff, 0xc4, 0x5, 0xd6, 0xff,
    0x10, 0x22, 0x97, 0xf3, 0x3, 0xff, 0xc0, 0xc0,
    0xff, 0xe0, 0xca, 0x97, 0xe6, 0xb0, 0xae, 0xdf,
    0xd1, 0xb0,

    /* U+5A "Z" */
    0x3f, 0xff, 0xf0, 0x30, 0x36, 0xff, 0xa0, 0x20,
    0x12, 0xff, 0x61, 0x80, 0xff, 0xa2, 0xa4, 0xf,
    0xf3, 0x68, 0x80, 0xff, 0x28, 0xd8, 0xf, 0xf1,
    0xaa, 0x1, 0xff, 0x65, 0x40, 0xff, 0xb1, 0xa0,
    0x7f, 0xd4, 0xe0, 0x3f, 0xea, 0xb0, 0x1f, 0xf4,
    0x54, 0x81, 0xfe, 0x6d, 0x10, 0x1f, 0xe5, 0x1b,
    0x1, 0xff, 0x53, 0x92, 0xff, 0x2, 0xd, 0xbf,
    0xe8, 0x0,

    /* U+5B "[" */
    0xb, 0x7f, 0xb0, 0xae, 0xdd, 0x10, 0xa9, 0x70,
    0x1f, 0xff, 0xf0, 0x3f, 0xf8, 0x28, 0x54, 0xb1,
    0x15, 0xdb, 0xa0,

    /* U+5C "\\" */
    0x1b, 0x0, 0xff, 0xe1, 0x73, 0x40, 0xff, 0xe0,
    0x9a, 0x98, 0x1f, 0xfc, 0x15, 0x21, 0x1, 0xff,
    0xc1, 0x6a, 0x90, 0x3f, 0xf8, 0x34, 0xf0, 0x3f,
    0xf8, 0x58, 0xd0, 0x3f, 0xf8, 0x59, 0x40, 0x3f,
    0xf8, 0x26, 0x34, 0x7, 0xff, 0x5, 0xb5, 0x20,
    0x7f, 0xf0, 0x61, 0xc0, 0x7f, 0xf0, 0xb0, 0xa0,
    0x7f, 0xf0, 0xb2, 0x80, 0x7f, 0xf0, 0x4d, 0x6c,
    0x7, 0xff, 0x5, 0x38, 0x40, 0xff, 0xe0, 0xc5,
    0x80,

    /* U+5D "]" */
    0x5f, 0xf4, 0xa, 0xdd, 0x1c, 0x9, 0x66, 0x88,
    0x1f, 0x30, 0x3f, 0xff, 0xe0, 0x66, 0x12, 0xcd,
    0x12, 0xb7, 0x45, 0x0,

    /* U+5E "^" */
    0x3, 0x52, 0x7, 0xb2, 0xc8, 0xd, 0x8e, 0x55,
    0x0, 0xc7, 0x1a, 0xaa, 0xb, 0x10, 0x15, 0xa0,

    /* U+5F "_" */
    0x3, 0xff, 0x86, 0xbf, 0xff, 0xe1, 0x2b, 0x7f,
    0xf8, 0x40,

    /* U+60 "`" */
    0x3, 0xbb, 0x0, 0xe1, 0x12, 0x8,

    /* U+61 "a" */
    0x3f, 0xff, 0x23, 0x6f, 0xe3, 0xc2, 0x5f, 0x9b,
    0xd, 0xff, 0xc8, 0x23, 0x6f, 0xc8, 0x9, 0x2f,
    0x88, 0x13, 0x4b, 0xe0, 0x8c, 0xb7, 0xc6, 0x0,

    /* U+62 "b" */
    0x5c, 0xf, 0xff, 0xf8, 0x1f, 0xfc, 0x1f, 0xff,
    0x10, 0x12, 0xdf, 0x18, 0x1, 0xa5, 0xf3, 0x3,
    0xff, 0xae, 0xd2, 0xf9, 0xa4, 0x96, 0xf8, 0xc0,

    /* U+63 "c" */
    0x17, 0xff, 0xc5, 0x24, 0xb7, 0xc6, 0x0, 0x69,
    0x7a, 0x90, 0x3f, 0x9c, 0x3, 0xff, 0x90, 0xe3,
    0xd, 0x2f, 0x52, 0x4a, 0x5b, 0xe3, 0x0,

    /* U+64 "d" */
    0x3, 0xfa, 0x60, 0x3f, 0xff, 0xe0, 0x57, 0xff,
    0x98, 0x49, 0x2d, 0xf1, 0x2, 0x69, 0x7c, 0x7,
    0xff, 0x61, 0xa5, 0xf3, 0x49, 0x2d, 0xf1, 0x80,

    /* U+65 "e" */
    0x17, 0xff, 0xc9, 0x52, 0xdf, 0x18, 0x1, 0x2f,
    0x98, 0x16, 0xff, 0xc8, 0xa, 0xdf, 0xdc, 0x2,
    0x5f, 0x30, 0x26, 0x97, 0xbc, 0x49, 0x2d, 0xf1,
    0x80,

    /* U+66 "f" */
    0x1, 0x3f, 0xc0, 0x1b, 0x5b, 0x2, 0x32, 0x50,
    0x1f, 0xfc, 0x27, 0x85, 0xfe, 0x74, 0x4b, 0x60,
    0x81, 0x4a, 0x3, 0xff, 0xd8,

    /* U+67 "g" */
    0x17, 0xff, 0xc5, 0x24, 0xb7, 0xc6, 0x0, 0x69,
    0x7c, 0xc0, 0xff, 0xeb, 0xb4, 0xbe, 0x9, 0x25,
    0xbe, 0x20, 0x2f, 0xff, 0x30, 0x3f, 0xf9, 0xa9,
    0x7e, 0x60, 0x2d, 0xf8, 0xc0,

    /* U+68 "h" */
    0x5c, 0xf, 0xff, 0xf8, 0x1f, 0xfc, 0x1f, 0xff,
    0x10, 0x12, 0xdf, 0x18, 0x1, 0xa5, 0xf3, 0x3,
    0xff, 0xd6,

    /* U+69 "i" */
    0x5c, 0xc, 0xb8, 0x19, 0x70, 0x3f, 0xf9, 0x40,

    /* U+6A "j" */
    0x1, 0x98, 0x2, 0x5, 0xd8, 0x1e, 0xec, 0xf,
    0xfe, 0xe1, 0x60, 0x2c, 0x2c,

    /* U+6B "k" */
    0x5c, 0xf, 0xff, 0xf9, 0xdc, 0x81, 0x96, 0x1c,
    0x81, 0x3a, 0xb0, 0x1a, 0xc7, 0x40, 0xe4, 0x1f,
    0xf1, 0x1, 0x6f, 0x18, 0x1, 0x2e, 0x60, 0x7f,
    0x80,

    /* U+6C "l" */
    0x5c, 0xf, 0xff, 0x10,

    /* U+6D "m" */
    0x17, 0xfe, 0xdf, 0xf9, 0x82, 0xa5, 0xb8, 0xab,
    0x72, 0x81, 0x86, 0x96, 0x24, 0x96, 0xe0, 0x7f,
    0xff, 0xc0, 0xf8,

    /* U+6E "n" */
    0x17, 0xff, 0xc4, 0x94, 0xb7, 0xc6, 0x30, 0xd2,
    0xf9, 0x81, 0xff, 0xeb,

    /* U+6F "o" */
    0x17, 0xff, 0xc5, 0x24, 0xb7, 0xca, 0x0, 0x69,
    0x78, 0xb0, 0x3f, 0xfa, 0xed, 0x2f, 0x16, 0x92,
    0x5b, 0xe5, 0x0,

    /* U+70 "p" */
    0x17, 0xff, 0xc5, 0x24, 0xb7, 0xc6, 0x0, 0x69,
    0x7c, 0xc0, 0xff, 0xeb, 0xb4, 0xbe, 0x60, 0x25,
    0xbe, 0x30, 0x7, 0xff, 0x88, 0x1f, 0xfe, 0x0,

    /* U+71 "q" */
    0x17, 0xff, 0xc9, 0x52, 0xdf, 0x1e, 0x1, 0xa5,
    0xf3, 0x3, 0xff, 0xae, 0xd2, 0xf8, 0x24, 0x96,
    0xf8, 0x80, 0xbf, 0xfc, 0xc0, 0xff, 0xf0, 0x80,

    /* U+72 "r" */
    0x17, 0xff, 0x82, 0x49, 0x6f, 0x80, 0x9a, 0x5e,
    0x3, 0xff, 0xce,

    /* U+73 "s" */
    0x1b, 0xff, 0xd5, 0x19, 0x6f, 0xd0, 0x2, 0x5f,
    0x8b, 0x1b, 0xff, 0x99, 0xf6, 0xfc, 0xa8, 0x4,
    0xbe, 0x20, 0x1b, 0xfc, 0x81, 0x93, 0xf1, 0x80,

    /* U+74 "t" */
    0x2, 0xec, 0xf, 0xff, 0x5f, 0xf0, 0x9f, 0xa0,
    0xb, 0x60, 0xed, 0x80, 0x25, 0xe, 0x90, 0x81,
    0xff, 0xec, 0xc9, 0x60, 0x35, 0x56, 0xe0,

    /* U+75 "u" */
    0x5c, 0xf, 0xa6, 0x3, 0xff, 0xd6, 0xc2, 0x5f,
    0x10, 0x55, 0xbf, 0x20,

    /* U+76 "v" */
    0x5c, 0xf, 0xad, 0x1, 0xfc, 0xc8, 0x1f, 0x89,
    0x60, 0x3f, 0x40, 0x80, 0xf9, 0xaa, 0x7, 0xd2,
    0x48, 0x9, 0xb4, 0xee, 0xb0, 0x9, 0x24, 0x27,
    0xd0, 0x10,

    /* U+77 "w" */
    0x5c, 0xe, 0xd0, 0xc, 0xb8, 0x1f, 0xff, 0xf0,
    0x3c, 0xc3, 0x4b, 0x12, 0x4b, 0x70, 0x5, 0x4b,
    0x71, 0x56, 0xe5, 0x0,

    /* U+78 "x" */
    0x17, 0x90, 0x35, 0xe8, 0x55, 0x88, 0xa, 0xaa,
    0x1, 0x56, 0x55, 0x54, 0x6, 0xaa, 0xaa, 0x80,
    0xff, 0xe2, 0x55, 0x55, 0x40, 0x6a, 0xb2, 0xaa,
    0xa0, 0x15, 0x62, 0x2, 0xaa, 0x80,

    /* U+79 "y" */
    0x5c, 0xf, 0xa6, 0x3, 0xff, 0xd6, 0xc3, 0x4b,
    0xe0, 0x54, 0xb7, 0xc4, 0x5, 0xff, 0xe6, 0x7,
    0xff, 0x35, 0x2f, 0xcc, 0x5, 0xbf, 0x18,

    /* U+7A "z" */
    0x3f, 0xff, 0x8b, 0x12, 0xdd, 0x8, 0x24, 0xbb,
    0x4a, 0x74, 0xbc, 0x7, 0xa6, 0x1a, 0x1, 0xc7,
    0x36, 0xc4, 0xe, 0x78, 0x58, 0x7, 0xb4, 0x14,
    0x25, 0xbb, 0x24, 0x2, 0x5b, 0xc0, 0x80,

    /* U+7B "{" */
    0x0, 0x77, 0xe0, 0x19, 0xda, 0x4, 0x44, 0x81,
    0x82, 0x7, 0xff, 0x39, 0x50, 0x32, 0x60, 0x76,
    0x3, 0xff, 0x8e, 0x40, 0x90, 0x89, 0x0, 0x57,
    0x68,

    /* U+7C "|" */
    0x5c, 0xf, 0xff, 0x43, 0x60,

    /* U+7D "}" */
    0x3f, 0xa8, 0x3, 0x63, 0xa0, 0x13, 0xa8, 0xf,
    0xfe, 0xc7, 0x3, 0xa8, 0x1c, 0x80, 0xff, 0xe3,
    0xa7, 0x48, 0x56, 0xe, 0x0,

    /* U+7E "~" */
    0x5, 0xb9, 0x1, 0xfb, 0x49, 0x5f, 0x5b, 0x5a,
    0x1, 0xbf, 0xb3, 0x69, 0x3, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 91, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 45, .box_w = 3, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 9, .adv_w = 109, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 11},
    {.bitmap_index = 20, .adv_w = 207, .box_w = 13, .box_h = 10, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 60, .adv_w = 191, .box_w = 12, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 113, .adv_w = 262, .box_w = 16, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 197, .adv_w = 212, .box_w = 13, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 253, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 11},
    {.bitmap_index = 260, .adv_w = 70, .box_w = 5, .box_h = 18, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 290, .adv_w = 70, .box_w = 5, .box_h = 18, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 321, .adv_w = 149, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 354, .adv_w = 180, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 375, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 382, .adv_w = 180, .box_w = 11, .box_h = 2, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 387, .adv_w = 45, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 390, .adv_w = 258, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 457, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 484, .adv_w = 94, .box_w = 6, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 497, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 228, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 563, .adv_w = 228, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 660, .adv_w = 236, .box_w = 15, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 717, .adv_w = 228, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 756, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 793, .adv_w = 45, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 802, .adv_w = 67, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 815, .adv_w = 122, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 841, .adv_w = 180, .box_w = 11, .box_h = 6, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 855, .adv_w = 122, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 881, .adv_w = 228, .box_w = 14, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 923, .adv_w = 230, .box_w = 14, .box_h = 18, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 988, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1016, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1055, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1091, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1135, .adv_w = 199, .box_w = 12, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1167, .adv_w = 156, .box_w = 12, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1193, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1235, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1259, .adv_w = 45, .box_w = 3, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1263, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1284, .adv_w = 217, .box_w = 13, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1325, .adv_w = 192, .box_w = 12, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1341, .adv_w = 238, .box_w = 15, .box_h = 18, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1391, .adv_w = 238, .box_w = 15, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1458, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1485, .adv_w = 228, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1517, .adv_w = 230, .box_w = 14, .box_h = 19, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1590, .adv_w = 249, .box_w = 15, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1646, .adv_w = 228, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1695, .adv_w = 171, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1715, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1734, .adv_w = 255, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1807, .adv_w = 268, .box_w = 17, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1901, .adv_w = 258, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1973, .adv_w = 230, .box_w = 14, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2007, .adv_w = 238, .box_w = 15, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2065, .adv_w = 136, .box_w = 8, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2084, .adv_w = 258, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2149, .adv_w = 136, .box_w = 8, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2169, .adv_w = 149, .box_w = 9, .box_h = 5, .ofs_x = 0, .ofs_y = 11},
    {.bitmap_index = 2185, .adv_w = 251, .box_w = 15, .box_h = 3, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 2195, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 10},
    {.bitmap_index = 2201, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2225, .adv_w = 184, .box_w = 11, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2249, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2272, .adv_w = 184, .box_w = 11, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2296, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2321, .adv_w = 80, .box_w = 7, .box_h = 16, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 2342, .adv_w = 184, .box_w = 11, .box_h = 13, .ofs_x = 0, .ofs_y = -5},
    {.bitmap_index = 2371, .adv_w = 184, .box_w = 11, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2389, .adv_w = 45, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2397, .adv_w = 51, .box_w = 4, .box_h = 17, .ofs_x = -1, .ofs_y = -5},
    {.bitmap_index = 2410, .adv_w = 151, .box_w = 9, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2435, .adv_w = 45, .box_w = 3, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2439, .adv_w = 252, .box_w = 16, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2458, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2470, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2489, .adv_w = 184, .box_w = 11, .box_h = 13, .ofs_x = 0, .ofs_y = -5},
    {.bitmap_index = 2513, .adv_w = 184, .box_w = 11, .box_h = 13, .ofs_x = 0, .ofs_y = -5},
    {.bitmap_index = 2537, .adv_w = 156, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2548, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2572, .adv_w = 140, .box_w = 10, .box_h = 16, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 2595, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2607, .adv_w = 184, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2633, .adv_w = 252, .box_w = 16, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2653, .adv_w = 185, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2683, .adv_w = 184, .box_w = 11, .box_h = 13, .ofs_x = 0, .ofs_y = -5},
    {.bitmap_index = 2706, .adv_w = 194, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2737, .adv_w = 107, .box_w = 6, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2762, .adv_w = 45, .box_w = 3, .box_h = 19, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 2767, .adv_w = 107, .box_w = 6, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2788, .adv_w = 204, .box_w = 13, .box_h = 3, .ofs_x = 0, .ofs_y = 6}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    35, 53,
    37, 53,
    39, 34,
    39, 35,
    39, 36,
    39, 37,
    39, 38,
    39, 39,
    39, 40,
    39, 41,
    39, 42,
    39, 44,
    39, 45,
    39, 46,
    39, 47,
    39, 48,
    39, 49,
    39, 50,
    39, 51,
    39, 52,
    39, 54,
    39, 55,
    39, 56,
    39, 57,
    39, 58,
    51, 53,
    53, 34,
    53, 35,
    53, 36,
    53, 37,
    53, 38,
    53, 39,
    53, 40,
    53, 41,
    53, 42,
    53, 44,
    53, 45,
    53, 46,
    53, 47,
    53, 48,
    53, 49,
    53, 50,
    53, 51,
    53, 52,
    53, 53,
    53, 54,
    53, 55,
    53, 56,
    53, 57,
    53, 58,
    53, 59,
    55, 39,
    69, 71,
    71, 67,
    71, 71,
    71, 73,
    71, 74,
    71, 75,
    71, 76,
    71, 77,
    71, 85,
    74, 71,
    74, 85,
    75, 71,
    75, 85,
    77, 71,
    85, 85
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -31, -21, 42, 42, 42, 42, 42, 42,
    42, 42, 42, 42, 42, 42, 42, 42,
    42, 42, 42, 42, 42, 42, 42, 42,
    42, -21, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 14, 60,
    60, 60, 60, -12, -5, 26, 7, 26,
    26, 17, 26, 26, 26, 5, 18, 5,
    18, -5, -23
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 67,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

/*Store all the custom data of the font*/
static lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = gylph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 3,
    .kern_classes = 0,
    .bitmap_format = 1
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
lv_font_t exninja_22 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 23,          /*The maximum line height required by the font*/
    .base_line = 5,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};

#endif /*#if EXNINJA_22*/

