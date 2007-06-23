/*
 * id3tag.c -- Write ID3 version 1 and 2 tags.
 *
 * Copyright (C) 2000 Don Melton.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * HISTORY: This source file is part of LAME (see http://www.mp3dev.org/mp3/)
 * and was originally adapted by Conrad Sanderson <c.sanderson@me.gu.edu.au>
 * from mp3info by Ricardo Cerqueira <rmc@rccn.net> to write only ID3 version 1
 * tags.  Don Melton <don@blivet.com> COMPLETELY rewrote it to support version
 * 2 tags and be more conformant to other standards while remaining flexible.
 *
 * NOTE: See http://id3.org/ for more information about ID3 tag formats.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef STDC_HEADERS
# include <stddef.h>
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char   *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif


#ifdef _MSC_VER
#define snprintf _snprintf
#endif


#include "lame.h"
#include "machine.h"
#include "encoder.h"
#include "id3tag.h"
#include "lame_global_flags.h"
#include "util.h"
#include "bitstream.h"


static const char *const genre_names[] = {
    /*
     * NOTE: The spelling of these genre names is identical to those found in
     * Winamp and mp3info.
     */
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
    "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
    "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alternative Rock",
    "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
    "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
    "Native US", "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
    "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin",
    "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech",
    "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
    "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
    "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet",
    "Punk Rock", "Drum Solo", "Acappella", "Euro-House", "Dance Hall",
    "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
    "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "SynthPop"
};

#define GENRE_NAME_COUNT \
    ((int)(sizeof genre_names / sizeof (const char *const)))

static const int genre_alpha_map[] = {
    123, 34, 74, 73, 99, 20, 40, 26, 145, 90, 116, 41, 135, 85, 96, 138, 89, 0,
    107, 132, 65, 88, 104, 102, 97, 136, 61, 141, 32, 1, 112, 128, 57, 140, 2,
    139, 58, 3, 125, 50, 22, 4, 55, 127, 122, 120, 98, 52, 48, 54, 124, 25, 84,
    80, 115, 81, 119, 5, 30, 36, 59, 126, 38, 49, 91, 6, 129, 79, 137, 7, 35,
    100, 131, 19, 33, 46, 47, 8, 29, 146, 63, 86, 71, 45, 142, 9, 77, 82, 64,
    133, 10, 66, 39, 11, 103, 12, 75, 134, 13, 53, 62, 109, 117, 23, 108, 92,
    67, 93, 43, 121, 15, 68, 14, 16, 76, 87, 118, 17, 78, 143, 114, 110, 69, 21,
    111, 95, 105, 42, 37, 24, 56, 44, 101, 83, 94, 106, 147, 113, 18, 51, 130,
    144, 60, 70, 31, 72, 27, 28
};

#define GENRE_ALPHA_COUNT ((int)(sizeof genre_alpha_map / sizeof (int)))

#define GENRE_INDEX_OTHER 12

void
id3tag_genre_list(void (*handler) (int, const char *, void *), void *cookie)
{
    if (handler) {
        int     i;
        for (i = 0; i < GENRE_NAME_COUNT; ++i) {
            if (i < GENRE_ALPHA_COUNT) {
                int     j = genre_alpha_map[i];
                handler(j, genre_names[j], cookie);
            }
        }
    }
}

#define GENRE_NUM_UNKNOWN 255

void
id3tag_init(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    memset(&gfc->tag_spec, 0, sizeof gfc->tag_spec);
    gfc->tag_spec.genre_id3v1 = GENRE_NUM_UNKNOWN;
}



void
id3tag_add_v2(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= ADD_V2_FLAG;
}

void
id3tag_v1_only(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~(ADD_V2_FLAG | V2_ONLY_FLAG);
    gfc->tag_spec.flags |= V1_ONLY_FLAG;
}

void
id3tag_v2_only(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= V2_ONLY_FLAG;
}

void
id3tag_space_v1(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~V2_ONLY_FLAG;
    gfc->tag_spec.flags |= SPACE_V1_FLAG;
}

void
id3tag_pad_v2(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= PAD_V2_FLAG;
}


