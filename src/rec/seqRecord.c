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
 * $Id$
 *
 *      Author:	John Winans
 *      Date:	09-21-92
 *
 */
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<lstLib.h>
#include	<string.h>
#include	<memLib.h>
#include	<wdLib.h>
#include	<sysLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<callback.h>

#define GEN_SIZE_OFFSET
#include	<seqRecord.h>
#undef  GEN_SIZE_OFFSET

int	seqRecDebug = 0;

/* Create RSET - Record Support Entry Table*/
static long 	init_record(), process(), asyncFinish(), get_precision();
static void	processCallback(), watchDog();

struct rset seqRSET={
	RSETNUMBER,
	NULL,			/* report */
	NULL,			/* initialize */
	init_record,		/* init_record */
	process,		/* process */
	NULL,			/* special */
	NULL,			/* get_value */
	NULL,			/* cvt_dbaddr */
	NULL,			/* get_array_info */
	NULL,			/* put_array_info */
	NULL,			/* get_units */
	get_precision,		/* get_precision */
	NULL,			/* get_enum_str */
	NULL,			/* get_enum_strs */
	NULL,			/* put_enum_str */
	NULL,			/* get_graphic_double */
	NULL,			/* get_control_double */
	NULL 			/* get_alarm_double */

};

/* Total number of link-groups in a sequence record */
#define NUM_LINKS	10
#define SELN_BIT_MASK	~(0xffff << NUM_LINKS)

/* This is what a link-group looks like in a sequence record */
struct	linkDesc {
  double          dly;	/* Delay value (in seconds) */
  struct link     dol;	/* Where to fetch the input value from */
  double          dov;	/* If dol is CONSTANT, this is the CONSTANT value */
  struct link     lnk;	/* Where to put the value from dol */
};

/* Callback structure used by the watchdog function to queue link processing */
struct	callbackSeq {
  CALLBACK		callBack;	/* used for the callback task */
  WDOG_ID		wd_id;		/* Watchdog udes for delays */
  struct linkDesc *plinks[NUM_LINKS+1]; /* Pointers to links to process */
  int			index;
};

int processNextLink();
/*****************************************************************************
 *
 * Initialize a sequence record.
 *
 * Allocate the callback request structure (tacked on dpvt.)
 * Initialize watch-dog ID
 * Initialize SELN based on the link-type of SELL
 * If SELL is a CA_LINK, inform CA about it
 * For each constant input link, fill in the DOV field
 *
 ******************************************************************************/
static long 
init_record(pseq, pass)
struct seqRecord *pseq;
int pass;
{
    int		index;
    struct  linkDesc      *plink;

    if (pass==0) return(0);

    if (seqRecDebug > 5)
      printf("init_record(%s) entered\n", pseq->name);

    /* Allocate a callback structure for use in processing */
    pseq->dpvt = (void *)malloc(sizeof(struct  callbackSeq));

    ((struct callbackSeq *) (pseq->dpvt))->callBack.callback = processCallback;
    ((struct callbackSeq *) (pseq->dpvt))->callBack.user = (void *) pseq;
    ((struct callbackSeq *) (pseq->dpvt))->wd_id = wdCreate();

    /* Get link selection if sell is a constant and nonzero */
    if (pseq->sell.type==CONSTANT)
    {
      if (seqRecDebug > 5)
        printf("init_record(%s) SELL is a constant\n", pseq->name);
      recGblInitConstantLink(&pseq->sell,DBF_USHORT,&pseq->seln);
    }

  /* Copy over ALL the input link constants here */
  plink = (struct linkDesc *)(&(pseq->dly1));

  index = 0;
  while (index < NUM_LINKS)
  {

    if (plink->dol.type == CONSTANT)
	recGblInitConstantLink(&plink->dol,DBF_DOUBLE,&plink->dov);

    index++;

    plink++;
  }

  return(0);
}
/*****************************************************************************
 *
 * Process a sequence record.
 *
 * If is async completion phase
 *   call asyncFinish() to finish things up
 * else
 *   figure out selection mechanism
 *   build the correct mask value using the mode and the selection value
 *   build a list of pointers to the selected link-group structures
 *   If there are no links to process
 *     call asyncFinish() to finish things up
 *   else
 *     call processNextLink() to schecule a delay for the first link-group
 *
 *
 * NOTE:
 *   dbScanLock is already held for pseq before this function is called.
 *
 *   We do NOT call processNextLink() if there is nothing to do, this prevents
 *   us from calling dbProcess() recursively.
 *
 ******************************************************************************/
