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
#include <config.h>
#endif


#include <string.h>
#include "version.h"    // macros of version numbers


#if defined(MMX_choose_table)
# define V1 "MMX "
#else
# define V1 ""
#endif

#if defined(KLEMM)
# define V2 "KLM "
#else
# define V2 ""
#endif

#if defined(RH)
# define V3 "RH "
#else
# define V3 ""
#endif

#define V   V1 V2 V3


// ISO don't guarantees rewritable strings for strcpy. That means, 
// that it is not guaranteed that you have a valid string while
// overwriting a string by a string with the same contents.

static void  thread_safe__strcpy ( char* dst, const char* src )
{
    while ( (*dst++ = *src++) != '\0' ) ;
}


// This is an example of such a non rewritable version.
// This may make sense on future computers and is also
// full ISO conform. ISO don't say anything about threads
// and strcpy is undefined if both strings overlap.

static void  no_thread_safe__strcpy ( char* dst, const char* src )
{
    size_t len = strlen (src);  // Special fast HW search statement

    memset ( dst, 0, len+1 );   // Write Cache pre-heating
    memcpy ( dst, src, len );   // Data Copy
}


// This is the only way to make such a function 100% thread safe.
// There are a lot more if the OS supports semaphores (critical sectioning)

const char*  get_lame_version ( void )          // primary to write screen reports
{
    static char ret [48];
    char        str [48];

    /* Here we can also add informations about compile time configurations */
    
    if (LAME_ALPHA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u.%02d " V "(alpha %u, %6.6s %5.5s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_ALPHA_VERSION, __DATE__, __TIME__ );
    else if (LAME_BETA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u.%02d " V "(beta %u, %s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_BETA_VERSION, __DATE__ );
    else
        snprintf ( str, sizeof (str), "%u.%02d " V, LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
        
    thread_safe__strcpy ( ret, str );
    return ret;
}


const char*  get_lame_short_version ( void )  // primary to write into MP3 files
{
    static char ret [32];
    char        str [32];
    
    /* adding date and time to version string makes it harder for output validation */

    if (LAME_ALPHA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u.%02d (alpha)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
    else if (LAME_BETA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u.%02d (beta)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
    else
        snprintf ( str, sizeof (str), "%u.%02d", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
	
    thread_safe__strcpy ( ret, str );
    return ret;
}


const char*  get_psy_version ( void )
{
    static char ret [48];
    char        str [48];
    
    if (PSY_ALPHA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u.%02d (alpha %u, %6.6s %5.5s)", PSY_MAJOR_VERSION, PSY_MINOR_VERSION, PSY_ALPHA_VERSION, __DATE__, __TIME__ );
    else if (PSY_BETA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u.%02d (beta %u, %s)", PSY_MAJOR_VERSION, PSY_MINOR_VERSION, PSY_BETA_VERSION, __DATE__ );
    else
        snprintf ( str, sizeof (str), "%u.%02d", PSY_MAJOR_VERSION, PSY_MINOR_VERSION );
        
    thread_safe__strcpy ( ret, str );
    return ret;
}


const char*  get_mp3x_version ( void )
{
    static char ret [48];
    char        str [48];
    
    if (MP3X_ALPHA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u:%02u (alpha %u, %6.6s %5.5s)", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION, MP3X_ALPHA_VERSION, __DATE__, __TIME__ );
    else if (MP3X_BETA_VERSION > 0)
        snprintf ( str, sizeof (str), "%u:%02u (beta %u, %s)", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION, MP3X_BETA_VERSION, __DATE__ );
    else
        snprintf ( str, sizeof (str), "%u:%02u", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION );
                
    thread_safe__strcpy ( ret, str );
    return ret;
}


const char*  get_lame_url ( void )
{
    return LAME_URL;
}    


// mt? pro, cons? about get_lame_about()?

const char*  get_lame_about ( void )   // all information for a Windows/KDE "About" dialog, '\n' separated, '\0' terminated
{
    static char ret [256];
    char        str [256];

    snprintf ( str, sizeof (str),    // example
              "LAME %s\n"
              "GPsycho %s\n"
	      "Analyzer %s\n"
	      "Copyright (C) 1997-2000 bla@fasel foo@bar\n"
	      "This Software stands on the LGPL software licence\n"
	      "Bug reports to <mp3encoder@minnie.cs.adfa.edu.au>\n"
	      "Special thanks to ....\n",
              get_lame_version(), get_psy_version(), get_mp3x_version() ) ;
                
    thread_safe__strcpy ( ret, str );
    return ret;
}

#undef V
#undef V1
#undef V2
#undef V3

/*
 *  Still remaining is the problem that malicious programs can temporary
 *  destroy this data. May be we should really change the interface to avoid
 *  this, if there is no better solution, but before changing the API we
 *  should at least discuss the problem. And the solution should not be a
 *  "Eierlegende Wollmilchsau", but the simpliest function doing this job,
 *  and not a function doing a lot of string manipulation and spell
 *  checking.
 */

/* And a last remark: for beta and release version also this proplem can be solved */

/* End of version.c */
