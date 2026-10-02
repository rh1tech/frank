/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#ifndef HDMI_H_
#define HDMI_H_

#include "inttypes.h"
#include "stdbool.h"
#include "hardware/dma.h" // Added for DMA_IRQ_0

#define VIDEO_DMA_IRQ (DMA_IRQ_1)

#ifndef HDMI_BASE_PIN
#define HDMI_BASE_PIN (6)
#endif

#ifndef PIO_VIDEO
#define PIO_VIDEO pio0
#endif
#ifndef PIO_VIDEO_ADDR
#define PIO_VIDEO_ADDR pio0
#endif

/* Waveshare RP2350-PiZero (Z0) wires the HDMI connector data-lanes-first
 * with non-inverted diff pairs and BGR channel order; every other board
 * we support wires clock-first / inverted / RGB. Key off PLATFORM_Z0 so
 * the PIO HDMI driver picks up the right layout automatically. */
#ifdef PLATFORM_Z0
#  define HDMI_PIN_RGB_notBGR       (0)
#  define HDMI_PIN_invert_diffpairs (0)
#  ifndef beginHDMI_PIN_data
#  define beginHDMI_PIN_data (HDMI_BASE_PIN)
#  endif
#  ifndef beginHDMI_PIN_clk
#  define beginHDMI_PIN_clk  (HDMI_BASE_PIN + 6)
#  endif
#else
#  define HDMI_PIN_RGB_notBGR       (1)
#  define HDMI_PIN_invert_diffpairs (1)
#  ifndef beginHDMI_PIN_data
#  define beginHDMI_PIN_data (HDMI_BASE_PIN + 2)
#  endif
#  ifndef beginHDMI_PIN_clk
#  define beginHDMI_PIN_clk  (HDMI_BASE_PIN)
#  endif
#endif


typedef enum g_out{
    g_out_VGA,g_out_HDMI
}g_out;

/* Runtime VGA vs HDMI autodetection.
 * main.c probes the HDMI connector's clock-pair pins (GPIO12/13) with
 * testPins() before graphics_init() and sets SELECT_VGA accordingly.
 * When set, graphics_init() / graphics_set_palette() / graphics_set_bgcolor()
 * dispatch to the VGA driver in HDMI_vga.c instead of the HDMI TMDS path. */
extern bool SELECT_VGA;
int testPins(uint32_t pin0, uint32_t pin1);

/* HDMI-only entry points (still available when the VGA dispatcher is in
 * place; used internally and by main.c during HDMI-only paths). */
void graphics_init_hdmi(void);
void graphics_set_palette_hdmi(uint8_t i, uint32_t color888);
void graphics_set_bgcolor_hdmi(uint32_t color888);

typedef struct video_mode_t{
  int h_total;
  int h_width;
  int freq;
  int vgaPxClk;
} video_mode_t;

enum graphics_mode_t {
    TEXTMODE_DEFAULT,
    GRAPHICSMODE_DEFAULT,
    GRAPHICSMODE_RGB565,  // Direct 16-bit RGB565 mode for SNES
};

void graphics_init(g_out g_out);
void graphics_set_buffer(uint8_t *buffer);
uint8_t* graphics_get_buffer(void);
uint32_t graphics_get_width(void);
uint32_t graphics_get_height(void);
void graphics_set_res(int w, int h);
void graphics_set_shift(int x, int y);
void graphics_set_palette(uint8_t i, uint32_t color888);
uint32_t graphics_get_palette(uint8_t i);
void graphics_restore_sync_colors(void);
void startVIDEO(uint8_t vol);
void set_palette(uint8_t n); // переключение палитр
void graphics_set_mode(enum graphics_mode_t mode);

struct video_mode_t graphics_get_video_mode(int mode);
void graphics_set_bgcolor(uint32_t color888);

#ifdef HDMI_HSTX
/* Push mono int16 audio samples into the HSTX data-island queue. */
void hdmi_hstx_push_samples(const int16_t *buf, int count);
void hdmi_hstx_fill_silence(int count);
#endif

void graphics_set_crt_active(bool active);
bool graphics_get_crt_active(void);
void graphics_set_greyscale(bool active);
bool graphics_get_greyscale(void);