static long 
process(pseq)
struct seqRecord *pseq;
{
  struct  callbackSeq	*pcb = (struct callbackSeq *) (pseq->dpvt);
  struct  linkDesc	*plink;
  unsigned short	lmask;
  int			tmp;

  if(seqRecDebug > 10)
    printf("seqRecord: process(%s) pact = %d\n", pseq->name, pseq->pact);

  if (pseq->pact)
  { /* In async completion phase */
    asyncFinish((struct dbCommon *)pseq);
    return(0);
  }
  pseq->pact = TRUE;

  /* Reset the PRIO in case it was changed */
  pcb->callBack.priority = pseq->prio;

  /*
   * We should not bother supporting seqSELM_All or seqSELM_Specified
   * they can both be supported by simply providing seqSELM_Mask.
   * build the proper mask using these other silly modes if necessary.
   */

  if (pseq->selm == seqSELM_All)
  {
    lmask = SELN_BIT_MASK;
  }
  else
  { 
    /* Fill in the SELN field */
    if (pseq->sell.type != CONSTANT)
    {
      dbGetLink(&(pseq->sell), DBR_USHORT, &(pseq->seln), 0,0);
    }
    if  (pseq->selm == seqSELM_Specified)
    {
      if(pseq->seln>10)
      { /* Invalid selection number */
        recGblSetSevr(pseq,SOFT_ALARM,INVALID_ALARM);
        return(asyncFinish(pseq));
      }
      if (pseq->seln == 0)
        return(asyncFinish(pseq));	/* Nothing selected */

      lmask = 1;
      lmask <<= pseq->seln - 1;
    }
    else if (pseq->selm == seqSELM_Mask)
    {
      lmask = (pseq->seln) & SELN_BIT_MASK;
    }
    else
    { /* Invalid selection option */
      recGblSetSevr(pseq,SOFT_ALARM,INVALID_ALARM);
      return(asyncFinish(pseq));
    }
  }
  /* Figure out which links are going to be processed */
  pcb->index = 0;
  plink = (struct linkDesc *)(&(pseq->dly1));
  tmp = 1;
  while (lmask)
  {
    if (seqRecDebug > 4)
      printf("seqRec-process Checking link %d - lnk %d dol %d\n", tmp,
      plink->lnk.type, plink->dol.type);

    if ((lmask & 1) && ((plink->lnk.type != CONSTANT)||(plink->dol.type != CONSTANT)))
    {
      if (seqRecDebug > 4)
	printf("  seqRec-process Adding link %d at index %d\n", tmp, pcb->index);

      pcb->plinks[pcb->index] = plink;
      pcb->index++;
    }
    lmask >>= 1;
    plink++;
    tmp++;
  }
  pcb->plinks[pcb->index] = NULL;	/* mark the bottom of the list */

  if (!pcb->index)
  { /* There was nothing to do, finish record processing here */
    return(asyncFinish(pseq));
  }

  pcb->index = 0;
  /* Start doing the first forward link (We have at least one for sure) */
  processNextLink(pseq);

  return(0);
}
/*****************************************************************************
 *
 * Find the next link-group that needs processing.
 *
 * If there are no link-groups left to process
 *   call bdProcess() to complete the async record processing.
 * else
 *   if the delay is > 0 seconds
 *     schedule the watch dog task to wake us up later
 *   else
 *     invoke the watch-dog wakeup routine now
 *
 *
 * NOTE:
 *   dbScanLock is already held for pseq before this function is called.
 *
 ******************************************************************************/
int processNextLink(pseq)
struct seqRecord *pseq;
{
  struct  callbackSeq   *pcb = (struct callbackSeq *) (pseq->dpvt);
  int			wdDelay;

  if (seqRecDebug > 5)
    printf("processNextLink(%s) looking for work to do, index = %d\n", pseq->name, pcb->index);

