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

#include <assert.h>
#include <windows.h>

#include "adebug.h"

#include "ACMStream.h"

// static methods

ACMStream * ACMStream::Create()
{
	ACMStream * Result;

	Result = new ACMStream;

	return Result;
}

const bool ACMStream::Erase(const ACMStream * a_ACMStream)
{
	delete a_ACMStream;
	return true;
}

// class methods

ACMStream::ACMStream() :
 m_WorkingBufferUseSize(0),
 gfp(NULL)
{
	 // TODO : get the debug level from the registry
	my_debug = new ADbg(DEBUG_LEVEL_CREATION);
	if (my_debug != NULL) {
		unsigned char DebugFileName[512];

		my_debug->setPrefix("LAMEstream"); // TODO : get it from the registry
		my_debug->setIncludeTime(true);  // TODO : get it from the registry

		// Check in the registry if we have to Output Debug information
		DebugFileName[0] = '\0';

		HKEY OssKey;
		if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\MUKOLI", 0, KEY_READ , &OssKey ) == ERROR_SUCCESS) {
			DWORD DataType;
			DWORD DebugFileNameSize = 512;
			if (RegQueryValueEx( OssKey, "DebugFile", NULL, &DataType, DebugFileName, &DebugFileNameSize ) == ERROR_SUCCESS) {
				if (DataType == REG_SZ) {
					my_debug->setUseFile(true);
					my_debug->setDebugFile((char *)DebugFileName);
					my_debug->OutPut("Debug file is %s",(char *)DebugFileName);
				}
			}
		}
		my_debug->OutPut(DEBUG_LEVEL_FUNC_START, "ACMStream Creation (0X%08X)",this);
	}
	else {
		ADbg debug;
		debug.OutPut("ACMStream::ACMACMStream : Impossible to create my_debug");
	}

}

ACMStream::~ACMStream()
{
	lame_close( gfp );

	if (my_debug != NULL)
	{
		my_debug->OutPut(DEBUG_LEVEL_FUNC_START, "ACMStream Deletion (0X%08X)",this);
		delete my_debug;
	}
}

bool ACMStream::init(const int nSamplesPerSec, const int nOutputSamplesPerSec, const int nChannels, const int nAvgBytesPerSec)
{
	bool bResult = false;

#ifdef ENABLE_DECODING
	if (m_MadDLL.Load("in_mad.dll"))
	{
		char path[512];
		m_MadDLL.GetFullLocation(path, 512);
my_debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "MAD Winamp DLL found in %s",path);
	}
	else
	{
my_debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "MAD Winamp DLL not found !");
	}
#endif // ENABLE_DECODING

	my_SamplesPerSec  = nSamplesPerSec;
	my_OutBytesPerSec = nOutputSamplesPerSec;
	my_Channels       = nChannels;
	my_AvgBytesPerSec = nAvgBytesPerSec;

	bResult = true;

	return bResult;

}

bool ACMStream::open()
{
	bool bResult = false;
/*
	BE_VERSION ver;
	m_LameDLL.Version(&ver);

	char path[512];
	m_LameDLL.GetFullLocation(path, 512);
my_debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "Lame DLL %d.%d (v%d.%d) found in %s",ver.byMajorVersion,
				                                                          ver.byMinorVersion,
																		  ver.byDLLMajorVersion,
																		  ver.byDLLMinorVersion,
																		  path);
*/
	// Init the MP3 Stream
	// Init the global flags structure
	gfp = lame_init();

	// Set input sample frequency
	lame_set_in_samplerate( gfp, my_SamplesPerSec );

	lame_set_num_channels( gfp, my_Channels );
	if (my_Channels == 1)
		lame_set_mode( gfp, MONO );
	else
		lame_set_mode( gfp, JOINT_STEREO ); /// \todo Get the mode from the default configuration

	lame_set_VBR( gfp, vbr_off ); /// \note VBR not supported for the moment

	// Set bitrate
	lame_set_brate( gfp, my_AvgBytesPerSec * 8 / 1000 );
	
	/// \todo Get the mode from the default configuration
	// Set copyright flag?
	lame_set_copyright( gfp, 0 );
	// Do we have to tag  it as non original 
	lame_set_original( gfp, 0 );
	// Add CRC?
	lame_set_error_protection( gfp, 1 );
	// Set private bit?
	lame_set_extension( gfp, 0 );

	if (0 == lame_init_params( gfp ))
	{
		//LAME encoding call will accept any number of samples.  
		if ( 0 == lame_get_version( gfp ) )
		{
			// For MPEG-II, only 576 samples per frame per channel
			my_SamplesPerBlock = 576 * lame_get_num_channels( gfp );
		}
		else
		{
			// For MPEG-I, 1152 samples per frame per channel
			my_SamplesPerBlock = 1152 * lame_get_num_channels( gfp );
		}
	}

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "version                =%d",lame_get_version( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Layer                  =3");
	switch ( lame_get_mode( gfp ) )
	{
		case STEREO:       my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "mode                   =Stereo" ); break;
		case JOINT_STEREO: my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "mode                   =Joint-Stereo" ); break;
		case DUAL_CHANNEL: my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "mode                   =Forced Stereo" ); break;
		case MONO:         my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "mode                   =Mono" ); break;
		case NOT_SET:      /* FALLTROUGH */
		default:           my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "mode                   =Error (unknown)" ); break;
	}

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "sampling frequency     =%.1f kHz", lame_get_in_samplerate( gfp ) /1000.0 );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "bitrate                =%d kbps", lame_get_brate( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Vbr Min bitrate        =%d kbps", lame_get_VBR_min_bitrate_kbps( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Vbr Max bitrate        =%d kbps", lame_get_VBR_max_bitrate_kbps( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Quality Setting        =%d", lame_get_quality( gfp ) );

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Low pass frequency     =%d", lame_get_lowpassfreq( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Low pass width         =%d", lame_get_lowpasswidth( gfp ) );

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "High pass frequency    =%d", lame_get_highpassfreq( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "High pass width        =%d", lame_get_highpasswidth( gfp ) );

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "No Short Blocks        =%d", lame_get_no_short_blocks( gfp ) );

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "de-emphasis            =%d", lame_get_emphasis( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "private flag           =%d", lame_get_extension( gfp ) );

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "copyright flag         =%d", lame_get_copyright( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "original flag          =%d",	lame_get_original( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "CRC                    =%s", lame_get_error_protection( gfp ) ? "on" : "off" );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Fast mode              =%s", ( lame_get_quality( gfp ) )? "enabled" : "disabled" );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Force mid/side stereo  =%s", ( lame_get_force_ms( gfp ) )?"enabled":"disabled" );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Padding Type           =%d", lame_get_padding_type( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Disable Resorvoir      =%d", lame_get_disable_reservoir( gfp ) );
	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "VBR                    =%s, VBR_q =%d, VBR method =",
					( lame_get_VBR( gfp ) !=vbr_off ) ? "enabled": "disabled",
		            lame_get_VBR_q( gfp ) );

	switch ( lame_get_VBR( gfp ) )
	{
		case vbr_off:	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "vbr_off" );	break;
		case vbr_mt :	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "vbr_mt" );	break;
		case vbr_rh :	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "vbr_rh" );	break;
		case vbr_mtrh:	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "vbr_mtrh" );	break;
		case vbr_abr: 
			my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG,  "vbr_abr (average bitrate %d kbps)", lame_get_VBR_mean_bitrate_kbps( gfp ) );
		break;
		default:
			my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "error, unknown VBR setting");
		break;
	}

	my_debug->OutPut(DEBUG_LEVEL_FUNC_DEBUG, "Write VBR Header       =%s\n", ( lame_get_bWriteVbrTag( gfp ) ) ?"Yes":"No");

