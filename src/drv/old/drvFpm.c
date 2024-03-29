/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvFpm.c */
/* base/src/drv $Id$ */

/*
 * control routines for use with the FP10M fast protect master modules
 *
 *      routines which are used to test and interface with the
 *      FP10S fast protect module
 *
 *      Author:          Matthew Stettler
 *      Date:            6-92
 *
 * Routines:
 *
 *		fpm_init	Finds and initializes FP10M cards
 *		fpm_driver	System interface to FP10M modules
 *		fpm_read	Carrier control readback
 *		fpm_reboot	clean up before soft reboots
 *
 * Daignostic Routines
 *		fpm_en		Enables/disables interrupts (diagnostic enable)
 *		fpm_mode	Sets interrupt reporting mode (logs mode
 *				changes to console)
 *		fpm_cdis	Disables carrier from console
 *		fpm_fail	Sets carrier failure mode
 *		fpm_srd		Reads current carrier status
 *		fpm_write	Command line interface to fpm_driver
 *
 * Routines return:
 *
 *		-1		Nonexistent card
 *		-2		Interrupt connection error
 *		-3		no memory
 *		-4		VME short IO bus nonexistent
 *		0-2		Successfull completion, or # cards found
 *
 */

static char *sccsId = "@(#)drvFpm.c	1.12\t8/4/93";

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <rebootLib.h>
#include <intLib.h>
#include <vxLib.h>
#include <vme.h>
#include <iv.h> /* in h/68k if this is compiling for a 68xxx */
#include "module_types.h"
#include <dbDefs.h>
#include <drvSup.h>

