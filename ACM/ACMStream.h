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

#if !defined(_ACMSTREAM_H__INCLUDED_)
#define _ACMSTREAM_H__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>

#include "ADbg/ADbg.h"

#include <config.h>
#include "util.h"

#ifdef ENABLE_DECODING
#include "AMadDLL.h"
#endif // ENABLE_DECODING

class ACMStream
{
public:
	ACMStream( );
	virtual ~ACMStream( );

	static ACMStream * Create();
	static const bool Erase(const ACMStream * a_ACMStream);

	bool init(const int nSamplesPerSec, const int nOutputSamplesPerSec, const int nChannels, const int nAvgBytesPerSec);
	bool open();
	bool close(LPBYTE pOutputBuffer, DWORD *pOutputSize);

	DWORD GetOutputSizeForInput(const DWORD the_SrcLength) const;
	bool  ConvertBuffer(LPACMDRVSTREAMHEADER a_StreamHeader);

	static unsigned int GetOutputSampleRate(int samples_per_sec, int bitrate, int channels);

protected:
	lame_global_flags * gfp;

	ADbg * my_debug;
	int    my_SamplesPerSec;
	int    my_Channels;
	int    my_AvgBytesPerSec;
	int    my_OutBytesPerSec;
	DWORD  my_SamplesPerBlock;

//	ALameDLL  m_LameDLL;
#ifdef ENABLE_DECODING
	/// \todo use the same library as the Lame command-line
	AMadDLL   m_MadDLL;
#endif // ENABLE_DECODING

	unsigned int m_WorkingBufferUseSize;
	char         m_WorkingBuffer[2304*2]; // should be at least twice my_SamplesPerBlock

	inline int GetBytesPerBlock(DWORD bytes_per_sec, DWORD samples_per_sec, int BlockAlign) const;

};

#endif // !defined(_ACMSTREAM_H__INCLUDED_)
