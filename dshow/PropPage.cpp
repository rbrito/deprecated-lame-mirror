/*
 *	MPEG Audio Encoder for DirectShow
 *	Basic property page
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
#include <olectlid.h>
#include <commctrl.h>
#include "iaudioprops.h"
#include "mpegac.h"
#include "resource.h"
#include "PropPage.h"
#include "Reg.h"

// Strings which apear in comboboxes
const char *szBitRateString[2][14] = {
	{
		"32 kbps","40 kbps","48 kbps","56 kbps",
		"64 kbps","80 kbps","96 kbps","112 kbps",
		"128 kbps","160 kbps","192 kbps","224 kbps",
		"256 kbps","320 kbps"
	},
	{
		"8 kbps","16 kbps","24 kbps","32 kbps",
		"40 kbps","48 kbps","56 kbps","64 kbps",
		"80 kbps","96 kbps","112 kbps","128 kbps",
		"144 kbps","160 kbps"
	}
};

LPCSTR szQualityDesc[10] = {
	"High", "High", "High", "High", "High",
	"Medium", "Medium",
	"Low", "Low",
	"Fast mode"
};

LPCSTR szVBRqDesc[10] = {
	"0 - ~1:4",
	"1 - ~1:5",
	"2 - ~1:6",
	"3 - ~1:7",
	"4 - ~1:9",
	"5 - ~1:9",
	"6 - ~1:10",
	"7 - ~1:11",
	"8 - ~1:12",
	"9 - ~1:14"
};
struct SSampleRate {
	DWORD dwSampleRate;
	LPCSTR lpSampleRate;
};
SSampleRate srRates[6] = {
	{48000, "48 kHz"},
	{44100, "44.1 kHz"},
	{32000, "32 kHz"},
	{24000, "24 kHz"},
	{22050, "22.05 kHz"},
	{16000, "16 kHz"}
};

////////////////////////////////////////////////////////////////
// CreateInstance
////////////////////////////////////////////////////////////////
CUnknown *CMpegAudEncPropertyPage::CreateInstance( LPUNKNOWN punk, HRESULT *phr )
{
    CMpegAudEncPropertyPage *pNewObject
        = new CMpegAudEncPropertyPage( punk, phr );

    if( pNewObject == NULL )
        *phr = E_OUTOFMEMORY;

    return pNewObject;
}

////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////
CMpegAudEncPropertyPage::CMpegAudEncPropertyPage(LPUNKNOWN punk, HRESULT *phr)
 : CBasePropertyPage(NAME("Encoder Property Page"), 
                      punk, IDD_AUDIOENCPROPS, IDS_AUDIO_PROPS_TITLE)                      
	, m_pAEProps(NULL)
	, m_fWindowInactive(TRUE)
{
    ASSERT(phr);

    InitCommonControls();
}

//
// OnConnect
//
// Give us the filter to communicate with
HRESULT CMpegAudEncPropertyPage::OnConnect(IUnknown *pUnknown)
{
	ASSERT(m_pAEProps == NULL);

	// Ask the filter for it's control interface

	HRESULT hr = pUnknown->QueryInterface(IID_IAudioEncoderProperties,(void **)&m_pAEProps);
	if (FAILED(hr)) {
		return E_NOINTERFACE;
	}

	ASSERT(m_pAEProps);

    // Get current filter state
    m_pAEProps->LoadAudioEncoderPropertiesFromRegistry();
	m_pAEProps->get_PESOutputEnabled(&m_dwPES);
	m_pAEProps->get_Bitrate(&m_dwBitrate);
    m_pAEProps->get_Variable(&m_dwVariable);
	m_pAEProps->get_VariableMin(&m_dwMin);
	m_pAEProps->get_VariableMax(&m_dwMax);
	m_pAEProps->get_Quality(&m_dwQuality);
	m_pAEProps->get_VariableQ(&m_dwVBRq);
	m_pAEProps->get_SampleRate(&m_dwSampleRate);
	m_pAEProps->get_ChannelMode(&m_dwChannelMode);
	m_pAEProps->get_CRCFlag(&m_dwCRC);
	m_pAEProps->get_CopyrightFlag(&m_dwCopyright);
	m_pAEProps->get_OriginalFlag(&m_dwOriginal);

	return NOERROR;
}

//
// OnDisconnect
//
// Release the interface

HRESULT CMpegAudEncPropertyPage::OnDisconnect()
{
    // Release the interface
    if (m_pAEProps == NULL) {
        return E_UNEXPECTED;
    }

	m_pAEProps->set_PESOutputEnabled(m_dwPES);
	m_pAEProps->set_Bitrate(m_dwBitrate);
	m_pAEProps->set_Variable(m_dwVariable);
	m_pAEProps->set_VariableMin(m_dwMin);
	m_pAEProps->set_VariableMax(m_dwMax);
	m_pAEProps->set_Quality(m_dwQuality);
	m_pAEProps->set_VariableQ(m_dwVBRq);
	m_pAEProps->set_SampleRate(m_dwSampleRate);
	m_pAEProps->set_ChannelMode(m_dwChannelMode);
	m_pAEProps->set_CRCFlag(m_dwCRC);
	m_pAEProps->set_CopyrightFlag(m_dwCopyright);
	m_pAEProps->set_OriginalFlag(m_dwOriginal);
	m_pAEProps->SaveAudioEncoderPropertiesToRegistry();

    m_pAEProps->Release();
    m_pAEProps = NULL;
    return NOERROR;
}

//
// OnActivate
//
// Called on dialog creation

HRESULT CMpegAudEncPropertyPage::OnActivate(void)
{
    InitPropertiesDialog(m_hwnd);

    m_fWindowInactive = FALSE;

    return NOERROR;
}

//
// OnDeactivate
//
// Called on dialog destruction

HRESULT CMpegAudEncPropertyPage::OnDeactivate(void)
{
    m_fWindowInactive = TRUE;
    return NOERROR;
}

////////////////////////////////////////////////////////////////
// OnReceiveMessage - message handler function
////////////////////////////////////////////////////////////////
BOOL CMpegAudEncPropertyPage::OnReceiveMessage(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch( uMsg )
	{
	case WM_HSCROLL:
		if ((HWND)lParam == m_hwndQuality) {
			int pos = SendMessage(m_hwndQuality,TBM_GETPOS,0,0);
			if (pos>=0 && pos<10) {
				SetDlgItemText(hwnd,IDC_TEXT_QUALITY,szQualityDesc[pos]);
				m_pAEProps->set_Quality(pos);
				SetDirty();
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_COMBO_CBR:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int nBitrate = SendDlgItemMessage(hwnd,IDC_COMBO_CBR,CB_GETCURSEL,0,0L);
				DWORD dwSampleRate;
				m_pAEProps->get_SampleRate(&dwSampleRate);
				DWORD dwBitrate;

				if (dwSampleRate >= 32000)
				{
					// Consider MPEG 1
					dwBitrate = dwBitRateValue[0][nBitrate];
				}
				else
				{
					// Consider MPEG 2
					dwBitrate = dwBitRateValue[1][nBitrate];
				}

				m_pAEProps->set_Bitrate(dwBitrate);

				SetDirty();
			}
			break;

		case IDC_COMBO_VBRMIN:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int nVariableMin = SendDlgItemMessage(hwnd,IDC_COMBO_VBRMIN,CB_GETCURSEL,0,0L);
				DWORD dwSampleRate;
				m_pAEProps->get_SampleRate(&dwSampleRate);
				DWORD dwMin;

				if (dwSampleRate >= 32000)
				{
					// Consider MPEG 1
					dwMin = dwBitRateValue[0][nVariableMin];
				}
				else
				{
					// Consider MPEG 2
					dwMin = dwBitRateValue[1][nVariableMin];
				}

				m_pAEProps->set_VariableMin(dwMin);

				SetDirty();
			}
			break;

		case IDC_COMBO_VBRMAX:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int nVariableMax = SendDlgItemMessage(hwnd,IDC_COMBO_VBRMAX,CB_GETCURSEL,0,0L);
				DWORD dwSampleRate;
				m_pAEProps->get_SampleRate(&dwSampleRate);
				DWORD dwMax;

				if (dwSampleRate >= 32000)
				{
					// Consider MPEG 1
					dwMax = dwBitRateValue[0][nVariableMax];
				}
				else
				{
					// Consider MPEG 2
					dwMax = dwBitRateValue[1][nVariableMax];
				}

				m_pAEProps->set_VariableMax(dwMax);

				SetDirty();
			}
			break;
	

		case IDC_COMBO_SAMPLE_RATE:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int nSampleRate = SendDlgItemMessage(hwnd, IDC_COMBO_SAMPLE_RATE, CB_GETCURSEL, 0, 0L);
				//DWORD dwSourceSampleRate;
				//m_pAEProps->get_SourceSampleRate(&dwSourceSampleRate);

				DWORD dwSampleRate;
			
				if ((nSampleRate >= 0) && (nSampleRate < 6)){
				dwSampleRate = srRates[nSampleRate].dwSampleRate;
				}
				m_pAEProps->set_SampleRate(dwSampleRate);
				InitPropertiesDialog(hwnd);
				SetDirty();
			}
			break;

		case IDC_COMBO_VBRq:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int nVBRq = SendDlgItemMessage(hwnd, IDC_COMBO_VBRq, CB_GETCURSEL, 0, 0L);
				if (nVBRq >=0 && nVBRq <=9) 
					m_pAEProps->set_VariableQ(nVBRq);
				SetDirty();
			}
			break;

		case IDC_RADIO_CBR:
		case IDC_RADIO_VBR:
			m_pAEProps->set_Variable(LOWORD(wParam)-IDC_RADIO_CBR);
			SetDirty();
			break;

/*		case IDC_COMBO_CHMOD:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int nChannelMode = SendDlgItemMessage(hwnd, IDC_COMBO_CHMOD, CB_GETCURSEL, 0, 0L);
				DWORD dwChannelMode;

				switch(nChannelMode) 
				{
					case 0: 
						dwChannelMode = MPGA_CHMOD_MONO;
						break;
					case 1:
						dwChannelMode = MPGA_CHMOD_STEREO;
						break;
					case 2:
						dwChannelMode = MPGA_CHMOD_JOINT;
						break;
					case 3:
						dwChannelMode = MPGA_CHMOD_DUAL;
						break;
					default:
						m_pAEProps->get_ChannelMode(&dwChannelMode);
						break;
				}

				m_pAEProps->set_ChannelMode(dwChannelMode);

				SetDirty();
			}
			break;
*/
		case IDC_CHECK_PES:
			m_pAEProps->set_PESOutputEnabled(IsDlgButtonChecked(hwnd, IDC_CHECK_PES));
			SetDirty();
			break;

		case IDC_CHECK_COPYRIGHT:
			m_pAEProps->set_CopyrightFlag(IsDlgButtonChecked(hwnd, IDC_CHECK_COPYRIGHT));
			SetDirty();
			break;

		case IDC_CHECK_ORIGINAL:
			m_pAEProps->set_OriginalFlag(IsDlgButtonChecked(hwnd, IDC_CHECK_ORIGINAL));
			SetDirty();
			break;

		case IDC_CHECK_CRC:
			m_pAEProps->set_CRCFlag(IsDlgButtonChecked(hwnd, IDC_CHECK_CRC));
			SetDirty();
			break;
		}
		return TRUE;

	case WM_DESTROY:
		return TRUE;

	default:
		return FALSE;
	}

	return TRUE;
}

