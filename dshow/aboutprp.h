//
//  Copyright (c) 2000  ELECARD.  All Rights Reserved.
//
//
//  aboutprp.h
//
//  CMAEAbout

class CMAEAbout : public CBasePropertyPage 
{

public:

    CMAEAbout(LPUNKNOWN lpUnk, HRESULT *phr);
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);

	HRESULT OnActivate();
    HRESULT OnConnect(IUnknown *pUnknown);
    HRESULT OnDisconnect();
    HRESULT OnDeactivate();
    HRESULT OnApplyChanges();
    BOOL OnReceiveMessage(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

	
private:

//    static BOOL CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void    SetDirty(void);

    BOOL	m_fWindowInActive;		// TRUE ==> dialog is in the process of being destroyed

};
