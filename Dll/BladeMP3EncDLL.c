/*
 *	Blade DLL Interface for LAME.
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

#include "machine.h"
#include "BladeMP3EncDLL.h"
#include <assert.h>
#include "util.h"
#include "version.h"
#include "VbrTag.h"
#include "lame.h"
#include "get_audio.h"
#include "globalflags.h"

#ifdef _DEBUG
	#define _DEBUGDLL 1
#endif


const int MAJORVERSION=1;
const int MINORVERSION=05;
const int CURRENT_STRUCT_VERSION=1;
const int CURRENT_STRUCT_SIZE=sizeof(BE_CONFIG);

static short int InputBuffer[2][1152];

int nBladeBufferSize;

// Local variables
static int		nPsychoModel=2;
static BOOL		bFirstFrame=TRUE;
static DWORD	dwSampleBufferSize=0;


#ifdef _DEBUGDLL
void dump_config( frame_params *fr_ps, int *psy, char *inPath, char *outPath);
#endif

// Taken from main.c
static char			original_file_name[MAX_NAME_SIZE];
static char			encoded_file_name[MAX_NAME_SIZE];
static lame_global_flags *gf;

extern Bit_stream_struc   bs;
extern III_side_info_t l3_side;
extern frame_params fr_ps;
extern FLOAT resample_ratio;

static void InitParams()
{
    // clear buffers
    memset(InputBuffer, 0,sizeof(InputBuffer));
    bFirstFrame=TRUE;
    // Initialize output buffer
    bs.pbtOutBuf=NULL;
    bs.nOutBufPos=0;
    gf=lame_init();

}


#define GPL_PSYCHOMODEL 0
#define ISO_PSYCHOMODEL 1



#define MAX_ARGV 25

__declspec(dllexport) BE_ERR	beInitStream(PBE_CONFIG pbeConfig, PDWORD dwSamples, PDWORD dwBufferSize, PHBE_STREAM phbeStream)
{
	char		strTmp[255];
	int			nDllArgC=0;
	char		DllArgV[20][80];
	char*		argv[MAX_ARGV];
	int			i;
	BE_CONFIG	lameConfig;
	layer*		pInfo = NULL;


	// clear out structure
	memset(&lameConfig,0x00,CURRENT_STRUCT_SIZE);

	// Check if this is a regular BLADE_ENCODER header
	if (pbeConfig->dwConfig!=BE_CONFIG_LAME)
	{
		int	nCRC=pbeConfig->format.mp3.bCRC;
		int nVBR=(nCRC>>12)&0x0F;

		// Copy parameter from old Blade structure
		lameConfig.format.LHV1.dwSampleRate	=pbeConfig->format.mp3.dwSampleRate;
		lameConfig.format.LHV1.dwReSampleRate=pbeConfig->format.mp3.dwSampleRate;
		lameConfig.format.LHV1.nMode		=(pbeConfig->format.mp3.byMode&0x0F);
		lameConfig.format.LHV1.dwBitrate	=pbeConfig->format.mp3.wBitrate;
		lameConfig.format.LHV1.bPrivate		=pbeConfig->format.mp3.bPrivate;
		lameConfig.format.LHV1.bPrivate		=pbeConfig->format.mp3.bPrivate;
		lameConfig.format.LHV1.bOriginal	=pbeConfig->format.mp3.bOriginal;
		lameConfig.format.LHV1.bCRC			=nCRC&0x01;
		lameConfig.format.LHV1.bCopyright	=pbeConfig->format.mp3.bCopyright;
	
		// Fill out the unknowns
		lameConfig.format.LHV1.dwStructSize=CURRENT_STRUCT_VERSION;
		lameConfig.format.LHV1.dwStructVersion=CURRENT_STRUCT_SIZE;

		// Get VBR setting from fourth nibble
		if (nVBR>0)
		{
			lameConfig.format.LHV1.bWriteVBRHeader=TRUE;
			lameConfig.format.LHV1.bEnableVBR=TRUE;
			lameConfig.format.LHV1.nVBRQuality=nVBR-1;
		}

		// Get Quality from third nibble
		lameConfig.format.LHV1.nQuality=(MPEG_QUALITY)((nCRC>>8)&0x0F);

	}
	else
	{
		// Copy the parameters
		memcpy(&lameConfig,pbeConfig,pbeConfig->format.LHV1.dwStructSize);
	}

	for (i=0;i<MAX_ARGV;i++)
		argv[i]=DllArgV[i];

	// Clear the external and local paramters
	InitParams();

	// Clear argument array
	memset(&DllArgV[0][0],0x00,sizeof(DllArgV));

	// Not used, always assign stream 1
	*phbeStream=1;

	// Set MP3 buffer size
	*dwBufferSize=BUFFER_SIZE*2;


	// --------------- Set arguments for ParseArg function -------------------------

	// Set zero argument, the filename
	strcpy(DllArgV[nDllArgC++],"LameDLLEncoder");

  	switch (lameConfig.format.LHV1.nMode)
	{
		case BE_MP3_MODE_STEREO:
			strcpy(DllArgV[nDllArgC++],"-ms");
		break;
		case BE_MP3_MODE_JSTEREO:
			strcpy(DllArgV[nDllArgC++],"-mj");
		break;
		case BE_MP3_MODE_MONO:
			strcpy(DllArgV[nDllArgC++],"-mm");
		break;
		case BE_MP3_MODE_DUALCHANNEL:
			strcpy(DllArgV[nDllArgC++],"-mf");
		break;
		default:
		{
			char lpszError[255];
			sprintf(lpszError,"Invalid lameConfig.format.LHV1.nMode, value is %d\n",lameConfig.format.LHV1.nMode);
			OutputDebugString(lpszError);
			return BE_ERR_INVALID_FORMAT_PARAMETERS;
		}
	}

	switch (lameConfig.format.LHV1.nQuality)
	{
		case NORMAL_QUALITY:	// Nothing special
		break;
		case LOW_QUALITY:		// -f flag
			strcpy(DllArgV[nDllArgC++],"-f");
		break;
		case HIGH_QUALITY:		// -h flag for high qualtiy
			strcpy(DllArgV[nDllArgC++],"-h");
        break;
		case VOICE_QUALITY:		// --voice flag for high qualtiy
			strcpy(DllArgV[nDllArgC++],"--voice");
		break;
	}

	if (lameConfig.format.LHV1.bEnableVBR)
	{
		// 0=no vbr 1..10 is VBR quality setting -1
		sprintf(DllArgV[nDllArgC++],"-v");
		sprintf(DllArgV[nDllArgC++],"-V%d",lameConfig.format.LHV1.nVBRQuality);
	}

	// Set frequency
	sprintf(strTmp,"-s %f",lameConfig.format.LHV1.dwSampleRate/1000.0);
	strcpy(DllArgV[nDllArgC++],strTmp);

	// Set frequency resampling rate, if specified
	if (lameConfig.format.LHV1.dwReSampleRate>0)
	{
		strcpy(DllArgV[nDllArgC++],"--resample");
		sprintf(strTmp,"%f",lameConfig.format.LHV1.dwReSampleRate/1000.0);
		strcpy(DllArgV[nDllArgC++],strTmp);
	}
	
	// Set bitrate
	sprintf(strTmp,"-b %d",lameConfig.format.LHV1.dwBitrate);
	strcpy(DllArgV[nDllArgC++],strTmp);
	
	// Set Maxbitrate, if specified
	if (lameConfig.format.LHV1.dwMaxBitrate>0)
	{
		sprintf(strTmp,"-B %d",lameConfig.format.LHV1.dwMaxBitrate);
		strcpy(DllArgV[nDllArgC++],strTmp);
	}

	// Set copyright flag?
    if (lameConfig.format.LHV1.bCopyright)
		strcpy(DllArgV[nDllArgC++],"-c");

	// Do we have to tag  it as non original 
    if (!lameConfig.format.LHV1.bOriginal)
		strcpy(DllArgV[nDllArgC++],"-o");

	// Add CRC?
    if (lameConfig.format.LHV1.bCRC)
		strcpy(DllArgV[nDllArgC++],"-p");

	// Add input filename
	strcpy(DllArgV[nDllArgC++],"NULL.wav");

	// Add output filename
	strcpy(DllArgV[nDllArgC++],"NULL.mp3");


	// Set the encoder variables
	lame_parse_args(nDllArgC,argv);
	gf.silent=1;  /* disable status ouput */
	lame_init_params();

	// Set pointer to fr_ps header
	pInfo = fr_ps.header;

	// Set private bit?
	if (lameConfig.format.LHV1.bPrivate)
	{
		pInfo->extension = 0;
	}
	else
	{
		pInfo->extension = 1;
	}


	//hdr_to_frps(&fr_ps);  /* now called in parse_args */

    if (pInfo->lay != 3)
	{
		char lpszError[255];
		sprintf(lpszError,"Invalid pInfo->Lay value (=%d), it shoud be 3\n",pInfo->lay);
		OutputDebugString(lpszError);
		return BE_ERR_INVALID_FORMAT_PARAMETERS;
    }

	// Set number of input samples depending on the number of samples
	*dwSamples=(pInfo->version==MPEG2)?1152:2304;

	*dwSamples=(DWORD)(resample_ratio* *dwSamples);
	if ((lameConfig.format.LHV1.nMode)== BE_MP3_MODE_MONO)
	{
		*dwSamples/=2;
	}


	// Set the input sample buffer size, so we know what we can expect
	dwSampleBufferSize=*dwSamples;

