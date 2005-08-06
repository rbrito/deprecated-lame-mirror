/*
 * tags.c --
 *      Write ID3 version 1 and 2 tags, Xing VBR tag, and LAME tag.
 *
 * Copyright (C) 1999 A.L. Faber
 *           (C) 2000 Don Melton.
 *           (c) 2004 Takehiro TOMINAGA
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
 * id3 code HISTORY:
 * This source file is part of LAME (see http://www.mp3dev.org/mp3/)
 * and was originally adapted by Conrad Sanderson <c.sanderson@me.gu.edu.au>
 * from mp3info by Ricardo Cerqueira <rmc@rccn.net> to write only ID3 version 1
 * tags.  Don Melton <don@blivet.com> COMPLETELY rewrote it to support version
 * 2 tags and be more conformant to other standards while remaining flexible.
 *
 * Support for UTF-8 and UCS-2 tags was added by Edmund Grimley Evans in
 * April 2005.
 *
 * NOTE: See http://id3.org/ for more information about ID3 tag formats.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

#include <assert.h>
#ifdef __sun__
/* woraround for SunOS 4.x, it has SEEK_* defined here */
# include <unistd.h>
#endif
#ifdef STDC_HEADERS
# include <stddef.h>
# include <stdlib.h>
# include <string.h>
#endif

#include "encoder.h"
#include "util.h"
#include "bitstream.h"
#include "tags.h"

