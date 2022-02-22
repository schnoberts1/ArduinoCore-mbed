/*
 * Copyright 2021 Arduino SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * GC2145 driver.
 */
#include "Wire.h"
#include "gc2145.h"

#define GC_MAX_WIN_W                    (1600)
#define GC_MAX_WIN_H                    (1200)

#define REG_AMODE1                      (0x17)
#define REG_AMODE1_DEF                  (0x14)
#define REG_AMODE1_SET_HMIRROR(r, x)    ((r&0xFE)|((x&1)<<0))
#define REG_AMODE1_SET_VMIRROR(r, x)    ((r&0xFD)|((x&1)<<1))

#define REG_OUTPUT_FMT                  (0x84)
#define REG_OUTPUT_FMT_RGB565           (0x06)
#define REG_OUTPUT_FMT_YCBYCR           (0x02)
#define REG_OUTPUT_FMT_BAYER            (0x17)
#define REG_OUTPUT_SET_FMT(r, x)        ((r&0xE0)|(x))

#define REG_SYNC_MODE                   (0x86)
#define REG_SYNC_MODE_DEF               (0x23)
#define REG_SYNC_MODE_COL_SWITCH        (0x10)
#define REG_SYNC_MODE_ROW_SWITCH        (0x20)

static const uint8_t default_regs[][2] = {
    {0xfe, 0xf0},
    {0xfe, 0xf0},
    {0xfe, 0xf0},
    {0xfc, 0x06},
    {0xf6, 0x00},
    {0xf7, 0x1d},
    {0xf8, 0x85},
    {0xfa, 0x00},
    {0xf9, 0xfe},
    {0xf2, 0x00},
    /////////////////////////////////////////////////
    //////////////////ISP reg//////////////////////
    ////////////////////////////////////////////////////
    {0xfe, 0x00},
    {0x03, 0x04},
    {0x04, 0xe2},

    {0x09, 0x00},   // row start
    {0x0a, 0x00},

    {0x0b, 0x00},   // col start
    {0x0c, 0x00},

    {0x0d, 0x04},   // Window height
    {0x0e, 0xc0},

    {0x0f, 0x06},   // Window width
    {0x10, 0x52},

    {0x99, 0x11},   // Subsample
    {0x9a, 0x0E},   // Subsample mode

    {0x12, 0x2e},   //
    {0x17, 0x14},   // Analog Mode 1 (vflip/mirror[1:0])
    {0x18, 0x22},   // Analog Mode 2
    {0x19, 0x0e},
    {0x1a, 0x01},
    {0x1b, 0x4b},
    {0x1c, 0x07},
    {0x1d, 0x10},
    {0x1e, 0x88},
    {0x1f, 0x78},
    {0x20, 0x03},
    {0x21, 0x40},
    {0x22, 0xa0},
    {0x24, 0x16},
    {0x25, 0x01},
    {0x26, 0x10},
    {0x2d, 0x60},
    {0x30, 0x01},
    {0x31, 0x90},
    {0x33, 0x06},
    {0x34, 0x01},
    {0x80, 0x7f},
    {0x81, 0x26},
    {0x82, 0xfa},
    {0x83, 0x00},
    {0x84, 0x06},   //RGB565
    {0x86, 0x23},
    {0x88, 0x03},
    {0x89, 0x03},
    {0x85, 0x08},
    {0x8a, 0x00},
    {0x8b, 0x00},
    {0xb0, 0x55},
    {0xc3, 0x00},
    {0xc4, 0x80},
    {0xc5, 0x90},
    {0xc6, 0x3b},
    {0xc7, 0x46},
    {0xec, 0x06},
    {0xed, 0x04},
    {0xee, 0x60},
    {0xef, 0x90},
    {0xb6, 0x01},

    {0x90, 0x01},   // Enable crop
    {0x91, 0x00},   // Y offset
    {0x92, 0x00},
    {0x93, 0x00},   // X offset
    {0x94, 0x00},
    {0x95, 0x02},   // Window height
    {0x96, 0x58},
    {0x97, 0x03},   // Window width
    {0x98, 0x20},
    {0x99, 0x22},   // Subsample
    {0x9a, 0x0E},   // Subsample mode

    {0x9b, 0x00},
    {0x9c, 0x00},
    {0x9d, 0x00},
    {0x9e, 0x00},
    {0x9f, 0x00},
    {0xa0, 0x00},
    {0xa1, 0x00},
    {0xa2, 0x00},
    /////////////////////////////////////////
    /////////// BLK ////////////////////////
    /////////////////////////////////////////
    {0xfe, 0x00},
    {0x40, 0x42},
    {0x41, 0x00},
    {0x43, 0x5b},
    {0x5e, 0x00},
    {0x5f, 0x00},
    {0x60, 0x00},
    {0x61, 0x00},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x00},
    {0x65, 0x00},
    {0x66, 0x20},
    {0x67, 0x20},
    {0x68, 0x20},
    {0x69, 0x20},
    {0x76, 0x00},
    {0x6a, 0x08},
    {0x6b, 0x08},
    {0x6c, 0x08},
    {0x6d, 0x08},
    {0x6e, 0x08},
    {0x6f, 0x08},
    {0x70, 0x08},
    {0x71, 0x08},
    {0x76, 0x00},
    {0x72, 0xf0},
    {0x7e, 0x3c},
    {0x7f, 0x00},
    {0xfe, 0x02},
    {0x48, 0x15},
    {0x49, 0x00},
    {0x4b, 0x0b},
    {0xfe, 0x00},
    ////////////////////////////////////////
    /////////// AEC ////////////////////////
    ////////////////////////////////////////
    {0xfe, 0x01},
    {0x01, 0x04},
    {0x02, 0xc0},
    {0x03, 0x04},
    {0x04, 0x90},
    {0x05, 0x30},
    {0x06, 0x90},
    {0x07, 0x30},
    {0x08, 0x80},
    {0x09, 0x00},
    {0x0a, 0x82},
    {0x0b, 0x11},
    {0x0c, 0x10},
    {0x11, 0x10},
    {0x13, 0x68}, //7b->68 bob
    {0x17, 0x00},
    {0x1c, 0x11},
    {0x1e, 0x61},
    {0x1f, 0x35},
    {0x20, 0x40},
    {0x22, 0x40},
    {0x23, 0x20},
    {0xfe, 0x02},
    {0x0f, 0x04},
    {0xfe, 0x01},
    {0x12, 0x30}, //35
    {0x15, 0xb0},
    {0x10, 0x31},
    {0x3e, 0x28},
    {0x3f, 0xb0},
    {0x40, 0x90},
    {0x41, 0x0f},
    /////////////////////////////
    //////// INTPEE /////////////
    /////////////////////////////
    {0xfe, 0x02},
    {0x90, 0x6c},
    {0x91, 0x03},
    {0x92, 0xcb},
    {0x94, 0x33},
    {0x95, 0x84},
    {0x97, 0x65}, // 54->65 bob
    {0xa2, 0x11},
    {0xfe, 0x00},
    /////////////////////////////
    //////// DNDD///////////////
    /////////////////////////////
    {0xfe, 0x02},
    {0x80, 0xc1},
    {0x81, 0x08},
    {0x82, 0x05}, //05
    {0x83, 0x08}, //08
    {0x84, 0x0a},
    {0x86, 0xf0},
    {0x87, 0x50},
    {0x88, 0x15},
    {0x89, 0xb0},
    {0x8a, 0x30},
    {0x8b, 0x10},
    /////////////////////////////////////////
    /////////// ASDE ////////////////////////
    /////////////////////////////////////////
    {0xfe, 0x01},
    {0x21, 0x04},
    {0xfe, 0x02},
    {0xa3, 0x50},
    {0xa4, 0x20},
    {0xa5, 0x40},
    {0xa6, 0x80},
    {0xab, 0x40},
    {0xae, 0x0c},
    {0xb3, 0x46},
    {0xb4, 0x64},
    {0xb6, 0x38},
    {0xb7, 0x01}, //01
    {0xb9, 0x2b}, //2b
    {0x3c, 0x04}, //04
    {0x3d, 0x15}, //15
    {0x4b, 0x06}, //06
    {0x4c, 0x20},
    {0xfe, 0x00},
    /////////////////////////////////////////
    /////////// GAMMA   ////////////////////////
    /////////////////////////////////////////
    ///////////////////gamma1////////////////////
    {0xfe, 0x02},
    {0x10, 0x09},
    {0x11, 0x0d},
    {0x12, 0x13},
    {0x13, 0x19},
    {0x14, 0x27},
    {0x15, 0x37},
    {0x16, 0x45},
    {0x17, 0x53},
    {0x18, 0x69},
    {0x19, 0x7d},
    {0x1a, 0x8f},
    {0x1b, 0x9d},
    {0x1c, 0xa9},
    {0x1d, 0xbd},
    {0x1e, 0xcd},
    {0x1f, 0xd9},
    {0x20, 0xe3},
    {0x21, 0xea},
    {0x22, 0xef},
    {0x23, 0xf5},
    {0x24, 0xf9},
    {0x25, 0xff},
    {0xfe, 0x00},
    {0xc6, 0x20},
    {0xc7, 0x2b},
    ///////////////////gamma2////////////////////
    {0xfe, 0x02},
    {0x26, 0x0f},
    {0x27, 0x14},
    {0x28, 0x19},
    {0x29, 0x1e},
    {0x2a, 0x27},
    {0x2b, 0x33},
    {0x2c, 0x3b},
    {0x2d, 0x45},
    {0x2e, 0x59},
    {0x2f, 0x69},
    {0x30, 0x7c},
    {0x31, 0x89},
    {0x32, 0x98},
    {0x33, 0xae},
    {0x34, 0xc0},
    {0x35, 0xcf},
    {0x36, 0xda},
    {0x37, 0xe2},
    {0x38, 0xe9},
    {0x39, 0xf3},
    {0x3a, 0xf9},
    {0x3b, 0xff},
    ///////////////////////////////////////////////
    ///////////YCP ///////////////////////
    ///////////////////////////////////////////////
    {0xfe, 0x02},
    {0xd1, 0x32}, //32->2d
    {0xd2, 0x32}, //32->2d bob
    {0xd3, 0x40},
    {0xd6, 0xf0},
    {0xd7, 0x10},
    {0xd8, 0xda},
    {0xdd, 0x14},
    {0xde, 0x86},
    {0xed, 0x80}, //80
    {0xee, 0x00}, //00
    {0xef, 0x3f},
    {0xd8, 0xd8},
    ///////////////////abs/////////////////
    {0xfe, 0x01},
    {0x9f, 0x40},
    /////////////////////////////////////////////
    //////////////////////// LSC ///////////////
    //////////////////////////////////////////
    {0xfe, 0x01},
    {0xc2, 0x14},
    {0xc3, 0x0d},
    {0xc4, 0x0c},
    {0xc8, 0x15},
    {0xc9, 0x0d},
    {0xca, 0x0a},
    {0xbc, 0x24},
    {0xbd, 0x10},
    {0xbe, 0x0b},
    {0xb6, 0x25},
    {0xb7, 0x16},
    {0xb8, 0x15},
    {0xc5, 0x00},
    {0xc6, 0x00},
    {0xc7, 0x00},
    {0xcb, 0x00},
    {0xcc, 0x00},
    {0xcd, 0x00},
    {0xbf, 0x07},
    {0xc0, 0x00},
    {0xc1, 0x00},
    {0xb9, 0x00},
    {0xba, 0x00},
    {0xbb, 0x00},
    {0xaa, 0x01},
    {0xab, 0x01},
    {0xac, 0x00},
    {0xad, 0x05},
    {0xae, 0x06},
    {0xaf, 0x0e},
    {0xb0, 0x0b},
    {0xb1, 0x07},
    {0xb2, 0x06},
    {0xb3, 0x17},
    {0xb4, 0x0e},
    {0xb5, 0x0e},
    {0xd0, 0x09},
    {0xd1, 0x00},
    {0xd2, 0x00},
    {0xd6, 0x08},
    {0xd7, 0x00},
    {0xd8, 0x00},
    {0xd9, 0x00},
    {0xda, 0x00},
    {0xdb, 0x00},
    {0xd3, 0x0a},
    {0xd4, 0x00},
    {0xd5, 0x00},
    {0xa4, 0x00},
    {0xa5, 0x00},
    {0xa6, 0x77},
    {0xa7, 0x77},
    {0xa8, 0x77},
    {0xa9, 0x77},
    {0xa1, 0x80},
    {0xa2, 0x80},

    {0xfe, 0x01},
    {0xdf, 0x0d},
    {0xdc, 0x25},
    {0xdd, 0x30},
    {0xe0, 0x77},
    {0xe1, 0x80},
    {0xe2, 0x77},
    {0xe3, 0x90},
    {0xe6, 0x90},
    {0xe7, 0xa0},
    {0xe8, 0x90},
    {0xe9, 0xa0},
    {0xfe, 0x00},
    ///////////////////////////////////////////////
    /////////// AWB////////////////////////
    ///////////////////////////////////////////////
    {0xfe, 0x01},
    {0x4f, 0x00},
    {0x4f, 0x00},
    {0x4b, 0x01},
    {0x4f, 0x00},

    {0x4c, 0x01}, // D75
    {0x4d, 0x71},
    {0x4e, 0x01},
    {0x4c, 0x01},
    {0x4d, 0x91},
    {0x4e, 0x01},
    {0x4c, 0x01},
    {0x4d, 0x70},
    {0x4e, 0x01},
    {0x4c, 0x01}, // D65
    {0x4d, 0x90},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0xb0},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0x8f},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0x6f},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0xaf},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0xd0},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0xf0},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0xcf},
    {0x4e, 0x02},
    {0x4c, 0x01},
    {0x4d, 0xef},
    {0x4e, 0x02},
    {0x4c, 0x01}, //D50
    {0x4d, 0x6e},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x8e},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xae},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xce},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x4d},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x6d},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x8d},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xad},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xcd},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x4c},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x6c},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x8c},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xac},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xcc},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xcb},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x4b},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x6b},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0x8b},
    {0x4e, 0x03},
    {0x4c, 0x01},
    {0x4d, 0xab},
    {0x4e, 0x03},
    {0x4c, 0x01}, //CWF
    {0x4d, 0x8a},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0xaa},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0xca},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0xca},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0xc9},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0x8a},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0x89},
    {0x4e, 0x04},
    {0x4c, 0x01},
    {0x4d, 0xa9},
    {0x4e, 0x04},
    {0x4c, 0x02}, //tl84
    {0x4d, 0x0b},
    {0x4e, 0x05},
    {0x4c, 0x02},
    {0x4d, 0x0a},
    {0x4e, 0x05},
    {0x4c, 0x01},
    {0x4d, 0xeb},
    {0x4e, 0x05},
    {0x4c, 0x01},
    {0x4d, 0xea},
    {0x4e, 0x05},
    {0x4c, 0x02},
    {0x4d, 0x09},
    {0x4e, 0x05},
    {0x4c, 0x02},
    {0x4d, 0x29},
    {0x4e, 0x05},
    {0x4c, 0x02},
    {0x4d, 0x2a},
    {0x4e, 0x05},
    {0x4c, 0x02},
    {0x4d, 0x4a},
    {0x4e, 0x05},
    {0x4c, 0x02},
    {0x4d, 0x8a},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0x49},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0x69},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0x89},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0xa9},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0x48},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0x68},
    {0x4e, 0x06},
    {0x4c, 0x02},
    {0x4d, 0x69},
    {0x4e, 0x06},
    {0x4c, 0x02}, //H
    {0x4d, 0xca},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xc9},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xe9},
    {0x4e, 0x07},
    {0x4c, 0x03},
    {0x4d, 0x09},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xc8},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xe8},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xa7},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xc7},
    {0x4e, 0x07},
    {0x4c, 0x02},
    {0x4d, 0xe7},
    {0x4e, 0x07},
    {0x4c, 0x03},
    {0x4d, 0x07},
    {0x4e, 0x07},

    {0x4f, 0x01},
    {0x50, 0x80},
    {0x51, 0xa8},
    {0x52, 0x47},
    {0x53, 0x38},
    {0x54, 0xc7},
    {0x56, 0x0e},
    {0x58, 0x08},
    {0x5b, 0x00},
    {0x5c, 0x74},
    {0x5d, 0x8b},
    {0x61, 0xdb},
    {0x62, 0xb8},
    {0x63, 0x86},
    {0x64, 0xc0},
    {0x65, 0x04},
    {0x67, 0xa8},
    {0x68, 0xb0},
    {0x69, 0x00},
    {0x6a, 0xa8},
    {0x6b, 0xb0},
    {0x6c, 0xaf},
    {0x6d, 0x8b},
    {0x6e, 0x50},
    {0x6f, 0x18},
    {0x73, 0xf0},
    {0x70, 0x0d},
    {0x71, 0x60},
    {0x72, 0x80},
    {0x74, 0x01},
    {0x75, 0x01},
    {0x7f, 0x0c},
    {0x76, 0x70},
    {0x77, 0x58},
    {0x78, 0xa0},
    {0x79, 0x5e},
    {0x7a, 0x54},
    {0x7b, 0x58},
    {0xfe, 0x00},
    //////////////////////////////////////////
    ///////////CC////////////////////////
    //////////////////////////////////////////
    {0xfe, 0x02},
    {0xc0, 0x01},
    {0xc1, 0x44},
    {0xc2, 0xfd},
    {0xc3, 0x04},
    {0xc4, 0xF0},
    {0xc5, 0x48},
    {0xc6, 0xfd},
    {0xc7, 0x46},
    {0xc8, 0xfd},
    {0xc9, 0x02},
    {0xca, 0xe0},
    {0xcb, 0x45},
    {0xcc, 0xec},
    {0xcd, 0x48},
    {0xce, 0xf0},
    {0xcf, 0xf0},
    {0xe3, 0x0c},
    {0xe4, 0x4b},
    {0xe5, 0xe0},
    //////////////////////////////////////////
    ///////////ABS ////////////////////
    //////////////////////////////////////////
    {0xfe, 0x01},
    {0x9f, 0x40},
    {0xfe, 0x00},

    //////////////////////////////////////
    ///////////  OUTPUT   ////////////////
    //////////////////////////////////////
    {0xfe, 0x00},
    {0xf2, 0x0f},

    ///////////////dark sun////////////////////
    {0xfe, 0x02},
    {0x40, 0xbf},
    {0x46, 0xcf},
    {0xfe, 0x00},

    //////////////frame rate control/////////
    {0xfe, 0x00},
    {0x05, 0x01},   // HBLANK
    {0x06, 0x1C},
    {0x07, 0x00},   // VBLANK
    {0x08, 0x32},
    {0x11, 0x00},   // SH Delay
    {0x12, 0x1D},
    {0x13, 0x00},   // St
    {0x14, 0x00},   // Et

    {0xfe, 0x01},
    {0x3c, 0x00},
    {0x3d, 0x04},
    {0xfe, 0x00},
    {0x00, 0x00},
};