#ifdef _DEBUGDLL
	dump_config(&fr_ps,&nPsychoModel,original_file_name,encoded_file_name);
#endif

	// Everything went OK, thus return SUCCESSFUL
	return BE_ERR_SUCCESSFUL;
}



__declspec(dllexport) BE_ERR	beDeinitStream(HBE_STREAM hbeStream, PBYTE pOutput, PDWORD pdwOutput)
{
	// Set output buffer
	bs.pbtOutBuf=pOutput;
	bs.nOutBufPos=0;

	// Crank out the last part of the buffer
	if (fr_ps.header->lay == 3 )
		III_FlushBitstream();

	// close the stream
	//close_bit_stream_w( &bs );  /* routine removed */

	// Flush last bytes
	//write_buffer(&bs, 1+bs.buf_byte_idx);  /* ouput to mp3 file */
	*pdwOutput = copy_buffer((char*)&bs.pbtOutBuf[bs.nOutBufPos],&bs);
	empty_buffer(&bs);  /* empty buffer */

	// Number of bytes in output buffer
	bs.nOutBufPos= *pdwOutput;

	// Deallocate all buffers
	desalloc_buffer(&bs);

	// Everything went OK, thus return SUCCESSFUL
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
	char lpszDate[20];
	char lpszTemp[5];


	// Set DLL interface version
	pbeVersion->byDLLMajorVersion=MAJORVERSION;
	pbeVersion->byDLLMinorVersion=MINORVERSION;

	// Set Engine version number (Same as Lame version)
	pbeVersion->byMajorVersion=LAME_MAJOR_VERSION;
	pbeVersion->byMinorVersion=LAME_MINOR_VERSION;

	// Get compilation date
	strcpy(lpszDate,__DATE__);

	// Get the first three character, which is the month
	strncpy(lpszTemp,lpszDate,3);

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

	strcpy(pbeVersion->zHomepage,"http://www.sulaco.org/mp3/");
}


