/*
 *	MPEG Audio Encoder for DirectShow
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
#include <olectl.h>
#include <initguid.h>
#include <olectlid.h>
#include "uids.h"
#include "iaudioprops.h"
#include "mpegac.h"
#include "resource.h"

#include "PropPage.h"
#include "PropPage_adv.h"
#include "Encoder.h"
#include "Reg.h"

#ifndef _INC_MMREG
#include <mmreg.h>
#endif

// default parameters
#define         DEFAULT_LAYER				3
#define         DEFAULT_STEREO_MODE			MPG_MD_JOINT_STEREO
#define			DEFAULT_FORCE_MS			0
#define			DEFAULT_MODE_FIXED			0
#define			DEFAULT_ENFORCE_MIN			0
#define			DEFAULT_VOICE				0
#define			DEFAULT_KEEP_ALL_FREQ		0
#define			DEFAULT_STRICT_ISO			0
#define			DEFAULT_DISABLE_SHORT_BLOCK	0
#define			DEFAULT_XING_TAG	0
#define         DEFAULT_SAMPLE_RATE			44100		
#define         DEFAULT_BITRATE				128		//Work with all mode
#define         DEFAULT_VARIABLE     	    0 	
#define         DEFAULT_CRC     		    0 	
#define         DEFAULT_COPYRIGHT     		0 	
#define         DEFAULT_ORIGINAL     		0 	
#define         DEFAULT_VARIABLEMIN			80		//Work with all mode
#define         DEFAULT_VARIABLEMAX			160		//Work with all mode
#define         DEFAULT_ENCODING_QUALITY	5
#define         DEFAULT_VBR_QUALITY		    4 
#define         DEFAULT_PES     		    0 	


/*  Registration setup stuff */
//  Setup data

AMOVIESETUP_MEDIATYPE sudMpgInputType[] =
{
    { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM }
};
AMOVIESETUP_MEDIATYPE sudMpgOutputType[] =
{
    { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG1AudioPayload },
    { &MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG2_AUDIO },
};

AMOVIESETUP_PIN sudMpgPins[3] =
{
    { L"PCM Input",
      FALSE,                               // bRendered
      FALSE,                               // bOutput
      FALSE,                               // bZero
      FALSE,                               // bMany
      &CLSID_NULL,                         // clsConnectsToFilter
      NULL,                                // ConnectsToPin
      NUMELMS(sudMpgInputType),            // Number of media types
      sudMpgInputType
    },
    { L"MPEG Output",
      FALSE,                               // bRendered
      TRUE,                                // bOutput
      FALSE,                               // bZero
      FALSE,                               // bMany
      &CLSID_NULL,                         // clsConnectsToFilter
      NULL,                                // ConnectsToPin
      NUMELMS(sudMpgOutputType),           // Number of media types
      sudMpgOutputType
    }
};

AMOVIESETUP_FILTER sudMpgAEnc =
{
    &CLSID_LAMEDShowFilter,
    L"LAME MPEG Layer III Audio Encoder",
    MERIT_SW_COMPRESSOR,                   // Don't use us for real!
    NUMELMS(sudMpgPins),                   // 3 pins
    sudMpgPins
};

/*****************************************************************************/
// COM Global table of objects in this dll
CFactoryTemplate g_Templates[] = 
{
  { L"LAME MPEG Layer III Audio Encoder", &CLSID_LAMEDShowFilter, CMpegAudEnc::CreateInstance, NULL, &sudMpgAEnc },
  { L"LAME MPEG Layer III Audio Encoder Property Page", &CLSID_LAMEDShow_PropertyPage, CMpegAudEncPropertyPage::CreateInstance},
  { L"LAME MPEG Layer III Audio Encoder Advanced Property Page", &CLSID_LAMEDShow_PropertyPageAdv, CMpegAudEncPropertyPageAdv::CreateInstance}
};
// Count of objects listed in g_cTemplates
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

CUnknown *CMpegAudEnc::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr) 
{
	CUnknown *punk = new CMpegAudEnc(lpunk, phr);
	if (punk == NULL) 
		*phr = E_OUTOFMEMORY;
	return punk;
}

CMpegAudEnc::CMpegAudEnc(LPUNKNOWN lpunk, HRESULT *phr)
 :	CTransformFilter(NAME("LAME MPEG Layer III Audio Encoder"), lpunk, 
	CLSID_LAMEDShowFilter),
    CPersistStream(lpunk, phr)
{
	MPEG_ENCODER_CONFIG mec;
	ReadPresetSettings(&mec);
	m_VEncoder.SetOutputType(mec);
}

CMpegAudEnc::~CMpegAudEnc(void)
{
}

LPAMOVIESETUP_FILTER CMpegAudEnc::GetSetupData()
{
    return &sudMpgAEnc;
}

