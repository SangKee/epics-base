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
 *	$Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 * Revision 1.4  1998/02/05 22:51:34  jhill
 * include resourceLib.h
 *
 * Revision 1.3  1997/08/05 00:47:02  jhill
 * fixed warnings
 *
 * Revision 1.2  1997/04/10 19:33:58  jhill
 * API changes
 *
 * Revision 1.1  1996/11/02 01:01:05  jhill
 * installed
 *
 *
 *
 */

//
// EPICS
// (some of these are included from casdef.h but
// are included first here so that they are included
// once only before epicsExportSharedSymbols is defined)
//
#include "alarm.h" // EPICS alarm severity/condition 
#include "errMdef.h" // EPICS error codes 
#include "gdd.h" // EPICS data descriptors 
#include "resourceLib.h" // EPICS hashing templates

#define epicsExportSharedSymbols
#include "casdef.h"

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncReadIO::~casAsyncReadIO()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncWriteIO::~casAsyncWriteIO()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncPVExistIO::~casAsyncPVExistIO()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncPVCreateIO::~casAsyncPVCreateIO()
{
}

