/************************************************************************
Project               : MP3 Windows ACM driver using lame
File version          : 0.1

BSD License post 1999 : 

Copyright (c) 2001, Steve Lhomme
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

- Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution. 

- The name of the author may not be used to endorse or promote products derived
from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO 
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE. 
************************************************************************/

#if !defined(STRICT)
#define STRICT
#endif // STRICT

#include <windows.h>
#include <windowsx.h>

#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>

#include <assert.h>

#include "adebug.h"
#include "resource.h"
#include "ACMStream.h"

#include "ACM.h"


#ifdef WIN32
//
//  32-bit versions
//
#if (WINVER >= 0x0400)
 #define VERSION_ACM_DRIVER  MAKE_ACM_VERSION(4,  0, 0)
#else
 #define VERSION_ACM_DRIVER  MAKE_ACM_VERSION(3, 51, 0)
#endif
#define VERSION_MSACM       MAKE_ACM_VERSION(3, 50, 0)

#else
//
//  16-bit versions
//
#define VERSION_ACM_DRIVER  MAKE_ACM_VERSION(1, 0, 0)
#define VERSION_MSACM       MAKE_ACM_VERSION(2, 1, 0)

#endif

#define PERSONAL_FORMAT    WAVE_FORMAT_MPEGLAYER3
#define SIZE_FORMAT_STRUCT sizeof(MPEGLAYER3WAVEFORMAT)
//#define FORMAT_BLOCK_ALIGN      1 // TODO : put it on a .h (to be included by the user application)
/// \todo change to 1 (and keep if it works : better granularity -> see in AVI encoding if it's more verbose or not)
#define FORMAT_BLOCK_ALIGN      1152 // TODO : put it on a .h (to be included by the user application)

//static const char channel_mode[][13] = {"mono","stereo","joint stereo","dual channel"};
static const char channel_mode[][13] = {"mono","stereo"};
static const unsigned int mpeg1_freq[] = {48000,44100,32000};
static const unsigned int mpeg2_freq[] = {24000,22050,16000,12000,11025,8000};
static const unsigned int mpeg1_bitrate[] = {320, 256, 224, 192, 160, 128, 112, 96, 80, 64, 56, 48, 40, 32};
static const unsigned int mpeg2_bitrate[] = {160, 144, 128, 112,  96,  80,  64, 56, 48, 40, 32, 24, 16,  8};

#define SIZE_CHANNEL_MODE  (sizeof(channel_mode)  / (sizeof(char) * 13))
#define SIZE_FREQ_MPEG1    (sizeof(mpeg1_freq)    / sizeof(unsigned int))
#define SIZE_FREQ_MPEG2    (sizeof(mpeg2_freq)    / sizeof(unsigned int))
#define SIZE_BITRATE_MPEG1 (sizeof(mpeg1_bitrate) / sizeof(unsigned int))
#define SIZE_BITRATE_MPEG2 (sizeof(mpeg2_bitrate) / sizeof(unsigned int))

static const int FORMAT_TAG_MAX_NB = 2; // PCM and PERSONAL (mandatory to have at least PCM and your format)
static const int FILTER_TAG_MAX_NB = 0; // this is a codec, not a filter

//static const int FORMAT_MAX_NB_PCM = 1; // number of supported PCM formats
static const int FORMAT_MAX_NB_PCM =
		 2 *                                           // number of PCM channel mode (stereo/mono)
			(SIZE_FREQ_MPEG1 + // number of MPEG 1 sampling freq
			 SIZE_FREQ_MPEG2); // number of MPEG 2 sampling freq

//static const int FORMAT_MAX_NB_PERSONAL = 2; // number of supported personal formats
static const int FORMAT_MAX_NB_PERSONAL =
  SIZE_CHANNEL_MODE *                     // number of channel mode (stereo/mono)
//			max(SIZE_BITRATE_MPEG1, SIZE_BITRATE_MPEG2); // number of MPEG supported bitrate
			(SIZE_FREQ_MPEG1 * SIZE_BITRATE_MPEG1 +  // number of MPEG 1 sampling freq * supported bitrate
			 SIZE_FREQ_MPEG2 * SIZE_BITRATE_MPEG2); // number of MPEG 2 sampling freq * supported bitrate
//////////////////////////////////////////////////////////////////////
// Configuration Dialog
//////////////////////////////////////////////////////////////////////

static CALLBACK ConfigProc(
  HWND hwndDlg,  // handle to dialog box
  UINT uMsg,     // message
  WPARAM wParam, // first message parameter
  LPARAM lParam  // second message parameter
  )
{
	BOOL bResult;

	switch (uMsg) {
		case WM_COMMAND:
			UINT command;
			command = GET_WM_COMMAND_ID(wParam, lParam);
            if (IDOK == command)
            {
                EndDialog(hwndDlg, (IDOK == command));
            } else if (IDCANCEL == command)
            {
                EndDialog(hwndDlg, (IDOK == command));
            }
            bResult = FALSE;
			break;
		default:
			bResult = FALSE; // will be treated by DefWindowProc
	}
	return bResult;
}