GC2145::GC2145(arduino::MbedI2C &i2c) : 
    _i2c(&i2c)
{
}

int GC2145::init()
{
    _i2c->begin();
    _i2c->setClock(100000);

    int ret = 0;

    // Write default regsiters
    for (int i = 0; default_regs[i][0]; i++) {
        ret |= reg_write(GC2145_I2C_ADDR, default_regs[i][0], default_regs[i][1]);
    }

    return ret;
}

int GC2145::setWindow(uint16_t reg, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    int ret = 0;

    // P0 regs
    ret |= reg_write(GC2145_I2C_ADDR, 0xFE, 0x00);

    // Y/row offset
    ret |= reg_write(GC2145_I2C_ADDR, reg++, y >> 8);
    ret |= reg_write(GC2145_I2C_ADDR, reg++, y & 0xff);

    // X/col offset
    ret |= reg_write(GC2145_I2C_ADDR, reg++, x >> 8);
    ret |= reg_write(GC2145_I2C_ADDR, reg++, x & 0xff);

    // Window height
    ret |= reg_write(GC2145_I2C_ADDR, reg++, h >> 8);
    ret |= reg_write(GC2145_I2C_ADDR, reg++, h & 0xff);

    // Window width
    ret |= reg_write(GC2145_I2C_ADDR, reg++, w >> 8);
    ret |= reg_write(GC2145_I2C_ADDR, reg++, w & 0xff);

    return ret;
}

