/*
 *	CEncoder wrapper for LAME
 *
 *	Copyright (c) 2000 Marie Orlova, Peter Gubanov, Elecard Ltd.
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

#include <streams.h>
#include "Encoder.h"

DWORD dwBitRateValue[2][14] = {
	{32,40,48,56,64,80,96,112,128,160,192,224,256,320},		// MPEG1 Layer 3
	{8,16,24,32,40,48,56,64,80,96,112,128,144,160}			// MPEG2 Layer 3
};

#define	SIZE_OF_SHORT 2
#define MPEG_TIME_DIVISOR	90000

/////////////////////////////////////////////////////////////
// MakePTS - converts DirectShow refrence time
//			 to MPEG time stamp format
/////////////////////////////////////////////////////////////
LONGLONG MakePTS(REFERENCE_TIME rt)
{
	return llMulDiv(CRefTime(rt).GetUnits(),MPEG_TIME_DIVISOR,UNITS,0);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CEncoder::CEncoder() :
	m_bInpuTypeSet(FALSE),
	m_bOutpuTypeSet(FALSE),
	m_rtLast(0),
	m_bLast(FALSE),
	m_nCounter(0),
	m_nPos(0),
	m_pPos(NULL),
	pgf(NULL)
{
}

CEncoder::~CEncoder()
{
	Close();
}

//////////////////////////////////////////////////////////////////////
// SetInputType - check if given input type is supported
//////////////////////////////////////////////////////////////////////
HRESULT CEncoder::SetInputType(LPWAVEFORMATEX lpwfex)
{
	CAutoLock l(this);

	if (lpwfex->wFormatTag			== WAVE_FORMAT_PCM) 
	{
 		if (lpwfex->nChannels			== 1 || 
			lpwfex->nChannels			== 2	 ) 
		{
 			if (lpwfex->nSamplesPerSec		== 48000 ||
				lpwfex->nSamplesPerSec		== 44100 ||
				lpwfex->nSamplesPerSec		== 32000 ||
				lpwfex->nSamplesPerSec		== 24000 ||
				lpwfex->nSamplesPerSec		== 22050 ||
				lpwfex->nSamplesPerSec		== 16000 ) 
			{
				if (lpwfex->wBitsPerSample		== 16)
				{
					memcpy(&m_wfex, lpwfex, sizeof(WAVEFORMATEX));
					m_bInpuTypeSet = true;

					return S_OK;
				}
			}
		}
	}
	m_bInpuTypeSet = false;
	return E_INVALIDARG;
}

//////////////////////////////////////////////////////////////////////
// SetOutputType - try to initialize encoder with given output type
//////////////////////////////////////////////////////////////////////
HRESULT CEncoder::SetOutputType(MPEG_ENCODER_CONFIG &mabsi)
{
	CAutoLock l(this);

	m_mabsi = mabsi;
	m_bOutpuTypeSet = true;
	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// SetDefaultOutputType - sets default MPEG audio properties according
// to input type
//////////////////////////////////////////////////////////////////////
HRESULT CEncoder::SetDefaultOutputType(LPWAVEFORMATEX lpwfex)
{
	CAutoLock l(this);

	if(lpwfex->nChannels == 1)
		m_mabsi.dwChMode = MPG_MD_MONO;

	if((lpwfex->nSamplesPerSec < m_mabsi.dwSampleRate) || (lpwfex->nSamplesPerSec % m_mabsi.dwSampleRate != 0))
		m_mabsi.dwSampleRate = lpwfex->nSamplesPerSec;

	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// Init - initialized or reiniyialized encoder SDK with given input 
// and output settings
//////////////////////////////////////////////////////////////////////
HRESULT CEncoder::Init()
{
	CAutoLock l(this);

	if(!pgf)
	{
		if (!m_bInpuTypeSet || !m_bOutpuTypeSet)
			return E_UNEXPECTED;

		// Init Lame library
		// note: newer, safer interface which doesn't 
		// allow or require direct access to 'gf' struct is being written
		// see the file 'API' included with LAME.
		pgf = lame_init();

		pgf->num_channels = m_wfex.nChannels;
		pgf->in_samplerate = m_wfex.nSamplesPerSec;
		pgf->out_samplerate = m_mabsi.dwSampleRate;
		pgf->brate = m_mabsi.dwBitrate;

		pgf->VBR = m_mabsi.vmVariable;
		pgf->VBR_min_bitrate_kbps = m_mabsi.dwVariableMin;
		pgf->VBR_max_bitrate_kbps = m_mabsi.dwVariableMax;

		pgf->copyright = m_mabsi.bCopyright;
		pgf->original = m_mabsi.bOriginal;
		pgf->error_protection = m_mabsi.bCRCProtect;

		pgf->no_short_blocks = m_mabsi.dwNoShortBlock;
		pgf->bWriteVbrTag = m_mabsi.dwXingTag;
		pgf->strict_ISO = m_mabsi.dwStrictISO;
		pgf->VBR_hard_min = m_mabsi.dwEnforceVBRmin;
		pgf->force_ms = m_mabsi.dwForceMS;
		pgf->mode = m_mabsi.dwChMode;
		pgf->mode_fixed = m_mabsi.dwModeFixed;

		if (m_mabsi.dwVoiceMode != 0)
		{
			pgf->lowpassfreq = 12000;
			pgf->VBR_max_bitrate_kbps = 160;
			pgf->no_short_blocks = 1;
		}

		if (m_mabsi.dwKeepAllFreq != 0)
		{
			pgf->lowpassfreq = -1;
			pgf->highpassfreq = -1;
		}

		pgf->quality = m_mabsi.dwQuality;
		pgf->VBR_q = m_mabsi.dwVBRq;

		lame_init_params(pgf);
	}

	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// Close - closes encoder
//////////////////////////////////////////////////////////////////////
HRESULT CEncoder::Close()
{
	CAutoLock l(this);

	if(pgf) {
		lame_close(pgf);
		pgf = NULL;
	}
	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// Encode - encodes data placed on pSrc with size dwSrcSize and returns
// encoded data in pDst pointer. REFERENCE_TIME rt is a DirectShow refrence
// time which is being converted in MPEG PTS and placed in PES header.
//////////////////////////////////////////////////////////////////////
HRESULT CEncoder::Encode(LPVOID pSrc, DWORD dwSrcSize, LPVOID pDst, LPDWORD lpdwDstSize, REFERENCE_TIME rt)
{
	CAutoLock l(this);
	BYTE temp_buffer[OUTPUT_BUFF_SIZE];

	*lpdwDstSize = 0;// If data are insufficient to be converted, return 0 in lpdwDstSize

	LPBYTE pData = temp_buffer;
	LONG lData = OUTPUT_BUFF_SIZE;

	int nsamples = dwSrcSize/(m_wfex.wBitsPerSample*m_wfex.nChannels/8);

	if (pgf) {
		lData = lame_encode_buffer_interleaved(pgf,(short*)pSrc,nsamples,pData,lData);

		if(m_mabsi.dwPES && lData > 0)
		{
			// Write PES header
			Reset();
			CreatePESHdr((LPBYTE)pDst, MakePTS(m_rtLast), lData);
			pDst = (LPBYTE)pDst + 0x0e;			// add PES header size

			m_bLast = false;
			m_rtLast = rt;
		}
		else
			m_bLast = true;

		memcpy(pDst,pData,lData);
		*lpdwDstSize = lData;
	}
	else {
		*lpdwDstSize = 0;
	}


	return S_OK;
}

//
// Finsh - flush the buffered samples. REFERENCE_TIME rt is a DirectShow refrence
// time which is being converted in MPEG PTS and placed in PES header.
//
HRESULT CEncoder::Finish(LPVOID pDst, LPDWORD lpdwDstSize)
{
	CAutoLock l(this);
	BYTE temp_buffer[OUTPUT_BUFF_SIZE];

	*lpdwDstSize = 0;// If data are insufficient to be converted, return 0 in lpdwDstSize

	LPBYTE pData = temp_buffer;
	LONG lData = OUTPUT_BUFF_SIZE;

#pragma message (REMIND("Finish encoding right!"))
	if (pgf) {
		lData = lame_encode_flush(pgf,pData,lData);

		if(m_mabsi.dwPES && lData > 0)
		{
			// Write PES header
			Reset();
			CreatePESHdr((LPBYTE)pDst, MakePTS(m_rtLast), lData);
			pDst = (LPBYTE)pDst + 0x0e;			// add PES header size
		}
		m_bLast = true;

		memcpy(pDst,pData,lData);
	}
	else
		lData = 0;

	*lpdwDstSize = lData;

	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// PES headers routines
//////////////////////////////////////////////////////////////////////
// WriteBits - writes nVal in nBits bits on m_pPos address 
//////////////////////////////////////////////////////////////////////
void CEncoder::WriteBits(int nBits, int nVal)
{
	int nCounter = m_nCounter + nBits,
		nPos = (m_nPos << nBits)|(nVal & (~( (~0L) << nBits)));
	while(nCounter >= 8)
	{
		nCounter -= 8;
		*m_pPos++ = (BYTE)(nPos >> nCounter) & 0xff;
	}
	m_nCounter = nCounter;
	m_nPos = nPos;
}

//////////////////////////////////////////////////////////////////////
// CreatePESHdr - creates PES header on ppHdr address and writes PTS
// and PES_packet_length fields
//////////////////////////////////////////////////////////////////////
void CEncoder::CreatePESHdr(LPBYTE ppHdr, LONGLONG dwPTS, int dwPacketSize)
{
	m_pPos = ppHdr;
	WriteBits(24, 0x000001);	// PES header start code prefix 0x000001
	WriteBits(8 , 0x0000c0);	//	stream_id 192?

	WriteBits(16, dwPacketSize + 0x000008);	//	PES_packet_length -- A 16 bit field specifying the number of bytes in the PES packet following the last byte of the field.

	WriteBits(2 , 0x000002);	//	marker
	WriteBits(2 , 0x000000);	//	PES_scrambling_control --  The 2 bit PES_scrambling_control indicates the scrambling mode of the PES 

	WriteBits(1 , 0x000000);	//	PES_priority 
	WriteBits(1 , 0x000000);	//	data_alignment_indicator When set to a value of '0' it is not defined whether any such alignment occurs or not.
	WriteBits(1 , 0x000000);	//	copyright
	WriteBits(1 , 0x000000);	//	original_or_copy

	WriteBits(2 , 0x000002);	//	PTS_DTS_flags -- A 2 bit flag. If the PTS_DTS_flags field equals '10', a PTS field is present in the PES packet header

	WriteBits(1 , 0x000000);	//	ESCR_flag
	WriteBits(1 , 0x000000);	//	ES_rate_flag
	WriteBits(1 , 0x000000);	//	DSM_trick_mode_flag
	WriteBits(1 , 0x000000);	//	additional_copy_info_flag
	WriteBits(1 , 0x000000);	//	PES_CRC_flag
	WriteBits(1 , 0x000000);	//	PES_extension_flag

	WriteBits(8 , 0x000005);	//	PES_header_data_length = 5 bytes
	// PTS
	WriteBits(4 , 0x000002);	//	marker
	WriteBits(3 , (int)(((dwPTS) >> 0x1f) & 0x07));	//	PTS [32.30]
	WriteBits(1 , 0x000001);	//	marker
	WriteBits(15 , (int)(((dwPTS) >> 0xf) & 0x7fff));	//	PTS [29.15]
	WriteBits(1 , 0x000001);	//	marker
	WriteBits(15 , (int)((dwPTS) & 0x7fff));	//	PTS [14.0]
	WriteBits(1 , 0x000001);	//	marker
}

//////////////////////////////////////////////////////////////////////
// Reset - resets all PES header stuff
//////////////////////////////////////////////////////////////////////
void CEncoder::Reset()
{
	m_nCounter = 0;
	m_nPos  = 0;
	m_pPos = NULL;
}