inline DWORD ACM::Configure(HWND hParentWindow, LPDRVCONFIGINFO pConfig)
{
	my_debug.OutPut(DEBUG_LEVEL_FUNC_START, "ACM : Configure (Parent Window = 0x%08X)",hParentWindow);

	DialogBoxParam( my_hModule, MAKEINTRESOURCE(IDD_DIALOG1), hParentWindow, ::ConfigProc , (LPARAM)this);

	return DRVCNF_OK; // Can also return
					  // DRVCNF_CANCEL
                      // and DRVCNF_RESTART
}

//////////////////////////////////////////////////////////////////////
// About Dialog
//////////////////////////////////////////////////////////////////////

static BOOL CALLBACK AboutProc(
  HWND hwndDlg,  // handle to dialog box
  UINT uMsg,     // message
  WPARAM wParam, // first message parameter
  LPARAM lParam  // second message parameter
  )
{
	BOOL bResult;

	switch (uMsg) {
		case WM_COMMAND:
			UINT command;
			command = GET_WM_COMMAND_ID(wParam, lParam);
            if (IDOK == command)
            {
                EndDialog(hwndDlg, TRUE);
            }
            bResult = FALSE;
			break;
		default:
			bResult = FALSE; // will be treated by DefWindowProc
	}
	return bResult;
}

inline DWORD ACM::About(HWND hParentWindow)
{
	my_debug.OutPut(DEBUG_LEVEL_FUNC_START, "ACM : About (Parent Window = 0x%08X)",hParentWindow);

	DialogBoxParam( my_hModule, MAKEINTRESOURCE(IDD_ABOUT), hParentWindow, ::AboutProc , (LPARAM)this);

	return DRVCNF_OK; // Can also return
					  // DRVCNF_CANCEL
                      // and DRVCNF_RESTART
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ACM::ACM( HMODULE hModule )
 :my_hIcon(NULL),
  my_debug(ADbg(DEBUG_LEVEL_CREATION)),
  my_hModule(hModule)
{
	/// \todo get the debug level from the registry
	unsigned char DebugFileName[512];

	char tmp[128];
	wsprintf(tmp,"LAMEacm 0x%08X",this);
	my_debug.setPrefix(tmp); // TODO : get it from the registry
	my_debug.setIncludeTime(true);  // TODO : get it from the registry

	// Check in the registry if we have to Output Debug information
	DebugFileName[0] = '\0';

	HKEY OssKey;
	if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\MUKOLI", 0, KEY_READ , &OssKey ) == ERROR_SUCCESS) {
		DWORD DataType;
		DWORD DebugFileNameSize = 512;
		if (RegQueryValueEx( OssKey, "DebugFile", NULL, &DataType, DebugFileName, &DebugFileNameSize ) == ERROR_SUCCESS) {
			if (DataType == REG_SZ) {
				my_debug.setUseFile(true);
				my_debug.setDebugFile((char *)DebugFileName);
				my_debug.OutPut("Debug file is %s",(char *)DebugFileName);
			}
		}
	}
	my_debug.OutPut(DEBUG_LEVEL_FUNC_START, "New ACM Creation (0x%08X)",this);
}

ACM::~ACM()
{
	if (my_hIcon != NULL)
		CloseHandle(my_hIcon);

	my_debug.OutPut(DEBUG_LEVEL_FUNC_START, "ACM Deletion (0x%08X)",this);
}

//////////////////////////////////////////////////////////////////////
// Main message handler
//////////////////////////////////////////////////////////////////////