static void
local_strdup( char** dst, const char* src )
{
    if (dst == 0) {
        return;
    }
    if (src == 0) {
        return;
    }
    if (*dst != 0) {
        free(*dst);
    }
    *dst = strdup(src);
}


void
id3tag_set_title(lame_global_flags * gfp, const char *title)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if (title && *title) {
        local_strdup(&gfc->tag_spec.title, title);
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_artist(lame_global_flags * gfp, const char *artist)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if (artist && *artist) {
        local_strdup(&gfc->tag_spec.artist, artist);
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_album(lame_global_flags * gfp, const char *album)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if (album && *album) {
        local_strdup(&gfc->tag_spec.album, album);
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_year(lame_global_flags * gfp, const char *year)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if (year && *year) {
        int     num = atoi(year);
        if (num < 0) {
            num = 0;
        }
        /* limit a year to 4 digits so it fits in a version 1 tag */
        if (num > 9999) {
            num = 9999;
        }
        if (num) {
            gfc->tag_spec.year = num;
            gfc->tag_spec.flags |= CHANGED_FLAG;
        }
    }
}

void
id3tag_set_comment(lame_global_flags * gfp, const char *comment)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if (comment && *comment) {
        local_strdup(&gfc->tag_spec.comment, comment);
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_track(lame_global_flags * gfp, const char *track)
{
    char   *trackcount;
    lame_internal_flags *gfc = gfp->internal_flags;

    if (track && *track) {
        int     num = atoi(track);
        if (num < 0) {
            num = 0;
        }
        /* limit a track to 255 so it fits in a version 1 tag even though CD
         * audio doesn't allow more than 99 tracks */
        if (num > 255) {
            num = 255;
        }
        if (num) {
            gfc->tag_spec.track_id3v1 = num;
            gfc->tag_spec.flags |= CHANGED_FLAG;
        }
        local_strdup(&gfc->tag_spec.track_id3v2, track);

        /* Look for the total track count after a "/", same restrictions */
        trackcount = strchr(track, '/');
        if (trackcount && *trackcount) {
            gfc->tag_spec.flags |= (CHANGED_FLAG | ADD_V2_FLAG);
        }
    }
}

/* would use real "strcasecmp" but it isn't portable */
static int
local_strcasecmp(const char *s1, const char *s2)
{
    unsigned char c1;
    unsigned char c2;
    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        if (!c1) {
            break;
        }
        ++s1;
        ++s2;
    } while (c1 == c2);
    return c1 - c2;
}

int
id3tag_set_genre(lame_global_flags * gfp, const char *genre)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int ret = 0;
    if (genre && *genre) {
        char   *str;
        int     num = strtol(genre, &str, 10);
        /* is the input a string or a valid number? */
        if (*str) {
            int     i;
            for (i = 0; i < GENRE_NAME_COUNT; ++i) {
                if (!local_strcasecmp(genre, genre_names[i])) {
                    num = i;
                    break;
                }
            }
            if (i == GENRE_NAME_COUNT) {
                num = GENRE_INDEX_OTHER;
                ret = 1;
            }
        }
        else {
            if ((num < 0) || (num >= GENRE_NAME_COUNT)) {
                return -1;
            }
            genre = genre_names[num];
        }
        local_strdup(&gfc->tag_spec.genre_id3v2, genre);
        gfc->tag_spec.genre_id3v1 = num;
        gfc->tag_spec.flags |= CHANGED_FLAG;
        if (ret) {
            gfc->tag_spec.flags |= ADD_V2_FLAG;
        }
    }
    return ret;
}


/*
Some existing options for ID3 tag can be specified by --tv option
as follows.
--tt <value>, --tv TIT2=value
--ta <value>, --tv TPE1=value
--tl <value>, --tv TALB=value
--ty <value>, --tv TYER=value
--tn <value>, --tv TRCK=value
--tg <value>, --tv TCON=value
(although some are not exactly same)*/

int
id3tag_set_fieldvalue(lame_global_flags * gfp, const char *fieldvalue)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if (fieldvalue && *fieldvalue) {
        char **p = NULL;
        if (strlen(fieldvalue) < 5 || fieldvalue[4] != '=') {
            return -1;
        }
        p = (char **)realloc(gfc->tag_spec.values, sizeof(char*) * (gfc->tag_spec.num_values + 1));
        if (!p) {
            return -1;
        }
        gfc->tag_spec.values = (char**)p;
        gfc->tag_spec.values[gfc->tag_spec.num_values++] = strdup(fieldvalue);
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
    id3tag_add_v2(gfp);
    return 0;
}

int
id3tag_set_albumart(lame_global_flags* gfp, const char* image, unsigned long size)
{
    int mimetype = 0;
    unsigned char *data = (unsigned char *)image;
    lame_internal_flags *gfc = gfp->internal_flags;

    /* make sure the image size is no larger than the maximum value */
    if (LAME_MAXALBUMART < size) {
        return -1;
    }
    /* determine MIME type from the actual image data */
    if (2 < size && data[0] == 0xFF && data[1] == 0xD8) {
        mimetype = MIMETYPE_JPEG;
    } else if (4 < size && data[0] == 0x89 && strncmp((const char *)&data[1], "PNG", 3) == 0) {
        mimetype = MIMETYPE_PNG;
    } else if (4 < size && strncmp((const char *)data, "GIF8", 4) == 0) { 
        mimetype = MIMETYPE_GIF;
    } else {
        return -1;
    }
    if (gfc->tag_spec.albumart != 0) {
        free(gfc->tag_spec.albumart);
        gfc->tag_spec.albumart = 0;
        gfc->tag_spec.albumart_size = 0;
        gfc->tag_spec.albumart_mimetype = MIMETYPE_NONE;
    }
    if (size < 1) {
        return 0;
    }
    gfc->tag_spec.albumart = malloc(size);
    if (gfc->tag_spec.albumart != 0) {
        memcpy(gfc->tag_spec.albumart, image, size);
        gfc->tag_spec.albumart_size = size;
        gfc->tag_spec.albumart_mimetype = mimetype;
        gfc->tag_spec.flags |= CHANGED_FLAG;
        id3tag_add_v2(gfp);
    }
    return 0;
}

static unsigned char *
set_4_byte_value(unsigned char *bytes, unsigned long value)
{
    int     index;
    for (index = 3; index >= 0; --index) {
        bytes[index] = value & 0xfful;
        value >>= 8;
    }
    return bytes + 4;
}

#define FRAME_ID(a, b, c, d) \
    ( ((unsigned long)(a) << 24) \
    | ((unsigned long)(b) << 16) \
    | ((unsigned long)(c) <<  8) \
    | ((unsigned long)(d) <<  0) )
#define TITLE_FRAME_ID FRAME_ID('T', 'I', 'T', '2')
#define ARTIST_FRAME_ID FRAME_ID('T', 'P', 'E', '1')
#define ALBUM_FRAME_ID FRAME_ID('T', 'A', 'L', 'B')
#define YEAR_FRAME_ID FRAME_ID('T', 'Y', 'E', 'R')
#define COMMENT_FRAME_ID FRAME_ID('C', 'O', 'M', 'M')
#define TRACK_FRAME_ID FRAME_ID('T', 'R', 'C', 'K')
#define GENRE_FRAME_ID FRAME_ID('T', 'C', 'O', 'N')
#define ENCODER_FRAME_ID FRAME_ID('T', 'S', 'S', 'E')
#define PLAYLENGTH_FRAME_ID FRAME_ID('T', 'L', 'E', 'N')

static unsigned char *
set_frame(unsigned char *frame, unsigned long id, const char *text, size_t length)
{
    if (length) {
        unsigned long frame_size = (unsigned long)length;
        frame_size += ((id == COMMENT_FRAME_ID) ? 5 : 1);
        frame = set_4_byte_value(frame, id);
        /* Set frame size = total size - header size.  Frame header and field
         * bytes include 2-byte header flags, 1 encoding descriptor byte, and
         * for comment frames: 3-byte language descriptor and 1 content
         * descriptor byte */
        frame = set_4_byte_value(frame, frame_size);
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        /* clear 1 encoding descriptor byte to indicate ISO-8859-1 format */
        *frame++ = 0;
        if (id == COMMENT_FRAME_ID) {
            /* use id3lib-compatible bogus language descriptor */
            *frame++ = 'X';
            *frame++ = 'X';
            *frame++ = 'X';
            /* clear 1 byte to make content descriptor empty string */
            *frame++ = 0;
        }
        while (length--) {
            *frame++ = *text++;
        }
    }
    return frame;
}

static unsigned char *
set_frame_custom(unsigned char *frame, const char *fieldvalue)
{
    if (fieldvalue && *fieldvalue) {
        const char *value = fieldvalue + 5;
        size_t length = strlen(value);
        *frame++ = *fieldvalue++;
        *frame++ = *fieldvalue++;
        *frame++ = *fieldvalue++;
        *frame++ = *fieldvalue++;
        frame = set_4_byte_value(frame, (unsigned long)(strlen(value) + 1));
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        /* clear 1 encoding descriptor byte to indicate ISO-8859-1 format */
        *frame++ = 0;
        while (length--) {
            *frame++ = *value++;
        }
    }
    return frame;
}

static unsigned char *
set_frame_apic(unsigned char *frame, const char *mimetype, const unsigned char *data, size_t size)
{
    /* ID3v2.3 standard APIC frame:
     *     <Header for 'Attached picture', ID: "APIC">
     *     Text encoding    $xx
     *     MIME type        <text string> $00
     *     Picture type     $xx
     *     Description      <text string according to encoding> $00 (00)
     *     Picture data     <binary data>
     */
    if (mimetype && data && size) {
        frame = set_4_byte_value(frame, FRAME_ID('A', 'P', 'I', 'C'));
        frame = set_4_byte_value(frame, (unsigned long)(4 + strlen(mimetype) + size));
        /* clear 2-byte header flags */
        *frame++ = 0;
        *frame++ = 0;
        /* clear 1 encoding descriptor byte to indicate ISO-8859-1 format */
        *frame++ = 0;
        /* copy mime_type */
        while (*mimetype) {
            *frame++ = *mimetype++;
        }
        *frame++ = 0;
        /* set picture type to 0 */
        *frame++ = 0;
        /* empty description field */
        *frame++ = 0;
        /* copy the image data */
        while (size--) {
            *frame++ = *data++;
        }
    }
    return frame;
}

int
id3tag_write_v2(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if ((gfc->tag_spec.flags & CHANGED_FLAG)
        && !(gfc->tag_spec.flags & V1_ONLY_FLAG)) {
        /* calculate length of four fields which may not fit in verion 1 tag */
        size_t  title_length = gfc->tag_spec.title ? strlen(gfc->tag_spec.title) : 0;
        size_t  artist_length = gfc->tag_spec.artist ? strlen(gfc->tag_spec.artist) : 0;
        size_t  album_length = gfc->tag_spec.album ? strlen(gfc->tag_spec.album) : 0;
        size_t  comment_length = gfc->tag_spec.comment ? strlen(gfc->tag_spec.comment) : 0;
        /* write tag if explicitly requested or if fields overflow */
        if ((gfc->tag_spec.flags & (ADD_V2_FLAG | V2_ONLY_FLAG))
            || (title_length > 30)
            || (artist_length > 30) || (album_length > 30)
            || (comment_length > 30)
            || (gfc->tag_spec.track_id3v1 && (comment_length > 28))) {
            size_t  tag_size;
            char    encoder[80];
            size_t  encoder_length;
            unsigned long playlength_ms;
            char    playlength[20];
            size_t  playlength_length;
            char    year[5];
            size_t  year_length;
            size_t  track_length;
            size_t  genre_length = 0;
            unsigned char *tag;
            unsigned char *p;
            size_t  adjusted_tag_size;
            unsigned int index;
            const char *albumart_mime = NULL;
            static const char *mime_jpeg = "image/jpeg";
            static const char *mime_png = "image/png";
            static const char *mime_gif = "image/gif";
            /* calculate playlength in milliseconds */
            {
                double const max_ulong = MAX_U_32_NUM;
                double ms = gfp->num_samples;
                ms *= 1000;
                ms /= gfp->in_samplerate;
                if (ms > max_ulong) {
                    playlength_ms = max_ulong;
                }
                else if (ms < 0) {
                    playlength_ms = 0;
                }
                else {
                    playlength_ms = ms;
                }
            }
            /* calulate size of tag starting with 10-byte tag header */
            tag_size = 10;
#if defined(__hpux) || defined(__svr4__) || defined(M_UNIX) || defined(_AIX)
            encoder_length = sprintf(encoder, "LAME v%s", get_lame_version());
            if (encoder_length + 1 > sizeof(encoder))
                abort();
            playlength_length = sprintf(playlength, "%lu", playlength_ms);
            if (playlength_length + 1 > sizeof(playlength))
                abort();
#else
#if defined(__sun__)
            (void) sprintf(encoder, "LAME v%s", get_lame_version());
            encoder_length = strlen(encoder);
            (void) sprintf(playlength, "%lu", playlength_ms);
            playlength_length = strlen(playlength);
#else
            encoder_length = snprintf(encoder, sizeof(encoder),
                                      "LAME v%s", get_lame_version());
            playlength_length = snprintf(playlength, sizeof(playlength), "%lu", playlength_ms);
#endif
#endif
            tag_size += 11 + encoder_length;
            tag_size += 11 + playlength_length;
            if (title_length) {
                /* add 10-byte frame header, 1 encoding descriptor byte ... */
                tag_size += 11 + title_length;
            }
            if (artist_length) {
                tag_size += 11 + artist_length;
            }
            if (album_length) {
                tag_size += 11 + album_length;
            }
            if (gfc->tag_spec.year) {
                year_length = sprintf(year, "%d", gfc->tag_spec.year);
                if (year_length + 1 > sizeof(year))
                    abort();
                tag_size += 11 + year_length;
            }
            else {
                year_length = 0;
            }
            if (comment_length) {
                /* add 10-byte frame header, 1 encoding descriptor byte,
                 * 3-byte language descriptor, 1 content descriptor byte ... */
                tag_size += 15 + comment_length;
            }
            if (gfc->tag_spec.track_id3v2 != 0) {
                track_length = strlen(gfc->tag_spec.track_id3v2);
                tag_size += 11 + track_length;
            }
            else {
                track_length = 0;
            }
            if (gfc->tag_spec.genre_id3v2 != 0) {
                genre_length = strlen(gfc->tag_spec.genre_id3v2);
                tag_size += 11 + genre_length;
            }
            else {
                genre_length = 0;
            }
            for (index = 0;index < gfc->tag_spec.num_values;++index) {
                tag_size += 6 + strlen(gfc->tag_spec.values[index]);
            }
            if (gfc->tag_spec.albumart && gfc->tag_spec.albumart_size) {
                switch (gfc->tag_spec.albumart_mimetype) {
                case MIMETYPE_JPEG:
                    albumart_mime = mime_jpeg;
                    break;
                case MIMETYPE_PNG:
                    albumart_mime = mime_png;
                    break;
                case MIMETYPE_GIF:
                    albumart_mime = mime_gif;
                    break;
                }
                tag_size += 10 + 4 + strlen(albumart_mime) + gfc->tag_spec.albumart_size;
            }
            if (gfc->tag_spec.flags & PAD_V2_FLAG) {
                /* add 128 bytes of padding */
                tag_size += 128;
            }
            tag = (unsigned char *) malloc(tag_size);
            if (!tag) {
                return -1;
            }
            p = tag;
            /* set tag header starting with file identifier */
            *p++ = 'I';
            *p++ = 'D';
            *p++ = '3';
            /* set version number word */
            *p++ = 3;
            *p++ = 0;
            /* clear flags byte */
            *p++ = 0;
            /* calculate and set tag size = total size - header size */
            adjusted_tag_size = tag_size - 10;
            /* encode adjusted size into four bytes where most significant 
             * bit is clear in each byte, for 28-bit total */
            *p++ = (unsigned char)((adjusted_tag_size >> 21) & 0x7fu);
            *p++ = (unsigned char)((adjusted_tag_size >> 14) & 0x7fu);
            *p++ = (unsigned char)((adjusted_tag_size >> 7) & 0x7fu);
            *p++ = (unsigned char)(adjusted_tag_size & 0x7fu);

            /*
             * NOTE: The remainder of the tag (frames and padding, if any)
             * are not "unsynchronized" to prevent false MPEG audio headers
             * from appearing in the bitstream.  Why?  Well, most players
             * and utilities know how to skip the ID3 version 2 tag by now
             * even if they don't read its contents, and it's actually
             * very unlikely that such a false "sync" pattern would occur
             * in just the simple text frames added here.
             */

            /* set each frame in tag */
            p = set_frame(p, ENCODER_FRAME_ID, encoder, encoder_length);
            p = set_frame(p, PLAYLENGTH_FRAME_ID, playlength, playlength_length);
            p = set_frame(p, TITLE_FRAME_ID, gfc->tag_spec.title, title_length);
            p = set_frame(p, ARTIST_FRAME_ID, gfc->tag_spec.artist, artist_length);
            p = set_frame(p, ALBUM_FRAME_ID, gfc->tag_spec.album, album_length);
            p = set_frame(p, YEAR_FRAME_ID, year, year_length);
            p = set_frame(p, COMMENT_FRAME_ID, gfc->tag_spec.comment, comment_length);
            p = set_frame(p, TRACK_FRAME_ID, gfc->tag_spec.track_id3v2, track_length);
            p = set_frame(p, GENRE_FRAME_ID, gfc->tag_spec.genre_id3v2, genre_length);
            p = set_frame_apic(p, albumart_mime, gfc->tag_spec.albumart, gfc->tag_spec.albumart_size);
            for (index = 0;index < gfc->tag_spec.num_values;++index) {
                p = set_frame_custom(p, gfc->tag_spec.values[index]);
            }
            /* clear any padding bytes */
            memset(p, 0, tag_size - (p - tag));
            /* write tag directly into bitstream at current position */
            for (index = 0; index < tag_size; ++index) {
                add_dummy_byte(gfp, tag[index], 1);
            }
            free(tag);
            return (int)tag_size;
        }
    }
    return 0;
}

static unsigned char *
set_text_field(unsigned char *field, const char *text, size_t size, int pad)
{
    while (size--) {
        if (text && *text) {
            *field++ = *text++;
        }
        else {
            *field++ = pad;
        }
    }
    return field;
}

int
id3tag_write_v1(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    if ((gfc->tag_spec.flags & CHANGED_FLAG)
        && !(gfc->tag_spec.flags & V2_ONLY_FLAG)) {
        unsigned char tag[128];
        unsigned char *p = tag;
        int     pad = (gfc->tag_spec.flags & SPACE_V1_FLAG) ? ' ' : 0;
        char    year[5];
        int     year_length;
        unsigned int index;
        /* set tag identifier */
        *p++ = 'T';
        *p++ = 'A';
        *p++ = 'G';
        /* set each field in tag */
        p = set_text_field(p, gfc->tag_spec.title, 30, pad);
        p = set_text_field(p, gfc->tag_spec.artist, 30, pad);
        p = set_text_field(p, gfc->tag_spec.album, 30, pad);
        year_length = sprintf(year, "%d", gfc->tag_spec.year);
        if (year_length + 1 > sizeof(year))
            abort();
        p = set_text_field(p, gfc->tag_spec.year ? year : NULL, 4, pad);
        /* limit comment field to 28 bytes if a track is specified */
        p = set_text_field(p, gfc->tag_spec.comment, gfc->tag_spec.track_id3v1 ? 28 : 30, pad);
        if (gfc->tag_spec.track_id3v1) {
            /* clear the next byte to indicate a version 1.1 tag */
            *p++ = 0;
            *p++ = gfc->tag_spec.track_id3v1;
        }
        *p++ = gfc->tag_spec.genre_id3v1;
        /* write tag directly into bitstream at current position */
        for (index = 0; index < 128; ++index) {
            add_dummy_byte(gfp, tag[index], 1);
        }
        return 128;
    }
    return 0;
}
