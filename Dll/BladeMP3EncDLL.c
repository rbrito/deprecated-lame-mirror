/*
 *	Blade DLL Interface for LAME.
 *
 *	Copyright (c) 1999 A.L. Faber
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
// This DLL should be a wrapper around libmp3lame, and thus only need to 
// include 'lame.h'.  However, the DLL provides better version information
// that is currently available via libmp3lame and thus needs version.h
#include "version.h"  
#include "lame.h"
//#include "util.h"
//#include "VbrTag.h"


#define _RELEASEDEBUG 0

const int MAJORVERSION=1;
const int MINORVERSION=20;


// Local variables
static DWORD				dwSampleBufferSize=0;
static HANDLE				gs_hModule=NULL;
static BOOL					gs_bLogFile=FALSE;
static lame_global_flags	gf;

// Local function prototypes
static void dump_config( );
static void DebugPrintf(const char* pzFormat, ...);
static void DispErr(LPSTR strErr);
static void PresetOptions(lame_global_flags *gfp,LONG myPreset);


static void DebugPrintf(const char* pzFormat, ...)
{
    char	szBuffer[1024]={'\0',};
	char	szFileName[MAX_PATH+1]={'\0',};
    va_list ap;

	// Get the full module file name
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
	if (gs_bLogFile) 
	{	
        FILE* fp = NULL;
		
		// try to open the log file
		fp=fopen(szFileName, "a+");

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


static void PresetOptions(lame_global_flags *gfp,LONG myPreset)
{
	switch (myPreset)
	{
		case LQP_NOPRESET:
		case LQP_NORMAL_QUALITY:
			break;
		break;
		case LQP_LOW_QUALITY:		// -f flag
			gf.quality=9;
			break;
		case LQP_HIGH_QUALITY:		// -h flag for high qualtiy
			gf.quality=2;
			break;
		case LQP_VOICE_QUALITY:		// --voice flag for experimental voice mode
			gf.lowpassfreq=12000;
			gf.VBR_max_bitrate_kbps=160;
			gf.no_short_blocks=1;
		break;

		case LQP_PHONE:
			gfp->out_samplerate =  8000;
			gfp->lowpassfreq=3200;
			gfp->lowpasswidth=1000;
			gfp->no_short_blocks=1;
			gfp->quality = 5;
			gfp->mode = MPG_MD_MONO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 16; 
			gfp->VBR_q=6;
			gfp->VBR_min_bitrate_kbps=8;
			gfp->VBR_max_bitrate_kbps=56;
		break;

		case LQP_SW:
			gfp->out_samplerate =  11025;
			gfp->lowpassfreq=4800;
			gfp->lowpasswidth=500;
			gfp->quality = 5;
			gfp->mode = MPG_MD_MONO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 24; 
			gfp->VBR_q=5;
			gfp->VBR_min_bitrate_kbps=8;
			gfp->VBR_max_bitrate_kbps=64;
		break;
		case LQP_AM:
			gfp->out_samplerate =  16000;
			gfp->lowpassfreq=7200;
			gfp->lowpasswidth=500;
			gfp->quality = 5;
			gfp->mode = MPG_MD_MONO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 32; 
			gfp->VBR_q=5;
			gfp->VBR_min_bitrate_kbps=16;
			gfp->VBR_max_bitrate_kbps=128;
		break;
		case LQP_FM:
			gfp->out_samplerate =  22050; 
			gfp->lowpassfreq=9950;
			gfp->lowpasswidth=880;
			gfp->quality = 5;
			gfp->mode = MPG_MD_JOINT_STEREO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 64; 
			gfp->VBR_q=5;
			gfp->VBR_min_bitrate_kbps=24;
			gfp->VBR_max_bitrate_kbps=160;
		break;
		case LQP_VOICE:
			gfp->out_samplerate =  32000; 
			gfp->lowpassfreq=12300;
			gfp->lowpasswidth=2000;
			gfp->no_short_blocks=1;
			gfp->quality = 5;
			gfp->mode = MPG_MD_MONO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 56; 
			gfp->VBR_q=4;
			gfp->VBR_min_bitrate_kbps=32;
			gfp->VBR_max_bitrate_kbps=128;
		break;
		case LQP_RADIO:
			gfp->lowpassfreq=15000;
			gfp->lowpasswidth=0;
			gfp->quality = 5;
			gfp->mode = MPG_MD_JOINT_STEREO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 112; 
			gfp->VBR_q=4;
			gfp->VBR_min_bitrate_kbps=64;
			gfp->VBR_max_bitrate_kbps=256;
		break;
		case LQP_TAPE:
			gfp->lowpassfreq=18500;
			gfp->lowpasswidth=2000;
			gfp->quality = 5;
			gfp->mode = MPG_MD_JOINT_STEREO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 128; 
			gfp->VBR_q=4;
			gfp->VBR_min_bitrate_kbps=96;
			gfp->VBR_max_bitrate_kbps=320;
		break;
		case LQP_HIFI:
			gfp->lowpassfreq=20240;
			gfp->lowpasswidth=2200;
			gfp->quality = 2;
			gfp->mode = MPG_MD_JOINT_STEREO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 160;            
			gfp->VBR_q=3;
			gfp->VBR_min_bitrate_kbps=112;
			gfp->VBR_max_bitrate_kbps=320;
		break;
		case LQP_CD:
			gfp->lowpassfreq=-1;
			gfp->highpassfreq=-1;
			gfp->quality = 2;
			gfp->mode = MPG_MD_STEREO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 192;  
			gfp->VBR_q=2;
			gfp->VBR_min_bitrate_kbps=128;
			gfp->VBR_max_bitrate_kbps=320;
		break;
		case LQP_STUDIO:
			gfp->lowpassfreq=-1;
			gfp->highpassfreq=-1;
			gfp->quality = 2; 
			gfp->mode = MPG_MD_STEREO; 
			gfp->mode_fixed = 1; 
			gfp->brate = 256; 
			gfp->VBR_q=0;
			gfp->VBR_min_bitrate_kbps=160;
			gfp->VBR_max_bitrate_kbps=320;
		break;
	}
}


__declspec(dllexport) BE_ERR	beInitStream(PBE_CONFIG pbeConfig, PDWORD dwSamples, PDWORD dwBufferSize, PHBE_STREAM phbeStream)
{
	int			nDllArgC=0;
	BE_CONFIG	lameConfig;

	// Init the global flags structure
	lame_init_old(&gf);

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
		if (nVBR>0)
		{
			lameConfig.format.LHV1.bWriteVBRHeader=TRUE;
			lameConfig.format.LHV1.bEnableVBR=TRUE;
			lameConfig.format.LHV1.nVBRQuality=nVBR-1;
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
	gf.in_samplerate=lameConfig.format.LHV1.dwSampleRate;

	// The following settings only use when preset is not one of the new LAME QUALITY Presets
	if ((int)lameConfig.format.LHV1.nPreset<(int)LQP_PHONE)
	{
  		switch (lameConfig.format.LHV1.nMode)
		{
			case BE_MP3_MODE_STEREO:
				gf.mode=0;
				gf.mode_fixed=1;  /* dont allow LAME to change the mode */
				gf.num_channels=2;
			break;
			case BE_MP3_MODE_JSTEREO:
				gf.mode=1;
				gf.mode_fixed=1;
				gf.num_channels=2;
			break;
			case BE_MP3_MODE_MONO:
				gf.mode=3;
				gf.mode_fixed=1;
				gf.num_channels=1;
			break;
			case BE_MP3_MODE_DUALCHANNEL:
				gf.force_ms=1;
				gf.mode=1;
				gf.mode_fixed=1;
				gf.num_channels=2;
			break;
			default:
			{
				DebugPrintf("Invalid lameConfig.format.LHV1.nMode, value is %d\n",lameConfig.format.LHV1.nMode);
				return BE_ERR_INVALID_FORMAT_PARAMETERS;
			}
		}

		if (lameConfig.format.LHV1.bEnableVBR)
		{
			// 0=no vbr 1..10 is VBR quality setting -1
			//gf.VBR=vbr_default;
			gf.VBR = vbr_mtrh;

			gf.VBR_q=lameConfig.format.LHV1.nVBRQuality;

			if (lameConfig.format.LHV1.bWriteVBRHeader==TRUE)
			{
				gf.bWriteVbrTag=TRUE;
			}
			else
			{
				gf.bWriteVbrTag=FALSE;
			}
		}

		// Set frequency resampling rate, if specified
		if (lameConfig.format.LHV1.dwReSampleRate>0)
			gf.out_samplerate=lameConfig.format.LHV1.dwReSampleRate;
		
	
		// Set bitrate.  (CDex users always specify bitrate=Min bitrate when using VBR)
		gf.brate=lameConfig.format.LHV1.dwBitrate;
		gf.VBR_min_bitrate_kbps=gf.brate;
			
		// Set Maxbitrate, if specified
		if (lameConfig.format.LHV1.dwMaxBitrate>0)
			gf.VBR_max_bitrate_kbps=lameConfig.format.LHV1.dwMaxBitrate;

		// Use ABR?
		if (lameConfig.format.LHV1.dwVbrAbr_bps>0)
		{
			// set VBR to ABR
			gf.VBR = vbr_abr; 

			// calculate to kbps
			gf.VBR_mean_bitrate_kbps = ( lameConfig.format.LHV1.dwVbrAbr_bps + 500 ) / 1000;
			// limit range
			if(gf.VBR_mean_bitrate_kbps>320) gf.VBR_mean_bitrate_kbps = 320;
			if(gf.VBR_mean_bitrate_kbps<8) gf.VBR_mean_bitrate_kbps = 8;

		}

	}
	
	// Set copyright flag?
	if (lameConfig.format.LHV1.bCopyright)
		gf.copyright=1;

	// Do we have to tag  it as non original 
	if (!lameConfig.format.LHV1.bOriginal)
	{
		gf.original=0;
	}
	else
	{
		gf.original=1;
	}

	// Add CRC?
	if (lameConfig.format.LHV1.bCRC)
	{
		gf.error_protection=1;
	}
	else
	{
		gf.error_protection=0;
	}

