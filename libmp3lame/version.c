/*
 *      Version numbering for LAME.
 *
 *      Copyright (c) 1999 A.L. Faber
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include <stdio.h>
#include "version.h"    /* macros of version numbers */

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#define STR(x)   #x
#define XSTR(x)  STR(x)

#if defined(MMX_choose_table)
# define V1  "MMX "
#else
# define V1  ""
#endif

#if defined(KLEMM)
# define V2  "KLM "
#else
# define V2  ""
#endif

#if defined(RH)
# define V3  "RH "
#else
# define V3  ""
#endif

#define V   V1 V2 V3


const char*  get_lame_version ( void )		/* primary to write screen reports */
{
    /* Here we can also add informations about compile time configurations */

    /*@-observertrans@*/
#if   LAME_ALPHA_VERSION > 0
    static const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " " V
        "(alpha " XSTR(LAME_ALPHA_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif LAME_BETA_VERSION > 0
    static const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " " V
        "(beta " XSTR(LAME_BETA_VERSION) ", " __DATE__ ")";
#else
    static const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " " V;
#endif

    /*@-statictrans@*/
    return str;
    /*@=statictrans =observertrans@*/
}


const char*  get_lame_short_version ( void )	/* primary to write into MP3 files */
{
    /* adding date and time to version string makes it harder for output validation */

    /*@-observertrans@*/
#if   LAME_ALPHA_VERSION > 0
    static const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " (alpha)";
#elif LAME_BETA_VERSION > 0
    static const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " (beta)";
#else
    static const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION)
#endif

    /*@-statictrans@*/
    return str;
    /*@=statictrans =observertrans@*/
}


const char*  get_psy_version ( void )
{
    /*@-observertrans@*/
#if   PSY_ALPHA_VERSION > 0
    static const char *const str =
        XSTR(PSY_MAJOR_VERSION) "." XSTR(PSY_MINOR_VERSION)
        " (alpha " XSTR(PSY_ALPHA_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif PSY_BETA_VERSION > 0
    static const char *const str =
        XSTR(PSY_MAJOR_VERSION) "." XSTR(PSY_MINOR_VERSION)
        " (beta " XSTR(PSY_BETA_VERSION) ", " __DATE__ ")";
#else
    static const char *const str =
        XSTR(PSY_MAJOR_VERSION) "." XSTR(PSY_MINOR_VERSION);
#endif

    /*@-statictrans@*/
    return str;
    /*@=statictrans =observertrans@*/
}


const char*  get_mp3x_version ( void )
{
    /*@-observertrans@*/
#if   MP3X_ALPHA_VERSION > 0
    static const char *const str =
        XSTR(MP3X_MAJOR_VERSION) "." XSTR(MP3X_MINOR_VERSION)
        " (alpha " XSTR(MP3X_ALPHA_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif MP3X_BETA_VERSION > 0
    static const char *const str =
        XSTR(MP3X_MAJOR_VERSION) "." XSTR(MP3X_MINOR_VERSION)
        " (beta " XSTR(MP3X_BETA_VERSION) ", " __DATE__ ")";
#else
    static const char *const str =
        XSTR(MP3X_MAJOR_VERSION) "." XSTR(MP3X_MINOR_VERSION);
#endif

    /*@-statictrans@*/
    return str;
    /*@=statictrans =observertrans@*/
}


const char*  get_lame_url ( void )
{
    /*@-observertrans@*/
    static const char *const str = LAME_URL;
    /*@-statictrans@*/
    return str;
    /*@=statictrans =observertrans@*/
}    

#if 0
void get_lame_version_numerical ( lame_version_t *const lvp )
{
    /*@-observertrans@*/
    static const char *const features = V;
    /*@=observertrans@*/

    /* generic version */
    lvp->major = LAME_MAJOR_VERSION;
    lvp->minor = LAME_MINOR_VERSION;
    lvp->alpha = LAME_ALPHA_VERSION;
    lvp->beta  = LAME_BETA_VERSION;

    /* psy version */
    lvp->psy_major = PSY_MAJOR_VERSION;
    lvp->psy_minor = PSY_MINOR_VERSION;
    lvp->psy_alpha = PSY_ALPHA_VERSION;
    lvp->psy_beta  = PSY_BETA_VERSION;

    /* compile time features */
    /*@-observertrans -mustfree -statictrans@*/
    lvp->features = features;
    /*@=statictrans =mustfree =observertrans@*/
}
#endif

/* end of version.c */