int GC2145::reset()
{
    return 0;
}

int GC2145::setFrameRate(int32_t framerate)
{
    return 0;
}

int GC2145::setResolution(int32_t resolution)
{
    int ret = 0;

    uint16_t win_w;
    uint16_t win_h;

    uint16_t w = restab[resolution][0];
    uint16_t h = restab[resolution][1];

    switch (resolution) {
        case CAMERA_R160x120:
            win_w = w * 4;
            win_h = h * 4;
            break;
        case CAMERA_R320x240:
        case CAMERA_R320x320:
            win_w = w * 3;
            win_h = h * 3;
            break;
        case CAMERA_R640x480:
            win_w = w * 2;
            win_h = h * 2;
            break;
        case CAMERA_R800x600:
        case CAMERA_R1600x1200:
            // For frames bigger than subsample using full UXGA window.
            win_w = 1600;
            win_h = 1200;
            break;
        default:
            return -1;
    }

    uint16_t c_ratio = win_w / w;
    uint16_t r_ratio = win_h / h;

    uint16_t x = (((win_w / c_ratio) - w) / 2);
    uint16_t y = (((win_h / r_ratio) - h) / 2);

    uint16_t win_x = ((GC_MAX_WIN_W - win_w) / 2);
    uint16_t win_y = ((GC_MAX_WIN_H - win_h) / 2);

    // Set readout window first.
    ret |= setWindow(0x09, win_x, win_y, win_w + 16, win_h + 8);

    // Set cropping window next.
    ret |= setWindow(0x91, x, y, w, h);

    // Enable crop
    ret |= reg_write(GC2145_I2C_ADDR, 0x90, 0x01);

    // Set Sub-sampling ratio and mode
    ret |= reg_write(GC2145_I2C_ADDR, 0x99, ((r_ratio << 4) | c_ratio));
    ret |= reg_write(GC2145_I2C_ADDR, 0x9A, 0x0E);

    return ret;

}