static long report();
static long init();
struct {
        long    number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvFpm={
        2,
        report,
        init};

static long report()
{
        fpm_io_report();
        return(0);
}

static long init()
{
    fpm_init(0);
    return(0);
}

/* general constants */
#define FPM_INTLEV	5		/* interrupt level */

/* control register bit definitions */
#define CR_CDIS		0x1		/* software carrier disable */
#define CR_FS0		0x2		/* fail select 0 */
#define CR_FS1		0x4		/* fail select 1 */
#define CR_FS2		0x8		/* fail select 2 */
#define CR_I0		0x10		/* interrupt level bit 0 */
#define CR_I1		0x20		/* interrupt level bit 1 */
#define CR_I2		0x40		/* interrupt level bit 2 */
#define CR_IEN		0x80		/* interrupt enable */

/* control register mask definitions */
#define CR_IM		0x70		/* interrupt level mask */

/* status register bit definitions */
#define SR_S0		0x1		/* error sequencer state bit 0 */
#define SR_S1		0x2		/* error sequencer state bit 1 */
#define SR_S2		0x3		/* error sequencer state bit 2 */

/* status register mask definitions */
#define SR_EM		0x7		/* error state mask */

/* operating modes */
#define FPM_NMSG	0		/* no messages to console */
#define FPM_TMSG	1		/* terse messages to console */
#define FPM_FMSG	2		/* full messages to console */

/* register address map for FP10M */
struct fp10m
	{
	unsigned short cr;		/* control register */
	unsigned short sr;		/* status register */
	unsigned short ivec;		/* interrupt vector */
	char end_pad[0xff-0x6];		/* pad to 256 byte boundary */
	};

/* control structure */
struct fpm_rec
	{
	struct fp10m *fmptr;		/* pointer to device registers */
	short type;			/* device type */
	short num;			/* board number */
	short vector;			/* interrupt vector */
	short mode;			/* operating mode */
	unsigned int int_num;		/* interrupt number */
	};

static struct fpm_rec *fpm;		/* fast protect control structure */

static int fpm_num;				/* # cards found - 1 */

static void 	fpm_reboot();

/*
 * fpm_int
 *
 * interrupt service routine
 *
 */
int fpm_int(ptr)
 register struct fpm_rec *ptr;
{
 register struct fp10m *regptr;

 regptr = ptr->fmptr;
 switch (ptr->mode)
  {
  case FPM_TMSG:
   logMsg("fast protect master interrupt!\n");
   break;
  case FPM_FMSG:
   logMsg("fast protect master interrupt!\n");
   logMsg("cr = %x sr = %x\n",regptr->cr,regptr->sr & 0x7);
   break;
  }
 ptr->int_num++;
 return(0);
}
/*
 * fpm_init
 *
 * initialization for fp10m fast protect master modules
 *
 */
int fpm_init(addr)
 unsigned int addr;
{
 int i;
 short junk;
 short intvec = AT8FPM_IVEC_BASE;
 struct fp10m *ptr;
 int status;

 fpm = (struct fpm_rec *) calloc(
				bo_num_cards[AT8_FP10M_BO],
				sizeof(*fpm));
 if(!fpm){
	return -3;
 }

 if(!addr){
        addr = bo_addrs[AT8_FP10M_BO];
 }

 status = sysBusToLocalAdrs(
                        VME_AM_SUP_SHORT_IO,
                        addr,
                        &ptr);
 if(status<0){
        logMsg("VME shrt IO addr err in the master fast protect driver\n");
        return -4;
 }

 status = rebootHookAdd(fpm_reboot);
 if(status<0){
        logMsg("%s: reboot hook add failed\n", __FILE__);
 }

 for (i = 0; (i < bo_num_cards[AT8_FP10M_BO]) && (vxMemProbe(ptr,READ,2,&junk) == OK);
      i++,ptr++)
  {
  /*
  register initialization
  */
  ptr->cr = 0x00;		/* disable interface */
  fpm[i].fmptr = ptr;		/* hardware location */
  fpm[i].vector = intvec++;	/* interrupt vector */
  ptr->ivec = fpm[i].vector;	/* load vector */
  fpm[i].mode = FPM_NMSG;	/* set default mode (no messages) */
  fpm[i].int_num = 0;		/* initialize interrupt number */
  fpm[i].type = 2;		/* board type */
  fpm[i].num = i;		/* board number */
  /*
  set up interrupt handler
  */
  ptr->cr |= FPM_INTLEV<<4;	/* set up board for level 5 interrupt */
  if (intConnect(INUM_TO_IVEC(fpm[i].vector),fpm_int,&fpm[i]) != OK)
   return -2;			/* abort if can't connect */
    sysIntEnable(FPM_INTLEV);
  }
 fpm_num = i - 1;		/* record last card # */
 return i;			/* return # cards found */
}


/*
 *
 * fpm_reboot()
 *
 * turn off interrupts to avoid ctrl X reboot problems
 */
LOCAL
void    fpm_reboot()
{
        int i;

	if(!fpm){
		return;
	}

        for (i = 0; i < bo_num_cards[AT8_FP10M_BO]; i++){

                if(!fpm[i].fmptr){
                        continue;
                }

                fpm[i].fmptr->cr &= ~CR_IEN;
        }
}

/*
 * fpm_en
 *
 * interrupt enable/disable
 * (toggles the int enable state - joh)
 *
 */
int fpm_en(card)
 short card;
{
 if (card < 0 || (card > fpm_num))
  return -1;
 fpm[card].fmptr->cr ^= CR_IEN;
 if (fpm[card].fmptr->cr & CR_IEN)
  printf("fast protect master interrupts enabled\n");
 else
  printf("fast protect master interrupts disabled\n");
 return 0;
}
/*
 * fpm_mode
 *
 * set interrupt reporting mode
 *
 */
int fpm_mode(card,mode)
 short card, mode;
{
 if (card < 0 || (card > fpm_num))
  return -1;
 fpm[card].mode = mode;
 return 0;
}
/*
 * fpm_cdis
 *
 * carrier disable (1), enable (0)
 *
 */
int fpm_cdis(card,disable)
 short card, disable;
{
 unsigned short temp;

 if (card < 0 || (card > fpm_num))
  return -1;
 temp = fpm[card].fmptr->cr;
 temp &= 0xfe;
 temp |= (disable & 0x01);
 fpm[card].fmptr->cr = temp;
 return 0;
}
/*
 * fpm_fail
 *
 * set failure mode
 *
 */
int fpm_fail(card,mode)
 short card, mode;
{
 unsigned short temp;

 if (card < 0 || (card > fpm_num))
  return -1;
 temp = fpm[card].fmptr->cr;
 temp &= 0xf1;
 temp |= (mode & 0x7)<<1;
 fpm[card].fmptr->cr = temp;
 return 0;
}
/*
 * fpm_srd
 *
 * read status bits
 *
 */
int fpm_srd(card)
 short card;
{
 if (card < 0 || ( card > fpm_num))
  return -1;
 return fpm[card].fmptr->sr & 0x7;
}
/*
 * fpm_driver
 *
 * epics interface to fast protect master
 *
 */
int fpm_driver(card,mask,prval)
register unsigned short card;
unsigned int mask;
register unsigned int prval;
{
	register unsigned int temp;

	if (card > fpm_num)
		return -1;
	temp = fpm[card].fmptr->cr;
	fpm[card].fmptr->cr = (temp & (~mask | 0xf0)) | ((prval & mask) & 0xf);
	return 0;
}
/*
 * fpm_write
 *
 * command line interface to fpm_driver
 *
 */
int fpm_write(card,val)
 short card;
 unsigned int val;
{
 return fpm_driver(card,0xffffffff,val);
}
/*
 * fpm_read
 *
 * read the current control register contents (readback)
 *
 */
int fpm_read(card,mask,pval)
register unsigned short card;
unsigned int mask;
register unsigned int *pval;
{
	if (card > fpm_num)
  		return -1;
	*pval = fpm[card].fmptr->cr & 0x000f;
	return 0;
}



/*
 * fpm_io_report()
 *
 */
int fpm_io_report(level)
int	level;
{
	int	i;

	for(i=0; i<=fpm_num; i++){
		printf("BO: AT8-FP-M:    card %d\n", i);
	}
	return(0);
}
