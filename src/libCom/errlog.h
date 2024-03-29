/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* src/libCom/errlog.h */

#ifndef INCerrlogh
#define INCerrlogh



#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#define epicsPrintUseProtoANSI
#endif

#ifdef __STDC__
#ifndef epicsPrintUseProtoANSI
#define epicsPrintUseProtoANSI
#endif
#endif

#ifdef epicsPrintUseProtoANSI
#	include <stdarg.h>
#else
#	include <varargs.h>
#endif

/* define errMessage with a macro so we can print the file and line number*/
#define errMessage(S, PM) \
         errPrintf(S, __FILE__, __LINE__, PM)
/* epicsPrintf and epicsVprintf old versions of errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

#ifdef __STDC__
typedef void(*errlogListener) (const char *message);
#else
typedef void(*errlogListener) ();
#endif
typedef enum {errlogInfo,errlogMinor,errlogMajor,errlogFatal} errlogSevEnum;

#ifdef ERRLOG_INIT
epicsShareDef char * errlogSevEnumString[] = {"info","minor","major","fatal"};
#else
epicsShareExtern char * errlogSevEnumString[];
#endif

#ifdef epicsPrintUseProtoANSI

epicsShareFunc int epicsShareAPIV errlogPrintf(
    const char *pformat, ...);
epicsShareFunc int epicsShareAPIV errlogVprintf(
    const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPIV errlogSevPrintf(
    const errlogSevEnum severity,const char *pformat, ...);
epicsShareFunc int epicsShareAPIV errlogSevVprintf(
    const errlogSevEnum severity,const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPI errlogMessage(
	const char *message);

epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity);
epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity );
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog(void);

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener);
epicsShareFunc void epicsShareAPI errlogRemoveListener(
    errlogListener listener);

epicsShareFunc int epicsShareAPI eltc(int yesno);
epicsShareFunc int epicsShareAPI errlogInit(int bufsize);

/*other routines that write to log file*/
epicsShareFunc void epicsShareAPIV errPrintf(long status, const char *pFileName,
    int lineno, const char *pformat, ...);

/* The following are added so that logMsg on vxWorks does not cause
 * the message to appear twice on the console
 */
epicsShareFunc int errlogPrintfNoConsole(
    const char *pformat, ...);
epicsShareFunc int errlogVprintfNoConsole(
    const char *pformat,va_list pvar);

#else /* not epicsPrintUseProtoANSI */
epicsShareFunc int epicsShareAPI errlogPrintf();
epicsShareFunc int epicsShareAPI errlogVprintf();
epicsShareFunc int epicsShareAPI errlogSevPrintf();
epicsShareFunc int epicsShareAPI errlogSevVprintf();
epicsShareFunc int epicsShareAPI errlogMessage();
epicsShareFunc char * epicsShareAPI errlogGetSevEnumString();
epicsShareFunc void epicsShareAPI errlogSetSevToLog();
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog();
epicsShareFunc void epicsShareAPI errlogAddListener();
epicsShareFunc void epicsShareAPI errlogRemoveListener();
epicsShareFunc void epicsShareAPI eltc();
epicsShareFunc void epicsShareAPI errlogInit();
epicsShareFunc void epicsShareAPI errPrintf();
#endif /* ifdef epicsPrintUseProtoANSI */

#ifdef __cplusplus
}
#endif

#endif /*INCerrlogh*/
