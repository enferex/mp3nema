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
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "main.h"
#include "utils.h"


/* Destinations (MP3 files that the inject data is spanned across/into) */
typedef struct _data_dest_t data_dest_t;
struct _data_dest_t {char *fname; size_t size; int frames;};


static int count_frames(FILE *dst)
{
    int            n_frames;
    id3_tag_t     *tag;
    mp3_frame_t   *frame;
    STREAM_OBJECT  type;

    n_frames = 0;
    while ((type = util_next_mp3_frame_or_id3v2(dst, NULL, 0, 0, NULL, NULL)))
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
    int            i, n_frames, n_blocks, block_sz, remainder_sz;
    long           start, end;
    unsigned char *buf, *block;
    id3_tag_t     *tag;
    mp3_frame_t   *frame;

    /* Chunks of data to break src into */
    remainder_sz = 0;
    n_frames = count_frames(dst);
    n_blocks = bytes / (n_frames - FRAMES_TO_IGNORE);
    if ((n_blocks == 0) || ((block_sz = bytes / n_blocks) == 0))
    {
        n_blocks = 1;
        block_sz = bytes;
    }
    else
      remainder_sz = bytes % n_blocks;

    fseek(dst, 0, SEEK_SET);
    buf = NULL;
    block = malloc(block_sz + remainder_sz);

    for (i=0; i<n_frames; i++)
    {
        /* Start/end read points to copy frame/tag */
        start = ftell(dst);
        switch (util_next_mp3_frame_or_id3v2(dst, NULL, 0, 0, NULL, NULL))
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

        /* Add in data (ignoring the first 'i' frames) */
        if (i > FRAMES_TO_IGNORE && n_blocks)
        {
            /* Add in remainder data if odd size */
            if ((n_blocks - 1) == 0)
              block_sz += remainder_sz;

            fread(block, block_sz, 1, src);
            fwrite(block, block_sz, 1, out);
            --n_blocks;
        }
    }

    free(buf);
    free(block);
}


/* Add a data destination object to the array given at index given */
static void add_dest(
    data_dest_t *dests,
    int          idx,
    const char  *fpath,
    const char  *fname)
{
    int            n_frames;
    struct stat    st;
    FILE          *dst;
    mp3_frame_t   *frame;
    id3_tag_t     *tag;
    STREAM_OBJECT  type;

    dests[idx].fname = malloc(2 + strlen(fname) + ((fpath)?strlen(fpath) : 0));
    if (!fpath)
      sprintf(dests[idx].fname, "%s", fname);
    else
      sprintf(dests[idx].fname, "%s/%s", fpath, fname);

    stat(dests[idx].fname, &st);
    dests[idx].size = st.st_size;

    if (!(dst = fopen(dests[idx].fname, "r")))
    {
        dests[idx].frames = 0;
        ERR("Could not open destination mp3 to obtain frame count");
        return;
    }

    /* Count frames */
    n_frames = 0;
    while ((type = util_next_mp3_frame_or_id3v2(dst, NULL, 0, 0, NULL, NULL)))
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

    fclose(dst);
    dests[idx].frames = n_frames;
}


/* Returns an array of data destinations (mp3 files) that the source data is to
 * be injected into.
 */
static data_dest_t *load_data_dests(const char *f_or_dir_name, int *n_dests)
{
    int            n_files;
    DIR           *dir;
    FILE          *dst;
    data_dest_t   *dests;
    struct dirent *entry;
    struct stat    st;
     
    dests = NULL;
    stat(f_or_dir_name, &st);

    if (n_dests)
      *n_dests = 0;

    /* Directory or single file */
    if (S_ISDIR(st.st_mode))
    {
        /* Count number of MP3 files */
        if (!(dir = opendir(f_or_dir_name)))
        {
            ERR("Could not open directory of mp3 files to inject data into");
            return NULL;
        }

        /* Assumes, by extension and not file data */
        n_files = 0;
        while ((entry = readdir(dir)))
          if (strstr(entry->d_name, ".mp3"))
            ++n_files;

        /* Load */
        dests = malloc(sizeof(data_dest_t) * n_files);
        n_files = 0;
        rewinddir(dir);
        while ((entry = readdir(dir)))
          if (strstr(entry->d_name, ".mp3"))
            add_dest(dests, n_files++, f_or_dir_name, entry->d_name);

        closedir(dir);
    }
    else /* Treat as a single file */
    {
        if (!(dst = fopen(f_or_dir_name, "r")))
        {
            ERR("Could not open mp3 to inject data into");
            return NULL;
        }

        dests = malloc(sizeof(data_dest_t));
        add_dest(dests, 0, NULL, f_or_dir_name);
        n_files = 1;
        fclose(dst);
    }

    if (n_dests)
      *n_dests = n_files;

    return dests;
}


static void free_dests(data_dest_t *dests, int n_dests)
{
    int i;

    for (i=0; i<n_dests; i++)
      free(dests[i].fname);
    free(dests);
}


void handle_as_insert(
    const char *f_or_dir_name,
    flags_t     flags,
    const char *datasrc)
{
    int          i, n_dests, err;
    char         dest_modifier[16];
    size_t       src_sz, sz;
    FILE         *dest, *src, *out;
    struct stat  st;
    data_dest_t *dests;

    /* Where we pull data to insert into */
    if (!(src = fopen(datasrc, "r")))
    {
        ERR("Could not open data file to read from");
        return;
    }

    /* Single file or directory? */
    if (!(dests = load_data_dests(f_or_dir_name, &n_dests)))
      return;
        
    /* Amount  of data to inject */
    stat(datasrc, &st);
    src_sz = st.st_size;
    fseek(src, 0, SEEK_SET);

    /* For each file we are to span accross */
    err = 0;
    for (i=0; i<n_dests; i++)
    {
        snprintf(dest_modifier, sizeof(dest_modifier), "injected-%d", i+1); 
        out = util_create_file(f_or_dir_name, dest_modifier, "mp3");
        
        /* Insert info between frame skipping two frames so data
         * is not always in the first frame.
         */
        if (!(dest = fopen(dests[i].fname, "r")))
        {
            ++err;
            continue;
        }

        /* Last one? Add in remainder for odd sizes */
        sz = src_sz / (n_dests - err);
        if (i+1 == n_dests)
          sz += src_sz % (n_dests - err);

        fseek(dest, 0, SEEK_SET);
        inject(dest, src, out, sz);

        fclose(dest);
        fclose(out);
    }

    /* Clean */
    free_dests(dests, n_dests);
    fclose(src);
}
