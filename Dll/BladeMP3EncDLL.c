/*
 *	Blade DLL Interface for LAME.
 *
 *	Copyright (c) 1999 - 2001 A.L. Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include <windows.h>
#include <Windef.h>
#include "BladeMP3EncDLL.h"
#include <assert.h>

#include "lame.h"

#define _RELEASEDEBUG 0

// lame_enc DLL version number
const int MAJORVERSION = 1;
const int MINORVERSION = 26;


// Local variables
static DWORD				dwSampleBufferSize=0;
static HANDLE				gs_hModule=NULL;
static BOOL					gs_bLogFile=FALSE;
static lame_global_flags*	gfp = NULL;

// Local function prototypes
static void dump_config( );
static void DebugPrintf( const char* pzFormat, ... );
static void DispErr( LPSTR strErr );
static void PresetOptions( lame_global_flags *gfp, LONG myPreset );



static void DebugPrintf(const char* pzFormat, ...)
{
    char	szBuffer[1024]={'\0',};
	char	szFileName[MAX_PATH+1]={'\0',};
    va_list ap;

	// Get the full module (DLL) file name
	GetModuleFileName(gs_hModule,szFileName,sizeof(szFileName));

	// change file name extention
	szFileName[strlen(szFileName)-3]='t';
	szFileName[strlen(szFileName)-2]='x';
	szFileName[strlen(szFileName)-1]='t';

	// start at beginning of the list
	va_start(ap, pzFormat);

	// copy it to the string buffer
	_vsnprintf(szBuffer, sizeof(szBuffer), pzFormat, ap);

	// log it to the file?
	if ( gs_bLogFile ) 
	{	
        FILE* fp = NULL;
		
		// try to open the log file
		fp=fopen( szFileName, "a+" );

		// check file open result
        if (fp)
		{
			// write string to the file
            fputs(szBuffer,fp);

			// close the file
            fclose(fp);
        }
    }

#if defined _DEBUG || defined _RELEASEDEBUG
    OutputDebugString(szBuffer);
#endif

	va_end(ap);
}


static void PresetOptions( lame_global_flags *gfp, LONG myPreset )
{
	switch (myPreset)
	{
		case LQP_NOPRESET:
		case LQP_NORMAL_QUALITY:
			break;
		break;
		case LQP_LOW_QUALITY:
			lame_set_quality( gfp, 9 );
			break;
		case LQP_HIGH_QUALITY:
			lame_set_quality( gfp, 2 );
			break;
		case LQP_VERYHIGH_QUALITY:
			lame_set_quality( gfp, 0 );
			break;
		case LQP_VOICE_QUALITY:		// --voice flag for experimental voice mode
            lame_set_lowpassfreq( gfp, 12000 );
            lame_set_VBR_max_bitrate_kbps( gfp, 160 );
			lame_set_no_short_blocks( gfp, 1 );
		break;
		case LQP_R3MIX_QUALITY:

            lame_set_exp_nspsytune( gfp, lame_get_exp_nspsytune( gfp ) | 1); /*nspsytune*/
            lame_set_experimentalX( gfp, 1 );

            (void) lame_set_scale( gfp, 0.98f ); /* --scale 0.98*/

            lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (8 << 20));

            lame_set_VBR( gfp,vbr_mtrh ); 
            lame_set_VBR_q( gfp, 1 );
            lame_set_quality( gfp, 2 );
            lame_set_lowpassfreq( gfp, 19500 );
            lame_set_mode( gfp, JOINT_STEREO );
            lame_set_ATHtype( gfp, 3 );
            lame_set_VBR_min_bitrate_kbps( gfp, 96 );
		break;


		case LQP_PHONE:
			lame_set_out_samplerate( gfp, 8000 );
            lame_set_lowpassfreq( gfp, 3200 );
			lame_set_lowpasswidth( gfp, 1000 );
			lame_set_no_short_blocks( gfp, 1 );
            lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, MONO );
			lame_set_brate( gfp, 16 );
            lame_set_VBR_q( gfp, 6 );
            lame_set_VBR_min_bitrate_kbps( gfp, 8 );
            lame_set_VBR_max_bitrate_kbps( gfp, 56 );
		break;

		case LQP_SW:
			lame_set_out_samplerate( gfp, 11025 );
            lame_set_lowpassfreq( gfp, 4800 );
			lame_set_lowpasswidth( gfp, 500 );
			lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, MONO );
			lame_set_brate( gfp, 24 );
            lame_set_VBR_q( gfp, 5 );
            lame_set_VBR_min_bitrate_kbps( gfp, 8 );
            lame_set_VBR_max_bitrate_kbps( gfp, 64 );
		break;
		case LQP_AM:
			lame_set_out_samplerate( gfp, 16000 );
            lame_set_lowpassfreq( gfp, 7200 );
			lame_set_lowpasswidth( gfp, 500 );
			lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, MONO );
			lame_set_brate( gfp, 32 );
            lame_set_VBR_q( gfp, 5 );
            lame_set_VBR_min_bitrate_kbps( gfp, 16 );
            lame_set_VBR_max_bitrate_kbps( gfp, 128 );
		break;
		case LQP_FM:
			lame_set_out_samplerate( gfp, 22050 );
            lame_set_lowpassfreq( gfp, 9950 );
			lame_set_lowpasswidth( gfp, 880 );
			lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, JOINT_STEREO );
			lame_set_brate( gfp, 64 );
            lame_set_VBR_q( gfp, 5 );
            lame_set_VBR_min_bitrate_kbps( gfp, 24 );
            lame_set_VBR_max_bitrate_kbps( gfp, 160 );
		break;
		case LQP_VOICE:
			lame_set_out_samplerate( gfp, 32000 );
            lame_set_lowpassfreq( gfp, 12300 );
			lame_set_lowpasswidth( gfp, 2000 );
			lame_set_no_short_blocks( gfp, 1 );
			lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, MONO );
			lame_set_brate( gfp, 56 );
            lame_set_VBR_q( gfp, 5 );
            lame_set_VBR_min_bitrate_kbps( gfp, 32 );
            lame_set_VBR_max_bitrate_kbps( gfp, 128 );
		break;
		case LQP_RADIO:
            lame_set_lowpassfreq( gfp, 15000 );
			lame_set_lowpasswidth( gfp, 0 );
			lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, JOINT_STEREO );
			lame_set_brate( gfp, 112 );
            lame_set_VBR_q( gfp, 4 );
            lame_set_VBR_min_bitrate_kbps( gfp, 64 );
            lame_set_VBR_max_bitrate_kbps( gfp, 256 );
		break;
		case LQP_TAPE:
            lame_set_lowpassfreq( gfp, 18500 );
			lame_set_lowpasswidth( gfp, 2000 );
			lame_set_quality( gfp, 5 );
            lame_set_mode( gfp, JOINT_STEREO );
			lame_set_brate( gfp, 128 );
            lame_set_VBR_q( gfp, 4 );
            lame_set_VBR_min_bitrate_kbps( gfp, 96 );
            lame_set_VBR_max_bitrate_kbps( gfp, 320 );
		break;
		case LQP_HIFI:
            lame_set_lowpassfreq( gfp, 20240 );
			lame_set_lowpasswidth( gfp, 2200 );
			lame_set_quality( gfp, 2 );
            lame_set_mode( gfp, JOINT_STEREO );
			lame_set_brate( gfp, 160 );
            lame_set_VBR_q( gfp, 3 );
            lame_set_VBR_min_bitrate_kbps( gfp, 112 );
            lame_set_VBR_max_bitrate_kbps( gfp, 320 );
		break;
		case LQP_CD:
            lame_set_lowpassfreq( gfp, -1 );
            lame_set_highpassfreq( gfp, -1 );
			lame_set_quality( gfp, 2 );
            lame_set_mode( gfp, STEREO );
			lame_set_brate( gfp, 192 );
            lame_set_VBR_q( gfp, 2 );
            lame_set_VBR_min_bitrate_kbps( gfp, 128 );
            lame_set_VBR_max_bitrate_kbps( gfp, 320 );
		break;
		case LQP_STUDIO:
            lame_set_lowpassfreq( gfp, -1 );
            lame_set_highpassfreq( gfp, -1 );
			lame_set_quality( gfp, 2 );
            lame_set_mode( gfp, STEREO );
			lame_set_brate( gfp, 256 );
            lame_set_VBR_q( gfp, 0 );
            lame_set_VBR_min_bitrate_kbps( gfp, 160 );
            lame_set_VBR_max_bitrate_kbps( gfp, 320 );
		break;
	}
}