//
// OnApplyChanges
//
HRESULT CMpegAudEncPropertyPage::OnApplyChanges() 
{
	m_pAEProps->get_PESOutputEnabled(&m_dwPES);
	m_pAEProps->get_Bitrate(&m_dwBitrate);
	m_pAEProps->get_Variable(&m_dwVariable);
	m_pAEProps->get_VariableMin(&m_dwMin);
	m_pAEProps->get_VariableMax(&m_dwMax);
	m_pAEProps->get_Quality(&m_dwQuality);
	m_pAEProps->get_VariableQ(&m_dwVBRq);
	m_pAEProps->get_SampleRate(&m_dwSampleRate);
	m_pAEProps->get_ChannelMode(&m_dwChannelMode);
	m_pAEProps->get_CRCFlag(&m_dwCRC);
	m_pAEProps->get_CopyrightFlag(&m_dwCopyright);
	m_pAEProps->get_OriginalFlag(&m_dwOriginal);
	m_pAEProps->SaveAudioEncoderPropertiesToRegistry();

	return S_OK; 
}

//
// Initialize dialogbox controls with proper values
//
void CMpegAudEncPropertyPage::InitPropertiesDialog(HWND hwndParent)
{
	EnableControls(hwndParent, TRUE);

	m_hwndQuality = GetDlgItem(hwndParent,IDC_SLIDER_QUALITY);
	DWORD dwQuality;
	m_pAEProps->get_Quality(&dwQuality);
    SendDlgItemMessage(hwndParent, IDC_SLIDER_QUALITY, TBM_SETRANGE, 1, MAKELONG (2,9));
	SendDlgItemMessage(hwndParent, IDC_SLIDER_QUALITY, TBM_SETPOS, 1, dwQuality);
	if (dwQuality>=0 && dwQuality<10)
		SetDlgItemText(hwndParent,IDC_TEXT_QUALITY,szQualityDesc[dwQuality]);

	//
	// initialize sample rate selection
	//
	DWORD dwSourceSampleRate;
	m_pAEProps->get_SourceSampleRate(&dwSourceSampleRate);
	/*	switch (dwSourceSampleRate) 
	{
		case 48000: 
		{	
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"48 kHz");
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"24 kHz");
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"16 kHz");
			break; 
		}
		case 44100: 
		{
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"44.1 kHz");
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"22.05 kHz");
			break; 
		}
		case 32000: 
		{
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"32 kHz");
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"16 kHz");
			break; 
		}
		case 24000:	
		{
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"24 kHz");
			break; 
		}
		case 22050:	
		{
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"22.05 kHz");
			break; 
		}
		case 16000:	
		{
			SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"16 kHz");
			break; 
		}
	}*/


	SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_RESETCONTENT, 0, 0L);
	for (int i = 0; i<6; i++)
		SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)srRates[i].lpSampleRate);
	DWORD dwSampleRate;
	m_pAEProps->get_SampleRate(&dwSampleRate);
	m_pAEProps->set_SampleRate(dwSampleRate);
	
	//int nSF = (dwSourceSampleRate/dwSampleRate - 1);
	//if (nSF < 0)
	//	nSF = 0;
	
	int nSR = 0;
	while (dwSampleRate != srRates[nSR].dwSampleRate && nSR<6){
		nSR++;
	}
	SendDlgItemMessage(hwndParent, IDC_COMBO_SAMPLE_RATE, CB_SETCURSEL, nSR, 0);


	DWORD dwChannels;
	m_pAEProps->get_SourceChannels(&dwChannels);

	/*
	SendDlgItemMessage(hwndParent, IDC_COMBO_CHMOD, CB_RESETCONTENT, 0, 0L);
	if(dwChannels == 2)
	{	for(int i = 0; i < 4; i++)
			SendDlgItemMessage(hwndParent, IDC_COMBO_CHMOD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)chChMode[i]);
	}
	else
		SendDlgItemMessage(hwndParent, IDC_COMBO_CHMOD, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)chChMode[0]);
	*/
