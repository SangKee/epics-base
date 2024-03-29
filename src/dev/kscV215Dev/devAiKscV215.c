/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAiKscV215.c */
/* base/src/dev $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            09-02-92
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<link.h>
#include	<module_types.h>
#include	<aiRecord.h>

#include	<drvKscV215.h>


static long init_ai();
static long ai_ioinfo();
static long read_ai();
static long ai_lincvt();


typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;} AIDSET;


AIDSET devAiKscV215={6,NULL,NULL,init_ai,ai_ioinfo,read_ai,ai_lincvt};

static long init_ai( struct aiRecord	*pai)
{
    unsigned short value;
    struct vmeio *pvmeio;
    long  status;

    /* ai.inp must be an VME_IO */
    switch (pai->inp.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAt5Vxi (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    pvmeio = (struct vmeio *)&(pai->inp.value);
    if(status=KscV215_ai_driver(pvmeio->card,pvmeio->signal,&value)) {
	recGblRecordError(status,(void *)pai,
		"devAiKscV215 (init_record) KscV215_ai_driver error");
	return(status);
    }
    return(0);
}

static long ai_ioinfo(
    int               cmd,
    struct aiRecord     *pai,
    IOSCANPVT		*ppvt)
{
    return KscV215_getioscanpvt(pai->inp.value.vmeio.card,ppvt);
}

static long read_ai(struct aiRecord	*pai)
{
	struct vmeio 	*pvmeio;
	long		status;
	unsigned short 	value;

	pvmeio = (struct vmeio *)&(pai->inp.value);
	status = KscV215_ai_driver(pvmeio->card,pvmeio->signal,&value);
	if(status == 0){
		pai->rval = value & 0xfff;
	}
	else{
                recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long ai_lincvt(struct aiRecord	*pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}