__declspec(dllexport) BE_ERR	beEncodeChunk(HBE_STREAM hbeStream, DWORD nSamples, PSHORT pSamples, PBYTE pOutput, PDWORD pdwOutput)
{
	int iSampleIndex;

	// Set output buffer
	bs.pbtOutBuf=pOutput;
	bs.nOutBufPos=0;

	// Is this the last (incomplete) frame
	if (nSamples<dwSampleBufferSize)
	{
		// Padd with zeros
		memset(pSamples+nSamples,0x00,(dwSampleBufferSize-nSamples)*sizeof(SHORT));
	}

	// Set buffer size, in number of bytes
	nBladeBufferSize=nSamples*sizeof(SHORT);

	if (gf.stereo==2)
	{
		for (iSampleIndex=0;iSampleIndex<nSamples/2;iSampleIndex++)
		{
			// Copy new sample data into InputBuffer
			InputBuffer[0][iSampleIndex]=*pSamples++;
			InputBuffer[1][iSampleIndex]=*pSamples++;
		}
	}
	else
	{
		// Mono, only put it data into buffer[0] (=left channel)
		for (iSampleIndex=0;iSampleIndex<nSamples;iSampleIndex++)
		{
			// Copy new sample data into InputBuffer
			InputBuffer[0][iSampleIndex]=*pSamples++;
		}
	}


	// Encode it
	*pdwOutput=lame_encode(InputBuffer,(char*)&bs.pbtOutBuf[bs.nOutBufPos]);

	// Set number of output bytes
	bs.nOutBufPos=*pdwOutput;

	return BE_ERR_SUCCESSFUL;
}