LONG ACM::DriverProcedure(const HDRVR hdrvr, const UINT msg, LONG lParam1, LONG lParam2)
{
    DWORD dwRes = 0L;

//my_debug.OutPut(DEBUG_LEVEL_MSG, "message 0x%08X for ThisACM 0x%08X", msg, this);

	switch (msg) {
    case DRV_INSTALL:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "DRV_INSTALL");
    // Sent when the driver is installed.
        dwRes = DRVCNF_OK;  // Can also return 
        break;              // DRVCNF_CANCEL
                            // and DRV_RESTART

    case DRV_REMOVE:
    // Sent when the driver is removed.
	my_debug.OutPut(DEBUG_LEVEL_MSG, "DRV_REMOVE");
        dwRes = 1L;  // return value ignored
        break;

    case DRV_QUERYCONFIGURE:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "DRV_QUERYCONFIGURE");
    // Sent to determine if the driver can be
    // configured.
        dwRes = 1L;  // Zero indicates configuration
        break;       // NOT supported

    case DRV_CONFIGURE:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "DRV_CONFIGURE");
    // Sent to display the configuration
    // dialog box for the driver.
		
		dwRes = Configure( (HWND) lParam1, (LPDRVCONFIGINFO) lParam2 );
        
		break;

	/**************************************
	// ACM additional messages
	***************************************/

	case ACMDM_DRIVER_ABOUT:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_DRIVER_ABOUT");

		dwRes = About( (HWND) lParam1 );

        break;

	case ACMDM_DRIVER_DETAILS:
	// Fill-in general informations about the driver/codec
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_DRIVER_DETAILS");

		dwRes = OnDriverDetails(hdrvr, (LPACMDRIVERDETAILS) lParam1);
        
		break;

	case ACMDM_FORMATTAG_DETAILS:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_FORMATTAG_DETAILS");

		dwRes = OnFormatTagDetails((LPACMFORMATTAGDETAILS) lParam1, lParam2);

        break;

	case ACMDM_FORMAT_DETAILS:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_FORMAT_DETAILS");

		dwRes = OnFormatDetails((LPACMFORMATDETAILS) lParam1, lParam2);
		
        break;           

    case ACMDM_FORMAT_SUGGEST:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_FORMAT_SUGGEST");
    // Sent to determine if the driver can be
    // configured.
		dwRes = OnFormatSuggest((LPACMDRVFORMATSUGGEST) lParam1);
        break; 

	/**************************************
	// ACM stream messages
	***************************************/

    case ACMDM_STREAM_OPEN:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_STREAM_OPEN");
    // Sent to determine if the driver can be
    // configured.
		dwRes = OnStreamOpen((LPACMDRVSTREAMINSTANCE) lParam1);
        break; 

	case ACMDM_STREAM_SIZE:
	// returns a recommended size for a source 
	// or destination buffer on an ACM stream
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_STREAM_SIZE");
		dwRes = OnStreamSize((LPACMDRVSTREAMINSTANCE)lParam1, (LPACMDRVSTREAMSIZE)lParam2);
        break; 

	case ACMDM_STREAM_PREPARE:
	// prepares an ACMSTREAMHEADER structure for
	// an ACM stream conversion
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_STREAM_PREPARE");
		dwRes = OnStreamPrepareHeader((LPACMDRVSTREAMINSTANCE)lParam1, (LPACMSTREAMHEADER) lParam2);
        break; 

	case ACMDM_STREAM_UNPREPARE:
	// cleans up the preparation performed by
	// the ACMDM_STREAM_PREPARE message for an ACM stream
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_STREAM_UNPREPARE");
		dwRes = OnStreamUnPrepareHeader((LPACMDRVSTREAMINSTANCE)lParam1, (LPACMSTREAMHEADER) lParam2);
        break; 

	case ACMDM_STREAM_CONVERT:
	// perform a conversion on the specified conversion stream
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_STREAM_CONVERT");
		dwRes = OnStreamConvert((LPACMDRVSTREAMINSTANCE)lParam1, (LPACMDRVSTREAMHEADER) lParam2);
		
        break; 

	case ACMDM_STREAM_CLOSE:
	// closes an ACM conversion stream
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACMDM_STREAM_CLOSE");
		dwRes = OnStreamClose((LPACMDRVSTREAMINSTANCE)lParam1);
        break;

	/**************************************
	// Unknown message
	***************************************/

    default:
	my_debug.OutPut(DEBUG_LEVEL_MSG, "ACM::DriverProc unknown message (0x%08X), lParam1 = 0x%08X, lParam2 = 0x%08X", msg, lParam1, lParam2);
    // Process any other messages.
        return DefDriverProc ((DWORD)this, hdrvr, msg, lParam1, lParam2);
    }

    return dwRes;
}

//////////////////////////////////////////////////////////////////////
// Special message handlers
//////////////////////////////////////////////////////////////////////

inline DWORD ACM::OnFormatDetails(LPACMFORMATDETAILS a_FormatDetails, const LPARAM a_Query)
{
	DWORD Result;

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "ACM_FORMATDETAILS a_Query = 0x%08X",a_Query);
	switch (a_Query & ACM_FORMATDETAILSF_QUERYMASK) {

		// Fill-in the informations corresponding to the FormatDetails->dwFormatTagIndex
		case ACM_FORMATDETAILSF_INDEX :
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "enter ACM_FORMATDETAILSF_INDEX for index 0x%04X:%03d",a_FormatDetails->dwFormatTag,a_FormatDetails->dwFormatIndex);
			if (a_FormatDetails->dwFormatTag == PERSONAL_FORMAT) {
				if (a_FormatDetails->dwFormatIndex < FORMAT_MAX_NB_PERSONAL) {
					LPWAVEFORMATEX WaveExt;
					WaveExt = a_FormatDetails->pwfx;

					WaveExt->wFormatTag = PERSONAL_FORMAT;

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "format in  : channels %d, sample rate %d", WaveExt->nChannels, WaveExt->nSamplesPerSec);
					GetOutFormatForIndex(a_FormatDetails->dwFormatIndex, *WaveExt, a_FormatDetails->szFormat);
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "format out : channels %d, sample rate %d", WaveExt->nChannels, WaveExt->nSamplesPerSec);
					Result = MMSYSERR_NOERROR;
				}
				else
				{
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "ACM_FORMATDETAILSF_INDEX unknown index 0x%04X:%03d",a_FormatDetails->dwFormatTag,a_FormatDetails->dwFormatIndex);
				}
			}
			else if (a_FormatDetails->dwFormatTag == WAVE_FORMAT_PCM) {
				if (a_FormatDetails->dwFormatIndex < FORMAT_MAX_NB_PCM) {
					LPWAVEFORMATEX WaveExt;
					WaveExt = a_FormatDetails->pwfx;

					WaveExt->wFormatTag = WAVE_FORMAT_PCM;

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "format in  : channels %d, sample rate %d", WaveExt->nChannels, WaveExt->nSamplesPerSec);
					GetInFormatForIndex(a_FormatDetails->dwFormatIndex, *WaveExt, a_FormatDetails->szFormat);
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "format out : channels %d, sample rate %d", WaveExt->nChannels, WaveExt->nSamplesPerSec);
					Result = MMSYSERR_NOERROR;
				}
				else
				{
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "ACM_FORMATDETAILSF_INDEX unknown index 0x%04X:%03d",a_FormatDetails->dwFormatTag,a_FormatDetails->dwFormatIndex);
				}
			}
			else
			{
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Unknown a_FormatDetails->dwFormatTag = 0x%08X",a_FormatDetails->dwFormatTag);
				Result = ACMERR_NOTPOSSIBLE;
			}

		case ACM_FORMATDETAILSF_FORMAT :
			/// \todo better handling of the suggest like other codec (see with a Debug build of VirtualDub)
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "enter ACM_FORMATDETAILSF_FORMAT : 0x%04X:%03d",a_FormatDetails->dwFormatTag,a_FormatDetails->dwFormatIndex);
			LPWAVEFORMATEX WaveExt;
			WaveExt = a_FormatDetails->pwfx;
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "format in : channels %d, sample rate %d", WaveExt->nChannels, WaveExt->nSamplesPerSec);

			Result = MMSYSERR_NOERROR;
			break;
		
		default:
			Result = ACMERR_NOTPOSSIBLE;
			break;
	}

	a_FormatDetails->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;

	return Result;
}

