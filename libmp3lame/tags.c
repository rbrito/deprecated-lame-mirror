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
	gfc->tag_spec.track = num;
	gfc->tag_spec.flags |= CHANGED_FLAG;
	if (n == 2)
	    gfc->tag_spec.totaltrack = totalnum;
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

static unsigned char *
set_frame(unsigned char *frame, unsigned long id, const char *text,
	  size_t length)
{
    if (length) {
        frame = CreateI4(frame, id);
        /* Set frame size = total size - header size.  Frame header and field
         * bytes include 2-byte header flags, 1 encoding descriptor byte, and
         * for comment frames: 3-byte language descriptor and 1 content
         * descriptor byte */
        frame = CreateI4(frame, ((id == COMMENT_FRAME_ID) ? 5 : 1) + length);
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

/*
 * NOTE: A version 2 tag will NOT be added unless one of the text fields won't
 * fit in a version 1 tag (e.g. the title string is longer than 30 characters),
 * or the "id3tag_add_v2" or "id3tag_v2_only" functions are used.
 */
int
id3tag_write_v2(lame_t gfc, unsigned char *buf, int size)
{
    if ((gfc->tag_spec.flags & CHANGED_FLAG)
	&& !(gfc->tag_spec.flags & V1_ONLY_FLAG)) {
        /* calculate length of four fields which may not fit in verion 1 tag */
        size_t title_length = gfc->tag_spec.title
            ? strlen(gfc->tag_spec.title) : 0;
        size_t artist_length = gfc->tag_spec.artist
            ? strlen(gfc->tag_spec.artist) : 0;
        size_t album_length = gfc->tag_spec.album
            ? strlen(gfc->tag_spec.album) : 0;
        size_t comment_length = gfc->tag_spec.comment
            ? strlen(gfc->tag_spec.comment) : 0;
        /* write tag if explicitly requested or if fields overflow */
        if ((gfc->tag_spec.flags & (ADD_V2_FLAG | V2_ONLY_FLAG))
	    || title_length > 30 || artist_length > 30 || album_length > 30
	    || comment_length > 30
	    || (gfc->tag_spec.track && comment_length > 28)
	    || gfc->tag_spec.track > 255 || gfc->tag_spec.totaltrack > 0) {
            size_t adjusted_tag_size, tag_size;
            char encoder[20];
            size_t encoder_length;
            char year[5];
            size_t year_length;
            char track[100];
            size_t track_length;
            char genre[6];
            size_t genre_length;
            unsigned char *tag;
            unsigned char *p;

            /* calulate size of tag starting with 10-byte tag header */
            tag_size = 10;
	    strcpy(encoder, "LAME v");
	    strncat(encoder, get_lame_short_version(), sizeof(encoder));
	    encoder_length = strlen(encoder);
            tag_size += 11 + encoder_length;
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
		if (gfc->tag_spec.year > 9999)
		    gfc->tag_spec.year = 9999;
                year_length = sprintf(year, "%d", gfc->tag_spec.year);
                tag_size += 11 + year_length;
            } else {
                year_length = 0;
            }
            if (comment_length) {
                /* add 10-byte frame header, 1 encoding descriptor byte,
                 * 3-byte language descriptor, 1 content descriptor byte ... */
                tag_size += 15 + comment_length;
            }
            if (gfc->tag_spec.track) {
		if (gfc->tag_spec.track > 9999)
		    gfc->tag_spec.track = 9999;
		track_length = sprintf(track, "%d", gfc->tag_spec.track);
		if (gfc->tag_spec.totaltrack > 0) {
		    if (gfc->tag_spec.totaltrack > 9999)
			gfc->tag_spec.totaltrack = 9999;
		    track_length += sprintf(&track[track_length], "/%d",
					    gfc->tag_spec.totaltrack);
		}
                tag_size += 11 + track_length;
            } else {
                track_length = 0;
            }
            if (gfc->tag_spec.genre != GENRE_NUM_UNKNOWN) {
                genre_length = sprintf(genre, "(%d)", gfc->tag_spec.genre);
                tag_size += 11 + genre_length;
            } else {
                genre_length = 0;
            }
            if (gfc->tag_spec.flags & PAD_V2_FLAG) {
                /* add 128 bytes of padding */
                tag_size += 128;
            }
            tag = (unsigned char *)malloc(tag_size);
            if (!tag) {
                return LAME_NOMEM;
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
            p = set_frame(p, ENCODER_FRAME_ID, encoder, encoder_length);
            p = set_frame(p, TITLE_FRAME_ID, gfc->tag_spec.title, title_length);
            p = set_frame(p, ARTIST_FRAME_ID, gfc->tag_spec.artist,
                    artist_length);
            p = set_frame(p, ALBUM_FRAME_ID, gfc->tag_spec.album, album_length);
            p = set_frame(p, YEAR_FRAME_ID, year, year_length);
            p = set_frame(p, COMMENT_FRAME_ID, gfc->tag_spec.comment,
                    comment_length);
            p = set_frame(p, TRACK_FRAME_ID, track, track_length);
            p = set_frame(p, GENRE_FRAME_ID, genre, genre_length);
            /* clear any padding bytes */
            memset(p, 0, tag_size - (p - tag));
            /* write tag directly into bitstream at current position */
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
id3tag_write_v1(lame_t gfc, unsigned char *buf, int size)
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
    if (gfc->tag_spec.track) {
	p[-2] = 0;
	p[-1] = gfc->tag_spec.track;
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

/***********************************************************************
 *  Robert Hegemann 2001-01-17
 ***********************************************************************/
static void
Xing_seek_table(VBR_seek_info_t * v, unsigned char *t)
{
    int i, idx, seek_point;
    if (v->pos <= 0)
        return;

    for (i = 1; i < NUMTOCENTRIES; ++i) {
	float j = i/(float)NUMTOCENTRIES, act, sum;
        idx = (int)(floor(j * v->pos));
        if (idx > v->pos-1)
            idx = v->pos-1;
        act = v->bag[idx];
        sum = v->sum;
        seek_point = (int)(256. * act / sum);
        if (seek_point > 255)
            seek_point = 255;
        t[i] = seek_point;
    }
}

/****************************************************************************
 * AddVbrFrame: Add VBR entry, used to fill the VBR the TOC entries
 ****************************************************************************/
void
AddVbrFrame(lame_t gfc)
{
    int i, bitrate = bitrate_table[gfc->version][gfc->bitrate_index];
    assert(gfc->VBR_seek_table.bag);

    gfc->VBR_seek_table.sum += bitrate;
    gfc->VBR_seek_table.seen++;

    if (gfc->VBR_seek_table.seen < gfc->VBR_seek_table.want)
        return;

    if (gfc->VBR_seek_table.pos < gfc->VBR_seek_table.size) {
	gfc->VBR_seek_table.bag[gfc->VBR_seek_table.pos++]
	    = gfc->VBR_seek_table.sum;
        gfc->VBR_seek_table.seen = 0;
    }
    if (gfc->VBR_seek_table.pos == gfc->VBR_seek_table.size) {
        for (i = 1; i < gfc->VBR_seek_table.size; i += 2)
            gfc->VBR_seek_table.bag[i/2] = gfc->VBR_seek_table.bag[i]; 

        gfc->VBR_seek_table.want *= 2;
        gfc->VBR_seek_table.pos  /= 2;
    }
}


/****************************************************************************
 * InitVbrTag: Initializes the header, and write empty frame to stream
 ****************************************************************************/
int
InitVbrTag(lame_t gfc)
{
#define MAXFRAMESIZE 2880 /* the max framesize freeformat 640 32kHz */
    int bitrate, tot;

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

    gfc->TotalFrameSize
	= ((gfc->version+1)*72000*bitrate) / gfc->out_samplerate;

    tot = (gfc->sideinfo_len+LAMEHEADERSIZE);
    if (!(tot <= gfc->TotalFrameSize && gfc->TotalFrameSize <= MAXFRAMESIZE)) {
	/* disable tag, it wont fit */
	gfc->bWriteVbrTag = 0;
	return 0;
    }
    if (!gfc->VBR_seek_table.bag
	&& !(gfc->VBR_seek_table.bag  = malloc (400*sizeof(int)))) {
	gfc->VBR_seek_table.size = 0;
	gfc->bWriteVbrTag = 0;
	gfc->report.errorf("Error: can't allocate VbrFrames buffer\n");
	return LAME_NOMEM;
    }   

    /*TOC shouldn't take into account the size of the VBR header itself, too*/
    gfc->VBR_seek_table.sum  = 0;
    gfc->VBR_seek_table.seen = 0;
    gfc->VBR_seek_table.want = 1;
    gfc->VBR_seek_table.pos  = 0;
    gfc->VBR_seek_table.size = 400;

    return gfc->TotalFrameSize;
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

    ieee754_float32_t fPeakSignalAmplitude = 0.0;	/*TODO... */
    uint16_t nRadioReplayGain = 0;			/*TODO... */
    uint16_t nAudioPhileReplayGain  = 0;		/*TODO... */

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
    memcpy(p, &fPeakSignalAmplitude, 4); p += 4;

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
 * PutVbrTag: Write final VBR tag to the file
 * Paramters:
 *			fpStream: pointer to output file stream
 ***********************************************************************/
static int
PutVbrTag(lame_t gfc, FILE *fpStream)
{
    uint8_t buf[MAXFRAMESIZE] = {0}, *p, id3v2Header[10];
    size_t id3v2TagSize, lFileSize;
    int i, bitrate;

    if (gfc->VBR_seek_table.pos <= 0 && gfc->VBR != cbr)
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
    fread(id3v2Header, 1, sizeof id3v2Header, fpStream);

    id3v2TagSize = 0;
    if (!strncmp(id3v2Header, "ID3", 3)) {
	/* the tag size (minus the 10-byte header) is encoded into four
	 * bytes where the most significant bit is clear in each byte */
	id3v2TagSize=(((id3v2Header[6] & 0x7f)<<21)
		      | ((id3v2Header[7] & 0x7f)<<14)
		      | ((id3v2Header[8] & 0x7f)<<7)
		      | (id3v2Header[9] & 0x7f))
	    + sizeof id3v2Header;
    }

    /* Read the header of the first valid frame */
    fseek(fpStream, id3v2TagSize + gfc->TotalFrameSize, SEEK_SET);
    fread(buf, 4, 1, fpStream);

    /* the default VBR header. 48 kbps layer III, no padding, no crc */
    /* but sampling freq, mode andy copyright/copy protection taken */
    /* from first valid frame */
    if (gfc->VBR != cbr) {
	bitrate = XING_BITRATE1_INDEX;
	if (gfc->version != 1) {
	    bitrate = XING_BITRATE2_INDEX;
	}
	assert(gfc->free_format == 0);
	buf[2] = (bitrate << 4) | (buf[2] & 0x0d);
    }
    /* note! Xing header specifies that Xing data goes in the
     * ancillary data with NO ERROR PROTECTION.  If error protecton
     * in enabled, the Xing data still starts at the same offset,
     * and now it is in sideinfo data block, and thus will not
     * decode correctly by non-Xing tag aware players */
    p = &buf[gfc->sideinfo_len];
    if (gfc->error_protection)
	p -= 2;

    /* Put Vbr tag */
    CreateI4(p+ 4, FRAMES_FLAG+BYTES_FLAG+TOC_FLAG+VBR_SCALE_FLAG);
    CreateI4(p+ 8, gfc->frameNum);
    CreateI4(p+12, lFileSize);

    /* Put TOC */
    if (gfc->VBR == cbr) {
	memcpy(p, VBRTag2, 4);
	p += 16;
	for (i = 0; i < NUMTOCENTRIES; ++i)
	    *p++ = 255*i/100;
    } else {
	memcpy(p, VBRTag, 4);
	p += 16;
	Xing_seek_table(&gfc->VBR_seek_table, p);
	p += NUMTOCENTRIES;
    }

    /* error_protection: add crc16 information to header !? */
    if (gfc->error_protection)
	CRC_writeheader(buf, gfc->sideinfo_len);

    /* Put LAME VBR info */
    PutLameVBR(gfc, lFileSize - id3v2TagSize, p, buf);

    fseek(fpStream, id3v2TagSize, SEEK_SET);
    if (fwrite(buf, gfc->TotalFrameSize, 1, fpStream) != 1)
	return LAME_WRITEERROR;
    return 0;
}

/*****************************************************************/
/* write VBR Xing header, and ID3 version 1 tag, if asked for    */
/*****************************************************************/
void
lame_mp3_tags_fid(lame_t gfc, FILE * fpStream)
{
    /* Write Xing header again */
    if (gfc->bWriteVbrTag && fpStream && !fseek(fpStream, 0, SEEK_SET))
	PutVbrTag(gfc, fpStream);
}
