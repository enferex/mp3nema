/******************************************************************************
 * utils.c 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2008 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * utils.c is part of mp3nema.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"


void util_url_to_host_port_file(const char *url, hostdata_t *hostdata)
{
    char *c;

    /* Protocol */
    if (strstr(url, "//"))
      hostdata->host = strdup(strstr(url, "//") + 2);
    else
      hostdata->host = strdup(url);

    /* File location */
    hostdata->file = NULL;
    if ((c = strchr(hostdata->host, '/')) &&
        (*(c+1) != ' ') && (*(c+1) != '\0'))
    {
        hostdata->file = strdup(c);
        *c = '\0';

        if ((c = strchr(hostdata->file, '\r')) ||
            (c = strchr(hostdata->file, '\n')))
          *c = '\0';
    }
    else
      hostdata->file = strdup("/");

    /* Port */
    hostdata->portnum = 80;
    if (strchr(hostdata->host, ':'))
    {
        hostdata->host = strtok(hostdata->host, ":");
        hostdata->port = strdup(strtok(NULL, ":"));
        if ((c = strchr(hostdata->port, '/')))
          *c = '\0';
        hostdata->portnum = atoi(hostdata->port);
    }
    else 
      hostdata->port = strdup("80");
}


FILE *util_create_file(
    const char *fname,
    const char *desc,
    const char *extension,
    int         is_stream)
{
    int          fileno;
    char        *outname;
    const char  *c, *r;
    FILE        *out;
    struct stat  st;

    /* Create output file (in the working directory) */
    if ((c = strrchr(fname, '/')) && strlen(c+1))
      ++c;
    else
      c = fname;

    if ((stat(c, &st) == 0) && S_ISDIR(st.st_mode))
      c = NAME;

    /* Avoid overwriting an existig file by appening a number to the name */
    fileno = 0;
    outname = NULL;
    do 
    {
        free(outname);
        outname = calloc(1, strlen(c) + 64);

        if (is_stream || !(r = strrchr(c, '.')))
          r = c + strlen(c);
    
        strncpy(outname, c, r - c);

        if (fileno > 0)
          sprintf(outname, "%s-%s-%d.%s", outname, desc, fileno, extension);
        else
          sprintf(outname, "%s-%s.%s", outname, desc, extension);
        ++fileno;
    }
    while ((out = fopen(outname, "r")) && !fclose(out));

    if (!(out = fopen(outname, "w")))
    {
        ERR("Could not create output file '%s'\n", outname);
        return NULL;
    }

    free(outname);
    return out;
}


/* Pass either data block or file handle
 * If both are passed, the file handle takes presecendence.
 */
STREAM_OBJECT util_next_mp3_frame_or_id3v2(
    FILE       *fp,
    const char *data,
    int         data_sz,
    int         ignore_oob,
    int        *frame_or_tag_index,
    FILE       *oob_to_file)
{
    int           i, n_blks, oob_size;
    unsigned char v[3] = {0}, *oob;
    long          start, end;
    STREAM_OBJECT ret;

    /* Start OOB data catch fresh */
    oob = NULL;
    n_blks = 0;
   
    /* Start/end positions for file or indecies into data */ 
    if (fp)
    {
        start = ftell(fp);
        fseek(fp, 0, SEEK_END);
        end = ftell(fp);
        fseek(fp, start, SEEK_SET);
    }
    else if (data)
    {
        start = 0;
        end = data_sz;
    }
    else
      return STREAM_OBJECT_UNKNOWN;

    /* Suck data until we hit another sync frame or id3v2 */
    oob_size = ret = 0;
    while (((start + 3) <= end))
    {
        if (fp && (fread(v, 1, 3, fp) == 0))
          break;
        else if (data)
          memcpy(v, data+start, 3);

        /* Reset start position before we did fread */
        if (fp)
          start = ftell(fp) - 3;

        /* Look for MP3 sync frame */
        if ((v[0] == 0xFF) && ((v[1] & 0xE0) == 0xE0) && 
            mp3_is_valid_frame(fp, start))
        {
            ret = STREAM_OBJECT_MP3_FRAME;
            break;
        }

        /* Not a sync frame, ID3v2 frame? */
        else if ((v[0] == 'I') && (v[1] == 'D') && (v[2] == '3'))
        {
            ret = STREAM_OBJECT_ID3V2_TAG;
            break;
        }

        /* ID3v1 tag (not from a stream) */
        else if (fp && (v[0] == 'T') && (v[1] == 'A') && (v[2] == 'G') &&
                 (start == (end - 128)))
        {
            fseek(fp, 128 - 3, SEEK_CUR);
            continue;
        }

        /* OOB */
        else
        {
            if (oob_size >= (n_blks * OOB_BLK_SIZE))
              oob = realloc(oob, (++n_blks) * OOB_BLK_SIZE);
            oob[oob_size++] = v[0];
        }

        /* Keep lookin */
        if (fp)
          fseek(fp, start + 1, SEEK_SET);
        else
          ++start;
    }

    /* Display OOB data */
    if (oob_size && !ignore_oob && IS_VERBOSE)
    {
        VERBOSE("--OOB Data Found: %d bytes--\n", oob_size);
        for (i=0; i<oob_size; i++)
          VERBOSE("0x%.2x(%c) ", oob[i], 
                 (oob[i] > 31 && oob[i]<127) ? oob[i] : ' ');
        VERBOSE("\n----------------------------\n\n");
    }
    else if (oob_size && !IS_VERBOSE)
      printf(TAG " %d bytes out-of-frame\n", oob_size);

    /* Write OOB data to file */
    if (oob_size && !ignore_oob && oob_to_file)
      fwrite(oob, 1, oob_size, oob_to_file);
   
    if (fp)
      fseek(fp, start, SEEK_SET);
    else if (frame_or_tag_index)
      *frame_or_tag_index = start;

    free(oob);
    return ret;
}