////////////////////////////////////////////////////////////////////////////
//	Transform - function where actual data transformations is performed
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::Transform(IMediaSample* pSource, IMediaSample* pDest)
{	
	if(m_State == State_Stopped)// if filter stoped do noting
	{	
		pDest->SetActualDataLength(0);
		return S_OK;
	}

	// Copy the sample data
	DWORD	dwDstSize;
	BYTE	*pSourceBuffer, *pDestBuffer;
	long	lSourceSize		= pSource->GetActualDataLength();
	long	lDestSize		= pDest->GetSize();		

	// Copy the sample times
	REFERENCE_TIME TimeStart, TimeEnd;
	if(SUCCEEDED(pSource->GetTime(&TimeStart, &TimeEnd))) {
		pDest->SetTime(&TimeStart, &TimeEnd);
		m_rtLast = TimeEnd;
	}

	// Get source and destination data buffers
	pSource->GetPointer(&pSourceBuffer);
	pDest->GetPointer(&pDestBuffer);    	

	// Encode data
	if(FAILED(m_VEncoder.Encode(pSourceBuffer, lSourceSize,
								pDestBuffer, &dwDstSize, TimeStart)))
		return E_UNEXPECTED;

	ASSERT(DWORD(lDestSize) >= dwDstSize);
	// Copy the actual data length
	pDest->SetActualDataLength(dwDstSize);
	

    LONGLONG MediaStart, MediaEnd;
    if (pSource->GetMediaTime(&MediaStart,&MediaEnd) == NOERROR) 
        pDest->SetMediaTime(&MediaStart,&MediaEnd);
		// Copy the Sync point property

	HRESULT hr = pSource->IsSyncPoint();
	if (hr == S_OK) 
		pDest->SetSyncPoint(TRUE);
	else if (hr == S_FALSE) 
		pDest->SetSyncPoint(FALSE);
	else // an unexpected error has occured...
		return E_UNEXPECTED;
	
	// Copy the media type
	AM_MEDIA_TYPE *pMediaType;
	pSource->GetMediaType(&pMediaType);
	pDest->SetMediaType(pMediaType);
		DeleteMediaType(pMediaType);

	// Copy the preroll property
	hr = pSource->IsPreroll();
	if (hr == S_OK) 
		pDest->SetPreroll(TRUE);
	else if (hr == S_FALSE) 
		pDest->SetPreroll(FALSE);
	else // an unexpected error has occured...
		return E_UNEXPECTED;

	// Copy the discontinuity property

	hr = pSource->IsDiscontinuity();
	if (hr == S_OK) 
		pDest->SetDiscontinuity(TRUE);
	else if (hr == S_FALSE) 
		pDest->SetDiscontinuity(FALSE);
	else // an unexpected error has occured...
		return E_UNEXPECTED;

	return S_OK;
}

////////////////////////////////////////////////////////////////////////////
//	StartStreaming - prepare to recive new data
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::StartStreaming()
{
	m_rtLast = CRefTime(0L);

	// Initialize encoder
	m_VEncoder.Init(); 
	return S_OK;	
}

////////////////////////////////////////////////////////////////////////////
//	 EndOfStream - stop data processing 
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::EndOfStream()
{
	// Close encoder
	m_VEncoder.Close(); 
	return CTransformFilter::EndOfStream();
}

////////////////////////////////////////////////////////////////////////////
//	BeginFlush  - stop data processing 
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::BeginFlush()
{
	BYTE *pBuffer;
	IMediaSample *pSample;

	m_pOutput->GetDeliveryBuffer(&pSample,NULL,NULL,0L);
	pSample->GetPointer(&pBuffer);

	DWORD dwSize = pSample->GetSize();
	m_VEncoder.Finish(pBuffer,&dwSize);
	if (dwSize>0) {
		pSample->SetActualDataLength(dwSize);
		m_pOutput->Deliver(pSample);
	}
	pSample->Release();

	// Close encoder
	m_VEncoder.Close(); 
	return CTransformFilter::BeginFlush();
}

////////////////////////////////////////////////////////////////////////////
//	SetMediaType - called when filters are connecting
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::SetMediaType(PIN_DIRECTION direction,const CMediaType *pmt)
{
	HRESULT hr = CTransformFilter::SetMediaType(direction, pmt);

	if (*pmt->FormatType() != FORMAT_WaveFormatEx)
		return VFW_E_INVALIDMEDIATYPE;

	if (pmt->FormatLength() < sizeof(WAVEFORMATEX))
		return VFW_E_INVALIDMEDIATYPE;

	if(direction == PINDIR_INPUT)
	{	
		DbgLog((LOG_TRACE,1,TEXT("CMpegAudEnc::SetMediaType(), direction = PINDIR_INPUT")));

		// Pass input media type to encoder
		m_VEncoder.SetInputType((LPWAVEFORMATEX)pmt->Format());

		// Set default output MPEG audio settings according to
		// input type
		SetOutMediaType();
		Reconnect();
	}
	else if (direction == PINDIR_OUTPUT)
	{
		WAVEFORMATEX wfIn;
		m_VEncoder.GetInputType(&wfIn);

		if (wfIn.nSamplesPerSec %
			((LPWAVEFORMATEX)pmt->Format())->nSamplesPerSec != 0)
			return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return hr;
}

////////////////////////////////////////////////////////////////////////////
// CheckInputType - check if you can support mtIn
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::CheckInputType(const CMediaType* mtIn)
{
	if (*mtIn->Type() == MEDIATYPE_Audio && *mtIn->FormatType() == FORMAT_WaveFormatEx)
		if (mtIn->FormatLength() >= sizeof(WAVEFORMATEX))
			if (mtIn->IsTemporalCompressed() == FALSE) 	
			{
				HRESULT hr = m_VEncoder.SetInputType((LPWAVEFORMATEX)mtIn->Format());

 				return hr;
			}

	return E_INVALIDARG;
}

////////////////////////////////////////////////////////////////////////////
// CheckTransform - checks if we can support the transform from this input to this output
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
	if (*mtOut->FormatType() != FORMAT_WaveFormatEx)
		return VFW_E_INVALIDMEDIATYPE;

	if (mtOut->FormatLength() < sizeof(WAVEFORMATEX))
		return VFW_E_INVALIDMEDIATYPE;

	MPEG_ENCODER_CONFIG	mec;
	if(FAILED(m_VEncoder.GetOutputType(&mec)))
		return S_OK;

	if (((LPWAVEFORMATEX)mtIn->Format())->nSamplesPerSec % mec.dwSampleRate != 0)
		return S_OK;

	if (mec.dwSampleRate != ((LPWAVEFORMATEX)mtOut->Format())->nSamplesPerSec)
		return VFW_E_TYPE_NOT_ACCEPTED;

	return S_OK;
}