inline DWORD ACM::OnFormatTagDetails(LPACMFORMATTAGDETAILS a_FormatTagDetails, const LPARAM a_Query)
{
	DWORD Result;
	DWORD the_format = WAVE_FORMAT_UNKNOWN;

	if (a_FormatTagDetails->cbStruct >= sizeof(*a_FormatTagDetails)) {

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "ACMDM_FORMATTAG_DETAILS, a_Query = 0x%08X",a_Query);
		switch(a_Query & ACM_FORMATTAGDETAILSF_QUERYMASK) {

			// Fill-in the informations corresponding to the a_FormatDetails->dwFormatTagIndex
			case ACM_FORMATTAGDETAILSF_INDEX:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "get ACM_FORMATTAGDETAILSF_INDEX for index %03d",a_FormatTagDetails->dwFormatTagIndex);

				if (a_FormatTagDetails->dwFormatTagIndex < FORMAT_TAG_MAX_NB) {
					switch (a_FormatTagDetails->dwFormatTagIndex)
					{
					case 0:
						the_format = PERSONAL_FORMAT;
						break;
					default :
						the_format = WAVE_FORMAT_PCM;
						break;
					}
				}
				else
				{
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "ACM_FORMATTAGDETAILSF_INDEX for unsupported index %03d",a_FormatTagDetails->dwFormatTagIndex);
					Result = ACMERR_NOTPOSSIBLE;
				}
				break;
			// Fill-in the informations corresponding to the a_FormatDetails->dwFormatTagIndex and hdrvr given
			case ACM_FORMATTAGDETAILSF_FORMATTAG:
				switch (a_FormatTagDetails->dwFormatTag)
				{
				case WAVE_FORMAT_PCM:
					the_format = WAVE_FORMAT_PCM;
					break;
				case PERSONAL_FORMAT:
					the_format = PERSONAL_FORMAT;
					break;
				default:
                    return (ACMERR_NOTPOSSIBLE);
				}
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "get ACM_FORMATTAGDETAILSF_FORMATTAG for index 0x%02X, cStandardFormats = %d",a_FormatTagDetails->dwFormatTagIndex,a_FormatTagDetails->cStandardFormats);
				break;
			case ACM_FORMATTAGDETAILSF_LARGESTSIZE:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "enter ACM_FORMATTAGDETAILSF_LARGESTSIZE");
				Result = 0L;
				break;
			default:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnFormatTagDetails Unknown Format tag query");
				Result = MMSYSERR_NOTSUPPORTED;
				break;
		}

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnFormatTagDetails the_format = 0x%08X",the_format);
		switch(the_format)
		{
			case WAVE_FORMAT_PCM:
				a_FormatTagDetails->dwFormatTag      = WAVE_FORMAT_PCM;
				a_FormatTagDetails->dwFormatTagIndex = 0;
				a_FormatTagDetails->cbFormatSize     = sizeof(PCMWAVEFORMAT);
				a_FormatTagDetails->fdwSupport       = ACMDRIVERDETAILS_SUPPORTF_CODEC;
				/// TEST try with 0 to specify a coder only
	            a_FormatTagDetails->cStandardFormats = FORMAT_MAX_NB_PCM;
				//
				//  the ACM is responsible for the PCM format tag name
				//
// should be filled by Windows				a_FormatTagDetails->szFormatTag[0] = '\0';
				Result = MMSYSERR_NOERROR;
				break;
			case PERSONAL_FORMAT:
				a_FormatTagDetails->dwFormatTag      = PERSONAL_FORMAT;
				a_FormatTagDetails->dwFormatTagIndex = 1;
				a_FormatTagDetails->cbFormatSize     = SIZE_FORMAT_STRUCT;
				a_FormatTagDetails->fdwSupport       = ACMDRIVERDETAILS_SUPPORTF_CODEC ;
				a_FormatTagDetails->cStandardFormats = FORMAT_MAX_NB_PERSONAL;
				lstrcpyW( a_FormatTagDetails->szFormatTag, L"Lame MP3 encoder (ACM)" );
//				lstrcpyW( a_FormatTagDetails->szFormatTag, L"Lame & MAD MP3 Codec" );
				Result = MMSYSERR_NOERROR;
				break;
			default:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnFormatTagDetails Unknown format 0x%08X",the_format);
				return (ACMERR_NOTPOSSIBLE);
		}
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnFormatTagDetails %d possibilities for format 0x%08X",a_FormatTagDetails->cStandardFormats,the_format);
	}
	else
	{
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "a_FormatTagDetails->cbStruct < sizeof(*a_FormatDetails)");
		Result = ACMERR_NOTPOSSIBLE;
	}

	return Result;
}