//
//initialize VBRq combo box
//
    int k;
	SendDlgItemMessage(hwndParent, IDC_COMBO_VBRq, CB_RESETCONTENT, 0, 0);
		for (k=0; k<10; k++)
		SendDlgItemMessage(hwndParent, IDC_COMBO_VBRq, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szVBRqDesc[k]);
	DWORD dwVBRq;
	m_pAEProps->get_VariableQ(&dwVBRq);
	if (dwVBRq<0)
		dwVBRq = 0;
	if (dwVBRq>9)
		dwVBRq = 9;
	m_pAEProps->set_VariableQ(dwVBRq);
	SendDlgItemMessage(hwndParent, IDC_COMBO_VBRq, CB_SETCURSEL, dwVBRq, 0);

//////////////////////////////////////
	// initialize CBR selection
//////////////////////////////////////
	int nSt;

	SendDlgItemMessage(hwndParent, IDC_COMBO_CBR, CB_RESETCONTENT, 0, 0);
	if (dwSampleRate >= 32000)
	{	
		// If target sampling rate is less than 32000, consider
		// MPEG 1 audio
		nSt = 0;
		for (int i=0; i<14 ;i++)
			SendDlgItemMessage(hwndParent, IDC_COMBO_CBR, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBitRateString[0][i]);
	}
	else
	{	
		// Consider MPEG 2 audio
		nSt = 1;
		for (int i=0; i<14 ;i++)
			SendDlgItemMessage(hwndParent, IDC_COMBO_CBR, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBitRateString[1][i]);
	}

	DWORD dwBitrate;
	m_pAEProps->get_Bitrate(&dwBitrate);

	int nBitrateSel = 0;
	// BitRateValue[][i] is in ascending order
	// We use this fact. We also know there are 14 bitrate values available.
	// We are going to use the closest possible, so we can limit loop with 13
	while (nBitrateSel<13 && dwBitRateValue[nSt][nBitrateSel] < dwBitrate)
		nBitrateSel++;
	SendDlgItemMessage(hwndParent, IDC_COMBO_CBR, CB_SETCURSEL, nBitrateSel, 0);
	
	// check if the specified bitrate is found exactly and correct if not
	if (dwBitRateValue[nSt][nBitrateSel] != dwBitrate)
	{
		dwBitrate = dwBitRateValue[nSt][nBitrateSel];
		// we can change it, because it is independent of any other parameters
		// (but depends on some of them!)
		m_pAEProps->set_Bitrate(dwBitrate);
	}

	//
	// Check VBR/CBR radio button
	//
	DWORD dwVariable;
	m_pAEProps->get_Variable(&dwVariable);