////////////////////////////////////////////////////////////////////////////
// DecideBufferSize - sets output buffers number and size
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::DecideBufferSize(
                        IMemAllocator*		  pAllocator,
                        ALLOCATOR_PROPERTIES* pProperties)
{   
	HRESULT hr = S_OK;

	///
    pProperties->cBuffers = 4;
    pProperties->cbBuffer = OUTPUT_BUFF_SIZE;
	//

    ASSERT(pProperties->cbBuffer);
    
    ALLOCATOR_PROPERTIES Actual;
    hr = pAllocator->SetProperties(pProperties,&Actual);
    if(FAILED(hr)) 
        return hr;

    ASSERT(Actual.cbAlign == 1);
    ASSERT(Actual.cbPrefix == 0);

    if (Actual.cbBuffer < pProperties->cbBuffer ||
        Actual.cBuffers < pProperties->cBuffers) 
	{// can't use this allocator
		return E_INVALIDARG;
    }
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////
// GetMediaType - overrideble for suggesting outpu pin media types
////////////////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::GetMediaType(int iPosition, CMediaType *pMediaType)
{	
    DbgLog((LOG_TRACE,1,TEXT("CMpegAudEnc::GetMediaType()")));

	if (iPosition < 0)						
		return E_INVALIDARG;
	
	
	switch(iPosition)
	{
		case 0:
		{// We can support two output streams - PES or AES	
			if(m_VEncoder.IsPES())
			{
				pMediaType->SetType(&MEDIATYPE_MPEG2_PES);
				pMediaType->SetSubtype(&MEDIASUBTYPE_MPEG2_AUDIO);		
			}
			else
			{
				pMediaType->SetType(&MEDIATYPE_Stream);
				pMediaType->SetSubtype(&MEDIASUBTYPE_MPEG1Audio);		
			}
			break;
		}
		default: 
			return VFW_S_NO_MORE_ITEMS;
	}	
	
	if (m_pInput->IsConnected() == FALSE)	
	{	
		pMediaType->SetFormatType(&FORMAT_None);
		return NOERROR;
	}			 	
	

	SetOutMediaType();
	pMediaType->SetSampleSize(4096);	
	pMediaType->SetTemporalCompression(FALSE);	
	pMediaType->AllocFormatBuffer(sizeof(MPEG1WAVEFORMAT));
	pMediaType->SetFormat((LPBYTE)&m_mwf, sizeof(MPEG1WAVEFORMAT));	
	pMediaType->SetFormatType(&FORMAT_WaveFormatEx);

	return S_OK;
}

////////////////////////////////////////////////////////////////////////////
//	SetOutMediaType - sets filter output media type according to 
//  current encoder out MPEG audio properties 
////////////////////////////////////////////////////////////////////////////
void CMpegAudEnc::SetOutMediaType()
{
	MPEG_ENCODER_CONFIG	mec;
	if(FAILED(m_VEncoder.GetOutputType(&mec)))
		return ;
	WAVEFORMATEX wf;
	if(FAILED(m_VEncoder.GetInputType(&wf)))
		return ;

	BOOL bDirty = FALSE;

	if ((wf.nSamplesPerSec % mec.dwSampleRate) != 0)
	{
		mec.dwSampleRate = wf.nSamplesPerSec;
		m_VEncoder.SetOutputType(mec);
		bDirty = TRUE;
	}

	if (wf.nChannels == 1 && mec.dwChMode != MPG_MD_MONO)
	{
		mec.dwChMode = MPG_MD_MONO;
		m_VEncoder.SetOutputType(mec);
		bDirty = TRUE;
	}

	if (wf.nChannels == 2 && mec.dwChMode == MPG_MD_MONO)
	{
		mec.dwChMode = MPG_MD_STEREO;
		m_VEncoder.SetOutputType(mec);
		bDirty = TRUE;
	}

	if (bDirty)
	{
		// we are tied to the registry, especially our configuration
		// so when we change the incorrect sample rate, we need
		// to change the value in registry too. Same to channel mode
		SaveAudioEncoderPropertiesToRegistry();
		DbgLog((LOG_TRACE, 1, TEXT("Changed sampling rate internally")));
	}

	// Fill MPEG1WAVEFORMAT DirectShow SDK structure
	m_mwf.wfx.wFormatTag		= WAVE_FORMAT_MPEG;
	m_mwf.wfx.cbSize			= sizeof(MPEG1WAVEFORMAT) - sizeof(WAVEFORMATEX);		

	if(mec.dwChMode == MPG_MD_MONO)
		m_mwf.wfx.nChannels	= 1;
	else
		m_mwf.wfx.nChannels	= 2;

	m_mwf.wfx.nSamplesPerSec	= mec.dwSampleRate;
	m_mwf.wfx.nBlockAlign		= 1;

	m_mwf.wfx.wBitsPerSample	= wf.wBitsPerSample; 
	m_mwf.wfx.nAvgBytesPerSec	= mec.dwBitrate/8;	
	m_mwf.fwHeadLayer			= ACM_MPEG_LAYER3;

	m_mwf.dwHeadBitrate		= mec.dwBitrate;



	if (mec.dwChMode == MPG_MD_MONO)
		m_mwf.fwHeadMode		= ACM_MPEG_SINGLECHANNEL;
	else  if(mec.dwChMode == MPG_MD_STEREO)
		m_mwf.fwHeadMode		= ACM_MPEG_STEREO;
	else  if(mec.dwChMode == MPG_MD_JOINT_STEREO)
		m_mwf.fwHeadMode		= ACM_MPEG_JOINTSTEREO;
	else  if(mec.dwChMode == MPG_MD_DUAL_CHANNEL)
		m_mwf.fwHeadMode		= ACM_MPEG_DUALCHANNEL;
	m_mwf.fwHeadModeExt		= 0;
	m_mwf.wHeadEmphasis		= 0;
	if(mec.bCRCProtect)
		m_mwf.fwHeadFlags		|= ACM_MPEG_PROTECTIONBIT;

	if(mec.bOriginal)
		m_mwf.fwHeadFlags		|= ACM_MPEG_ORIGINALHOME;

	if(mec.bCopyright)
		m_mwf.fwHeadFlags		|= ACM_MPEG_COPYRIGHT;

	m_mwf.fwHeadFlags			|= ACM_MPEG_ID_MPEG1;

	m_mwf.dwPTSLow			= 0;
	m_mwf.dwPTSHigh			= 0;
}

HRESULT CMpegAudEnc::Reconnect()
{
	if(m_pOutput && m_pOutput->IsConnected())
	{// Reconnect all afterward filters	

		MPEG_ENCODER_CONFIG mec;
		ReadPresetSettings(&mec);
		if(m_VEncoder.SetOutputType(mec) == S_FALSE)
			return S_FALSE;

		SetOutMediaType();

//		FILTER_INFO fInfo;
//		QueryFilterInfo(&fInfo);
//		fInfo.pGraph->Reconnect(m_pOutput);
//		QueryFilterInfoReleaseGraph(fInfo);

        DbgLog((LOG_TRACE,1,TEXT("Need to reconnect our streaming pin")));
        CMediaType cmt;
        GetMediaType(0,&cmt);
        if (S_OK == m_pOutput->GetConnected()->QueryAccept(&cmt))
		{
            m_pGraph->Reconnect(m_pOutput);
        } else {
            // We're going to have to get clever and insert some
            // filters between us to help us reconnect
            DbgLog((LOG_TRACE,1,TEXT("Whoa! We *really* need to reconnect!")));
            IPin *pCon = m_pOutput->GetConnected();
            pCon->AddRef();	// or it will go away in Disconnect
            m_pGraph->Disconnect(m_pOutput->GetConnected());
            m_pGraph->Disconnect(m_pOutput);
            IGraphBuilder *pFG;
            HRESULT hr = m_pGraph->QueryInterface(
                IID_IGraphBuilder, (void **)&pFG);
            if (hr == NOERROR) {
                hr = pFG->Connect(m_pOutput, pCon);
                pFG->Release();
            }
            pCon->Release();
            if (hr != NOERROR)
                DbgLog((LOG_ERROR,1,TEXT("*** RECONNECT FAILED! ***")));
            // !!! We need to notify application that graph is different
        }
	}

	return S_OK;
}

//
// Read persistent configuration from Registry
//
void CMpegAudEnc::ReadPresetSettings(MPEG_ENCODER_CONFIG * pmec)
{
	DbgLog((LOG_TRACE,1,TEXT("CMpegAudEnc::ReadPresetSettings()")));

	CRegKey		rk(HKEY_LOCAL_MACHINE, KEY_LAME_ENCODER);

	pmec->dwBitrate		    = rk.getDWORD(VALUE_BITRATE,DEFAULT_BITRATE);
	pmec->dwVariableMin		= rk.getDWORD(VALUE_VARIABLEMIN,DEFAULT_VARIABLEMIN);
	pmec->dwVariableMax		= rk.getDWORD(VALUE_VARIABLEMAX,DEFAULT_VARIABLEMAX);
    pmec->vmVariable		= rk.getDWORD(VALUE_VARIABLE, DEFAULT_VARIABLE) ? vbr_rh : vbr_off;
	pmec->dwQuality    		= rk.getDWORD(VALUE_QUALITY,DEFAULT_ENCODING_QUALITY);
	pmec->dwVBRq    		    = rk.getDWORD(VALUE_VBR_QUALITY,DEFAULT_VBR_QUALITY);
	pmec->lLayer			    = rk.getDWORD(VALUE_LAYER, DEFAULT_LAYER);
	pmec->bCRCProtect		    = rk.getDWORD(VALUE_CRC, DEFAULT_CRC);
	pmec->bCopyright		    = rk.getDWORD(VALUE_COPYRIGHT, DEFAULT_COPYRIGHT); 
	pmec->bOriginal		    = rk.getDWORD(VALUE_ORIGINAL, DEFAULT_ORIGINAL); 
	pmec->dwSampleRate	    = rk.getDWORD(VALUE_SAMPLE_RATE, DEFAULT_SAMPLE_RATE);
	pmec->dwPES		        = rk.getDWORD(VALUE_PES, DEFAULT_PES); 

	pmec->dwChMode		    = rk.getDWORD(VALUE_STEREO_MODE, DEFAULT_STEREO_MODE);
	pmec->dwForceMS			= rk.getDWORD(VALUE_FORCE_MS, DEFAULT_FORCE_MS);

	pmec->dwEnforceVBRmin		= rk.getDWORD(VALUE_ENFORCE_MIN, DEFAULT_ENFORCE_MIN);
	pmec->dwVoiceMode		    = rk.getDWORD(VALUE_VOICE, DEFAULT_VOICE); 
	pmec->dwKeepAllFreq		= rk.getDWORD(VALUE_KEEP_ALL_FREQ, DEFAULT_KEEP_ALL_FREQ); 
	pmec->dwStrictISO		    = rk.getDWORD(VALUE_STRICT_ISO, DEFAULT_STRICT_ISO); 
	pmec->dwNoShortBlock	    = rk.getDWORD(VALUE_DISABLE_SHORT_BLOCK, DEFAULT_DISABLE_SHORT_BLOCK); 
	pmec->dwXingTag		    = rk.getDWORD(VALUE_XING_TAG, DEFAULT_XING_TAG); 
	pmec->dwModeFixed		    = rk.getDWORD(VALUE_MODE_FIXED, DEFAULT_MODE_FIXED); 


	rk.Close();
}

////////////////////////////////////////////////////////////////
//  Property page handling 
////////////////////////////////////////////////////////////////
HRESULT CMpegAudEnc::GetPages(CAUUID *pcauuid) 
{
    GUID *pguid;
   
	pcauuid->cElems = 2;
    pcauuid->pElems = pguid = (GUID *) CoTaskMemAlloc(sizeof(GUID) * pcauuid->cElems);

    if (pcauuid->pElems == NULL)
        return E_OUTOFMEMORY;

    pguid[0] = CLSID_LAMEDShow_PropertyPage;
    pguid[1] = CLSID_LAMEDShow_PropertyPageAdv;

    return S_OK;
}

STDMETHODIMP CMpegAudEnc::NonDelegatingQueryInterface(REFIID riid, void ** ppv) 
{

    if (riid == IID_ISpecifyPropertyPages)
        return GetInterface((ISpecifyPropertyPages *) this, ppv);
	else if(riid == IID_IPersistStream)
        return GetInterface((IPersistStream *)this, ppv);
//    else if (riid == IID_IVAudioEncSettings)
//        return GetInterface((IVAudioEncSettings*) this, ppv);
    else if (riid == IID_IAudioEncoderProperties)
        return GetInterface((IAudioEncoderProperties*) this, ppv);

    return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

////////////////////////////////////////////////////////////////
//IVAudioEncSettings interface methods
////////////////////////////////////////////////////////////////

/*
STDMETHODIMP CMpegAudEnc::SetOutputType(MPEG_ENCODER_CONFIG mec, BOOL bPES)
{
	m_VEncoder.SetPES(bPES);
	return m_VEncoder.SetOutputType(mec);
}

STDMETHODIMP CMpegAudEnc::GetOutputType(MPEG_ENCODER_CONFIG *pmec, BOOL *pbPuase)
{
	*pbPuase = m_VEncoder.IsPES();
	return m_VEncoder.GetOutputType(pmec);
}
*/

//
// IAudioEncoderProperties
//
STDMETHODIMP CMpegAudEnc::get_PESOutputEnabled(DWORD *dwEnabled)
{
	*dwEnabled = (DWORD)m_VEncoder.IsPES();
	DbgLog((LOG_TRACE, 1, TEXT("get_PESOutputEnabled -> %d"), *dwEnabled));

	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_PESOutputEnabled(DWORD dwEnabled)
{
	m_VEncoder.SetPES((BOOL)!!dwEnabled);
	DbgLog((LOG_TRACE, 1, TEXT("set_PESOutputEnabled(%d)"), !!dwEnabled));

	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_MPEGLayer(DWORD *dwLayer)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwLayer = (DWORD)mec.lLayer;

	DbgLog((LOG_TRACE, 1, TEXT("get_MPEGLayer -> %d"), *dwLayer));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_MPEGLayer(DWORD dwLayer)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	if (dwLayer == 2)
		mec.lLayer = 2;
	else if (dwLayer == 1)
		mec.lLayer = 1;
	m_VEncoder.SetOutputType(mec);

	DbgLog((LOG_TRACE, 1, TEXT("set_MPEGLayer(%d)"), dwLayer));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_Bitrate(DWORD *dwBitrate)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwBitrate = (DWORD)mec.dwBitrate;
	DbgLog((LOG_TRACE, 1, TEXT("get_Bitrate -> %d"), *dwBitrate));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_Bitrate(DWORD dwBitrate)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwBitrate = dwBitrate;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_Bitrate(%d)"), dwBitrate));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_Variable(DWORD *dwVariable)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwVariable = (DWORD)(mec.vmVariable == vbr_off ? 0 : 1);
	DbgLog((LOG_TRACE, 1, TEXT("get_Variable -> %d"), *dwVariable));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_Variable(DWORD dwVariable)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);

	mec.vmVariable = dwVariable ? vbr_rh : vbr_off;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_Variable(%d)"), dwVariable));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_VariableMin(DWORD *dwMin)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwMin = (DWORD)mec.dwVariableMin;
	DbgLog((LOG_TRACE, 1, TEXT("get_Variablemin -> %d"), *dwMin));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_VariableMin(DWORD dwMin)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwVariableMin = dwMin;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_Variablemin(%d)"), dwMin));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_VariableMax(DWORD *dwMax)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwMax = (DWORD)mec.dwVariableMax;
	DbgLog((LOG_TRACE, 1, TEXT("get_Variablemax -> %d"), *dwMax));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_VariableMax(DWORD dwMax)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwVariableMax = dwMax;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_Variablemax(%d)"), dwMax));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_Quality(DWORD *dwQuality)             
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwQuality=(DWORD)mec.dwQuality;
	DbgLog((LOG_TRACE, 1, TEXT("get_Quality -> %d"), *dwQuality));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_Quality(DWORD dwQuality)             
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwQuality = dwQuality;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_Quality(%d)"), dwQuality));
	return S_OK;
}
STDMETHODIMP CMpegAudEnc::get_VariableQ(DWORD *dwVBRq)             
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwVBRq=(DWORD)mec.dwVBRq;
	DbgLog((LOG_TRACE, 1, TEXT("get_VariableQ -> %d"), *dwVBRq));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_VariableQ(DWORD dwVBRq)             
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwVBRq = dwVBRq;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_VariableQ(%d)"), dwVBRq));
	return S_OK;
}


STDMETHODIMP CMpegAudEnc::get_SourceSampleRate(DWORD *dwSampleRate)
{
	*dwSampleRate = 0;

	WAVEFORMATEX wf;
	if(FAILED(m_VEncoder.GetInputType(&wf)))
		return E_FAIL;

	*dwSampleRate = wf.nSamplesPerSec;
	DbgLog((LOG_TRACE, 1, TEXT("get_SourceSampleRate -> %d"), *dwSampleRate));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_SourceChannels(DWORD *dwChannels)
{
	WAVEFORMATEX wf;
	if(FAILED(m_VEncoder.GetInputType(&wf)))
		return E_FAIL;

	*dwChannels = wf.nChannels;
	DbgLog((LOG_TRACE, 1, TEXT("get_SourceChannels -> %d"), *dwChannels));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_SampleRate(DWORD *dwSampleRate)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwSampleRate = mec.dwSampleRate;
	DbgLog((LOG_TRACE, 1, TEXT("get_SampleRate -> %d"), *dwSampleRate));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_SampleRate(DWORD dwSampleRate)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwSampleRate = dwSampleRate;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_SampleRate(%d)"), dwSampleRate));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_ChannelMode(DWORD *dwChannelMode)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwChannelMode = mec.dwChMode;
	DbgLog((LOG_TRACE, 1, TEXT("get_ChannelMode -> %d"), *dwChannelMode));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_ChannelMode(DWORD dwChannelMode)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwChMode = dwChannelMode;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_ChannelMode(%d)"), dwChannelMode));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_ForceMS(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.dwForceMS;
	DbgLog((LOG_TRACE, 1, TEXT("get_ForceMS -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_ForceMS(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwForceMS = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_ForceMS(%d)"), dwFlag));
	return S_OK;
}


STDMETHODIMP CMpegAudEnc::get_CRCFlag(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.bCRCProtect;
	DbgLog((LOG_TRACE, 1, TEXT("get_CRCFlag -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_CRCFlag(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.bCRCProtect = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_CRCFlag(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_EnforceVBRmin(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.dwEnforceVBRmin;
	DbgLog((LOG_TRACE, 1, TEXT("get_EnforceVBRmin -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_EnforceVBRmin(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwEnforceVBRmin = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_EnforceVBRmin(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_VoiceMode(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.dwVoiceMode;
	DbgLog((LOG_TRACE, 1, TEXT("get_VoiceMode -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_VoiceMode(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwVoiceMode = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_VoiceMode(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_KeepAllFreq(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.dwKeepAllFreq;
	DbgLog((LOG_TRACE, 1, TEXT("get_KeepAllFreq -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_KeepAllFreq(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwKeepAllFreq = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_KeepAllFreq(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_StrictISO(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.dwStrictISO;
	DbgLog((LOG_TRACE, 1, TEXT("get_StrictISO -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_StrictISO(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwStrictISO = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_StrictISO(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_NoShortBlock(DWORD *dwNoShortBlock)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwNoShortBlock = mec.dwNoShortBlock;
	DbgLog((LOG_TRACE, 1, TEXT("get_NoShortBlock -> %d"), *dwNoShortBlock));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_NoShortBlock(DWORD dwNoShortBlock)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwNoShortBlock = dwNoShortBlock;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_NoShortBlock(%d)"), dwNoShortBlock));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_XingTag(DWORD *dwXingTag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwXingTag = mec.dwXingTag;
	DbgLog((LOG_TRACE, 1, TEXT("get_XingTag -> %d"), *dwXingTag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_XingTag(DWORD dwXingTag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwXingTag = dwXingTag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_XingTag(%d)"), dwXingTag));
	return S_OK;
}



STDMETHODIMP CMpegAudEnc::get_OriginalFlag(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.bOriginal;
	DbgLog((LOG_TRACE, 1, TEXT("get_OriginalFlag -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_OriginalFlag(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.bOriginal = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_OriginalFlag(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_CopyrightFlag(DWORD *dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwFlag = mec.bCopyright;
	DbgLog((LOG_TRACE, 1, TEXT("get_CopyrightFlag -> %d"), *dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_CopyrightFlag(DWORD dwFlag)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.bCopyright = dwFlag;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_CopyrightFlag(%d)"), dwFlag));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_ModeFixed(DWORD *dwModeFixed)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	*dwModeFixed = mec.dwModeFixed;
	DbgLog((LOG_TRACE, 1, TEXT("get_ModeFixed -> %d"), *dwModeFixed));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::set_ModeFixed(DWORD dwModeFixed)
{
	MPEG_ENCODER_CONFIG mec;
	m_VEncoder.GetOutputType(&mec);
	mec.dwModeFixed = dwModeFixed;
	m_VEncoder.SetOutputType(mec);
	DbgLog((LOG_TRACE, 1, TEXT("set_ModeFixed(%d)"), dwModeFixed));
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::get_ParameterBlockSize(BYTE *pcBlock, DWORD *pdwSize)
{
	DbgLog((LOG_TRACE, 1, TEXT("get_ParameterBlockSize -> %d%d"), *pcBlock, *pdwSize));

	if (pcBlock != NULL) {
		if (*pdwSize >= sizeof(MPEG_ENCODER_CONFIG)) {
			m_VEncoder.GetOutputType((MPEG_ENCODER_CONFIG*)pcBlock);
			return S_OK;
		}
		else {
			*pdwSize = sizeof(MPEG_ENCODER_CONFIG);
			return E_FAIL;
		}
	}
	else if (pdwSize != NULL) {
		*pdwSize = sizeof(MPEG_ENCODER_CONFIG);
		return S_OK;
	}

	return E_FAIL;
}

STDMETHODIMP CMpegAudEnc::set_ParameterBlockSize(BYTE *pcBlock, DWORD dwSize)
{
	DbgLog((LOG_TRACE, 1, TEXT("get_ParameterBlockSize(%d, %d)"), *pcBlock, dwSize));
	if (sizeof(MPEG_ENCODER_CONFIG) == dwSize){
		m_VEncoder.SetOutputType(*(MPEG_ENCODER_CONFIG*)pcBlock);
		return S_OK;
	}
	else return E_FAIL;	
}


STDMETHODIMP CMpegAudEnc::DefaultAudioEncoderProperties()
{
	DbgLog((LOG_TRACE, 1, TEXT("DefaultAudioEncoderProperties()")));

	HRESULT hr = InputTypeDefined();
	if (FAILED(hr))
		return hr;

	DWORD dwSourceSampleRate;
	get_SourceSampleRate(&dwSourceSampleRate);

	set_PESOutputEnabled(DEFAULT_PES);
	set_MPEGLayer(DEFAULT_LAYER);

	set_Bitrate(DEFAULT_BITRATE);
	set_Variable(FALSE);
	set_VariableMin(DEFAULT_VARIABLEMIN);
	set_VariableMax(DEFAULT_VARIABLEMAX);
	set_Quality(DEFAULT_ENCODING_QUALITY);
	set_VariableQ(DEFAULT_VBR_QUALITY);

	set_SampleRate(dwSourceSampleRate);
	set_CRCFlag(FALSE);
	set_OriginalFlag(FALSE);
	set_CopyrightFlag(FALSE);

	set_EnforceVBRmin(DEFAULT_ENFORCE_MIN);
	set_VoiceMode(DEFAULT_VOICE);
	set_KeepAllFreq(DEFAULT_KEEP_ALL_FREQ);
	set_StrictISO(DEFAULT_STRICT_ISO);
	set_NoShortBlock(DEFAULT_DISABLE_SHORT_BLOCK);
	set_XingTag(DEFAULT_XING_TAG);
	set_ForceMS(DEFAULT_FORCE_MS);
	set_ChannelMode(DEFAULT_STEREO_MODE);
	set_ModeFixed(DEFAULT_MODE_FIXED);

	return S_OK;
}

STDMETHODIMP CMpegAudEnc::LoadAudioEncoderPropertiesFromRegistry()
{
	DbgLog((LOG_TRACE, 1, TEXT("LoadAudioEncoderPropertiesFromRegistry()")));

	MPEG_ENCODER_CONFIG mec;
	ReadPresetSettings(&mec);
	if(m_VEncoder.SetOutputType(mec) == S_FALSE)
		return S_FALSE;
	return S_OK;
}

STDMETHODIMP CMpegAudEnc::SaveAudioEncoderPropertiesToRegistry()
{
	DbgLog((LOG_TRACE, 1, TEXT("SaveAudioEncoderPropertiesToRegistry()")));
	CRegKey		rk;

	MPEG_ENCODER_CONFIG mec;
	if(m_VEncoder.GetOutputType(&mec) == S_FALSE)
		return E_FAIL;

	if(rk.Create(HKEY_LOCAL_MACHINE, KEY_LAME_ENCODER))
	{
		rk.setDWORD(VALUE_BITRATE, mec.dwBitrate);
		rk.setDWORD(VALUE_VARIABLE, mec.vmVariable);
		rk.setDWORD(VALUE_VARIABLEMIN, mec.dwVariableMin);
		rk.setDWORD(VALUE_VARIABLEMAX, mec.dwVariableMax);
		rk.setDWORD(VALUE_QUALITY, mec.dwQuality);
		rk.setDWORD(VALUE_VBR_QUALITY, mec.dwVBRq);

		rk.setDWORD(VALUE_CRC, mec.bCRCProtect);
		rk.setDWORD(VALUE_PES, mec.dwPES);
		rk.setDWORD(VALUE_COPYRIGHT, mec.bCopyright); 
		rk.setDWORD(VALUE_ORIGINAL, mec.bOriginal); 
		rk.setDWORD(VALUE_SAMPLE_RATE, mec.dwSampleRate); 

		rk.setDWORD(VALUE_STEREO_MODE, mec.dwChMode);
		rk.setDWORD(VALUE_FORCE_MS, mec.dwForceMS);
		rk.setDWORD(VALUE_XING_TAG, mec.dwXingTag);
		rk.setDWORD(VALUE_DISABLE_SHORT_BLOCK, mec.dwNoShortBlock);
		rk.setDWORD(VALUE_STRICT_ISO, mec.dwStrictISO);
		rk.setDWORD(VALUE_KEEP_ALL_FREQ, mec.dwKeepAllFreq);
		rk.setDWORD(VALUE_VOICE, mec.dwVoiceMode);
		rk.setDWORD(VALUE_ENFORCE_MIN, mec.dwEnforceVBRmin);
		rk.setDWORD(VALUE_MODE_FIXED, mec.dwModeFixed);
	
		rk.Close();
	}

	// Reconnect filter graph
	Reconnect(); 

	return S_OK;
}

STDMETHODIMP CMpegAudEnc::InputTypeDefined()
{
	WAVEFORMATEX wf;
	if(FAILED(m_VEncoder.GetInputType(&wf)))
	{
		DbgLog((LOG_TRACE, 1, TEXT("!InputTypeDefined()")));
		return E_FAIL;
	}  

	DbgLog((LOG_TRACE, 1, TEXT("InputTypeDefined()")));
	return S_OK;
}


//
// CPersistStream stuff
//

// what is our class ID?
STDMETHODIMP CMpegAudEnc::GetClassID(CLSID *pClsid)
{
    CheckPointer(pClsid, E_POINTER);
    *pClsid = CLSID_LAMEDShowFilter;
    return S_OK;
}

HRESULT CMpegAudEnc::WriteToStream(IStream *pStream)
{
    CAutoLock l(&m_VEncoder);

    DbgLog((LOG_TRACE,1,TEXT("WriteToStream()")));

	MPEG_ENCODER_CONFIG mec;

	if(m_VEncoder.GetOutputType(&mec) == S_FALSE)
		return E_FAIL;

    return pStream->Write(&mec, sizeof(mec), 0);
}


// what device should we use?  Used to re-create a .GRF file that we
// are in
HRESULT CMpegAudEnc::ReadFromStream(IStream *pStream)
{
    CAutoLock l(&m_VEncoder);

	MPEG_ENCODER_CONFIG mec;

    HRESULT hr = pStream->Read(&mec, sizeof(mec), 0);
    if(FAILED(hr))
        return hr;

	if(m_VEncoder.SetOutputType(mec) == S_FALSE)
		return S_FALSE;

    DbgLog((LOG_TRACE,1,TEXT("ReadFromStream() succeeded")));

    hr = S_OK;
    return hr;
}


// How long is our data?
int CMpegAudEnc::SizeMax()
{
    return sizeof(MPEG_ENCODER_CONFIG);
}

/*
//
// IPersistPropertyBag stuff
//
// Load is called to tell us what device to use.  There may be several
// capture cards on the system that we could use
STDMETHODIMP CMpegAudEnc::Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog)
{
    HRESULT hr;
    CAutoLock l(&m_VEncoder);

    DbgLog((LOG_TRACE,1,TEXT("Load...")));

    // Default settings
    if (pPropBag == NULL) {
        DbgLog((LOG_TRACE,1,TEXT("Using default settings")));
		return hr;
    }

	MPEG_ENCODER_CONFIG mec;
	if(m_VEncoder.GetOutputType(&mec) == S_FALSE)
		return E_FAIL;

    // find out what device to use
    // different filters look in different places to find this info
    VARIANT var;
    var.vt = VT_UI4;
    hr = pPropBag->Read(OLESTR(VALUE_BITRATE), &var, 0);
    if(SUCCEEDED(hr))
    {
		mec.dwBitrate = var.ulVal;
    }

	if(m_VEncoder.SetOutputType(mec) == S_FALSE)
		return S_FALSE;

    return hr;
}


STDMETHODIMP CMpegAudEnc::Save(
    LPPROPERTYBAG pPropBag, BOOL fClearDirty,
    BOOL fSaveAllProperties)
{
    CAutoLock l(&m_VEncoder);

	MPEG_ENCODER_CONFIG mec;
	if(m_VEncoder.GetOutputType(&mec) == S_FALSE)
		return E_FAIL;

	HRESULT hr;
	VARIANT var;

	var.vt = VT_UI4;
	var.ulVal = mec.dwBitrate;
	hr = pPropBag->Write(OLESTR(VALUE_BITRATE),&var);

	rk.setDWORD(VALUE_BITRATE, mec.dwBitrate);
	rk.setDWORD(VALUE_VARIABLE, mec.dwVariable);
	rk.setDWORD(VALUE_VARIABLEMIN, mec.dwVariableMin);
	rk.setDWORD(VALUE_VARIABLEMAX, mec.dwVariableMax);
	rk.setDWORD(VALUE_QUALITY, mec.dwQuality);
	rk.setDWORD(VALUE_VBR_QUALITY, mec.dwVBRq);

	rk.setDWORD(VALUE_CRC, mec.bCRCProtect);
	rk.setDWORD(VALUE_PES, mec.dwPES);
	rk.setDWORD(VALUE_COPYRIGHT, mec.bCopyright); 
	rk.setDWORD(VALUE_ORIGINAL, mec.bOriginal); 
	rk.setDWORD(VALUE_SAMPLE_RATE, mec.dwSampleRate); 

	rk.setDWORD(VALUE_STEREO_MODE, mec.dwChMode);
	rk.setDWORD(VALUE_FORCE_MS, mec.dwForceMS);
	rk.setDWORD(VALUE_XING_TAG, mec.dwXingTag);
	rk.setDWORD(VALUE_DISABLE_SHORT_BLOCK, mec.dwNoShortBlock);
	rk.setDWORD(VALUE_STRICT_ISO, mec.dwStrictISO);
	rk.setDWORD(VALUE_KEEP_ALL_FREQ, mec.dwKeepAllFreq);
	rk.setDWORD(VALUE_VOICE, mec.dwVoiceMode);
	rk.setDWORD(VALUE_ENFORCE_MIN, mec.dwEnforceVBRmin);
	rk.setDWORD(VALUE_MODE_FIXED, mec.dwModeFixed);

    // E_NOTIMPL is not really a valid return code as any object implementing
    // this interface must support the entire functionality of the
    // interface.
    return E_NOTIMPL;

  return hr;
}


// have we been initialized yet?  (Has somebody called Load)
STDMETHODIMP CMpegAudEnc::InitNew()
{
	CAutoLock l(&m_VEncoder);

	return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
//	return S_OK;
}
*/

////////////////////////////////////////////
STDAPI DllRegisterServer()
{
	// Create entry in HKEY_CLASSES_ROOT\Filter
	OLECHAR szCLSID[CHARS_IN_GUID];
	TCHAR achTemp[MAX_PATH];
	HKEY hKey;

	HRESULT hr = StringFromGUID2(*g_Templates[0].m_ClsID, szCLSID, CHARS_IN_GUID);
	wsprintf(achTemp, TEXT("Filter\\%ls"), szCLSID);
	// create key
	RegCreateKey(HKEY_CLASSES_ROOT, (LPCTSTR)achTemp, &hKey);
	RegCloseKey(hKey);
	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	// Delete entry in HKEY_CLASSES_ROOT\Filter
	OLECHAR szCLSID[CHARS_IN_GUID];
	TCHAR achTemp[MAX_PATH];

	HRESULT hr = StringFromGUID2(*g_Templates[0].m_ClsID, szCLSID, CHARS_IN_GUID);
	wsprintf(achTemp, TEXT("Filter\\%ls"), szCLSID);
	// create key
	RegDeleteKey(HKEY_CLASSES_ROOT, (LPCTSTR)achTemp);
	return AMovieDllRegisterServer2(FALSE);
}