int GC2145::setPixelFormat(int32_t pixformat)
{
    int ret = 0;
    uint8_t reg;

    // P0 regs
    ret |= reg_write(GC2145_I2C_ADDR, 0xFE, 0x00);

    // Read current output format reg
    reg = reg_read(GC2145_I2C_ADDR, REG_OUTPUT_FMT);

    switch (pixformat) {
        case CAMERA_RGB565:
            ret |= reg_write(GC2145_I2C_ADDR,
                    REG_OUTPUT_FMT, REG_OUTPUT_SET_FMT(reg, REG_OUTPUT_FMT_RGB565));
            break;
        case CAMERA_GRAYSCALE:
            // TODO: There's no support for extracting GS from YUV so we use Bayer for 1BPP for now.
            //ret |= reg_write(GC2145_I2C_ADDR,
            //        REG_OUTPUT_FMT, REG_OUTPUT_SET_FMT(reg, REG_OUTPUT_FMT_YCBYCR));
            //break;
        case CAMERA_BAYER:
            // There's no BAYER support so it will just look off.
            // Make sure odd/even row are switched to work with our bayer conversion.
            ret |= reg_write(GC2145_I2C_ADDR,
                    REG_SYNC_MODE, REG_SYNC_MODE_DEF | REG_SYNC_MODE_ROW_SWITCH);
            ret |= reg_write(GC2145_I2C_ADDR,
                    REG_OUTPUT_FMT, REG_OUTPUT_SET_FMT(reg, REG_OUTPUT_FMT_BAYER));
            break;
        default:
            return -1;
    }

    return ret;
}

