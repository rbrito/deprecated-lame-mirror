/*
 * id3tag.h -- Interface to write ID3 version 1 and 2 tags.
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

#ifndef ID3TAG_H_
#define ID3TAG_H_

#ifdef ID3TAG_INDEPENDENCE
#include <stdio.h>
#else
#include "lame.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* utility to obtain alphabetically sorted list of genre names with numbers */
extern void id3tag_genre_list(void (*handler)(int, const char *, void *),
    void *cookie);

#ifdef ID3TAG_INDEPENDENCE
struct id3tag_spec
{
    /* private data members */
    int flags;
    const char *title;
    const char *artist;
    const char *album;
    int year;
    const char *comment;
    int track;
    int genre;
};
#endif

extern void id3tag_init(struct id3tag_spec *spec);

/* force addition of version 2 tag */
extern void id3tag_add_v2(struct id3tag_spec *spec);
/* add only a version 1 tag */
extern void id3tag_v1_only(struct id3tag_spec *spec);
/* add only a version 2 tag */
extern void id3tag_v2_only(struct id3tag_spec *spec);
/* pad version 1 tag with spaces instead of nulls */
extern void id3tag_space_v1(struct id3tag_spec *spec);
/* pad version 2 tag with extra 128 bytes */
extern void id3tag_pad_v2(struct id3tag_spec *spec);

extern void id3tag_set_title(struct id3tag_spec *spec, const char *title);
extern void id3tag_set_artist(struct id3tag_spec *spec, const char *artist);
extern void id3tag_set_album(struct id3tag_spec *spec, const char *album);
extern void id3tag_set_year(struct id3tag_spec *spec, const char *year);
extern void id3tag_set_comment(struct id3tag_spec *spec, const char *comment);
extern void id3tag_set_track(struct id3tag_spec *spec, const char *track);

/* return non-zero result if genre name or number is invalid */
extern int id3tag_set_genre(struct id3tag_spec *spec, const char *genre);

/*
 * NOTE: A version 2 tag will NOT be added unless one of the text fields won't
 * fit in a version 1 tag (e.g. the title string is longer than 30 characters),
 * or the "id3tag_add_v2" or "id3tag_v2_only" functions are used.
 */

/* write tag into stream at current position */
extern int id3tag_write_v2(struct id3tag_spec *spec, FILE *stream);
extern int id3tag_write_v1(struct id3tag_spec *spec, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
