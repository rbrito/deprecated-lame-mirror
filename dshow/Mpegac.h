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

#include <mmreg.h>	
#include "Encoder.h"	// Added by ClassView

#define	KEY_LAME_ENCODER	"SOFTWARE\\GNU\\LAME MPEG Layer III Audio Encoder Filter"

#define	VALUE_BITRATE				"Bitrate"
#define	VALUE_VARIABLE				"Variable"
#define	VALUE_VARIABLEMIN			"VariableMin"
#define	VALUE_VARIABLEMAX			"VariableMax"
#define	VALUE_QUALITY				"Quality"
#define VALUE_VBR_QUALITY			"VBR Quality"
#define	VALUE_SAMPLE_RATE			"Sample Rate"

#define	VALUE_STEREO_MODE			"Stereo Mode"
#define VALUE_FORCE_MS				"Force MS"

#define	VALUE_LAYER					"Layer"
#define	VALUE_ORIGINAL				"Original"
#define	VALUE_COPYRIGHT				"Copyright"
#define	VALUE_CRC					"CRC"
#define	VALUE_PES					"PES"

#define	VALUE_ENFORCE_MIN			"EnforceVBRmin"
#define	VALUE_VOICE					"Voice Mode"
#define	VALUE_KEEP_ALL_FREQ			"Keep All Frequencies"
#define	VALUE_STRICT_ISO			"Strict ISO"
#define	VALUE_DISABLE_SHORT_BLOCK	"No Short Block"
#define	VALUE_XING_TAG				"Xing Tag"
#define VALUE_MODE_FIXED            "Mode Fixed"

///////////////////////////////////////////////////////////////////
// CMpegAudEnc class - implementation for ITransformFilter interface
///////////////////////////////////////////////////////////////////
class CMpegAudEncPropertyPage;
class CMpegAudEnc : public CTransformFilter,
					public ISpecifyPropertyPages,
					public IAudioEncoderProperties,
