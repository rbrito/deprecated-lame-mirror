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
 * HISTORY: This source file is part of LAME (see http://sulaco.org/mp3/) and
 * was originally adapted by Conrad Sanderson <c.sanderson@me.gu.edu.au> from
 * mp3info by Ricardo Cerqueira <rmc@rccn.net> to write only ID3 version 1 tags.
 * Don Melton <don@blivet.com> COMPLETELY rewrote it to support version 2 tags,
 * be more conformant to other standards while remaining flexible, and be more
 * independent from LAME's APIs and data structures.
 *
 * NOTE: See http://id3.org/ for more information about ID3 tag formats.
 */

#include "id3tag.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static const char *genre_names[] =
{
    /*
     * NOTE: The spelling of these genre names is identical to those found in
     * Winamp and mp3info.
     */
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
    "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
    "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alt. Rock",
    "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
    "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta Rap", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
    "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
    "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk/Rock", "National Folk", "Swing", "Fast-Fusion", "Bebob", "Latin",
    "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech",
    "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
    "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
    "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet",
    "Punk Rock", "Drum Solo", "A Cappella", "Euro-House", "Dance Hall",
    "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
    "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "Synthpop"
};

#define GENRE_NAME_COUNT ((int)(sizeof genre_names / sizeof (const char *)))

static const unsigned char genre_alpha_map [] = {
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

void
id3tag_genre_list(void (*handler)(int, const char *, void *), void *cookie)
{
    if (handler) {
        int i;
        for (i = 0; i < GENRE_NAME_COUNT; ++i) {
            if (i < GENRE_ALPHA_COUNT) {
                int j = genre_alpha_map[i];
                handler(j, genre_names[j], cookie);
            }
        }
    }
}

#define GENRE_NUM_UNKNOWN 255

void
id3tag_init(struct id3tag_spec *spec)
{
    if (spec) {
        memset(spec, 0, sizeof spec);
        spec->genre = GENRE_NUM_UNKNOWN;
    }
}

#define CHANGED_FLAG    (1U << 0)
#define ADD_V2_FLAG     (1U << 1)
#define V1_ONLY_FLAG    (1U << 2)
#define V2_ONLY_FLAG    (1U << 3)
#define SPACE_V1_FLAG   (1U << 4)
#define PAD_V2_FLAG     (1U << 5)

void
id3tag_add_v2(struct id3tag_spec *spec)
{
    if (spec) {
        spec->flags &= ~V1_ONLY_FLAG;
        spec->flags |= ADD_V2_FLAG;
    }
}

void
id3tag_v1_only(struct id3tag_spec *spec)
{
    if (spec) {
        spec->flags &= ~(ADD_V2_FLAG | V2_ONLY_FLAG);
        spec->flags |= V1_ONLY_FLAG;
    }
}

void
id3tag_v2_only(struct id3tag_spec *spec)
{
    if (spec) {
        spec->flags &= ~V1_ONLY_FLAG;
        spec->flags |= V2_ONLY_FLAG;
    }
}

void
id3tag_space_v1(struct id3tag_spec *spec)
{
    if (spec) {
        spec->flags &= ~V2_ONLY_FLAG;
        spec->flags |= SPACE_V1_FLAG;
    }
}

void
id3tag_pad_v2(struct id3tag_spec *spec)
{
    if (spec) {
        spec->flags &= ~V1_ONLY_FLAG;
        spec->flags |= PAD_V2_FLAG;
    }
}

void
id3tag_set_title(struct id3tag_spec *spec, const char *title)
{
    if (spec && title && *title) {
        spec->title = title;
        spec->flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_artist(struct id3tag_spec *spec, const char *artist)
{
    if (spec && artist && *artist) {
        spec->artist = artist;
        spec->flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_album(struct id3tag_spec *spec, const char *album)
{
    if (spec && album && *album) {
        spec->album = album;
        spec->flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_year(struct id3tag_spec *spec, const char *year)
{
    if (spec && year && *year) {
        int num = atoi(year);
        if (num < 0) {
            num = 0;
        }
        /* limit a year to 4 digits so they fit in version 1 tags */
        if (num > 9999) {
            num = 9999;
        }
        if (num) {
            spec->year = num;
            spec->flags |= CHANGED_FLAG;
        }
    }
}

void
id3tag_set_comment(struct id3tag_spec *spec, const char *comment)
{
    if (spec && comment && *comment) {
        spec->comment = comment;
        spec->flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_track(struct id3tag_spec *spec, const char *track)
{
    if (spec && track && *track) {
        int num = atoi(track);
        if (num < 0) {
            num = 0;
        }
        /* limit a track to 99 because CD audio doesn't allow more anyway */
        if (num > 99) {
            num = 99;
        }
        if (num) {
            spec->track = num;
            spec->flags |= CHANGED_FLAG;
        }
    }
}

int
id3tag_set_genre(struct id3tag_spec *spec, const char *genre)
{
    if (spec && genre && *genre) {
        char *str;
        int num = strtol(genre, &str, 10);
        /* is the input a string or a valid number? */
        if (*str) {
            int i;
            for (i = 0; i < GENRE_NAME_COUNT; ++i) {
                if (!local_strcasecmp(genre, genre_names[i])) {
                    num = i;
                    break;
                }
            }
            if (i == GENRE_NAME_COUNT) {
                return -1;
            }
        } else if ((num < 0) || (num >= GENRE_NAME_COUNT)) {
            return -1;
        }
        spec->genre = num;
        spec->flags |= CHANGED_FLAG;
    }
    return 0;
}

static unsigned char *
set_4_byte_value(unsigned char *bytes, unsigned long value)
{
    int index;
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
#define TRACK_FRAME_ID FRAME_ID('T', 'R', 'C', 'K')
#define GENRE_FRAME_ID FRAME_ID('T', 'C', 'O', 'N')
#define COMMENT_FRAME_ID FRAME_ID('C', 'O', 'M', 'M')

static unsigned char *
set_frame(unsigned char *frame, unsigned long id, const char *text,
    size_t length)
{
    if (length) {
        size_t clear;
        frame = set_4_byte_value(frame, id);
        /* calulate size of cleared frame header and field bytes,
         * including 2-byte header flags, 1 encoding descriptor byte,
         * and for comment frames: 3-byte language descriptor and
         * 1 content descriptor byte */
        clear = (id == COMMENT_FRAME_ID) ? 7 : 3;
        /* set frame size = total size - header size */
        frame = set_4_byte_value(frame, (clear - 2) + length);

        /*
         * NOTE: Since the encoding descriptor is set to zero here, that
         * means all text is in ISO-8859-1 format.  And since the language
         * descriptor is also set to zero, that means the language is not
         * specified.
         */

        while (clear--) {
            *frame++ = 0;
        }
        while (length--) {
            *frame++ = *text++;
        }
    }
    return frame;
}

int
id3tag_write_v2(struct id3tag_spec *spec, FILE *stream)
{
    if (!spec || !stream) {
        return -1;
    }
    if ((spec->flags & CHANGED_FLAG) && !(spec->flags & V1_ONLY_FLAG)) {
        /* calculate length of four fields which may not fit in verion 1 tag */
        size_t title_length = spec->title ? strlen(spec->title) : 0;
        size_t artist_length = spec->artist ? strlen(spec->artist) : 0;
        size_t album_length = spec->album ? strlen(spec->album) : 0;
        size_t comment_length = spec->comment ? strlen(spec->comment) : 0;
        /* wrtie tag if explicitly requested or if fields overflow */
        if ((spec->flags & (ADD_V2_FLAG | V2_ONLY_FLAG)) || (title_length > 30)
            || (artist_length > 30) || (album_length > 30)
            || (comment_length > 30) || (spec->track & (comment_length > 28))) {
            size_t tag_size;
            char year[5];
            size_t year_length;
            char track[3];
            size_t track_length;
            const char *genre = NULL;
            size_t genre_length;
            unsigned char *tag;
            unsigned char *p;
            size_t adjusted_tag_size;
            /* calulate size of tag starting with 10-byte tag header */
            tag_size = 10;
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
            if (spec->year) {
                sprintf(year, "%d", spec->year);
                year_length = strlen(year);
                tag_size += 11 + year_length;
            } else {
                year_length = 0;
            }
            if (spec->track) {
                sprintf(track, "%d", spec->track);
                track_length = strlen(track);
                tag_size += 11 + track_length;
            } else {
                track_length = 0;
            }
            if (spec->genre != GENRE_NUM_UNKNOWN) {
                genre = genre_names[spec->genre];
                genre_length = strlen(genre);
                tag_size += 11 + genre_length;
            } else {
                genre_length = 0;
            }
            if (comment_length) {
                /* add 10-byte frame header, 1 encoding descriptor byte,
                 * 3-byte language descriptor, 1 content descriptor byte ... */
                tag_size += 15 + comment_length;
            }
            if (spec->flags & PAD_V2_FLAG) {
                /* add 128 bytes of padding */
                tag_size += 128;
            }
            tag = (unsigned char *)malloc(tag_size);
            if (!tag) {
                return -1;
            }
            p = tag;
            /* set tag header starting with file identifier */
            *p++ = 'I'; *p++ = 'D'; *p++ = '3';
            /* set version number word */
            *p++ = 3; *p++ = 0;
            /* clear flags byte */
            *p++ = 0;
            /* calculate and set tag size = total size - header size */
            adjusted_tag_size = tag_size - 10;
            /* encode adjusted size into four bytes where most significant 
             * bit is clear in each byte, for 28-bit total */
            *p++ = (adjusted_tag_size >> 21) & 0x7fu;
            *p++ = (adjusted_tag_size >> 14) & 0x7fu;
            *p++ = (adjusted_tag_size >> 7) & 0x7fu;
            *p++ = adjusted_tag_size & 0x7fu;

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
            p = set_frame(p, TITLE_FRAME_ID, spec->title, title_length);
            p = set_frame(p, ARTIST_FRAME_ID, spec->artist, artist_length);
            p = set_frame(p, ALBUM_FRAME_ID, spec->album, album_length);
            p = set_frame(p, YEAR_FRAME_ID, year, year_length);
            p = set_frame(p, TRACK_FRAME_ID, track, track_length);
            p = set_frame(p, GENRE_FRAME_ID, genre, genre_length);
            p = set_frame(p, COMMENT_FRAME_ID, spec->comment, comment_length);
            /* clear any padding bytes */
            memset(p, 0, tag_size - (p - tag));
            /* write tag into stream at current position */
            if (fwrite(tag, 1, tag_size, stream) != tag_size) {
                free(tag);
                return -1;
            }
            free(tag);
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
        } else {
            *field++ = pad;
        }
    }
    return field;
}

int
id3tag_write_v1(struct id3tag_spec *spec, FILE *stream)
{
    if (!spec || !stream) {
        return -1;
    }
    if ((spec->flags & CHANGED_FLAG) && !(spec->flags & V2_ONLY_FLAG)) {
        unsigned char tag[128];
        unsigned char *p = tag;
        int pad = (spec->flags & SPACE_V1_FLAG) ? ' ' : 0;
        char year[5];
        /* set tag identifier */
        *p++ = 'T'; *p++ = 'A'; *p++ = 'G';
        /* set each field in tag */
        p = set_text_field(p, spec->title, 30, pad);
        p = set_text_field(p, spec->artist, 30, pad);
        p = set_text_field(p, spec->album, 30, pad);
        sprintf(year, "%d", spec->year);
        p = set_text_field(p, spec->year ? year : NULL, 4, pad);
        /* limit comment field to 28 bytes if a track is specified */
        p = set_text_field(p, spec->comment, spec->track ? 28 : 30, pad);
        if (spec->track) {
            /* clear the next byte to indicate a version 1.1 tag */
            *p++ = 0;
            *p++ = spec->track;
        }
        *p++ = spec->genre;
        /* write tag into stream at current position */
        if (fwrite(tag, 1, sizeof tag, stream) != sizeof tag) {
            return -1;
        }
    }
    return 0;
}
