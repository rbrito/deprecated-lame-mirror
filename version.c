/*
 *	Version numbering for LAME.
 *
 *	Copyright (c) 1999 A.L. Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include "version.h"
#include "lame.h"
#include "get_audio.h"

void lame_print_version ( FILE* fp )
{
#ifdef LIBSNDFILE
    char  tmp [80];
#endif
    
    fprintf ( fp, "LAME version %s    (http://www.sulaco.org/mp3/) \n", get_lame_version() );
 // fprintf ( fp, "GPSYCHO: GPL psycho-acoustic and noise shaping model version %s. \n", get_psy_version () );
  
#ifdef LIBSNDFILE
    sf_get_lib_version ( tmp, sizeof (tmp) );
    fprintf ( fp, "Input handled by %s  (http://www.zip.com.au/~erikd/libsndfile/)\n", tmp );
#endif
}

const char* get_lame_version ( void )
{
    static char ret [48];
    
    if (LAME_ALPHAVERSION > 0u)
        sprintf ( ret, "%u.%02d (alpha %u, %6.6s %5.5s)", LAME_MAJOR_VERSION, LAME_MINOR_VERSION, LAME_ALPHAVERSION, __DATE__, __TIME__ );
    else if (LAME_BETAVERSION > 0u)
	sprintf ( ret, "%u.%02d (beta %u, %s)", LAME_MAJOR_VERSION ,LAME_MINOR_VERSION ,LAME_BETAVERSION ,__DATE__ );
    else
        sprintf ( ret, "%u.%02d", LAME_MAJOR_VERSION, LAME_MINOR_VERSION );
	
    return ret;
}

const char* get_psy_version ( void )
{
    static char ret [48];
    
    if (PSY_ALPHAVERSION > 0u)
        sprintf ( ret, "%u.%02d (alpha %u, %6.6s %5.5s)", PSY_MAJOR_VERSION, PSY_MINOR_VERSION, PSY_ALPHAVERSION, __DATE__, __TIME__ );
    else if (PSY_BETAVERSION > 0u)
	sprintf ( ret, "%u.%02d (beta %u, %s)", PSY_MAJOR_VERSION, PSY_MINOR_VERSION, PSY_BETAVERSION ,__DATE__ );
    else
        sprintf ( ret, "%u.%02d", PSY_MAJOR_VERSION, PSY_MINOR_VERSION );
	
    return ret;
}

const char* get_mp3x_version ( void )
{
    static char ret [48];
    
    if (MP3X_ALPHAVERSION > 0u)
	sprintf ( ret, "%u:%02u (alpha %u, %6.6s %5.5s)", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION, MP3X_ALPHAVERSION, __DATE__, __TIME__ );
    else if (MP3X_BETAVERSION > 0u)
	sprintf ( ret, "%u:%02u (beta %u, %s)", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION, MP3X_BETAVERSION ,__DATE__ );
    else
	sprintf ( ret, "%u:%02u", MP3X_MAJOR_VERSION, MP3X_MINOR_VERSION );
		
    return ret;
}
