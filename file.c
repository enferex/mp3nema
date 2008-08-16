/******************************************************************************
 * file.c 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2008 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * file.c is part of mp3nema.
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

#include <stdlib.h>
#include "main.h"
#include "utils.h"


void handle_as_file(const char *fname, flags_t flags)
{
    int            n_frames, n_tags;
    FILE          *fp, *oob_file;
    mp3_frame_t   *frame;
    id3_tag_t     *tag;
    STREAM_OBJECT  type;

    if (!(fp = fopen(fname, "rb")))
      abort();

    oob_file = NULL;
    if (flags & FLAG_EXTRACT_MODE)
      if (!(oob_file = util_create_file(fname, "extracted-oob", "dat")))
        ERR("Could not create a file to store out of band data\n"
            "Normal analysis will still occur.\n");
    
    n_frames = n_tags = 0;

    while ((type = util_next_mp3_frame_or_id3v2(fp, NULL, 0, 0,NULL,oob_file)))
    {
        switch (type)
        {
            case STREAM_OBJECT_MP3_FRAME:
                frame = mp3_get_frame(fp);
                mp3_free_frame(frame);
                ++n_frames;
                break;

            case STREAM_OBJECT_ID3V2_TAG:
                tag = id3_get_tag(fp);
                free(tag);
                ++n_tags;
                break;

            default:
                break;
        }
    }

    printf(TAG " Frames: %d\n", n_frames);
    printf(TAG " ID3v2 Tags: %d\n", n_tags);

    /* Clean */
    if (oob_file)
      fclose(oob_file);
    fclose(fp);
}