inline DWORD ACM::OnDriverDetails(const HDRVR hdrvr, LPACMDRIVERDETAILS a_DriverDetail)
{
	a_DriverDetail->fccType     = ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC;
	a_DriverDetail->fccComp     = ACMDRIVERDETAILS_FCCCOMP_UNDEFINED;

	/// \note this is an explicit hack of the FhG values
	/// \note later it could be a new value when the decoding is done
	a_DriverDetail->wMid        = MM_FRAUNHOFER_IIS;
	a_DriverDetail->wPid        = MM_FHGIIS_MPEGLAYER3;

	a_DriverDetail->vdwACM      = VERSION_MSACM;
	a_DriverDetail->vdwDriver   = VERSION_ACM_DRIVER;
	a_DriverDetail->fdwSupport  = ACMDRIVERDETAILS_SUPPORTF_CODEC;
	a_DriverDetail->cFormatTags = FORMAT_TAG_MAX_NB;
	a_DriverDetail->cFilterTags = FILTER_TAG_MAX_NB;

	/// \todo get these from a resource
	if (my_hIcon == NULL)
		my_hIcon = LoadIcon(GetDriverModuleHandle(hdrvr), MAKEINTRESOURCE(IDI_ICON1));
	a_DriverDetail->hicon       = my_hIcon;
	lstrcpyW( a_DriverDetail->szShortName, L"LAME MP3" ); // TODO: 
	lstrcpyW( a_DriverDetail->szLongName , L"LAME MP3 Codec (coder only)" );
	lstrcpyW( a_DriverDetail->szCopyright, L"Steve Lhomme" );
	lstrcpyW( a_DriverDetail->szLicensing, L"LGPL" );
	lstrcpyW( a_DriverDetail->szFeatures , L"only CBR implementation" );

    return MMSYSERR_NOERROR;  // Can also return
	                          // DRVCNF_CANCEL
}