/*
	if (dwVariable)
		SendDlgItemMessage(hwndParent, IDC_RADIO_VBR, BM_SETCHECK, 1, 0);
	else
		SendDlgItemMessage(hwndParent, IDC_RADIO_CBR, BM_SETCHECK, 1, 0);
*/
	CheckRadioButton(hwndParent,IDC_RADIO_CBR,IDC_RADIO_VBR,IDC_RADIO_CBR+dwVariable);

//////////////////////////////////////////////////
	// initialize VBR selection
//////////////////////////////////////////////////
	//VBRMIN, VBRMAX
	int j, nST;

	SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMIN, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMAX, CB_RESETCONTENT, 0, 0);

	if (dwSampleRate >= 32000) {
			nST = 0;
			for (j=0; j<14 ;j++) {
				SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMIN, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBitRateString[0][j]);
				SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMAX, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBitRateString[0][j]);
			}
	}
	else {
			nST = 1;
			for (j=0; j<14 ;j++) {
				SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMIN, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBitRateString[1][j]);
				SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMAX, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)szBitRateString[1][j]);
			}
	}

	DWORD dwMin,dwMax;
	m_pAEProps->get_VariableMin(&dwMin);
	m_pAEProps->get_VariableMax(&dwMax);

	int nVariableMinSel = 0;
	int nVariableMaxSel = 0;
	
	// BitRateValue[][i] is in ascending order
	// We use this fact. We also know there are 14 bitrate values available.
	// We are going to use the closest possible, so we can limit loop with 13
	while (nVariableMinSel<13 && dwBitRateValue[nST][nVariableMinSel] < dwMin)
		nVariableMinSel++;
	SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMIN, CB_SETCURSEL, nVariableMinSel, 0);

	while (nVariableMaxSel<13 && dwBitRateValue[nST][nVariableMaxSel] < dwMax)
		nVariableMaxSel++;
	SendDlgItemMessage(hwndParent, IDC_COMBO_VBRMAX, CB_SETCURSEL, nVariableMaxSel, 0);

	
	// check if the specified bitrate is found exactly and correct if not
	if (dwBitRateValue[nST][nVariableMinSel] != dwMin)
	{
		dwMin = dwBitRateValue[nST][nVariableMinSel];
		// we can change it, because it is independent of any other parameters
		// (but depends on some of them!)
		m_pAEProps->set_VariableMin(dwMin);
	}

	// check if the specified bitrate is found exactly and correct if not
	if (dwBitRateValue[nST][nVariableMaxSel] != dwMax)
	{
		dwMax = dwBitRateValue[nST][nVariableMaxSel];
		// we can change it, because it is independent of any other parameters
		// (but depends on some of them!)
		m_pAEProps->set_VariableMax(dwMax);
	}
	
	
	//
	// initialize checkboxes
	//
	DWORD dwPES;
	m_pAEProps->get_PESOutputEnabled(&dwPES);

	CheckDlgButton(hwndParent, IDC_CHECK_PES, dwPES ? BST_CHECKED : BST_UNCHECKED);
