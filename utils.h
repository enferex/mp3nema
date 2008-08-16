/******************************************************************************
 * utils.h 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2008 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * utils.h is part of mp3nema.
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
#ifndef UTILS_H_INCLUDE
#define UTILS_H_INCLUDE

#include <stdio.h>
#include "main.h"


typedef struct _hostdata_t 
{
    char *host;
    char *port;
    char *file;
    int   portnum;
} hostdata_t;


/* Returns the host, port, file as strings and the port is also in the returned
 * structure.  This data is extracted from the passed 'url' The populated
 * strings (host, port, file) should be deallocated when through.
 * The hostdata object should already be allocated.
 */
extern void util_url_to_host_port_file(const char *url, hostdata_t *hostdata);


/* Returns a file pointer to a newly created file, in the current working
 * directory, with the description inserted in the name.  If this is a 
 * stream, assume 'fname' is a URL/host, so avoid lopping off an extension,
 * because that would probably be part of the address.
 */
extern FILE *util_create_file(
    const char *fname,
    const char *desc,
    const char *extension,
    int         is_stream);


/* Searches the file stream or data stream for the start of the next mp3 frame
 * or id3v2 tag.  If a data stream is searched, and index into that stream is
 * returned where the frame or tag begins.
 * If 'oob_to_file' is specified, the OOB data is written here.
 */
extern STREAM_OBJECT util_next_mp3_frame_or_id3v2(
    FILE       *fp,
    const char *data,
    int         data_sz,
    int         ignore_oob,
    int        *frame_or_tag_index,
    FILE       *oob_to_file);


/* MP3 Frames */
extern mp3_frame_t *mp3_get_frame(FILE *fp);
extern void mp3_free_frame(mp3_frame_t *frame);
extern void mp3_write_frame(FILE *fp, const mp3_frame_t *frame);
extern int mp3_frame_length(const mp3_frame_t *frame);
extern void mp3_set_header(mp3_frame_t *frame, const char header[4]);
extern int mp3_is_valid_header(const mp3_frame_t *frame);
extern int mp3_is_valid_frame(FILE *fp, long start);


/* ID3 Tags */
extern id3_tag_t *id3_get_tag(FILE *fp);
extern void id3_set_header(id3_tag_t *tag, const char header[10]);


#endif /* UTILS_H_INCLUDE */