inline DWORD ACM::OnFormatSuggest(LPACMDRVFORMATSUGGEST a_FormatSuggest)
{
	DWORD Result = MMSYSERR_NOTSUPPORTED;
    DWORD fdwSuggest = (ACM_FORMATSUGGESTF_TYPEMASK & a_FormatSuggest->fdwSuggest);

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggest %s%s%s%s (0x%08X)",
				 (fdwSuggest & ACM_FORMATSUGGESTF_NCHANNELS) ? "channels, ":"",
				 (fdwSuggest & ACM_FORMATSUGGESTF_NSAMPLESPERSEC) ? "samples/sec, ":"",
				 (fdwSuggest & ACM_FORMATSUGGESTF_WBITSPERSAMPLE) ? "bits/sample, ":"",
				 (fdwSuggest & ACM_FORMATSUGGESTF_WFORMATTAG) ? "format, ":"",
				 fdwSuggest);

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggest for source format = 0x%04X, channels = %d, Samples/s = %d, AvgB/s = %d, BlockAlign = %d, b/sample = %d",
				 a_FormatSuggest->pwfxSrc->wFormatTag,
				 a_FormatSuggest->pwfxSrc->nChannels,
				 a_FormatSuggest->pwfxSrc->nSamplesPerSec,
				 a_FormatSuggest->pwfxSrc->nAvgBytesPerSec,
				 a_FormatSuggest->pwfxSrc->nBlockAlign,
				 a_FormatSuggest->pwfxSrc->wBitsPerSample);

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggested destination format = 0x%04X, channels = %d, Samples/s = %d, AvgB/s = %d, BlockAlign = %d, b/sample = %d",
			 a_FormatSuggest->pwfxDst->wFormatTag,
			 a_FormatSuggest->pwfxDst->nChannels,
			 a_FormatSuggest->pwfxDst->nSamplesPerSec,
			 a_FormatSuggest->pwfxDst->nAvgBytesPerSec,
			 a_FormatSuggest->pwfxDst->nBlockAlign,
			 a_FormatSuggest->pwfxDst->wBitsPerSample);

	switch (a_FormatSuggest->pwfxSrc->wFormatTag)
	{
        case WAVE_FORMAT_PCM:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggest for PCM source");
			break;
		case PERSONAL_FORMAT:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggest for PERSONAL source");
            //
            //  if the destination format tag is restricted, verify that
            //  it is within our capabilities...
            //
            //  this driver is able to decode to PCM
            //
            if (ACM_FORMATSUGGESTF_WFORMATTAG & fdwSuggest)
            {
                if (WAVE_FORMAT_PCM != a_FormatSuggest->pwfxDst->wFormatTag)
                    return (ACMERR_NOTPOSSIBLE);
            }
            else
            {
                a_FormatSuggest->pwfxDst->wFormatTag = WAVE_FORMAT_PCM;
            }


            //
            //  if the destination channel count is restricted, verify that
            //  it is within our capabilities...
            //
            //  this driver is not able to change the number of channels
            //
            if (ACM_FORMATSUGGESTF_NCHANNELS & fdwSuggest)
            {
                if (a_FormatSuggest->pwfxSrc->nChannels != a_FormatSuggest->pwfxDst->nChannels)
                    return (ACMERR_NOTPOSSIBLE);
            }
            else
            {
                a_FormatSuggest->pwfxDst->nChannels = a_FormatSuggest->pwfxSrc->nChannels;
            }


            //
            //  if the destination samples per second is restricted, verify
            //  that it is within our capabilities...
            //
            //  this driver is not able to change the sample rate
            //
            if (ACM_FORMATSUGGESTF_NSAMPLESPERSEC & fdwSuggest)
            {
                if (a_FormatSuggest->pwfxSrc->nSamplesPerSec != a_FormatSuggest->pwfxDst->nSamplesPerSec)
                    return (ACMERR_NOTPOSSIBLE);
            }
            else
            {
                a_FormatSuggest->pwfxDst->nSamplesPerSec = a_FormatSuggest->pwfxSrc->nSamplesPerSec;
            }


            //
            //  if the destination bits per sample is restricted, verify
            //  that it is within our capabilities...
            //
            //  We prefer decoding to 16-bit PCM.
            //
            if (ACM_FORMATSUGGESTF_WBITSPERSAMPLE & fdwSuggest)
            {
                if ( (16 != a_FormatSuggest->pwfxDst->wBitsPerSample) && (8 != a_FormatSuggest->pwfxDst->wBitsPerSample) )
                    return (ACMERR_NOTPOSSIBLE);
            }
            else
            {
                a_FormatSuggest->pwfxDst->wBitsPerSample = 16;
            }

//			a_FormatSuggest->pwfxDst->nBlockAlign = FORMAT_BLOCK_ALIGN;
			a_FormatSuggest->pwfxDst->nBlockAlign = a_FormatSuggest->pwfxDst->nChannels * a_FormatSuggest->pwfxDst->wBitsPerSample / 8;
			
			// this value must be a correct one !
			a_FormatSuggest->pwfxDst->nAvgBytesPerSec = a_FormatSuggest->pwfxDst->nSamplesPerSec * a_FormatSuggest->pwfxDst->nChannels * a_FormatSuggest->pwfxDst->wBitsPerSample / 8;

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggest succeed");
			Result = MMSYSERR_NOERROR;


			break;
	}

	my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Suggested destination format = 0x%04X, channels = %d, Samples/s = %d, AvgB/s = %d, BlockAlign = %d, b/sample = %d",
				 a_FormatSuggest->pwfxDst->wFormatTag,
				 a_FormatSuggest->pwfxDst->nChannels,
				 a_FormatSuggest->pwfxDst->nSamplesPerSec,
				 a_FormatSuggest->pwfxDst->nAvgBytesPerSec,
				 a_FormatSuggest->pwfxDst->nBlockAlign,
				 a_FormatSuggest->pwfxDst->wBitsPerSample);

	return Result;
}

