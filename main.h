/******************************************************************************
 * main.h 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2008 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * main.h is part of mp3nema.
 * mp3nema is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mp3nema is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mp3nema.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifndef MAIN_H_INCLUDE
#define MAIN_H_INCLUDE


/* Thanks to the wonderful resource found at:
 * http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 */


/* Terminal Display Name */
#define NAME "mp3nema"
#define TAG  "[" NAME "]"

/* Version */
#define _VER(_major, _minor) #_major"."#_minor
#define VERSION _VER(0, 1) /* Major, Minor */

/* Main argument flags */
#define FLAG_INSERT_MODE  1
#define FLAG_CAPTURE_MODE 2
#define FLAG_EXTRACT_MODE 4
typedef unsigned short int flags_t;

/* Error Reporting */
#define ERR(...) {fprintf(stderr, TAG "Error: " __VA_ARGS__);}

/* For array allocation */
#define DEFAULT_BLK_SZ 512 /* Bytes */

/* Out of Band data (OOB) what we are looking for */
#define OOB_BLK_SIZE DEFAULT_BLK_SZ


typedef struct _mp3_frame_t
{
    unsigned char header[6]; /* 6 bytes 4 for header 2 for CRC if it exists */
    int header_size;
    int version;
    int layer;
    int crc;
    int padding;
    int bitrate;
    int samplerate;
    int audio_size; /* AKA frame length */
    unsigned char *audio;
} mp3_frame_t;


/* What we have found in the mp3 */
typedef enum _stream_thang STREAM_OBJECT;
enum _stream_thang
{
    STREAM_OBJECT_UNKNOWN,   /* Error */
    STREAM_OBJECT_MP3_FRAME,
    STREAM_OBJECT_ID3V2_TAG
};


/* MP3 Version */
#define MP3_HDR_VERSION(_h) ((_h[1] & 0x18) >> 3)
#define V1   0x3
#define V2   0x2
#define V2_5 0x0

/* MP3 Layer */
#define MP3_HDR_LAYER(_h) ((_h[1] & 0x06) >> 1)
#define L1 0x3
#define L2 0x2
#define L3 0x1

/* Other Header Fields */
#define MP3_HDR_CRC(_h)         ((_h[1] & 0x01))
#define MP3_HDR_PADDING(_h)     ((_h[2] & 0x02) >> 1)
#define MP3_HDR_BIT_RATE(_h)    ((_h[2] & 0xF0) >> 4)
#define MP3_HDR_SAMPLE_RATE(_h) ((_h[2] & 0x0C) >> 2)

/* Bit Rate Table
 * 
 * Row index: bitrate value from frame header
 * Col index: Based on MP3 Version and Layer
 */
static const int bitrate_table[][5] = {
    /* 0000 */ {0,  0,  0,  0,  0},
    /* 0001 */ {32, 32, 32, 32, 8},
    /* 0010 */ {64, 48, 40, 48, 16},
    /* 0011 */ {96, 56, 48, 56, 24},
    
    /* 0100 */ {128, 64,  56, 64,  32},
    /* 0101 */ {160, 80,  64, 80,  40},
    /* 0110 */ {192, 96,  80, 96,  48},
    /* 0111 */ {224, 112, 96, 112, 56},

    /* 1000 */ {256, 128, 112, 128, 64},
    /* 1001 */ {288, 160, 128, 144, 80},
    /* 1010 */ {320, 192, 160, 160, 96},
    /* 1011 */ {352, 224, 192, 176, 112},

    /* 1100 */ {384, 256, 224, 192, 128},
    /* 1101 */ {416, 320, 256, 224, 144},
    /* 1110 */ {448, 384, 320, 256, 160},
    /* 1111 */ {-1,  -1,  -1,  -1,  -1},
};

/* Sample rate table
 *
 * Row: index from header
 * Col: index from MP3 version
 */
static const int sample_rate_table[][3] = {
    /* 00 */ {44100, 22050, 11025},
    /* 01 */ {48000, 24000, 12000},
    /* 10 */ {32000, 16000, 8000},
    /* 11 */ {0,     0,     0}
};


/* ID3v2 Tag */
typedef struct _tag
{
    int extended_header;
    int footer;
    unsigned int size;
} id3_tag_t;

#define ID3_HDR_EXTENDED(_h) ((_h[2] & 0x40) >> 4)
#define ID3_HDR_FOOTER(_h)   ((_h[2] & 0x10) >> 4)
#define ID3_HDR_SIZE(_h)  \
    (((_h[6] << 23) | (_h[7] << 16)) | ((_h[8] << 7) | (_h[9])))


/* 
 * Routines
 */

/* Handle the fname as a directory or single mp3 file that data from 'datasrc'
 * file is to be injected into */ 
extern void handle_as_insert(
    const char *f_or_dir_name,
    flags_t     flags,
    const char *datasrc);

/* Handle the name as a mp3 file */
extern void handle_as_file(
    const char *fname,
    flags_t     flags);

/* Only scan the incoming data for OOB info */
extern void handle_as_stream(
    const char *url,
    flags_t    flags);


#endif /* MAIN_H_INCLUDE */