int GC2145::reg_write(uint8_t dev_addr, uint16_t reg_addr, uint8_t reg_data, bool wide_addr)
{
    _i2c->beginTransmission(dev_addr);
    uint8_t buf[3] = {(uint8_t) (reg_addr >> 8), (uint8_t) (reg_addr & 0xFF), reg_data};
    if (wide_addr == true) {
        _i2c->write(buf, 1);
    }
    _i2c->write(&buf[1], 2);
    return _i2c->endTransmission();
}

uint8_t GC2145::reg_read(uint8_t dev_addr, uint16_t reg_addr, bool wide_addr)
{
    uint8_t reg_data = 0;
    uint8_t buf[2] = {(uint8_t) (reg_addr >> 8), (uint8_t) (reg_addr & 0xFF)};
    _i2c->beginTransmission(dev_addr);
    if (wide_addr) {
        _i2c->write(buf, 2);
    } else {
        _i2c->write(&buf[1], 1);
    }
    _i2c->endTransmission(false);
    _i2c->requestFrom(dev_addr, 1);
    if (_i2c->available()) {
        reg_data = _i2c->read();
    }
    while (_i2c->available()) {
        _i2c->read();
    }
    return reg_data;
}

void GC2145::debug(Stream &stream)
{
  _debug = &stream;
}