mp3_frame_t *mp3_get_frame(FILE *fp)
{
    char         header[6];
    mp3_frame_t *frame;

    if (!(fread(header, 4, 1, fp)))
       return NULL;

    /* Check if certain fields are valid, if not try again */
    frame = calloc(1, sizeof(mp3_frame_t));
    mp3_set_header(frame, header);

    /* Skip CRC data */
    if (frame->crc)
      fseek(fp, 2, SEEK_CUR);

    /* Good header read? */
    if (!mp3_is_valid_header(frame))
    {
        mp3_free_frame(frame);
        return mp3_get_frame(fp);
    }

    if (frame->audio_size < 1)
      return mp3_get_frame(fp);

    /* Suck in the rest of the frame */
    frame->audio = malloc(frame->audio_size);
    fread(frame->audio, frame->audio_size, 1, fp);

    return frame;
}


void mp3_free_frame(mp3_frame_t *frame)
{
    if (!frame)
      return;

    free(frame->audio);
    free(frame);
}


void mp3_write_frame(
    FILE              *fp,
    const mp3_frame_t *frame)
{
    fwrite(frame->header, frame->header_size, 1, fp); 
    fwrite(frame->audio, frame->audio_size, 1, fp); 
}


/* Returns bytes not including frame header
 *
 * Great resource where this information was extracted from
 * http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 *
 * This site mentions that padding is to be one slot.
 * http://www.codeproject.com/KB/audio-video/mpegaudioinfo.aspx#MPEGAudioFrame
 * In fact, I figured out why my header extraction was improper, after looking
 * at the way they compared their header (sync frame check)
 */
int mp3_frame_length(const mp3_frame_t *frame)
{
    int bit_rate, sample_rate, value_idx;

    /* Bit rate (from header to actual rate value) */
    if ((frame->version == V2) &&
        (((frame->layer == L2)) || (frame->layer == L3)))
      value_idx = 4;
    else if ((frame->version == V2) && (frame->layer == L1))
      value_idx = 3;
    else
      value_idx  = frame->version - frame->layer;

    if ((bit_rate = bitrate_table[frame->bitrate][value_idx] * 1000) < 0)
      return 0;

    /* Sample rate */
    if (frame->version == V1)
      value_idx = 0;
    else if (frame->version == V2)
      value_idx = 1;
    else /* (version == V2_5) */
      value_idx = 2;

    if ((sample_rate = sample_rate_table[frame->samplerate][value_idx]) == 0)
      return 0;
    
    /* Frame length (bytes) */
    if (frame->layer == L1)
      return ((12 * bit_rate / sample_rate + frame->padding) * 4);
    else
      return (144 * bit_rate / sample_rate + frame->padding);
}


/* Sets the values for the fields in the header, leaves audio data NULL */
void mp3_set_header(mp3_frame_t *frame, const char header[4])
{
    memcpy(frame->header, header, 4);
    frame->version = MP3_HDR_VERSION(header);
    frame->layer = MP3_HDR_LAYER(header);
    frame->bitrate = MP3_HDR_BIT_RATE(header);
    frame->samplerate = MP3_HDR_SAMPLE_RATE(header);
    frame->padding = MP3_HDR_PADDING(header);
    frame->crc = MP3_HDR_CRC(header);
    
    if (frame->crc)
      frame->header_size = 6;
    else
      frame->header_size = 4;
    
    /* Set our fp to the beginning of the header */
    frame->audio_size = mp3_frame_length(frame) - frame->header_size;
    frame->audio = NULL;
}


int mp3_is_valid_header(const mp3_frame_t *frame)
{
    return !((frame->samplerate == 0x0003) || (frame->bitrate == 0x000F) ||
            (frame->audio_size < 0));
}


int mp3_is_valid_frame(FILE *fp, long start)
{
    int          valid;
    long         orig;
    char         header[6];
    mp3_frame_t *frame;

    /* TODO Handle stream vs FP data */
    if (!fp)
      return 1;

    orig = ftell(fp);
    fseek(fp, start, SEEK_SET);

    if (!(fread(header, 4, 1, fp)))
    {
        fseek(fp, orig, SEEK_SET);
        return 0;
    }

    frame = calloc(1, sizeof(mp3_frame_t));
    mp3_set_header(frame, header);
    valid = mp3_is_valid_header(frame);

    free(frame);
    fseek(fp, orig, SEEK_SET);

    return valid;
}


id3_tag_t *id3_get_tag(FILE *fp)
{
    id3_tag_t *tag;
    char       header[10];

    fread(header, 10, 1, fp);
    tag = malloc(sizeof(id3_tag_t));
    id3_set_header(tag, header);

    /* Skip the tag data */ 
    fseek(fp, tag->size, SEEK_CUR);

    return tag;
}


void id3_set_header(id3_tag_t *tag, const char header[10])
{    
    tag->size = ID3_HDR_SIZE(header);
    tag->extended_header = ID3_HDR_EXTENDED(header); 
    tag->footer = ID3_HDR_FOOTER(header);
}