inline DWORD ACM::OnStreamOpen(LPACMDRVSTREAMINSTANCE a_StreamInstance)
{
	DWORD Result = ACMERR_NOTPOSSIBLE;

    //
    //  the most important condition to check before doing anything else
    //  is that this ACM driver can actually perform the conversion we are
    //  being opened for. this check should fail as quickly as possible
    //  if the conversion is not possible by this driver.
    //
    //  it is VERY important to fail quickly so the ACM can attempt to
    //  find a driver that is suitable for the conversion. also note that
    //  the ACM may call this driver several times with slightly different
    //  format specifications before giving up.
    //
    //  this driver first verifies that the source and destination formats
    //  are acceptable...
    //
    switch (a_StreamInstance->pwfxSrc->wFormatTag)
	{
        case WAVE_FORMAT_PCM:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Open stream for PCM source (%05d samples %d channels %d bits/sample)",a_StreamInstance->pwfxSrc->nSamplesPerSec,a_StreamInstance->pwfxSrc->nChannels,a_StreamInstance->pwfxSrc->wBitsPerSample);
			if (a_StreamInstance->pwfxDst->wFormatTag == PERSONAL_FORMAT)
			{
				unsigned int OutputFrequency;

				/// \todo Smart mode
/*
				if (bSmartMode)
					OutputFrequency = ACMStream::GetOutputSampleRate(a_StreamInstance->pwfxSrc->nSamplesPerSec,a_StreamInstance->pwfxDst->nAvgBytesPerSec,a_StreamInstance->pwfxDst->nChannels);
				else
*/
					OutputFrequency = a_StreamInstance->pwfxSrc->nSamplesPerSec;

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Open stream for PERSONAL output (%05d samples %d channels %d bits/sample %d bytes/sec)",a_StreamInstance->pwfxDst->nSamplesPerSec,a_StreamInstance->pwfxDst->nChannels,a_StreamInstance->pwfxDst->wBitsPerSample,a_StreamInstance->pwfxDst->nAvgBytesPerSec);

				/// \todo add the possibility to have channel resampling (mono to stereo / stereo to mono)
				/// \todo support resampling ?
				/// \todo only do the test on OutputFrequency in "Smart Output" mode
				if (a_StreamInstance->pwfxDst->nSamplesPerSec != OutputFrequency ||
//					a_StreamInstance->pwfxSrc->nSamplesPerSec != a_StreamInstance->pwfxDst->nSamplesPerSec ||
					a_StreamInstance->pwfxSrc->nChannels != a_StreamInstance->pwfxDst->nChannels ||
					a_StreamInstance->pwfxSrc->wBitsPerSample != 16)
				{
					Result = ACMERR_NOTPOSSIBLE;
				} else {
					if ((a_StreamInstance->fdwOpen & ACM_STREAMOPENF_QUERY) == 0)
					{
						ACMStream * the_stream = ACMStream::Create();
						a_StreamInstance->dwInstance = (DWORD) the_stream;

						if (the_stream != NULL)
						{
							if (the_stream->init(a_StreamInstance->pwfxDst->nSamplesPerSec,
												 OutputFrequency,
												 a_StreamInstance->pwfxDst->nChannels,
												 a_StreamInstance->pwfxDst->nAvgBytesPerSec ))
//							if (the_stream->init())
								Result = MMSYSERR_NOERROR;
							else
								ACMStream::Erase( the_stream );
						}
					}
					else
					{
						Result = MMSYSERR_NOERROR;
					}
				}
			}
			break;
		case PERSONAL_FORMAT:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Open stream for PERSONAL source");
			break;
	}

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Open stream Result = %d",Result);
	return Result;
}

inline DWORD ACM::OnStreamSize(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMDRVSTREAMSIZE the_StreamSize)
{
	DWORD Result = ACMERR_NOTPOSSIBLE;

    switch (ACM_STREAMSIZEF_QUERYMASK & the_StreamSize->fdwSize)
    {
	case ACM_STREAMSIZEF_DESTINATION:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Get source buffer size for destination size = %d",the_StreamSize->cbDstLength);
		break;
	case ACM_STREAMSIZEF_SOURCE:
my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "Get destination buffer size for source size = %d",the_StreamSize->cbSrcLength);
        if (PERSONAL_FORMAT == a_StreamInstance->pwfxDst->wFormatTag)
        {
			ACMStream * the_stream = (ACMStream *) a_StreamInstance->dwInstance;
			if (the_stream != NULL)
			{
				the_StreamSize->cbDstLength = the_stream->GetOutputSizeForInput(the_StreamSize->cbSrcLength);
				Result = MMSYSERR_NOERROR;
			}
		}
		break;
	default:
		Result = MMSYSERR_INVALFLAG;
		break;
	}

	return Result;
}

inline DWORD ACM::OnStreamClose(LPACMDRVSTREAMINSTANCE a_StreamInstance)
{
	DWORD Result = ACMERR_NOTPOSSIBLE;

my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnStreamClose the stream 0x%X",a_StreamInstance->dwInstance);
	ACMStream::Erase( (ACMStream *) a_StreamInstance->dwInstance );

	// nothing to do yet
	Result = MMSYSERR_NOERROR;

	return Result;
}

inline DWORD ACM::OnStreamPrepareHeader(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMSTREAMHEADER a_StreamHeader)
{
	my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "  prepare : Src : %d (0x%08X) / %d - Dst : %d (0x%08X) / %d"
		                                      , a_StreamHeader->cbSrcLength
											  , a_StreamHeader->pbSrc
		                                      , a_StreamHeader->cbSrcLengthUsed
		                                      , a_StreamHeader->cbDstLength
											  , a_StreamHeader->pbDst
		                                      , a_StreamHeader->cbDstLengthUsed
											  );

	ACMStream * the_stream = (ACMStream *)a_StreamInstance->dwInstance;
	if (the_stream->open())
		return MMSYSERR_NOERROR;
	else
		return ACMERR_NOTPOSSIBLE;
}