__declspec(dllexport) BE_ERR beWriteVBRHeader(LPCSTR lpszFileName)
{
	if (gf.bWriteVbrTag)
	{
		// Calculate relative quality of VBR stream 
		// 0=best, 100=worst
		int nQuality=VBR_q*100/9;

		// Write Xing header again
		return PutVbrTag((LPSTR)lpszFileName,nQuality);
	}
	return BE_ERR_INVALID_FORMAT_PARAMETERS;
}


BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  ul_reason_for_call, 
                      LPVOID lpReserved)
{
    switch( ul_reason_for_call )
	{
		case DLL_PROCESS_ATTACH:
#ifdef _DEBUGDLL
			OutputDebugString("Attach Process \n");
#endif
		break;
		case DLL_THREAD_ATTACH:
#ifdef _DEBUGDLL
			OutputDebugString("Attach Thread \n");
#endif
		break;
		case DLL_THREAD_DETACH:
#ifdef _DEBUGDLL
			OutputDebugString("Detach Thread \n");
#endif
		break;
		case DLL_PROCESS_DETACH:
#ifdef _DEBUGDLL
			OutputDebugString("Detach Process \n");
#endif
		break;
    }
    return TRUE;
}


#ifdef _DEBUGDLL
void dump_config( frame_params *fr_ps, int *psy, char *inPath, char *outPath)
{
	layer *info = fr_ps->header;
	char strTmp[255];
extern int VBR;
extern int VBR_q;
extern int voice_mode;
extern int force_ms;

	OutputDebugString("Encoding configuration:\n");


	sprintf(strTmp,"Write VBR Header=%s\n",(gf.bWriteVbrTag)?"Yes":"No");
	OutputDebugString(strTmp);
	sprintf(strTmp,"info->version=%d\n",info->version);
	OutputDebugString(strTmp);

	if(info->mode != MPG_MD_JOINT_STEREO)
	{
		sprintf(strTmp,"Layer=%d   mode=%d   extn=%d   psy model=%d\n",info->lay,info->mode,info->mode_ext, *psy);
		OutputDebugString(strTmp);
	}
	else
	{
		sprintf(strTmp,"Layer=%d   mode=%d   extn=data dependent   psy model=%d\n",info->lay, info->mode, *psy);
		OutputDebugString(strTmp);
	}

	sprintf(strTmp,"samp frq=%.1f kHz   total bitrate=%d kbps\n",s_freq[info->version][info->sampling_frequency],bitrate[info->version][info->lay-1][info->bitrate_index]);
	OutputDebugString(strTmp);

	sprintf(strTmp,"de-emph=%d   c/right=%d   orig=%d   errprot=%s\n",info->emphasis, info->copyright, info->original,((info->error_protection) ? "on" : "off"));
	OutputDebugString(strTmp);

	sprintf(strTmp,"16 Khz cut off is %s\n",(sfb21)?"enabled":"disabled");
	OutputDebugString(strTmp);

	sprintf(strTmp,"Fast mode is %s\n",(fast_mode)?"enabled":"disabled");
	OutputDebugString(strTmp);

	sprintf(strTmp,"Force ms %s\n",(force_ms)?"enabled":"disabled");
	OutputDebugString(strTmp);

	sprintf(strTmp,"GPsycho acoustic model is %s\n",(gpsycho)?"enabled":"disabled");
	OutputDebugString(strTmp);

	sprintf(strTmp,"VRB is %s, VBR_q value is  %d\n",(VBR)?"enabled":"disabled",VBR_q);
	OutputDebugString(strTmp);

//	sprintf(strTmp,"input file: '%s'   output file: '%s'\n", inPath, outPath);
//	OutputDebugString(strTmp);

	sprintf(strTmp,"Voice mode %s\n",(voice_mode)?"enabled":"disabled");
	OutputDebugString(strTmp);

	sprintf(strTmp,"Encoding as %.1f kHz %d kbps %d MPEG-%d LayerIII file\n",s_freq[info->version][info->sampling_frequency],bitrate[info->version][info->lay-1][info->bitrate_index],info->mode,2-info->version);
	OutputDebugString(strTmp);
}


void DispErr(LPSTR strErr)
{
	MessageBox(NULL,strErr,"",MB_OK);
}

#endif