//					public IPersistPropertyBag,
					public CPersistStream
{
public:
	DECLARE_IUNKNOWN

    static CUnknown *CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);

	LPAMOVIESETUP_FILTER GetSetupData();

	HRESULT Reconnect();

	// ITransformFilter interface methods
	HRESULT Transform(IMediaSample * pIn, IMediaSample *pOut);    
    HRESULT CheckInputType(const CMediaType* mtIn);    
    HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut);    
    HRESULT DecideBufferSize(IMemAllocator * pAllocator,
							 ALLOCATOR_PROPERTIES *pprop);
    HRESULT GetMediaType  (int iPosition, CMediaType *pMediaType);
	HRESULT SetMediaType  (PIN_DIRECTION direction,const CMediaType *pmt);	


	//
	HRESULT StartStreaming();	
	HRESULT EndOfStream();
	HRESULT BeginFlush();

	~CMpegAudEnc(void);

	// ISpecifyPropertyPages
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);    
	STDMETHODIMP GetPages(CAUUID *pPages);

	// IAudioEncoderProperties
    STDMETHODIMP get_PESOutputEnabled(DWORD *dwEnabled);
    STDMETHODIMP set_PESOutputEnabled(DWORD dwEnabled);

    STDMETHODIMP get_MPEGLayer(DWORD *dwLayer);
    STDMETHODIMP set_MPEGLayer(DWORD dwLayer);

    STDMETHODIMP get_Bitrate(DWORD *dwBitrate);
    STDMETHODIMP set_Bitrate(DWORD dwBitrate);   
    STDMETHODIMP get_Variable(DWORD *dwVariable);
    STDMETHODIMP set_Variable(DWORD dwVariable);
	STDMETHODIMP get_VariableMin(DWORD *dwMin);
    STDMETHODIMP set_VariableMin(DWORD dwMin);
	STDMETHODIMP get_VariableMax(DWORD *dwMax);
    STDMETHODIMP set_VariableMax(DWORD dwMax);
	STDMETHODIMP get_Quality(DWORD *dwQuality);
    STDMETHODIMP set_Quality(DWORD dwQuality);
	STDMETHODIMP get_VariableQ(DWORD *dwVBRq);
    STDMETHODIMP set_VariableQ(DWORD dwVBRq);
    STDMETHODIMP get_SourceSampleRate(DWORD *dwSampleRate);
	STDMETHODIMP get_SourceChannels(DWORD *dwChannels);
    STDMETHODIMP get_SampleRate(DWORD *dwSampleRate);
    STDMETHODIMP set_SampleRate(DWORD dwSampleRate);

    STDMETHODIMP get_ChannelMode(DWORD *dwChannelMode);
    STDMETHODIMP set_ChannelMode(DWORD dwChannelMode);
	STDMETHODIMP get_ForceMS(DWORD *dwFlag);
    STDMETHODIMP set_ForceMS(DWORD dwFlag);
	STDMETHODIMP get_EnforceVBRmin(DWORD *dwFlag);
    STDMETHODIMP set_EnforceVBRmin(DWORD dwFlag);
	STDMETHODIMP get_VoiceMode(DWORD *dwFlag);
    STDMETHODIMP set_VoiceMode(DWORD dwFlag);
	STDMETHODIMP get_KeepAllFreq(DWORD *dwFlag);
    STDMETHODIMP set_KeepAllFreq(DWORD dwFlag);
	STDMETHODIMP get_StrictISO(DWORD *dwFlag);
    STDMETHODIMP set_StrictISO(DWORD dwFlag);
	STDMETHODIMP get_NoShortBlock(DWORD *dwNoShortBlock);
    STDMETHODIMP set_NoShortBlock(DWORD dwNoShortBlock);
	STDMETHODIMP get_XingTag(DWORD *dwXingTag);
    STDMETHODIMP set_XingTag(DWORD dwXingTag);
	STDMETHODIMP get_ModeFixed(DWORD *dwModeFixed);
    STDMETHODIMP set_ModeFixed(DWORD dwModeFixed);


	STDMETHODIMP get_CRCFlag(DWORD *dwFlag);
    STDMETHODIMP set_CRCFlag(DWORD dwFlag);
    STDMETHODIMP get_OriginalFlag(DWORD *dwFlag);
    STDMETHODIMP set_OriginalFlag(DWORD dwFlag);
    STDMETHODIMP get_CopyrightFlag(DWORD *dwFlag);
    STDMETHODIMP set_CopyrightFlag(DWORD dwFlag);

	STDMETHODIMP get_ParameterBlockSize(BYTE *pcBlock, DWORD *pdwSize);
    STDMETHODIMP set_ParameterBlockSize(BYTE *pcBlock, DWORD dwSize);

    STDMETHODIMP DefaultAudioEncoderProperties();
    STDMETHODIMP LoadAudioEncoderPropertiesFromRegistry();
    STDMETHODIMP SaveAudioEncoderPropertiesToRegistry();
	STDMETHODIMP InputTypeDefined();

    // IPersistPropertyBag methods
//    STDMETHODIMP InitNew();
//    STDMETHODIMP Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog);
//    STDMETHODIMP Save(LPPROPERTYBAG pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties);

    // CPersistStream
    HRESULT WriteToStream(IStream *pStream);
    HRESULT ReadFromStream(IStream *pStream);
    int SizeMax();
    STDMETHODIMP GetClassID(CLSID *pClsid);

private:
	CMpegAudEnc(LPUNKNOWN lpunk, HRESULT *phr);

	void		SetOutMediaType();

	void ReadPresetSettings(MPEG_ENCODER_CONFIG *pmabsi);

	// Encoder object
	CEncoder			m_VEncoder;

	// Current media out format
	MPEG1WAVEFORMAT		m_mwf;

	//
	// Sample times
	//
	REFERENCE_TIME      m_rtLast;

protected:
	friend class CMpegAudEncPropertyPage;
};


