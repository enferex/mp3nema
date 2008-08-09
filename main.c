/******************************************************************************
 * main.c 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2008 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * main.c is part of mp3nema.
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
#include <string.h>
#include "main.h"


void usage(void)
{
    printf("-- MP3nema -- v. " VERSION " --\n"
           "A MP3 analysis, data capturing, and data hiding utility\n");

    printf("Usage: ./mp3nema <source.mp3 | stream> [[-e] | [-i file]]\n"
           "\t-c Capture audio from network stream\n"
           "\t-i <file> Inject data from 'file' into the mp3 between frames\n"
           "\t-e Extract out of band data to a file\n");
    _Exit(0);
}


int is_file(const char *name)
{
    FILE *fp;

    if ((fp = fopen(name, "rb")))
    {
        fclose(fp);
        return 1;
    }

    return 0;
}


int main(
    int    argc,
    char **argv)
{
    int      i;
    char    *datasrc, *fname;
    flags_t  flags;

    if (argc < 2)
      usage();

    fname = datasrc = NULL;

    /* Args */
    flags = 0;
    for (i=1; i<argc; i++)
    {
        /* Insert */
        if (strncmp(argv[i], "-i", 2) == 0)
        {
            if (i+1<argc && argv[i+1][0] != '-')
            {
                datasrc = argv[++i];
                flags |= FLAG_INSERT_MODE;
            }
            else
              usage();
        }

        /* Extract to file */
        else if (strncmp(argv[i], "-e", 2) == 0)
          flags |= FLAG_EXTRACT_MODE;

        /* Capture Stream */
        else if (strncmp(argv[i], "-c", 2) == 0)
          flags |= FLAG_CAPTURE_MODE;

        /* Source file or stream */
        else if (argv[i][0] != '-')
          fname = argv[i];
    }

    if (!fname)
      usage();

    if (flags & FLAG_INSERT_MODE)
      handle_as_insert(fname, flags, datasrc);
    else if (is_file(fname))
      handle_as_file(fname, flags);
    else
      handle_as_stream(fname, flags);
    
    return 0;
}
