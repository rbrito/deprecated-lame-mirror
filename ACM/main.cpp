/************************************************************************
Project               : generic Windows ACM driver
File version          : 0.8

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

/// The ACM is considered as a driver and run in Kernel-Mode
/// So the new/delete operators have to be overriden in order to use memory
/// readable out of the calling process

void * operator new( unsigned int cb )
{
	return LocalAlloc(LPTR, cb); // VirtualAlloc
}

void operator delete(void *block) {
	LocalFree(block);
}

#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>

#include <assert.h>

#include "ACM.h"
#include "resource.h"
#include "adebug.h"
#include "version.h"

#define ACM_VERSION "0.6.2"

ADbg * debug = NULL;

LONG WINAPI DriverProc(DWORD dwDriverId, HDRVR hdrvr, UINT msg, LONG lParam1, LONG lParam2)
{

	switch (msg)
	{
		case DRV_OPEN: //acmDriverOpen
		{
			if (debug == NULL) {
				debug = new ADbg(DEBUG_LEVEL_CREATION);
				debug->setPrefix("LAMEdrv");
			}

			if (debug != NULL)
			{
				// Sent when the driver is opened.
				if (lParam2 != NULL)
					debug->OutPut(DEBUG_LEVEL_MSG, "DRV_OPEN (ID 0x%08X), pDesc = 0x%08X",dwDriverId,lParam2);
				else
					debug->OutPut(DEBUG_LEVEL_MSG, "DRV_OPEN (ID 0x%08X), pDesc = NULL",dwDriverId);
			}

			if (lParam2 != NULL) {
				LPACMDRVOPENDESC pDesc = (LPACMDRVOPENDESC)lParam2;

				if (pDesc->fccType != ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC) {
					if (debug != NULL)
					{
						debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "wrong pDesc->fccType (0x%08X)",pDesc->fccType);
					}
					return NULL;
				}
			} else {
				if (debug != NULL)
				{
					debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "pDesc == NULL");
				}
			}

			ACM * ThisACM = new ACM(GetDriverModuleHandle(hdrvr));

			if (debug != NULL)
			{
				debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "OPENED instance 0x%08X",ThisACM);
			}

			return (LONG)ThisACM;// returns 0L to fail
								// value subsequently used
								// for dwDriverId.
		}
		break;

		case DRV_CLOSE:
		{
			if (debug != NULL)
			{
				// Sent when the driver is closed. Drivers are
				// unloaded when the open count reaches zero.
				debug->OutPut(DEBUG_LEVEL_MSG, "DRV_CLOSE");
			}

			ACM * ThisACM = (ACM *)dwDriverId;
			delete ThisACM;
			if (debug != NULL)
			{
				debug->OutPut(DEBUG_LEVEL_FUNC_CODE, "CLOSED instance 0x%08X",ThisACM);
				delete debug;
				debug = NULL;
			}
			return 1L;  // returns 0L to fail
		}
		break;

		case DRV_LOAD:
		{
			// nothing to do
			if (debug != NULL)
			{
				debug->OutPut(DEBUG_LEVEL_MSG, "DRV_LOAD, version %s %s %s", ACM_VERSION, __DATE__, __TIME__);
			}
			return 1L;
		}
		break;

		case DRV_ENABLE:
		{
			// nothing to do
			if (debug != NULL)
			{
				debug->OutPut(DEBUG_LEVEL_MSG, "DRV_ENABLE");
			}
			return 1L;
		}
		break;

		case DRV_DISABLE:
		{
			// nothing to do
			if (debug != NULL)
			{
				debug->OutPut(DEBUG_LEVEL_MSG, "DRV_DISABLE");
			}
			return 1L;
		}
		break;

		case DRV_FREE:
		{
			if (debug != NULL)
			{
				debug->OutPut(DEBUG_LEVEL_MSG, "DRV_FREE");
			}
			return 1L;
		}
		break;

		default:
		{
			ACM * ThisACM = (ACM *)dwDriverId;

			if (ThisACM != NULL)
				return ThisACM->DriverProcedure(hdrvr, msg, lParam1, lParam2);
			else
			{
				if (debug != NULL)
				{
					debug->OutPut(DEBUG_LEVEL_MSG, "Driver not opened, unknown message (0x%08X), lParam1 = 0x%08X, lParam2 = 0x%08X", msg, lParam1, lParam2);
				}

				return DefDriverProc (dwDriverId, hdrvr, msg, lParam1, lParam2);
			}
		}
		break;
	}
}
 