__declspec(dllexport) BE_ERR	beInitStream(PBE_CONFIG pbeConfig, PDWORD dwSamples, PDWORD dwBufferSize, PHBE_STREAM phbeStream)
{
	int			nDllArgC=0;
	BE_CONFIG	lameConfig;

	// Init the global flags structure
	gfp = lame_init();

	// clear out structure
	memset(&lameConfig,0x00,CURRENT_STRUCT_SIZE);

	// Check if this is a regular BLADE_ENCODER header
	if (pbeConfig->dwConfig!=BE_CONFIG_LAME)
	{
		int	nCRC=pbeConfig->format.mp3.bCRC;
		int nVBR=(nCRC>>12)&0x0F;

		// Copy parameter from old Blade structure
		lameConfig.format.LHV1.dwSampleRate	=pbeConfig->format.mp3.dwSampleRate;
		//for low bitrates, LAME will automatically downsample for better
		//sound quality.  Forcing output samplerate = input samplerate is not a good idea 
		//unless the user specifically requests it:
		//lameConfig.format.LHV1.dwReSampleRate=pbeConfig->format.mp3.dwSampleRate;
		lameConfig.format.LHV1.nMode		=(pbeConfig->format.mp3.byMode&0x0F);
		lameConfig.format.LHV1.dwBitrate	=pbeConfig->format.mp3.wBitrate;
		lameConfig.format.LHV1.bPrivate		=pbeConfig->format.mp3.bPrivate;
		lameConfig.format.LHV1.bOriginal	=pbeConfig->format.mp3.bOriginal;
		lameConfig.format.LHV1.bCRC			=nCRC&0x01;
		lameConfig.format.LHV1.bCopyright	=pbeConfig->format.mp3.bCopyright;
	
		// Fill out the unknowns
		lameConfig.format.LHV1.dwStructSize=CURRENT_STRUCT_SIZE;
		lameConfig.format.LHV1.dwStructVersion=CURRENT_STRUCT_VERSION;

		// Get VBR setting from fourth nibble
		if ( nVBR>0 )
		{
			lameConfig.format.LHV1.bWriteVBRHeader = TRUE;
			lameConfig.format.LHV1.bEnableVBR = TRUE;
			lameConfig.format.LHV1.nVBRQuality = nVBR-1;
		}

		// Get Quality from third nibble
		lameConfig.format.LHV1.nPreset=((nCRC>>8)&0x0F);

	}
	else
	{
		// Copy the parameters
		memcpy(&lameConfig,pbeConfig,pbeConfig->format.LHV1.dwStructSize);
	}


	// Not used, always assign stream 1
	*phbeStream=1;


	// --------------- Set arguments to LAME encoder -------------------------

	// Set input sample frequency
	lame_set_in_samplerate( gfp, lameConfig.format.LHV1.dwSampleRate );

	// The following settings only use when preset is not one of the new LAME QUALITY Presets
	if ( (int)lameConfig.format.LHV1.nPreset < (int) LQP_PHONE )
	{
  		switch ( lameConfig.format.LHV1.nMode )
		{
			case BE_MP3_MODE_STEREO:
	            lame_set_mode( gfp, STEREO );
				lame_set_num_channels( gfp, 2 );
			break;
			case BE_MP3_MODE_JSTEREO:
	            lame_set_mode( gfp, JOINT_STEREO );
				lame_set_num_channels( gfp, 2 );
			break;
			case BE_MP3_MODE_MONO:
	            lame_set_mode( gfp, MONO );
				lame_set_num_channels( gfp, 1 );
			break;
			case BE_MP3_MODE_DUALCHANNEL:
				lame_set_force_ms( gfp, 1 );
	            lame_set_mode( gfp, JOINT_STEREO );
				lame_set_num_channels( gfp, 2 );
			break;
			default:
			{
				DebugPrintf("Invalid lameConfig.format.LHV1.nMode, value is %d\n",lameConfig.format.LHV1.nMode);
				return BE_ERR_INVALID_FORMAT_PARAMETERS;
			}
		}

		if ( lameConfig.format.LHV1.bEnableVBR )
		{
			lame_set_VBR( gfp, vbr_default );

			lame_set_VBR_q( gfp, lameConfig.format.LHV1.nVBRQuality );

			if (lameConfig.format.LHV1.bWriteVBRHeader==TRUE)
			{
				lame_set_bWriteVbrTag( gfp, 1 );
			}
			else
			{
				lame_set_bWriteVbrTag( gfp, 0 );
			}
		}
		else
		{
			lame_set_VBR( gfp, vbr_off );
		}

		// Set frequency resampling rate, if specified
		if ( lameConfig.format.LHV1.dwReSampleRate > 0 )
		{
			lame_set_out_samplerate( gfp, lameConfig.format.LHV1.dwReSampleRate );
		}
		
		// Set bitrate.  (CDex users always specify bitrate=Min bitrate when using VBR)
		lame_set_brate( gfp, lameConfig.format.LHV1.dwBitrate );
			
		// Use ABR?
		if (lameConfig.format.LHV1.dwVbrAbr_bps>0)
		{
			// set VBR to ABR
			lame_set_VBR( gfp, vbr_abr );

			// calculate to kbps
			lame_set_VBR_mean_bitrate_kbps( gfp, ( lameConfig.format.LHV1.dwVbrAbr_bps + 500 ) / 1000 );

			// limit range
			if( lame_get_VBR_mean_bitrate_kbps( gfp ) > 320)
			{
				lame_set_VBR_mean_bitrate_kbps( gfp, 320 );
			}

			if( lame_get_VBR_mean_bitrate_kbps( gfp ) < 8 )
			{
				lame_set_VBR_mean_bitrate_kbps( gfp, 8 );
			}
		}


		if ( lameConfig.format.LHV1.bEnableVBR )
		{
			switch ( lameConfig.format.LHV1.nVbrMethod)
			{
				case VBR_METHOD_NONE:
					lame_set_VBR( gfp, vbr_off );
				break;

				case VBR_METHOD_DEFAULT:
		            lame_set_VBR( gfp, vbr_default ); 
				break;

				case VBR_METHOD_OLD:
		            lame_set_VBR( gfp, vbr_rh ); 
				break;

				case VBR_METHOD_NEW:
		            lame_set_VBR( gfp, vbr_mt ); 
				break;

				case VBR_METHOD_MTRH:
		            lame_set_VBR( gfp, vbr_mtrh ); 
				break;

				case VBR_METHOD_ABR:
		            lame_set_VBR( gfp, vbr_abr ); 
				break;
				default:
					assert( FALSE );

			}
		}
	}
	
	// Set copyright flag?
	if ( lameConfig.format.LHV1.bCopyright )
	{
		lame_set_copyright( gfp, 1 );
	}

	// Do we have to tag  it as non original 
	if ( !lameConfig.format.LHV1.bOriginal )
	{
		lame_set_original( gfp, 0 );
	}
	else
	{
		lame_set_original( gfp, 1 );
	}

	// Add CRC?
	if ( lameConfig.format.LHV1.bCRC )
	{
		lame_set_error_protection( gfp, 1 );
	}
	else
	{
		lame_set_error_protection( gfp, 0 );
	}

	// Set private bit?
	if ( lameConfig.format.LHV1.bPrivate )
	{
		lame_set_extension( gfp, 1 );
	}
	else
	{
		lame_set_extension( gfp, 0 );
	}
	

	// First set all the preset options
	if ( LQP_NOPRESET !=  lameConfig.format.LHV1.nPreset )
	{
		PresetOptions( gfp, lameConfig.format.LHV1.nPreset );
	}

	// Set VBR min bitrate, if specified
	if ( lameConfig.format.LHV1.dwBitrate > 0 )
	{
		lame_set_VBR_min_bitrate_kbps( gfp, lameConfig.format.LHV1.dwBitrate );
	}

	// Set Maxbitrate, if specified
	if ( lameConfig.format.LHV1.dwMaxBitrate > 0 )
	{
		lame_set_VBR_max_bitrate_kbps( gfp, lameConfig.format.LHV1.dwMaxBitrate );
	}

	if (lameConfig.format.LHV1.bNoRes)
	{
		lame_set_disable_reservoir( gfp,1 );
		lame_set_padding_type( gfp, 0 );
	}

	lame_init_params(gfp);	

	//LAME encoding call will accept any number of samples.  
	if ( 0 == lame_get_version( gfp ) )
	{
		// For MPEG-II, only 576 samples per frame per channel
		*dwSamples= 576 * lame_get_num_channels( gfp );
	}
	else
	{
		// For MPEG-I, 1152 samples per frame per channel
		*dwSamples= 1152 * lame_get_num_channels( gfp );
	}

	// Set the input sample buffer size, so we know what we can expect
	dwSampleBufferSize = *dwSamples;

	// Set MP3 buffer size, conservative estimate
	*dwBufferSize=(DWORD)( 1.25 * ( *dwSamples / lame_get_num_channels( gfp ) ) + 7200 );

	// For debugging purposes
	dump_config( );

	// Everything went OK, thus return SUCCESSFUL
	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR	beDeinitStream(HBE_STREAM hbeStream, PBYTE pOutput, PDWORD pdwOutput)
{

//	*pdwOutput = lame_encode_finish(gfp,pOutput,0);

    *pdwOutput = lame_encode_flush( gfp, pOutput, 0 );

	if (*pdwOutput<0) {
		*pdwOutput=0;
		return BE_ERR_BUFFER_TOO_SMALL;
	}

    
	// lame will be close in VbrWriteTag function
	if ( !lame_get_bWriteVbrTag( gfp ) )
	{
		// clean up of allocated memory
		lame_close( gfp );
	}

	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR	beCloseStream(HBE_STREAM hbeStream)
{
	// DeInit encoder
//	return DeInitEncoder();
	return BE_ERR_SUCCESSFUL;
}



__declspec(dllexport) VOID		beVersion(PBE_VERSION pbeVersion)
{
	// DLL Release date
	char lpszDate[20]	= { '\0', };
	char lpszTemp[5]	= { '\0', };
	lame_version_t lv   = { 0, };


	// Set DLL interface version
	pbeVersion->byDLLMajorVersion=MAJORVERSION;
	pbeVersion->byDLLMinorVersion=MINORVERSION;

	get_lame_version_numerical ( &lv );

	// Set Engine version number (Same as Lame version)
	pbeVersion->byMajorVersion = lv.major;
	pbeVersion->byMinorVersion = lv.minor;
	pbeVersion->byAlphaLevel   = lv.alpha;
	pbeVersion->byBetaLevel	   = lv.beta;

#ifdef MMX_choose_table
	pbeVersion->byMMXEnabled=1;
#else
	pbeVersion->byMMXEnabled=0;
#endif

	memset( pbeVersion->btReserved, 0, sizeof( pbeVersion->btReserved ) );

	// Get compilation date
	strcpy(lpszDate,__DATE__);

	// Get the first three character, which is the month
	strncpy(lpszTemp,lpszDate,3);
	lpszTemp[3] = '\0';
	pbeVersion->byMonth=1;

	// Set month
	if (strcmp(lpszTemp,"Jan")==0)	pbeVersion->byMonth = 1;
	if (strcmp(lpszTemp,"Feb")==0)	pbeVersion->byMonth = 2;
	if (strcmp(lpszTemp,"Mar")==0)	pbeVersion->byMonth = 3;
	if (strcmp(lpszTemp,"Apr")==0)	pbeVersion->byMonth = 4;
	if (strcmp(lpszTemp,"May")==0)	pbeVersion->byMonth = 5;
	if (strcmp(lpszTemp,"Jun")==0)	pbeVersion->byMonth = 6;
	if (strcmp(lpszTemp,"Jul")==0)	pbeVersion->byMonth = 7;
	if (strcmp(lpszTemp,"Aug")==0)	pbeVersion->byMonth = 8;
	if (strcmp(lpszTemp,"Sep")==0)	pbeVersion->byMonth = 9;
	if (strcmp(lpszTemp,"Oct")==0)	pbeVersion->byMonth = 10;
	if (strcmp(lpszTemp,"Nov")==0)	pbeVersion->byMonth = 11;
	if (strcmp(lpszTemp,"Dec")==0)	pbeVersion->byMonth = 12;

	// Get day of month string (char [4..5])
	pbeVersion->byDay=atoi( lpszDate + 4 );

	// Get year of compilation date (char [7..10])
	pbeVersion->wYear = atoi( lpszDate + 7 );

	memset( pbeVersion->zHomepage, 0x00, BE_MAX_HOMEPAGE );

	strcpy( pbeVersion->zHomepage, "http://www.mp3dev.org/" );
}

__declspec(dllexport) BE_ERR	beEncodeChunk(HBE_STREAM hbeStream, DWORD nSamples, 
			 PSHORT pSamples, PBYTE pOutput, PDWORD pdwOutput)
{

	// Encode it
	int dwSamples;

	dwSamples = nSamples / lame_get_num_channels( gfp );

	// old versions of lame_enc.dll required exactly 1152 samples
	// and worked even if nSamples accidently set to 2304 
	// simulate this behavoir:
	if ( 1 == lame_get_num_channels( gfp ) && nSamples == 2304)
	{
	  dwSamples/= 2;
	}


	if ( 1 == lame_get_num_channels( gfp ) )
	{
		*pdwOutput=lame_encode_buffer(gfp,pSamples,pSamples,dwSamples,pOutput,0);
	}
	else
	{
		*pdwOutput=lame_encode_buffer_interleaved(gfp,pSamples,dwSamples,pOutput,0);
	}


	if (*pdwOutput<0) {
		*pdwOutput=0;
		return BE_ERR_BUFFER_TOO_SMALL;
	}

	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR beWriteVBRHeader(LPCSTR lpszFileName)
{
	FILE* fpStream	=NULL;
	BE_ERR beResult	=BE_ERR_SUCCESSFUL;

	// Do we have to write the VBR tag?
	if (	( lame_get_bWriteVbrTag( gfp ) ) && 
			( vbr_off != lame_get_VBR( gfp ) ) )
	{
		// Calculate relative quality of VBR stream 
		// 0=best, 100=worst
		int nQuality = lame_get_VBR_q( gfp ) * 100 / 9;

		// Try to open the file
		fpStream=fopen( lpszFileName, "rb+" );

		// Check file open result
		if ( NULL == fpStream )
		{
			return BE_ERR_INVALID_FORMAT_PARAMETERS;
		}

		// Write Xing header again
		lame_mp3_tags_fid( gfp, fpStream );

		// Close the file stream
		fclose( fpStream );

	}

	if ( lame_get_bWriteVbrTag( gfp ) )
	{
		// clean up of allocated memory
		lame_close( gfp );
	}

	// return result
	return beResult;
}


BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  ul_reason_for_call, 
                      LPVOID lpReserved)
{
	gs_hModule=hModule;

    switch( ul_reason_for_call )
	{
		case DLL_PROCESS_ATTACH:
			// Enable debug/logging?
			gs_bLogFile = GetPrivateProfileInt("Debug","WriteLogFile",gs_bLogFile,"lame_enc.ini");
		break;
		case DLL_THREAD_ATTACH:
		break;
		case DLL_THREAD_DETACH:
		break;
		case DLL_PROCESS_DETACH:
		break;
    }
    return TRUE;
}


static void dump_config( )
{
	DebugPrintf("\n\nLame_enc configuration options:\n");
	DebugPrintf("==========================================================\n");

	DebugPrintf("version                =%d\n",lame_get_version( gfp ) );
	DebugPrintf("Layer                  =3\n");
	DebugPrintf("mode                   =");
	switch ( lame_get_mode( gfp ) )
	{
		case 0:DebugPrintf( "Stereo\n" ); break;
		case 1:DebugPrintf( "Joint-Stereo\n" ); break;
		case 2:DebugPrintf( "Forced Stereo\n" ); break;
		case 3:DebugPrintf( "Mono\n" ); break;
		default:DebugPrintf( "Error (unknown)\n" ); break;
	}

	DebugPrintf("sampling frequency     =%.1f kHz\n", lame_get_in_samplerate( gfp ) /1000.0 );
	DebugPrintf("bitrate                =%d kbps\n", lame_get_brate( gfp ) );
	DebugPrintf("Vbr Min bitrate        =%d kbps\n", lame_get_VBR_min_bitrate_kbps( gfp ) );
	DebugPrintf("Vbr Max bitrate        =%d kbps\n", lame_get_VBR_max_bitrate_kbps( gfp ) );
	DebugPrintf("Quality Setting        =%d\n", lame_get_quality( gfp ) );

	DebugPrintf("Low pass frequency     =%d\n", lame_get_lowpassfreq( gfp ) );
	DebugPrintf("Low pass width         =%d\n", lame_get_lowpasswidth( gfp ) );

	DebugPrintf("High pass frequency    =%d\n", lame_get_highpassfreq( gfp ) );
	DebugPrintf("High pass width        =%d\n", lame_get_highpasswidth( gfp ) );

	DebugPrintf("No Short Blocks        =%d\n", lame_get_no_short_blocks( gfp ) );

	DebugPrintf("de-emphasis            =%d\n", lame_get_emphasis( gfp ) );
	DebugPrintf("private flag           =%d\n", lame_get_extension( gfp ) );

	DebugPrintf("copyright flag         =%d\n", lame_get_copyright( gfp ) );
	DebugPrintf("original flag          =%d\n",	lame_get_original( gfp ) );
	DebugPrintf("CRC                    =%s\n", lame_get_error_protection( gfp ) ? "on" : "off" );
	DebugPrintf("Fast mode              =%s\n", ( lame_get_quality( gfp ) )? "enabled" : "disabled" );
	DebugPrintf("Force mid/side stereo  =%s\n", ( lame_get_force_ms( gfp ) )?"enabled":"disabled" );
	DebugPrintf("Padding Type           =%d\n", lame_get_padding_type( gfp ) );
	DebugPrintf("Disable Resorvoir      =%d\n", lame_get_disable_reservoir( gfp ) );
	DebugPrintf("VBR                    =%s, VBR_q =%d, VBR method =",
					( lame_get_VBR( gfp ) !=vbr_off ) ? "enabled": "disabled",
		            lame_get_VBR_q( gfp ) );

	switch ( lame_get_VBR( gfp ) )
	{
		case vbr_off:	DebugPrintf( "vbr_off\n" );	break;
		case vbr_mt :	DebugPrintf( "vbr_mt \n" );	break;
		case vbr_rh :	DebugPrintf( "vbr_rh \n" );	break;
		case vbr_mtrh:	DebugPrintf( "vbr_mtrh \n" );	break;
		case vbr_abr: 
			DebugPrintf( "vbr_abr (average bitrate %d kbps)\n", lame_get_VBR_mean_bitrate_kbps( gfp ) );
		break;
		default:
			DebugPrintf("error, unknown VBR setting\n");
		break;
	}

	DebugPrintf("Write VBR Header       =%s\n", ( lame_get_bWriteVbrTag( gfp ) ) ?"Yes":"No");
}


static void DispErr(LPSTR strErr)
{
	MessageBox(NULL,strErr,"",MB_OK);
}

