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

#include "version.h"    // macros of version numbers


const char*  get_lame_version ( void )  // primary for reports on screen
{
    static char ret [48];
    
    if (LAME_ALPHA_VERSION > 0)
        sprintf ( ret, "%u.%02d (alpha %u, %6.6s %5.5s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_ALPHA_VERSION, __DATE__, __TIME__ );
    else if (LAME_BETA_VERSION > 0)
        sprintf ( ret, "%u.%02d (beta %u, %s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_BETA_VERSION, __DATE__ );
    else
        sprintf ( ret, "%u.%02d", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
        
    return ret;
}

const char*  get_lame_short_version ( void )  // primary to write into MP3 files
{
    static char ret [32];
    
    /* adding date and time to version string makes it harder for
     * output validation
     * I also removed version string and replaced by a "1" to be mt friendly 
     */
    
    if (LAME_ALPHA_VERSION > 0)
        sprintf ( ret, "%u.%02d (alpha 1)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
    else if (LAME_BETA_VERSION > 0)
        sprintf ( ret, "%u.%02d (beta 1)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
    else
        sprintf ( ret, "%u.%02d", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
        
    return ret;
}

const char*  get_psy_version ( void )
{
    static char ret [48];
    
    if (PSY_ALPHA_VERSION > 0)
        sprintf ( ret, "%u.%02d (alpha %u, %6.6s %5.5s)", PSY_MAJOR_VERSION, PSY_MINOR_VERSION, PSY_ALPHA_VERSION, __DATE__, __TIME__ );
    else if (PSY_BETA_VERSION > 0)
        sprintf ( ret, "%u.%02d (beta %u, %s)", PSY_MAJOR_VERSION, PSY_MINOR_VERSION, PSY_BETA_VERSION, __DATE__ );
    else
        sprintf ( ret, "%u.%02d", PSY_MAJOR_VERSION, PSY_MINOR_VERSION );
        
    return ret;
}

const char*  get_mp3x_version ( void )
{
    static char ret [48];
    
    if (MP3X_ALPHA_VERSION > 0)
        sprintf ( ret, "%u:%02u (alpha %u, %6.6s %5.5s)", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION, MP3X_ALPHA_VERSION, __DATE__, __TIME__ );
    else if (MP3X_BETA_VERSION > 0)
        sprintf ( ret, "%u:%02u (beta %u, %s)", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION, MP3X_BETA_VERSION, __DATE__ );
    else
        sprintf ( ret, "%u:%02u", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION );
                
    return ret;
}

/* End of version.c */