/*
	DWORD dwChannelMode;
	m_pAEProps->get_ChannelMode(&dwChannelMode);

	int iChannelMode;
	switch (dwChannelMode)
	{
	case MPGA_CHMOD_MONO:
		iChannelMode = 0;
		break;
	case MPGA_CHMOD_STEREO:
		iChannelMode = 1;
		break;
	case MPGA_CHMOD_JOINT:
		iChannelMode = 2;
		break;
	case MPGA_CHMOD_DUAL:
		iChannelMode = 3;
		break;
	default:
		iChannelMode = 0;
		break;
	}
	SendDlgItemMessage(hwndParent, IDC_COMBO_CHMOD, CB_SETCURSEL, iChannelMode, 0);
*/
	DWORD dwCRC;
	m_pAEProps->get_CRCFlag(&dwCRC);
	CheckDlgButton(hwndParent, IDC_CHECK_CRC, dwCRC ? BST_CHECKED : BST_UNCHECKED);

	DWORD dwCopyright;
	m_pAEProps->get_CopyrightFlag(&dwCopyright);
	CheckDlgButton(hwndParent, IDC_CHECK_COPYRIGHT, dwCopyright ? BST_CHECKED : BST_UNCHECKED);

	DWORD dwOriginal;
	m_pAEProps->get_OriginalFlag(&dwOriginal);
	CheckDlgButton(hwndParent, IDC_CHECK_ORIGINAL, dwOriginal ? BST_CHECKED : BST_UNCHECKED);
}


////////////////////////////////////////////////////////////////
// EnableControls
////////////////////////////////////////////////////////////////
void CMpegAudEncPropertyPage::EnableControls(HWND hwndParent, bool bEnable)
{   
	EnableWindow(GetDlgItem(hwndParent, IDC_CHECK_PES), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_RADIO_CBR), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_COMBO_CBR), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_RADIO_VBR), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_COMBO_VBRMIN), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_COMBO_VBRMAX), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_CHECK_COPYRIGHT), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_CHECK_ORIGINAL), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_CHECK_CRC), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_SLIDER_QUALITY), bEnable);
	EnableWindow(GetDlgItem(hwndParent, IDC_COMBO_SAMPLE_RATE), bEnable);
}

//
// SetDirty
//
// notifies the property page site of changes

void CMpegAudEncPropertyPage::SetDirty()
{
    m_bDirty = TRUE;
    if (m_pPageSite)
        m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
}