//	gf.silent=1;  /* disable status ouput */

	// Set private bit?
	if (lameConfig.format.LHV1.bPrivate)
	{
		gf.extension = 1;
	}
	else
	{
		gf.extension = 0;
	}
	

	// First set all the preset options
	if ((int)lameConfig.format.LHV1.nPreset)
		PresetOptions(&gf,lameConfig.format.LHV1.nPreset);

	if (lameConfig.format.LHV1.bNoRes)
	{
		gf.disable_reservoir=1;
		gf.padding_type=0;
	}

	lame_init_params(&gf);	

	//LAME encoding call will accept any number of samples.  
	if (gf.version==0)
	{
		// For MPEG-II, only 576 samples per frame per channel
		*dwSamples=576*gf.num_channels;
	}
	else
	{
		// For MPEG-I, 1152 samples per frame per channel
		*dwSamples=1152*gf.num_channels;
	}

	// Set the input sample buffer size, so we know what we can expect
	dwSampleBufferSize=*dwSamples;

	// Set MP3 buffer size
	// conservative estimate
	*dwBufferSize=(DWORD)(1.25*(*dwSamples/gf.num_channels) + 7200);


	// For debugging purposes
	dump_config( );

	// Everything went OK, thus return SUCCESSFUL
	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR	beDeinitStream(HBE_STREAM hbeStream, PBYTE pOutput, PDWORD pdwOutput)
{

//	*pdwOutput = lame_encode_finish(&gf,pOutput,0);

    *pdwOutput = lame_encode_flush( &gf, pOutput, 0 );

	if (*pdwOutput<0) {
		*pdwOutput=0;
		return BE_ERR_BUFFER_TOO_SMALL;
	}

    

	if ( !gf.bWriteVbrTag )
	{
		lame_close( &gf );
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
	char lpszDate[20]	={'\0',};
	char lpszTemp[5]	={'\0',};


	// Set DLL interface version
	pbeVersion->byDLLMajorVersion=MAJORVERSION;
	pbeVersion->byDLLMinorVersion=MINORVERSION;

	// Set Engine version number (Same as Lame version)
	pbeVersion->byMajorVersion=LAME_MAJOR_VERSION;
	pbeVersion->byMinorVersion=LAME_MINOR_VERSION;
	pbeVersion->byAlphaLevel=LAME_ALPHA_VERSION;
	pbeVersion->byBetaLevel=LAME_BETA_VERSION;

#ifdef MMX_choose_table
	pbeVersion->byMMXEnabled=1;
#else
	pbeVersion->byMMXEnabled=0;
#endif

	memset(pbeVersion->btReserved,0, sizeof(pbeVersion->btReserved));

	// Get compilation date
	strcpy(lpszDate,__DATE__);

	// Get the first three character, which is the month
	strncpy(lpszTemp,lpszDate,3);
	lpszTemp[3]='\0';
	pbeVersion->byMonth=1;

	// Set month
	if (strcmp(lpszTemp,"Jan")==0)	pbeVersion->byMonth=1;
	if (strcmp(lpszTemp,"Feb")==0)	pbeVersion->byMonth=2;
	if (strcmp(lpszTemp,"Mar")==0)	pbeVersion->byMonth=3;
	if (strcmp(lpszTemp,"Apr")==0)	pbeVersion->byMonth=4;
	if (strcmp(lpszTemp,"May")==0)	pbeVersion->byMonth=5;
	if (strcmp(lpszTemp,"Jun")==0)	pbeVersion->byMonth=6;
	if (strcmp(lpszTemp,"Jul")==0)	pbeVersion->byMonth=7;
	if (strcmp(lpszTemp,"Aug")==0)	pbeVersion->byMonth=8;
	if (strcmp(lpszTemp,"Sep")==0)	pbeVersion->byMonth=9;
	if (strcmp(lpszTemp,"Oct")==0)	pbeVersion->byMonth=10;
	if (strcmp(lpszTemp,"Nov")==0)	pbeVersion->byMonth=11;
	if (strcmp(lpszTemp,"Dec")==0)	pbeVersion->byMonth=12;

	// Get day of month string (char [4..5])
	pbeVersion->byDay=atoi(lpszDate+4);

	// Get year of compilation date (char [7..10])
	pbeVersion->wYear=atoi(lpszDate+7);

	memset(pbeVersion->zHomepage,0x00,BE_MAX_HOMEPAGE);

	strcpy(pbeVersion->zHomepage,"http://www.mp3dev.org/");
}

__declspec(dllexport) BE_ERR	beEncodeChunk(HBE_STREAM hbeStream, DWORD nSamples, 
			 PSHORT pSamples, PBYTE pOutput, PDWORD pdwOutput)
{

	// Encode it
        int dwSamples;
	dwSamples=nSamples/gf.num_channels;

	// old versions of lame_enc.dll required exactly 1152 samples
	// and worked even if nSamples accidently set to 2304 
	// simulate this behavoir:
	if (gf.num_channels==1 && nSamples == 2304)
	  dwSamples/=2;

	*pdwOutput=lame_encode_buffer_interleaved(&gf,pSamples,dwSamples,pOutput,0);


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
	if ( (gf.bWriteVbrTag) && (gf.VBR!=vbr_off) )
	{
		// Calculate relative quality of VBR stream 
		// 0=best, 100=worst
		int nQuality=gf.VBR_q*100/9;

		// Try to open the file
		fpStream=fopen(lpszFileName,"rb+");

		// Check file open result
		if (fpStream==NULL)
		  return BE_ERR_INVALID_FORMAT_PARAMETERS;

#if 0
		// Write Xing header again
		beResult=PutVbrTag(&gf,fpStream,nQuality);
#else
		lame_mp3_tags_fid(&gf,fpStream);
#endif

		// Close the file stream
		fclose(fpStream);

	}

	if ( gf.bWriteVbrTag )
	{
		// clean up of allocated memory
		lame_close( &gf );
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
			gs_bLogFile=GetPrivateProfileInt("Debug","WriteLogFile",gs_bLogFile,"lame_enc.ini");
//			DebugPrintf("Attach Process \n");
		break;
		case DLL_THREAD_ATTACH:
//			DebugPrintf("Attach Thread \n");
		break;
		case DLL_THREAD_DETACH:
//			DebugPrintf("Detach Thread \n");
		break;
		case DLL_PROCESS_DETACH:
//			DebugPrintf("Detach Process \n");
		break;
    }
    return TRUE;
}


static void dump_config( )
{
	DebugPrintf("\n\nLame_enc configuration options:\n");
	DebugPrintf("==========================================================\n");

	DebugPrintf("version                =%d\n",gf.version);
	DebugPrintf("Layer                  =3\n");
	DebugPrintf("mode                   =");
	switch (gf.mode)
	{
		case 0:DebugPrintf("Stereo\n");break;
		case 1:DebugPrintf("Joint-Stereo\n");break;
		case 2:DebugPrintf("Dual-Channel\n");break;
		case 3:DebugPrintf("Mono\n");break;
		default:DebugPrintf("Error (unknown)\n");break;
	}

	DebugPrintf("sampling frequency     =%.1f kHz\n",gf.in_samplerate/1000.0);
	DebugPrintf("bitrate                =%d kbps\n",gf.brate);
	DebugPrintf("Vbr Min bitrate        =%d kbps\n",gf.VBR_min_bitrate_kbps);
	DebugPrintf("Vbr Max bitrate        =%d kbps\n",gf.VBR_max_bitrate_kbps);
	DebugPrintf("Quality Setting        =%d\n",gf.quality);
	DebugPrintf("Low pass frequency     =%d\n",gf.lowpassfreq);
	DebugPrintf("High pass frequency    =%d\n",gf.highpassfreq);
	DebugPrintf("No Short Blocks        =%s\n",(gf.no_short_blocks)?"yes":"no");
	DebugPrintf("de-emphasis            =%d\n",gf.emphasis);
	DebugPrintf("private flag           =%d\n",gf.extension);
	DebugPrintf("copyright flag         =%d\n",gf.copyright);
	DebugPrintf("original flag          =%d\n",gf.original);
	DebugPrintf("CRC                    =%s\n",(gf.error_protection) ? "on" : "off");
	DebugPrintf("Fast mode              =%s\n",(gf.quality==9)?"enabled":"disabled");
	DebugPrintf("Force mid/side stereo  =%s\n",(gf.force_ms)?"enabled":"disabled");
	DebugPrintf("Padding Type           =%d\n",gf.padding_type);
	DebugPrintf("Disable Resorvoir      =%d\n",gf.disable_reservoir);
	DebugPrintf("VBR                    =%s, VBR_q =%d, VBR method =",(gf.VBR!=vbr_off)?"enabled":"disabled",gf.VBR_q);

	switch (gf.VBR)
	{
		case vbr_off:	DebugPrintf("vbr_off\n");	break;
		case vbr_mt :	DebugPrintf("vbr_mt \n");	break;
		case vbr_rh :	DebugPrintf("vbr_rh \n");	break;
		case vbr_mtrh:	DebugPrintf("vbr_mtrh \n");	break;
		case vbr_abr: 
			DebugPrintf(" vbr_abr (average bitrate %d kbps)\n",gf.VBR_mean_bitrate_kbps);
		break;
		default:
			DebugPrintf("error, unknown VBR setting\n");
		break;
	}

	DebugPrintf("Write VBR Header       =%s\n",(gf.bWriteVbrTag)?"Yes":"No");
//	DebugPrintf("input file: '%s'   output file: '%s'\n", inPath, outPath);
//	DebugPrintf("Voice mode %s\n",(voice_mode)?"enabled":"disabled");
//	DebugPrintf("Encoding as %.1f kHz %d kbps MPEG-%d LayerIII file\n",gf.out_samplerate/1000.0,gf.brate,gf.mode,3 - gf.version);
}


static void DispErr(LPSTR strErr)
{
	MessageBox(NULL,strErr,"",MB_OK);
}

