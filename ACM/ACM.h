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

#if !defined(_ACM_H__INCLUDED_)
#define _ACM_H__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <windows.h>
#include <mmsystem.h>

#include "ADbg/ADbg.h"

class ACM  
{
public:
	ACM( HMODULE hModule );
	virtual ~ACM();

	LONG DriverProcedure(const HDRVR hdrvr, const UINT msg, LONG lParam1, LONG lParam2);

protected:
	inline DWORD Configure( HWND hParentWindow, LPDRVCONFIGINFO pConfig );
	inline DWORD About( HWND hParentWindow );

	inline DWORD OnDriverDetails(const HDRVR hdrvr, LPACMDRIVERDETAILS a_DriverDetail);
	inline DWORD OnFormatTagDetails(LPACMFORMATTAGDETAILS a_FormatTagDetails, const LPARAM a_Query);
	inline DWORD OnFormatDetails(LPACMFORMATDETAILS a_FormatDetails, const LPARAM a_Query);
	inline DWORD OnFormatSuggest(LPACMDRVFORMATSUGGEST a_FormatSuggest);
	inline DWORD OnStreamOpen(LPACMDRVSTREAMINSTANCE a_StreamInstance);
	inline DWORD OnStreamClose(LPACMDRVSTREAMINSTANCE a_StreamInstance);
	inline DWORD OnStreamSize(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMDRVSTREAMSIZE the_StreamSize);
	inline DWORD OnStreamPrepareHeader(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMSTREAMHEADER a_StreamHeader);
	inline DWORD OnStreamUnPrepareHeader(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMSTREAMHEADER a_StreamHeader);
	inline DWORD OnStreamConvert(LPACMDRVSTREAMINSTANCE a_StreamInstance, LPACMDRVSTREAMHEADER a_StreamHeader);

	void GetOutFormatForIndex(const DWORD the_Index, WAVEFORMATEX & the_Format, unsigned short the_String[ACMFORMATDETAILS_FORMAT_CHARS]) const;
	void GetInFormatForIndex(const DWORD the_Index, WAVEFORMATEX & the_Format, unsigned short the_String[ACMFORMATDETAILS_FORMAT_CHARS]) const;

	HMODULE my_hModule;
	HICON   my_hIcon;
	ADbg    my_debug;

};

#endif // !defined(_ACM_H__INCLUDED_)