  if (pcb->plinks[pcb->index] == NULL)
  {
    /* None left, finish up. */
    (*(struct rset *)(pseq->rset)).process(pseq);
  }
  else
  {
    if (pcb->plinks[pcb->index]->dly > 0.0)
    {
      /* Use the watch-dog as a delay mechanism */
      wdDelay = pcb->plinks[pcb->index]->dly * sysClkRateGet();
      wdStart(pcb->wd_id, wdDelay, (FUNCPTR)watchDog, (int)(&(pcb->callBack)));
    }
    else
    {
      /* No delay, do it now.  Avoid recursion by using the callback task */
      watchDog((int)(&(pcb->callBack)));
    }
  }
  return(0);
}
/*****************************************************************************
 *
 * Finish record processing by posting any events and processing forward links.
 *
 * NOTE:
 *   dbScanLock is already held for pseq before this function is called.
 *
 ******************************************************************************/
static long
asyncFinish(pseq)
struct seqRecord *pseq;
{
  unsigned short MonitorMask;

  if (seqRecDebug > 5)
    printf("asyncFinish(%s) completing processing\n", pseq->name);
  pseq->udf = FALSE;
 
  MonitorMask = recGblResetAlarms(pseq);

  if (MonitorMask)
    db_post_events(pseq, &pseq->val, MonitorMask);

  /* process the forward scan link record */
  recGblFwdLink(pseq);

  recGblGetTimeStamp(pseq);
  /* tsLocalTime(&pseq->time); */
  pseq->pact = FALSE;

  return(0);
}
/*****************************************************************************
 *
 * Schedule the process continuation via the callback tasks.
 *
 * This function is called by the watchdog task when it is time to process the
 * "next" link-group in the sequence record.
 *
 ******************************************************************************/
static void
watchDog(pcallback)
CALLBACK *pcallback;
{
  callbackRequest(pcallback);
  return;
}
/*****************************************************************************
 *
 * Link-group processing function.
 *
 * if the input link is not a constant
 *   call dbGetLink() to get the link value
 * else
 *   get the value from the DOV field
 * call dbPutLink() to forward the value to destination location
 * call processNextLink() to schedule the processing of the next link-group
 *
 * NOTE:
 *   dbScanLock is NOT held for pseq when this function is called!!
 *
 ******************************************************************************/
static void
processCallback(pCallback)
CALLBACK *pCallback;
{
  struct seqRecord *pseq = (struct seqRecord *)(pCallback->user);
  struct  callbackSeq   *pcb = (struct callbackSeq *) (pseq->dpvt);
  double		myDouble;

  dbScanLock((struct dbCommon *)pseq);

  if (seqRecDebug > 5)
    printf("processCallback(%s) processing field index %d\n", pseq->name, pcb->index+1);

  /* Save the old value */
  myDouble = pcb->plinks[pcb->index]->dov;

  dbGetLink(&(pcb->plinks[pcb->index]->dol), DBR_DOUBLE,
	&(pcb->plinks[pcb->index]->dov),0,0);

  /* Dump the value to the destination field */
  dbPutLink(&(pcb->plinks[pcb->index]->lnk), DBR_DOUBLE,
	&(pcb->plinks[pcb->index]->dov),1);

  if (myDouble != pcb->plinks[pcb->index]->dov)
  {
    if (seqRecDebug > 0)
      printf("link %d changed from %f to %f\n", pcb->index, myDouble, pcb->plinks[pcb->index]->dov);
    db_post_events(pseq, &pcb->plinks[pcb->index]->dov, DBE_VALUE|DBE_LOG);
  }
  else
  {
    if (seqRecDebug > 0)
      printf("link %d not changed... %f\n", pcb->index, pcb->plinks[pcb->index]->dov);
  }

  /* Find the 'next' link-seq that is ready for processing. */
  pcb->index++;
  processNextLink(pseq);

  dbScanUnlock((struct dbCommon *)pseq);
  return;
}
/*****************************************************************************
 *
 * Return the precision value from PREC
 *
 *****************************************************************************/
static long
get_precision(paddr, precision)
struct dbAddr *paddr;
long          *precision;
{
  struct	seqRecord	*pseq = (struct seqRecord *) paddr->precord;

  *precision = pseq->prec;

  if(paddr->pfield < (void *)&pseq->val)
    return(0);				/* Field is NOT in dbCommon */

  recGblGetPrec(paddr, precision);	/* Field is in dbCommon */
  return(0);
}
