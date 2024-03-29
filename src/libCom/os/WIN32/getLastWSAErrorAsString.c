/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Author: Jeff Hill
 */

/*
 * ANSI C
 */
#include <stdio.h>

/*
 * WIN32 specific
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

/*
 * this does specify that winsock should be exported,
 * but instead that EPICS winsock specific functions
 * should be exported.
 */
#define epicsExportSharedSymbols
#include "bsdSocketResource.h"

/*
 * getLastWSAErrorAsString()
 */
epicsShareFunc const char * epicsShareAPI getLastWSAErrorAsString()
{
	static char errString[128];
	static int init = 0;


	/*
	 * unfortunately, this does not work ...
	 * and there is no obvious replacement ...
	 */
#if 0
	DWORD W32status;

	W32status = FormatMessage( 
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError (),
		MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
		errString,
		sizeof(errString)/sizeof(errString[0]),
		NULL 
	);

	if (W32status==0) {
		sprintf (errString, "WIN32 Socket Library Error %d", WSAGetLastError ());
	}
	return errString;
#else
	sprintf (errString, "WIN32 Socket Library Error %d", WSAGetLastError ());
	return errString;
#endif
}