inline DWORD ACM::OnStreamUnPrepareHeader(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMSTREAMHEADER a_StreamHeader)
{
	my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "unprepare : Src : %d / %d - Dst : %d / %d"
		                                      , a_StreamHeader->cbSrcLength
		                                      , a_StreamHeader->cbSrcLengthUsed
		                                      , a_StreamHeader->cbDstLength
		                                      , a_StreamHeader->cbDstLengthUsed
											  );
	ACMStream * the_stream = (ACMStream *)a_StreamInstance->dwInstance;
	DWORD OutputSize = a_StreamHeader->cbDstLength;
	if (the_stream->close(a_StreamHeader->pbDst, &OutputSize) && (OutputSize <= a_StreamHeader->cbDstLength))
	{
		a_StreamHeader->cbDstLengthUsed = OutputSize;
		return MMSYSERR_NOERROR;
	}
	else
		return ACMERR_NOTPOSSIBLE;
}

inline DWORD ACM::OnStreamConvert(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMDRVSTREAMHEADER a_StreamHeader)
{
	DWORD Result = ACMERR_NOTPOSSIBLE;

	if (a_StreamInstance->pwfxSrc->wFormatTag == WAVE_FORMAT_PCM)
	{
		my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnStreamConvert SRC = PCM (encode)");

		ACMStream * the_stream = (ACMStream *) a_StreamInstance->dwInstance;
		if (the_stream != NULL)
		{
			if (the_stream->ConvertBuffer( a_StreamHeader ))
				Result = MMSYSERR_NOERROR;
		}
	}
	else
		my_debug.OutPut(DEBUG_LEVEL_FUNC_CODE, "OnStreamConvert SRC = Personal (decode unsupported yet)");

	return Result;
}



void ACM::GetOutFormatForIndex(const DWORD the_Index, WAVEFORMATEX & the_Format, unsigned short the_String[ACMFORMATDETAILS_FORMAT_CHARS]) const
{
	/// \todo block size is 576 on MPEG2 ?
	the_Format.nBlockAlign = FORMAT_BLOCK_ALIGN;
	the_Format.wBitsPerSample = 16;

	the_Format.cbSize = 2; // never know

	/// \todo handle more channel modes (mono, stereo, joint-stereo, dual-channel)
	the_Format.nChannels = SIZE_CHANNEL_MODE - int(the_Index % SIZE_CHANNEL_MODE);

	DWORD a_Channel_Independent = the_Index / SIZE_CHANNEL_MODE;

	int mpeg_ver;
	int mpeg_bitrate;
	int mpeg_freq;

	// first MPEG1 frequencies
	if (a_Channel_Independent < SIZE_FREQ_MPEG1 * SIZE_BITRATE_MPEG1)
	{
		mpeg_ver = 1;
		mpeg_freq = a_Channel_Independent / (SIZE_BITRATE_MPEG1);
		mpeg_bitrate = a_Channel_Independent % (SIZE_BITRATE_MPEG1);
	}
	// then MPEG2 frequencies
	else
	{
		mpeg_ver = 2;
		a_Channel_Independent -= SIZE_FREQ_MPEG1 * SIZE_BITRATE_MPEG1; // skip MPEG1
		mpeg_freq = a_Channel_Independent / (SIZE_BITRATE_MPEG2);
		mpeg_bitrate = a_Channel_Independent % (SIZE_BITRATE_MPEG2);
	}

	if (mpeg_ver == 1)
	{
		the_Format.nSamplesPerSec = mpeg1_freq[mpeg_freq];
		the_Format.nAvgBytesPerSec = mpeg1_bitrate[mpeg_bitrate] * 1000 / 8;
	}
	else
	{
		the_Format.nSamplesPerSec = mpeg2_freq[mpeg_freq];
		the_Format.nAvgBytesPerSec = mpeg2_bitrate[mpeg_bitrate] * 1000 / 8;
	}

	/// \todo : generate the string with the appropriate stereo mode
	wsprintfW( the_String, L"%d kHz, %d kbps CBR, %s", the_Format.nSamplesPerSec, the_Format.nAvgBytesPerSec * 8 / 1000, (the_Format.nChannels == 1)?L"Mono":L"Stereo");
}

void ACM::GetInFormatForIndex(const DWORD the_Index, WAVEFORMATEX & the_Format, unsigned short the_String[ACMFORMATDETAILS_FORMAT_CHARS]) const
{
	the_Format.nChannels = SIZE_CHANNEL_MODE - int(the_Index % SIZE_CHANNEL_MODE);
	the_Format.wBitsPerSample = 16;
	the_Format.nBlockAlign = the_Format.nChannels;


	DWORD a_Channel_Independent = the_Index / SIZE_CHANNEL_MODE;

	// first MPEG1 frequencies
	if (a_Channel_Independent < SIZE_FREQ_MPEG1)
	{
		the_Format.nSamplesPerSec = mpeg1_freq[a_Channel_Independent];
	}
	else
	{
		a_Channel_Independent -= SIZE_FREQ_MPEG1;
		the_Format.nSamplesPerSec = mpeg2_freq[a_Channel_Independent];
	}

	the_Format.nAvgBytesPerSec = the_Format.nSamplesPerSec * the_Format.wBitsPerSample / 8;
}
