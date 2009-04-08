/******************************************************************************
 * stream.c 
 *
 * mp3nema - MP3 analysis and data hiding utility
 *
 * Copyright (C) 2009 Matt Davis (enferex) of 757Labs (www.757labs.com)
 *
 * stream.c is part of mp3nema.
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
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "main.h"
#include "utils.h"


/* Globals so we can gracefully exit */
static FILE             *insert_fp = NULL;   /* File   */
static const int        *insert_sd = NULL;   /* Socket */
static const hostdata_t *insert_host = NULL; /* Host   */


/* Gracefully exit if the user kills us */
static void signal_handler(int signum)
{
    if (insert_fp)
      fclose(insert_fp);
    if (insert_sd)
      close(*insert_sd);
    if (insert_host)
    {
        free(insert_host->file);
        free(insert_host->host);
        free(insert_host->port);
    }
    
    printf("\n" TAG " session gracefully terminated\n");
    fflush(stdout);

    exit(0);
}


static int connect_host(const char *host, int port)
{
    int                 sd;
    struct hostent     *hostname;
    struct sockaddr_in  addr;

    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
      return 0;

    if (!(hostname = gethostbyname(host)))
      return 0;

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, hostname->h_addr, hostname->h_length);

    if (connect(sd, (const struct sockaddr *)&addr,
                sizeof(struct sockaddr_in)) == -1)
      return 0;

    return sd;
}


static void suck_data_from_stream(
    int         sockfd,
    FILE       *savefp,
    flags_t     flags,
    const char *host)
{
    static const int brain_sz = DEFAULT_BLK_SZ * 4;

    int            ignore_oob, index;
    int            recv_sz, curr_brain_sz, frame_length;
    char           data[DEFAULT_BLK_SZ], brain[brain_sz];
    FILE          *oob_file;
    STREAM_OBJECT  type;
    mp3_frame_t    frame;
    id3_tag_t      id3_tag;
    
    /* If we want to store oob data */ 
    oob_file = NULL;
    if (flags & FLAG_EXTRACT_MODE)
      oob_file = util_create_file(host, "extracted-oob", "dat", 0);

    /* Grab data from stream (don't analyize first chunk) */
    ignore_oob = 1;
    index = 0;
    curr_brain_sz = 0;
    while ((recv_sz = read(sockfd, data, sizeof(data))) > 0)
    {
        /* Once buffer is full analyize all frames */
        if (curr_brain_sz + recv_sz >= brain_sz)
        {
            for ( ;; )
            {
                type = util_next_mp3_frame_or_id3v2(NULL, brain, curr_brain_sz,
                                                    ignore_oob, &index,
                                                    oob_file);

                /* At the end if UNKNOWN or our length does not match 
                 * amount in buffer * continue gathering 
                 */
                if (type == STREAM_OBJECT_UNKNOWN)
                {
                    curr_brain_sz = 0;
                    ignore_oob = 1;
                    break;
                }
                else if (type == STREAM_OBJECT_MP3_FRAME)
                {
                     mp3_set_header(&frame, brain + index);
                     frame_length = frame.audio_size + frame.header_size;
#ifdef DEBUG
                     printf("frame: %d\n", frame_length);
#endif
                   /* Gather more data */
                   if (frame_length >= curr_brain_sz)
                       break;

                   /* Remove the frame and continue analyizing */
                   else if (frame_length != 0)
                   {
                       frame_length += index;
                       memmove(brain, brain + frame_length,
                               brain_sz - frame_length);
                       curr_brain_sz -= frame_length;
                    }
                    else
                    {
                        curr_brain_sz = 0;
                        ignore_oob = 1;
                        break;
                    }
                }
                else if (type == STREAM_OBJECT_ID3V2_TAG)
                {
                    id3_set_header(&id3_tag, brain + index);
                    id3_tag.size += 10 + ((id3_tag.footer) ? 10 : 0);
                  
                    /* Gather more data */
                    if (id3_tag.size >= curr_brain_sz)
                      break;

                    /* Remove the tag and continue analyizing */
                    else if (id3_tag.size != 0)
                    {
#ifdef DEBUG
                     printf("tag: %d\n", id3_tag.size);
#endif
                       id3_tag.size += index;
                       memmove(brain, brain + id3_tag.size,
                               brain_sz - id3_tag.size);
                       curr_brain_sz -= id3_tag.size;
                    }
                    else
                    {
                        curr_brain_sz = 0;
                        ignore_oob = 1;
                        break;
                    }
                }
                ignore_oob = 0;
            }
        }

        /* Add data to buffer */
        if (curr_brain_sz + recv_sz > brain_sz)
        {
            curr_brain_sz = 0;
            continue;
        }

        memcpy(brain + curr_brain_sz, data, recv_sz);
        curr_brain_sz += recv_sz;

        if (flags & FLAG_CAPTURE_MODE)
          fwrite(data, recv_sz, 1, savefp);
    }
}