#ifdef FROM_DLL
	beConfig.format.LHV1.dwReSampleRate		= my_OutBytesPerSec;	  // force the user resampling
#endif // FROM_DLL

	bResult = true;

	return bResult;
}

bool ACMStream::close(LPBYTE pOutputBuffer, DWORD *pOutputSize)
{
#ifdef FROM_DLL
	m_LameDLL.DeinitStream( pOutputBuffer, pOutputSize);

	m_LameDLL.CloseStream( );

	m_LameDLL.Free();
#endif // FROM_DLL

	bool bResult = false;

	int nOutputSamples = 0;

    nOutputSamples = lame_encode_flush( gfp, pOutputBuffer, 0 );

	if ( nOutputSamples < 0 )
	{
		// BUFFER_TOO_SMALL
		*pOutputSize = 0;
	}
	else
	{
		*pOutputSize = nOutputSamples;

		bResult = true;
	}

	// lame will be close in VbrWriteTag function
	if ( !lame_get_bWriteVbrTag( gfp ) )
	{
		// clean up of allocated memory
		lame_close( gfp );
	}
    
	return bResult;
}

DWORD ACMStream::GetOutputSizeForInput(const DWORD the_SrcLength) const
{
	double OutputInputRatio = double(my_AvgBytesPerSec) / double(my_OutBytesPerSec * 2);

	OutputInputRatio *= 1.15; // allow 15% more

	DWORD Result;

	Result = DWORD(double(the_SrcLength) * OutputInputRatio);

my_debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "Result = %d",Result);

	return Result;
}

bool ACMStream::ConvertBuffer(LPACMDRVSTREAMHEADER a_StreamHeader)
{
	bool result;

	DWORD InSize = a_StreamHeader->cbSrcLength / 2, OutSize = a_StreamHeader->cbDstLength; // 2 for 8<->16 bits

#ifdef FROM_DLL
	result = m_LameDLL.Encode(InSize, (PSHORT)a_StreamHeader->pbSrc, my_Channels, &OutSize, a_StreamHeader->pbDst, my_Channels);
#endif // FROM_DLL

	// Encode it
	int dwSamples;
	int	nOutputSamples = 0;

	dwSamples = InSize / lame_get_num_channels( gfp );

	if ( 1 == lame_get_num_channels( gfp ) )
	{
		nOutputSamples = lame_encode_buffer(gfp,(PSHORT)a_StreamHeader->pbSrc,(PSHORT)a_StreamHeader->pbSrc,dwSamples,a_StreamHeader->pbDst,a_StreamHeader->cbDstLength);
	}
	else
	{
		nOutputSamples = lame_encode_buffer_interleaved(gfp,(PSHORT)a_StreamHeader->pbSrc,dwSamples,a_StreamHeader->pbDst,a_StreamHeader->cbDstLength);
	}

	a_StreamHeader->cbSrcLengthUsed = a_StreamHeader->cbSrcLength;
	a_StreamHeader->cbDstLengthUsed = nOutputSamples;

	my_debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "UsedSize = %d / EncodedSize = %d", InSize, OutSize);

	return result;
}

unsigned int ACMStream::GetOutputSampleRate(int samples_per_sec, int bitrate, int channels)
{
	unsigned int OutputFrequency;
	double compression_ratio = double(samples_per_sec * 16 * channels / (bitrate * 8));
	if (compression_ratio > 13.)
		OutputFrequency = map2MP3Frequency( (10. * bitrate * 8) / (16 * channels));
	else
		OutputFrequency = map2MP3Frequency( 0.97 * samples_per_sec );

	return OutputFrequency;
}
