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

void  lame_print_version ( FILE* fp )
{
    fprintf ( fp, "LAME version %s    (http://www.mp3dev.org) \n", get_lame_version() );
 // fprintf ( fp, "GPSYCHO: GPL psycho-acoustic and noise shaping model version %s. \n", get_psy_version () );
}

const char*  get_lame_version ( void )
{
    static char ret [48];
    
    if (LAME_ALPHA_VERSION > 0)
      /* adding date and time to version string makes it harder for
       * output validation */
      /*        sprintf ( ret, "%u.%02d (alpha %u, %6.6s %5.5s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_ALPHA_VERSION, __DATE__, __TIME__ ); */
        sprintf ( ret, "%u.%02d (alpha %u)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_ALPHA_VERSION);
    else if (LAME_BETA_VERSION > 0)
        sprintf ( ret, "%u.%02d (beta %u, %s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_BETA_VERSION, __DATE__ );
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