static const char *const
genre_names[] = {
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

#define GENRE_NAME_COUNT \
    ((int)(sizeof genre_names / sizeof (const char *const)))

static const int genre_alpha_map [] = {
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

#define CHANGED_FLAG    (1U << 0)
#define ADD_V2_FLAG     (1U << 1)
#define V1_ONLY_FLAG    (1U << 2)
#define V2_ONLY_FLAG    (1U << 3)
#define SPACE_V1_FLAG   (1U << 4)
#define PAD_V2_FLAG     (1U << 5)

static uint8_t*
CreateI4(uint8_t *buf, int nValue)
{
    *buf++ = nValue >> 24;
    *buf++ = nValue >> 16;
    *buf++ = nValue >>  8;
    *buf++ = nValue;
    return buf;
}

static uint8_t*
CreateI2(uint8_t *buf, int nValue)
{
    *buf++= nValue >> 8;
    *buf++ = nValue;
    return buf;
}


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
id3tag_init(lame_t gfc)
{
    memset(&gfc->tag_spec, 0, sizeof gfc->tag_spec);
    gfc->tag_spec.genre = GENRE_NUM_UNKNOWN;
}

void
id3tag_u8(lame_t gfc)
{
    gfc->tag_spec.utf8 = 1;
}

void
id3tag_add_v2(lame_t gfc)
{
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= ADD_V2_FLAG;
}

void
id3tag_v1_only(lame_t gfc)
{
    gfc->tag_spec.flags &= ~(ADD_V2_FLAG | V2_ONLY_FLAG);
    gfc->tag_spec.flags |= V1_ONLY_FLAG;
}

void
id3tag_v2_only(lame_t gfc)
{
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= V2_ONLY_FLAG;
}

void
id3tag_space_v1(lame_t gfc)
{
    gfc->tag_spec.flags &= ~V2_ONLY_FLAG;
    gfc->tag_spec.flags |= SPACE_V1_FLAG;
}

void
id3tag_pad_v2(lame_t gfc)
{
    gfc->tag_spec.flags &= ~V1_ONLY_FLAG;
    gfc->tag_spec.flags |= PAD_V2_FLAG;
}

void
id3tag_set_title(lame_t gfc, const char *title)
{
    if (title && *title) {
        gfc->tag_spec.title = title;
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_artist(lame_t gfc, const char *artist)
{
    if (artist && *artist) {
        gfc->tag_spec.artist = artist;
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_album(lame_t gfc, const char *album)
{
    if (album && *album) {
        gfc->tag_spec.album = album;
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_year(lame_t gfc, const char *year)
{
    if (year && *year) {
        int num = atoi(year);
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
id3tag_set_comment(lame_t gfc, const char *comment)
{
    if (comment && *comment) {
        gfc->tag_spec.comment = comment;
        gfc->tag_spec.flags |= CHANGED_FLAG;
    }
}

void
id3tag_set_track(lame_t gfc, const char *track)
{
    int num, totalnum, n;
    if (!track)
	return;

    n = sscanf(track, "%d/%d", &num, &totalnum);
    if (n >= 1) {
	gfc->tag_spec.tracknum = num;
	if (n >= 2 || num > 255) {
	    gfc->tag_spec.flags |= ADD_V2_FLAG;
	}
	gfc->tag_spec.track = track;  /* for ID3v2 tag */
	gfc->tag_spec.flags |= CHANGED_FLAG;
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
id3tag_set_genre(lame_t gfc, const char *genre)
{
    if (genre && *genre) {
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
                return LAME_GENERICERROR;
            }
        } else if ((num < 0) || (num >= GENRE_NAME_COUNT)) {
            return LAME_GENERICERROR;
        }
        gfc->tag_spec.genre = num;
        gfc->tag_spec.flags |= CHANGED_FLAG;
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
#define COMMENT_FRAME_ID FRAME_ID('C', 'O', 'M', 'M')
#define TRACK_FRAME_ID FRAME_ID('T', 'R', 'C', 'K')
#define GENRE_FRAME_ID FRAME_ID('T', 'C', 'O', 'N')
#define ENCODER_FRAME_ID FRAME_ID('T', 'S', 'S', 'E')
#define TITLE_LENGTH_ID FRAME_ID('T', 'L', 'E', 'N')

unsigned int
read_utf8(const char **ptext, size_t *plength)
{
    const char *text = *ptext;
    size_t length = *plength;
    unsigned int wc;
    int ulen;
    unsigned char c;
    if (!length) {
        return 0;
    }
    ulen = 1;
    wc = '?';
    c = *text;
    if (c < 0x80) {
        wc = c;
    } else if (c < 0xc0) {
    } else if (c < 0xe0) {
        ulen = 2;
    } else if (c < 0xf0) {
        ulen = 3;
    } else if (c < 0xf8) {
        ulen = 4;
    } else if (c < 0xfc) {
        ulen = 5;
    } else if (c < 0xfe) {
        ulen = 6;
    }
    if (ulen > length) {
        ulen = 1;
    }
    if (ulen > 1) {
        int i;
        wc = c & ((1 << (7 - ulen)) - 1);
        for (i = 1; i < ulen; i++) {
          c = text[i];
          if (c < 0x80 || c > 0xbf) {
              ulen = 1;
              wc = '?';
              break;
          }
          wc = (wc << 6) | (c & 0x3f);
        }
    }
    *ptext = text + ulen;
    *plength = length - ulen;
    return wc;
}

static size_t
convert_to_ucs2(unsigned char *p, int *platin1, const char *text, size_t length)
{
    size_t len = 2; /* BOM */
    int latin1 = 1;
    if (p) {
        *p++ = 0xfe;
        *p++ = 0xff;
    }
    while (length) {
        unsigned int wc = read_utf8(&text, &length);
        if (wc > 255) {
            latin1 = 0;
        }
        if (p) {
            if (wc > 65535) {
                wc = '?';
            }
            *p++ = wc >> 8;
            *p++ = wc;
        }
        len += 2;
    }
    if (platin1) {
        *platin1 = latin1;
    }
    return len;
}

static size_t
convert_to_latin1(unsigned char *p, const char *text, size_t length)
{
    size_t len = 0;
    while (length) {
        unsigned int wc = read_utf8(&text, &length);
        if (p) {
            if (wc > 255) {
                wc = '?';
            }
            *p++ = wc;
        }
        ++len;
    }
    return len;
}

typedef struct frame_struct_t {
    unsigned long id; /* type of frame */
    const char *text; /* pointer to original string */
    int utf;          /* text encoding: 0 = ISO-8859-1, 1 = UTF-8 */
    int encoding;     /* target encoding: 0 = ISO-8859-1, 1 = UCS-2 */
    size_t v1_len;    /* length of string, or 999 if the string is not pure ISO-8859-1 */
    size_t length;    /* length of frame including header */
} frame_struct;

static void
init_frame_struct(frame_struct *frame, unsigned long id, const char *text, int utf)
{
    frame->id = id;
    frame->text = text;
    frame->utf = utf;
    if (text) {
        size_t length = strlen(text);
        if (utf) {
            size_t ucs_length;
            int latin1;
            ucs_length = convert_to_ucs2(0, &latin1, text, length);
            if (latin1) {
                frame->encoding = 0;
                frame->v1_len = convert_to_latin1(0, text, length);
                frame->length = frame->v1_len + 11;
            } else {
                frame->encoding = 1;
                frame->v1_len = 999;
                frame->length = ucs_length + 11;
            }
        } else {
            frame->encoding = 0;
            frame->v1_len = length;
            frame->length = length + 11;
        }
        if (frame->id == COMMENT_FRAME_ID) {
            frame->length += 4 + frame->encoding;
        }
    } else {
        frame->encoding = 0;
        frame->v1_len = 0;
        frame->length = 0;
    }
}

static unsigned char *
write_frame(unsigned char *p, frame_struct *frame)
{
    unsigned char *p0 = p;
    if (frame->length) {
        const char *text = frame->text;
        size_t length = strlen(text);
        p = set_4_byte_value(p, frame->id);
        p = set_4_byte_value(p, frame->length - 10);
        /* clear 2-byte header flags */
        *p++ = 0;
        *p++ = 0;
        *p++ = frame->encoding;
        if (frame->id == COMMENT_FRAME_ID) {
            /* use id3lib-compatible bogus language descriptor */
            *p++ = 'X';
            *p++ = 'X';
            *p++ = 'X';
            /* clear 1 or 2 bytes to make content descriptor empty string */
            *p++ = 0;
            if (frame->encoding) {
                *p++ = 0;
            }
        }
        if (frame->utf) {
            if (frame->encoding) {
                p += convert_to_ucs2(p, 0, text, length);
            } else {
                p += convert_to_latin1(p, text, length);
            }
        } else {
            memcpy(p, text, length);
            p += length;
        }
    }
    if (frame->length != p - p0) {
        fprintf(stderr, "Internal error: %d != %d\n",
		(int)frame->length, (int)(p - p0));
        exit(1);
    }
    return p;
}

/*
 * NOTE: A version 2 tag will NOT be added unless one of the text fields won't
 * fit in a version 1 tag (e.g. the title string is longer than 30 characters),
 * or the "id3tag_add_v2" or "id3tag_v2_only" functions are used.
 */
int
id3tag_write_v2(lame_t gfc, unsigned char *buf, size_t size)
{
    if ((gfc->tag_spec.flags & CHANGED_FLAG)
            && !(gfc->tag_spec.flags & V1_ONLY_FLAG)) {
        frame_struct encoder, title, artist, album;
        frame_struct year, comment, track, genre;
        char encoder_buf[20], year_buf[5], genre_buf[6];
        int utf8 = gfc->tag_spec.utf8;
        sprintf(encoder_buf, "LAME v%.13s", get_lame_short_version());
        init_frame_struct(&encoder, ENCODER_FRAME_ID, encoder_buf, 0);
        init_frame_struct(&title, TITLE_FRAME_ID,
                          gfc->tag_spec.title, utf8);
        init_frame_struct(&artist, ARTIST_FRAME_ID,
                          gfc->tag_spec.artist, utf8);
        init_frame_struct(&album, ALBUM_FRAME_ID,
                          gfc->tag_spec.album, utf8);
        sprintf(year_buf, "%d", gfc->tag_spec.year);
        init_frame_struct(&year, YEAR_FRAME_ID,
                          gfc->tag_spec.year ? year_buf : 0, 0);
        init_frame_struct(&comment, COMMENT_FRAME_ID,
                          gfc->tag_spec.comment, utf8);
        init_frame_struct(&track, TRACK_FRAME_ID, gfc->tag_spec.track, 0);
        sprintf(genre_buf, "(%d)", gfc->tag_spec.genre);
        init_frame_struct(&genre, GENRE_FRAME_ID,
                          gfc->tag_spec.genre != GENRE_NUM_UNKNOWN ?
                          genre_buf : 0, 0);
        /* write tag if explicitly requested or if fields overflow */
        if ((gfc->tag_spec.flags & (ADD_V2_FLAG | V2_ONLY_FLAG))
	    || title.v1_len > 30 || artist.v1_len > 30 || album.v1_len > 30
	    || comment.v1_len > 30
	    || (gfc->tag_spec.tracknum && comment.v1_len > 28)
	    || gfc->tag_spec.tracknum > 255 || gfc->tag_spec.track) {
            size_t tag_size;
            unsigned char *tag;
            unsigned char *p;
            size_t adjusted_tag_size;
            /* calulate size of tag starting with 10-byte tag header */
            tag_size = 10 + encoder.length + title.length +
              artist.length + album.length + year.length +
              comment.length + track.length + genre.length;
            if (gfc->tag_spec.flags & PAD_V2_FLAG) {
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
            p = write_frame(p, &encoder);
            p = write_frame(p, &title);
            p = write_frame(p, &artist);
            p = write_frame(p, &album);
            p = write_frame(p, &year);
            p = write_frame(p, &comment);
            p = write_frame(p, &track);
            p = write_frame(p, &genre);
            /* clear any padding bytes */
            memset(p, 0, tag_size - (p - tag));
            /* output tags into output buffer */
	    if (size && size <= tag_size)
		return -1;

	    memcpy(buf, tag, tag_size);
            free(tag);
            return tag_size;
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
id3tag_write_v1(lame_t gfc, unsigned char *buf, size_t size)
{
    unsigned char tag[128], *p = tag, year[5];
    int pad = (gfc->tag_spec.flags & SPACE_V1_FLAG) ? ' ' : 0;

    if (!(gfc->tag_spec.flags & CHANGED_FLAG)
	|| (gfc->tag_spec.flags & V2_ONLY_FLAG))
	return 0;

    if (size != 0 && size < 128)
	return LAME_INSUFFICIENTBUF; /* buffer overrun ! */

    /* set tag identifier */
    *p++ = 'T'; *p++ = 'A'; *p++ = 'G';
    /* set each field in tag */
    p = set_text_field(p, gfc->tag_spec.title, 30, pad);
    p = set_text_field(p, gfc->tag_spec.artist, 30, pad);
    p = set_text_field(p, gfc->tag_spec.album, 30, pad);
    sprintf(year, "%d", gfc->tag_spec.year);
    p = set_text_field(p, gfc->tag_spec.year ? year : NULL, 4, pad);
    p = set_text_field(p, gfc->tag_spec.comment, 30, pad);
    /* limit comment field to 28 bytes if a track is specified */
    /* clear the last 2 bytes to indicate a version 1.1 tag */
    if (gfc->tag_spec.tracknum <= 255) {
	p[-2] = 0;
	p[-1] = gfc->tag_spec.tracknum;
    }
    *p++ = gfc->tag_spec.genre;
    /* write tag directly into bitstream at current position */
    memcpy(buf, tag, 128);
    return 128;
}

/*
 *    4 bytes for Header Tag
 *    4 bytes for Header Flags
 *  100 bytes for entry (NUMTOCENTRIES)
 *    4 bytes for FRAME SIZE
 *    4 bytes for STREAM_SIZE
 *    4 bytes for VBR SCALE. a VBR quality indicator: 0=best 100=worst
 *   20 bytes for LAME tag.  for example, "LAME3.12 (beta 6)"
 * ___________
 *  140 bytes
 */
#define VBRHEADERSIZE (NUMTOCENTRIES+4+4+4+4+4)

#define LAMEHEADERSIZE (VBRHEADERSIZE + 9 + 1 + 1 + 8 + 1 + 1 + 3 + 1 + 1 + 2 + 4 + 2 + 2)

/* the size of the Xing header (MPEG1 and MPEG2) in kbps */
#define XING_BITRATE1_INDEX  9
#define XING_BITRATE2_INDEX  8

static const char	VBRTag[]={"Xing"};
static const char	VBRTag2[]={"Info"};

/****************************************************************************
 * AddVbrFrame: Add VBR entry, used to fill the VBR the TOC entries
 ****************************************************************************/
void
AddVbrFrame(lame_t gfc)
{
    int i, bitrate = bitrate_table[gfc->version][gfc->bitrate_index];
    assert(gfc->seekTable.bag);

    gfc->seekTable.sum += bitrate;
    gfc->seekTable.seen++;

    if (gfc->seekTable.seen < gfc->seekTable.want)
        return;

    if (gfc->seekTable.pos < gfc->seekTable.size) {
	gfc->seekTable.bag[gfc->seekTable.pos++]
	    = gfc->seekTable.sum;
        gfc->seekTable.seen = 0;
    }
    if (gfc->seekTable.pos == gfc->seekTable.size) {
        for (i = 1; i < gfc->seekTable.size; i += 2)
            gfc->seekTable.bag[i/2] = gfc->seekTable.bag[i]; 

        gfc->seekTable.want *= 2;
        gfc->seekTable.pos  /= 2;
    }
}


/****************************************************************************
 * InitVbrTag: Initializes the header, and write empty frame to stream
 ****************************************************************************/
int
InitVbrTag(lame_t gfc)
{
#define MAXFRAMESIZE 2880 /* the max framesize freeformat 640 32kHz */
    int bitrate, tot, totalFrameSize;

    if (!gfc->bWriteVbrTag)
	return 0;
    /*
     * Xing VBR pretends to be a 48kbs layer III frame.  (at 44.1kHz).
     * (at 48kHz they use 56kbs since 48kbs frame not big enough for
     * table of contents)
     * let's always embed Xing header inside a 64kbs layer III frame.
     * this gives us enough room for a LAME version string too.
     * size determined by sampling frequency (MPEG1)
     * 32kHz:    216 bytes@48kbs  288 bytes@64kbs
     * 44.1kHz:  156              208        (+1 if padding = 1)
     * 48kHz:    144              192
     *
     * MPEG 2 values are the same since the framesize and samplerate
     * are each reduced by a factor of 2.
     */
    bitrate = gfc->mean_bitrate_kbps;
    if (gfc->VBR != cbr) {
	bitrate = XING_BITRATE1_INDEX;
	if (gfc->version != 1) {
	    bitrate = XING_BITRATE2_INDEX;
	}
	bitrate = bitrate_table[gfc->version][bitrate];
    }

    totalFrameSize = ((gfc->version+1)*72000*bitrate) / gfc->out_samplerate;

    tot = (gfc->sideinfo_len+LAMEHEADERSIZE);
    if (!(tot <= totalFrameSize && totalFrameSize <= MAXFRAMESIZE)) {
	/* disable tag, it wont fit */
	gfc->bWriteVbrTag = 0;
	return 0;
    }
    if (!gfc->seekTable.bag
	&& !(gfc->seekTable.bag  = malloc (400*sizeof(int)))) {
	gfc->seekTable.size = 0;
	gfc->bWriteVbrTag = 0;
	gfc->report.errorf("Error: can't allocate VbrFrames buffer\n");
	return LAME_NOMEM;
    }   

    /*TOC shouldn't take into account the size of the VBR header itself, too*/
    gfc->seekTable.sum  = 0;
    gfc->seekTable.seen = 0;
    gfc->seekTable.want = 1;
    gfc->seekTable.pos  = 0;
    gfc->seekTable.size = 400;

    return totalFrameSize;
}

/****************************************************************************
 * Jonathan Dee 2001/08/31
 * PutLameVBR: Write LAME info: mini version + info on various switches used
 ***************************************************************************/
/* -----------------------------------------------------------
 * A Vbr header may be present in the ancillary
 * data field of the first frame of an mp3 bitstream
 * The Vbr header (optionally) contains
 *      frames      total number of audio frames in the bitstream
 *      bytes       total number of bytes in the bitstream
 *      toc         table of contents

 * toc (table of contents) gives seek points
 * for random access
 * the ith entry determines the seek point for
 * i-percent duration
 * seek point in bytes = (toc[i]/256.0) * total_bitstream_bytes
 * e.g. half duration seek point = (toc[50]/256.0) * total_bitstream_bytes
 */
static void
PutLameVBR(lame_t gfc, size_t nMusicLength, uint8_t *p, uint8_t *p0)
{
#define nRevision 0
#define bExpNPsyTune 1
#define bSafeJoint 0

    int enc_delay = lame_get_encoder_delay(gfc);       /* encoder delay */
    int enc_padding = lame_get_encoder_padding(gfc);   /* encoder padding  */
    uint8_t vbr_type_translator[] = {1,5,3};

    uint32_t nPeakSignalAmplitude = 0;
    uint16_t nRadioReplayGain = 0;
    uint16_t nAudioPhileReplayGain  = 0;	/* TODO */

    int	bNonOptimal = 0;
    int nStereoMode, nSourceFreq;
    int bNoGapMore	= 0;
    int bNoGapPrevious	= 0;
    int nNoGapCount = 0;/* gfc->nogap_total; TODO */
    int nNoGapCurr  = 0;/* gfc->nogap_current; TODO */

    /*nogap */
    if (nNoGapCount != -1) {
	if (nNoGapCurr > 0)
	    bNoGapPrevious = 1;

	if (nNoGapCurr < nNoGapCount-1)
	    bNoGapMore = 1;
    }

    /*stereo mode field... a bit ugly.*/
    switch(gfc->mode) {
    case MONO:		nStereoMode = 0; break;
    case STEREO:	nStereoMode = 1; break;
    case DUAL_CHANNEL:	nStereoMode = 2; break;
    case JOINT_STEREO:	nStereoMode = 3; break;
    default:		nStereoMode = 7; break;
    }
    if (gfc->force_ms)
	nStereoMode = 4;
    if (gfc->use_istereo)
	nStereoMode = 6;

    nSourceFreq = 0;
    if (gfc->in_samplerate == 48000)
	nSourceFreq = 2;
    else if (gfc->in_samplerate > 48000)
	nSourceFreq = 3;
    else if (gfc->in_samplerate == 44100)
	nSourceFreq = 1;

    /*Check if the user overrided the default LAME behaviour with some nasty options */

    if ((gfc->lowpassfreq == -1 && gfc->highpassfreq == -1)
	|| (gfc->scale_left != gfc->scale_right)
	|| gfc->disable_reservoir
	|| gfc->noATH
	|| gfc->ATHonly
	|| gfc->in_samplerate <= 32000)
	bNonOptimal = 1;

    if ((gfc->tag_spec.flags & CHANGED_FLAG)
	&& !(gfc->tag_spec.flags & V2_ONLY_FLAG))
	nMusicLength -= 128;                     /*id3v1 present. */

    /*Write all this information into the stream*/
    p = CreateI4(p, Max(100 - 10 * gfc->VBR_q - gfc->quality, 0));
    strncpy(p, get_lame_very_short_version(), 9); p += 9;

    *p++ = (nRevision << 4) + vbr_type_translator[lame_get_VBR(gfc)];
    *p++ = Min((int)((gfc->lowpassfreq / 100.0)+.5), 255);

    /* ReplayGain */
    if (gfc->findReplayGain) { 
	if (gfc->RadioGain > 0x1FE)
	    gfc->RadioGain = 0x1FE;
	if (gfc->RadioGain < -0x1FE)
	    gfc->RadioGain = -0x1FE;

	nRadioReplayGain = 0x2000; /* set name code */
	nRadioReplayGain |= 0xC00; /* set originator code to `determined automatically' */

	if (gfc->RadioGain >= 0) 
	    nRadioReplayGain |= gfc->RadioGain; /* set gain adjustment */
	else {
            nRadioReplayGain |= 0x200; /* set the sign bit */
	    nRadioReplayGain |= -gfc->RadioGain; /* set gain adjustment */
	}
    }
    if (gfc->decode_on_the_fly)
	nPeakSignalAmplitude = (int)(fabs(gfc->PeakSample) * (FLOAT)((1 << 23) / 32767.0) + (FLOAT).5);

    p = CreateI4(p, nPeakSignalAmplitude);
    p = CreateI2(p, nRadioReplayGain);
    p = CreateI2(p, nAudioPhileReplayGain);

    *p++ = (((int)gfc->ATHcurve) & 15) /*nAthType*/
	+ (bExpNPsyTune		<< 4)
	+ (bSafeJoint		<< 5)
	+ (bNoGapMore		<< 6)
	+ (bNoGapPrevious	<< 7);

    /* if ABR, {store bitrate <=255} else {store "-b"} */
    *p++ = Min(gfc->mean_bitrate_kbps, 255);

    *p++ = enc_delay >> 4;
    *p++ = (enc_delay << 4) + (enc_padding >> 8);
    *p++ = enc_padding;

    *p++ = (gfc->noise_shaping_amp >= 0)
	+ (nStereoMode << 2)
	+ (bNonOptimal << 5)
	+ (nSourceFreq << 6);

    *p++ = 0;	/*unused in rev1 */

    /* preset value for LAME4 is always 0 */
    p = CreateI2(p, 0);
    p = CreateI4(p, nMusicLength);
    p = CreateI2(p, gfc->nMusicCRC);

    /*Calculate tag CRC.... must be done here, since it includes
     *previous information*/
    /* work out CRC so far: initially crc = 0 */
    p = CreateI2(p, calculateCRC(p0, p - p0, 0));
}

/***********************************************************************
 * lame_mp3_tags_fid: Write final VBR tag to the file
 * Paramters:
 *			fpStream: pointer to output file stream
 ***********************************************************************/
int
lame_mp3_tags_fid(lame_t gfc, FILE * fpStream)
{
    uint8_t buf[MAXFRAMESIZE], *p;
    size_t id3v2TagSize, lFileSize, totalFrameSize;
    int i, idx, bitrate;

    if (!gfc->bWriteVbrTag || !fpStream
	|| (gfc->seekTable.pos <= 0 && gfc->VBR != cbr))
	return LAME_GENERICERROR;

    /* Get file size */
    fseek(fpStream, 0, SEEK_END);
    if ((lFileSize=ftell(fpStream)) == 0)
	return LAME_GENERICERROR;

    /*
     * The VBR tag may NOT be located at the beginning of the stream.
     * If an ID3v2 tag was added, then it must be skipped to write
     * the VBR tag data.
     */
    /* read 10 bytes from very the beginning,
       in case there's an ID3 version 2 header here */
    fseek(fpStream, 0, SEEK_SET);
    fread(buf, 1, 10, fpStream);

    id3v2TagSize = 0;
    if (!strncmp(buf, "ID3", 3)) {
	/* the tag size (minus the 10-byte header) is encoded into four
	 * bytes where the most significant bit is clear in each byte */
	id3v2TagSize = 10 + (((buf[6] & 0x7f)<<21)
			     | ((buf[7] & 0x7f)<<14)
			     | ((buf[8] & 0x7f)<<7)
			     |  (buf[9] & 0x7f));
    }

    /* the default VBR header. 48 kbps layer III, no padding, no crc */
    /* but sampling freq, mode andy copyright/copy protection taken */
    /* from first valid frame */
    bitrate = gfc->bitrate_index;
    if (gfc->VBR != cbr) {
	bitrate = XING_BITRATE1_INDEX;
	if (gfc->version != 1) {
	    bitrate = XING_BITRATE2_INDEX;
	}
    }

    /* Read the header of the first valid frame */
    totalFrameSize = ((gfc->version+1)*72000*bitrate_table[gfc->version][bitrate]) / gfc->out_samplerate;
    fseek(fpStream, id3v2TagSize + totalFrameSize, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    fread(buf, 4, 1, fpStream);

    /* note! Xing header specifies that Xing data goes in the
     * ancillary data with NO ERROR PROTECTION.  If error protecton
     * in enabled, the Xing data still starts at the same offset,
     * and now it is in sideinfo data block, and thus will not
     * decode correctly by non-Xing tag aware players */
    p = &buf[gfc->sideinfo_len];
    if (gfc->error_protection)
	p -= 2;

    /* Put VBR tag */
    CreateI4(p+ 4, FRAMES_FLAG+BYTES_FLAG+TOC_FLAG+VBR_SCALE_FLAG);
    CreateI4(p+ 8, gfc->frameNum);
    CreateI4(p+12, lFileSize);

    /* Put TOC */
    if (gfc->VBR == cbr) {
	assert(gfc->free_format == 0);
	memcpy(p, VBRTag2, 4);
	p += 16;
	for (i = 0; i < NUMTOCENTRIES; ++i)
	    *p++ = 255 * i / NUMTOCENTRIES;
    } else {
	memcpy(p, VBRTag, 4);
	p += 16;
	assert(gfc->seekTable.pos > 0);
	buf[2] = (bitrate << 4) | (buf[2] & 0x0d);

	*p++ = 0;
	for (i = 1; i < NUMTOCENTRIES; ++i) {
	    idx = (int)floor(i / (float)NUMTOCENTRIES * gfc->seekTable.pos);
	    if (idx > gfc->seekTable.pos-1)
		idx = gfc->seekTable.pos-1;
	    idx = (int)(256. * gfc->seekTable.bag[idx] / gfc->seekTable.sum);
	    if (idx > 255)
		idx = 255;
	    *p++ = idx;
	}
    }

    /* error_protection: add crc16 information to header !? */
    if (gfc->error_protection)
	CRC_writeheader(buf, gfc->sideinfo_len);

    /* Put LAME VBR info */
    PutLameVBR(gfc, lFileSize - id3v2TagSize, p, buf);

    fseek(fpStream, id3v2TagSize, SEEK_SET);
    if (fwrite(buf, totalFrameSize, 1, fpStream) != 1)
	return LAME_WRITEERROR;
    return 0;
}