static int get_stream_info(flags_t flags, int sd, const hostdata_t *host)
{
    static const int buf_blk_sz = DEFAULT_BLK_SZ;

    int            ret, redirected, total_sz, recv_sz, n_buf_blks;
    char          *buf, *c, query[DEFAULT_BLK_SZ];
    char           data[DEFAULT_BLK_SZ];
    FILE          *fp;
    fd_set         readfds;
    hostdata_t     newhost;
    struct timeval tv;

    snprintf(query, sizeof(query), 
             "GET %s HTTP/1.0\r\nHost: %s:%s\r\n\r\n",
             host->file, host->host, host->port);

#ifdef DEBUG
        printf("query: %s", query);
#endif

    if (write(sd, query, strlen(query)) < 1)
      return 0;

    /* Query for stream info */
    fp = NULL;
    total_sz = 0;
    n_buf_blks = 0;
    buf = NULL;
    redirected = 0;

    FD_ZERO(&readfds);
    FD_SET(sd, &readfds);
    memset(&tv, 0, sizeof(struct timeval));
    tv.tv_sec = 3;

    for ( ;; )
    {
        ret = select(sd+1, &readfds, NULL, NULL, &tv);
        if (ret == -1)
        {
            ERR("Could not establish connection to remote host\n");
            break;
        }
        else if (!ret)
          break;
        
        /* Socket is ready ... */
        if ((recv_sz = read(sd, data, sizeof(data))) > 0 )
        {
            if ((recv_sz + total_sz) >= (n_buf_blks * buf_blk_sz))
              buf = realloc(buf, (++n_buf_blks) * buf_blk_sz);

            memcpy(buf + total_sz, data, recv_sz);
            total_sz += recv_sz;

            if ((total_sz > 4) && (strncmp(buf, "HTTP", 4) == 0))
              redirected = 1;
        }

        if (recv_sz == 0 || !redirected)
          break;
    }

    /* Get the potential new host from the just read in data */
    if (redirected && buf && !(c = strstr(buf, "http://")))
    {
        free(buf);
        return 0;
    }
    else if (redirected && buf)
    {
        strtok(c, "\n");
        util_url_to_host_port_file(c, &newhost);
    }

    free(buf);

    /* Disconnect here and contact server in m3u/pls */
    if (redirected)
    {
        close(sd);
        if (!(sd = connect_host(newhost.host, newhost.portnum)))
          return 0;

        /* Get data from new host */
        snprintf(query, sizeof(query),
                 "GET %s HTTP/1.0\r\nHost: %s:%s\r\n\r\n",
                 newhost.file, newhost.host, newhost.port);
#ifdef DEBUG
            printf("query: %s", query);
#endif
        free(newhost.file);
        free(newhost.host);
        free(newhost.port);
    
        /* Query */
        if (write(sd, query, strlen(query)) < 1)
          return 0;
    }

    /* Create file to capture stream to */
    if ((flags & FLAG_CAPTURE_MODE) &&
        (!(fp = util_create_file(host->host, "captured-stream", "mp3", 1))))
      return 0;

    /* Pull data from stream and analyize */
    insert_host = host;
    insert_fp = fp;
    suck_data_from_stream(sd, fp, flags, host->host);

    if (fp)
      fclose(fp);

    return 1;
}


void handle_as_stream(const char *url, flags_t flags)
{
    int        sd;
    hostdata_t host;

    util_url_to_host_port_file(url, &host);
    
    if (!(sd = connect_host(host.host, host.portnum)))
      return;

    /* Gracefully quit */
    insert_sd = &sd;
    signal(SIGINT, signal_handler);

    if (!(get_stream_info(flags, sd, &host)))
      return;

    /* Disconnect */
    close(sd);
}
