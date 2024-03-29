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
 * Revision 1.10  1998/07/08 15:38:06  jhill
 * fixed lost monitors during flow control problem
 *
 * Revision 1.9  1998/06/16 02:29:57  jhill
 * use smart gdd ptr
 *
 * Revision 1.8  1997/05/05 04:50:11  jhill
 * moved pLog = NULL down
 *
 * Revision 1.7  1997/04/10 19:34:12  jhill
 * API changes
 *
 * Revision 1.6  1996/11/02 00:54:18  jhill
 * many improvements
 *
 * Revision 1.5  1996/09/16 18:24:03  jhill
 * vxWorks port changes
 *
 * Revision 1.4  1996/07/24 22:00:49  jhill
 * added pushOnToEventQueue()
 *
 * Revision 1.3  1996/07/01 19:56:11  jhill
 * one last update prior to first release
 *
 * Revision 1.2  1996/06/26 21:18:56  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#include "server.h"
#include "casChannelIIL.h" // casChannelI inline func
#include "casEventSysIL.h" // casEventSys inline func
#include "casMonEventIL.h" // casMonEvent inline func
#include "casCtxIL.h" // casCtx inline func


//
// casMonitor::casMonitor()
//
casMonitor::casMonitor(caResId clientIdIn, casChannelI &chan,
	unsigned long nElemIn, unsigned dbrTypeIn,
	const casEventMask &maskIn, osiMutex &mutexIn) :
	nElem(nElemIn),
	mutex(mutexIn),
	ciu(chan),
	mask(maskIn),
	clientId(clientIdIn),
	dbrType(dbrTypeIn),
	nPend(0u),
	ovf(FALSE),
	enabled(FALSE)
{
	//
	// If these are nill it is a programmer error
	//
	assert (&this->ciu);

	this->ciu.addMonitor(*this);

	this->enable();
}

//
// casMonitor::~casMonitor()
//
casMonitor::~casMonitor()
{
	casCoreClient &client = this->ciu.getClient();
	
	this->mutex.osiLock();
	
	this->disable();
	
	//
	// remove from the event system
	//
	if (this->ovf) {
		client.removeFromEventQueue (this->overFlowEvent);
	}

	this->ciu.deleteMonitor(*this);
	
	this->mutex.osiUnlock();
}

//
// casMonitor::enable()
//
void casMonitor::enable()
{
	caStatus status;
	
	this->mutex.osiLock();
	if (!this->enabled && this->ciu->readAccess()) {
		this->enabled = TRUE;
		status = this->ciu.getPVI().registerEvent();
		if (status) {
			errMessage(status,
				"Server tool failed to register event\n");
		}
	}
	this->mutex.osiUnlock();
}

//
// casMonitor::disable()
//
void casMonitor::disable()
{
	this->mutex.osiLock();
	if (this->enabled) {
		this->enabled = FALSE;
		this->ciu.getPVI().unregisterEvent();
	}
	this->mutex.osiUnlock();
}

//
// casMonitor::push()
//
void casMonitor::push(gdd &newValue)
{
	casCoreClient	&client = this->ciu.getClient();
	casMonEvent 	*pLog;
	char			full;
	
	this->mutex.osiLock();
	
	//
	// get a new block if we havent exceeded quotas
	//
	full = (this->nPend>=individualEventEntries) 
		|| client.casEventSys::full();
	if (!full) {
		pLog = new casMonEvent(*this, newValue);
		if (pLog) {
			this->nPend++;
		}
	}
	else {
		pLog = NULL;
	}
	
	if (this->ovf) {
		if (pLog) {
			//
			// swap values
			// (ugly - but avoids purify ukn sym type problem)
			// (better to create a temp event object)
			//
			smartGDDPointer pValue = this->overFlowEvent.getValue();
			assert (pValue!=NULL);
			this->overFlowEvent = *pLog;
			pLog->assign(*this, pValue);
			client.insertEventQueue(*pLog, this->overFlowEvent);
		}
		else {
			//
			// replace the value with the current one
			//
			this->overFlowEvent.assign(*this, &newValue);
		}
		client.removeFromEventQueue(this->overFlowEvent);
		pLog = &this->overFlowEvent;
	}
	else if (!pLog) {
		//
		// no log block
		// => use the over flow block in the event structure
		//
		this->ovf = TRUE;
		this->overFlowEvent.assign(*this, &newValue);
		this->nPend++;
		pLog = &this->overFlowEvent;
	}
	
	client.addToEventQueue(*pLog);
	
	this->mutex.osiUnlock();
}

//
// casMonitor::executeEvent()
//
caStatus casMonitor::executeEvent(casMonEvent *pEV)
{
	caStatus status;
	smartGDDPointer pVal;
	
	pVal = pEV->getValue ();
	assert (pVal!=NULL);
	
	this->mutex.osiLock();
	status = this->callBack (*pVal);
	this->mutex.osiUnlock();
	
	//
	// if the event isnt accepted we will try
	// again later (and the event returns to the queue)
	//
	if (status) {
		return status;
	}
	
	//
	// decrement the count of the number of events pending
	//
	this->nPend--;
	
	//
	// delete event object if it isnt a cache entry
	// saved in the call back object
	//
	if (pEV == &this->overFlowEvent) {
		assert (this->ovf==TRUE);
		this->ovf = FALSE;
		pEV->clear();
	}
	else {
		delete pEV;
	}
	
	return S_cas_success;
}

//
// casMonitor::show(unsigned level)
//
void casMonitor::show(unsigned level) const
{
        if (level>1u) {
                printf(
"\tmonitor type=%u count=%lu client id=%u enabled=%u OVF=%u nPend=%u\n",
                        dbrType, nElem, clientId, enabled, ovf, nPend);
		this->mask.show(level);
        }
}