static const uint32_t tab_color[11][16] =
{
    /*1 палитра spectaculator 1   */
{
   /*яркость 0*/ 
   0x000000, //черный
   0x0000CE, // синий
   0xCE0000, // красный
   0xCE00CE, //фиолетовый
   0x00CA00, // зеленый
   0x00CACE, // голубой
   0xCECA00, // желтое
   0xCECACE, //белый
  /*яркость 1*/
   0x000000, //черный
   0x0000FF, // синий
   0xFF0000, // красный
   0xFF00FF, //фиолетовый
   0x00FB00, // зеленый
   0x00FBFF, // голубой
   0xFFFB00, // желтое
   0xFFFBFF //белый
},
/*2 палитра base-graph   */
{
   /*яркость 0*/ 
   0x000000, //черный
   0x0000A0, // синий
   0xDC0000, // красный
   0xE400B4, //фиолетовый
   0x00D400, // зеленый
   0x00D4D4, // голубой
   0xD0D000, // желтое
   0xCECACE, //белый
  /*яркость 1*/
   0x000000, //черный
   0x0000AC, // синий
   0xF00000, // красный
   0xFC00DC, //фиолетовый
   0x00F000, // зеленый
   0x00FCFC, // голубой
   0xFCFC00, // желтое
   0xFCFCFC //белый
},
/*3 палитра ч/б  */
{
     /*яркость 0*/ 
   0x101010, //черный
   0x292d29, // синий
   0x4a4d4a, // красный
   0x6b6d6b, //фиолетовый
   0x7b7d7b, // зеленый
   0x9c9e9c, // голубой
   0xbdbebd, // желтое
   0xdedfde, //белый
  /*яркость 1*/
   0x101010, //черный
   0x313131, // синий
   0x5a5d5a, // красный
   0x7b7d7b, //фиолетовый
   0x9c9e9c, // зеленый
   0xbdbebd, // голубой
   0xe6e3e6, // желтое
   0xffffff //белый
},
//4) MARS1
{
   /*яркость 0*/ 
   0x000000, //черный
   0x000090, // синий
   0xC03000, // красный
   0xC03090, //фиолетовый
   0x00AA2a, // зеленый 0x009030, // зеленый
   0x0090C0, // голубой
   0xC0C030, // желтое
   0xC0C0C0, //белый
  /*яркость 1*/
   0x000000, //черный
   0x0000BF, // синий
   0xFF3F00, // красный
   0xFF3FBF, //фиолетовый
   0x00fF3F, // зеленый
   0x00BFFF, // голубой
   0xFFFF3F, // желтое
   0xffffff //белый
},
//5 OCEAN1
{
   /*яркость 0*/ 
   0x202020, //черный
   0x3838A0, // синий
   0x882020, // красный
   0xA038A0, //фиолетовый
   0x208820, // зеленый
   0x38A0A0, // голубой
   0x888820, // желтое
   0xA0A0A0, //белый
  /*яркость 1*/
   0x202020, //черный
   0x4444E0, // синий
   0xBC2020, // красный
   0xE044E0, //фиолетовый
   0x20BC20, // зеленый
   0x44E0E0, // голубой
   0xBCBC20, // желтое
   0xE0E0E0 //белый
},
//6 Unreal-Grey1
{
   /*яркость 0*/ 
   0x000000, //черный
   0x1b1b1b, // синий
   0x363636, // красный
   0x515151, //фиолетовый
   0x6d6d6d, // зеленый
   0x888888, // голубой
   0xa4a4a4, // желтое
   0xbfbfbf, //белый
  /*яркость 1*/
   0x000000, //черный
   0x232323, // синий
   0x484848, // красный
   0x6c6c6c, //фиолетовый
   0x919191, // зеленый
   0xb5b5b5, // голубой
   0xdadada, // желтое
   0xfefefe //белый
},
//7  alone1
{
   /*яркость 0*/ 
   0x000000, //черный
   0x0000aa, // синий
   0xaa0000, // красный
   0xaa00aa, //фиолетовый
   0x00aa00, // зеленый
   0x00aaaa, // голубой
   0xaaaa00, // желтое
   0xaaaaaa, //белый
  /*яркость 1*/
   0x000000, //черный
   0x0000ff, // синий
   0xff0000, // красный
   0xff00ff, //фиолетовый
   0x00ff00, // зеленый
   0x00ffff, // голубой
   0xffff00, // желтое
   0xffffff //белый
},
// 8 pulsar1
{
   /*яркость 0*/ 
   0x000000, //черный
   0x0000cd, // синий
   0xcd0000, // красный
   0xcd00cd, //фиолетовый
   0x00cd00, // зеленый
   0x00cdcd, // голубой
   0xcdcd00, // желтое
   0xcdcdcd, //белый
  /*яркость 1*/
   0x000000, //черный
   0x0000ff, // синий
   0xff0000, // красный
   0xff00ff, //фиолетовый
   0x00ff00, // зеленый
   0x00ffff, // голубой
   0xffff00, // желтое
   0xffffff //белый
},

//9  HAH2
{
   /*яркость 0*/ 
   0x000000, //черный
   0x3300CC, // синий
   0xff5500, // красный
   0xAA00AA, //фиолетовый
   0x66CC00, // зеленый
   0x66FFFF, // голубой
   0xFFFF66, // желтое
   0xAAAAAA, //белый
  /*яркость 1*/
   0x000000, //черный
   0x5555ff, // синий
   0xff6633, // красный
   0xfe00fe, //фиолетовый
   0xCCFF99, // зеленый
   0xCCFFFF, // голубой
   0xFFFFCC, // желтое
   0xFFFFFF //белый
},



//10 UNREAL
{
   /*яркость 0*/ 
   0x000000, //черный
   0x0000bf, // синий
   0xbf0000, // красный
   0xbf00bf, //фиолетовый
   0x00bf00, // зеленый
   0x00bfbf, // голубой
   0xbfbf00, // желтое
   0xbfbfbf, //белый
  /*яркость 1*/
   0x000000, //черный
   0x0000fe, // синий
   0xfe0000, // красный
   0xfe00fe, //фиолетовый
   0x00fe00, // зеленый
   0x00fefe, // голубой
   0xfefe00, // желтое
   0xfefefe //белый
},

//11 HAH
{
   /*яркость 0*/ 
   0x000000, //черный
   0x3300CC, // синий
   0xff3300, // красный
   0xff0099, //фиолетовый
   0x66CC00, // зеленый
   0x66FFFF, // голубой
   0xFFFF66, // желтое
   0x999999, //белый
  /*яркость 1*/
   0x000000, //черный
   0x3333ff, // синий
   0xff6633, // красный
   0xff99cc, //фиолетовый
   0xCCFF99, // зеленый
   0xCCFFFF, // голубой
   0xFFFFCC, // желтое
   0xFFFFFF //белый
}

};



#endif