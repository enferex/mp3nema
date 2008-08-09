/******************************************************************************
 * insert.c 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2008 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * insert.c is part of mp3nema.
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "main.h"
#include "utils.h"


static int count_frames(FILE *dst)
{
    int            n_frames;
    id3_tag_t     *tag;
    mp3_frame_t   *frame;
    STREAM_OBJECT  type;

    n_frames = 0;
    while ((type = next_mp3_frame_or_id3v2(dst, NULL, 0, 0, NULL, NULL)))
    {
        switch (type)
        {
            case STREAM_OBJECT_MP3_FRAME:
                frame = mp3_get_frame(dst);
                mp3_free_frame(frame);
                ++n_frames;
                break;

            case STREAM_OBJECT_ID3V2_TAG:
                tag = id3_get_tag(dst);
                free(tag);
                break;

            default: break;
        }
    }

    return n_frames;
}


static void inject(FILE *dst, FILE *src, FILE *out, int bytes)
{
    int            i, n_frames, n_blocks, block_sz;
    long           start, end;
    unsigned char *buf, *block;
    id3_tag_t     *tag;
    mp3_frame_t   *frame;

    /* Chunks of data to break src into */
    n_frames = count_frames(dst);
    n_blocks = bytes / (n_frames - 1);
    if ((n_blocks == 0) || ((block_sz = bytes / n_blocks) == 0))
    {
        n_blocks = 1;
        block_sz = bytes;
    }

    fseek(dst, 0, SEEK_SET);
    buf = NULL;
    block = malloc(block_sz);

    for (i=0; i<n_frames; i++)
    {
        /* Start/end read points to copy frame/tag */
        start = ftell(dst);
        switch (next_mp3_frame_or_id3v2(dst, NULL, 0, 0, NULL, NULL))
        {
            case STREAM_OBJECT_MP3_FRAME:
                frame = mp3_get_frame(dst);
                mp3_free_frame(frame);
                break;

            case STREAM_OBJECT_ID3V2_TAG:
                tag = id3_get_tag(dst);
                free(tag);
                break;

            default:
                break;
        }
        end = ftell(dst);

        /* Copy tag/frame */
        free(buf);
        buf = calloc(1, end - start);
        fseek(dst, start, SEEK_SET);
        fread(buf, end - start, 1, dst);
        fseek(dst, end, SEEK_SET);
        fwrite(buf, end - start, 1, out);

        /* Add in data (ignoreing the first 'i' frames) */
        if (i > 15 && n_blocks)
        {
            fread(block, block_sz, 1, src);
            fwrite(block, block_sz, 1, out);
            --n_blocks;
        }
    }

    free(buf);
    free(block);
}


void handle_as_insert(
    const char *f_or_dir_name,
    flags_t     flags,
    const char *datasrc)
{
    size_t       src_sz;
    FILE        *dst, *src, *out;
    struct stat  st;

    /* Where we pull data to insert into */
    if (!(src = fopen(datasrc, "r")))
    {
        ERR("Could not open data file to read from");
        return;
    }

    /* Treat as a single file */
    if (!(dst = fopen(f_or_dir_name, "r")))
    {
        ERR("Could not open mp3 to inject data into");
        return;
    }

    out = util_create_file(f_or_dir_name, "injected", "mp3");
    
    /* Size */
    stat(datasrc, &st);
    src_sz = st.st_size;

    /* Insert info between frame skipping two frames so data
     * is not always in the first frame.
     */
    fseek(src, 0, SEEK_SET);
    fseek(dst, 0, SEEK_SET);
    inject(dst, src, out, src_sz);

    /* Clean */
    fclose(src);
    fclose(dst);
    fclose(out);
}
