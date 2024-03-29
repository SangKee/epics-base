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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include"server.h"

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
epicsShareFunc casAsyncIO::~casAsyncIO()
{
}

//
// casAsyncIO::destroy()
// (default is a normal delete)
//
epicsShareFunc void casAsyncIO::destroy()
{
	delete this;
}

