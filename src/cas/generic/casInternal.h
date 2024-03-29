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
 * Revision 1.18.2.2  2000/07/11 01:30:09  jhill
 * fixed DLL export for the Borlund build
 *
 * Revision 1.18.2.1  2000/07/06 15:21:34  jhill
 * added DLL export to destructor
 *
 * Revision 1.18  1998/11/18 18:52:49  jhill
 * fixed casChannelI undefined symbols on WIN32
 *
 * Revision 1.17  1998/10/23 00:28:20  jhill
 * fixed HP-UX warnings
 *
 * Revision 1.16  1998/07/08 15:38:05  jhill
 * fixed lost monitors during flow control problem
 *
 * Revision 1.15  1998/06/16 02:26:09  jhill
 * allow PVs to exist without a server
 *
 * Revision 1.14  1998/02/05 22:56:12  jhill
 * cosmetic
 *
 * Revision 1.13  1997/08/05 00:47:09  jhill
 * fixed warnings
 *
 * Revision 1.12  1997/06/13 09:15:58  jhill
 * connect proto changes
 *
 * Revision 1.11  1997/04/10 19:34:10  jhill
 * API changes
 *
 * Revision 1.10  1997/01/10 21:17:55  jhill
 * code around gnu g++ inline bug when -O isnt used
 *
 * Revision 1.9  1996/12/06 22:33:49  jhill
 * virtual ~casPVI(), ~casPVListChan(), ~casChannelI()
 *
 * Revision 1.8  1996/11/02 00:54:14  jhill
 * many improvements
 *
 * Revision 1.7  1996/09/16 18:24:02  jhill
 * vxWorks port changes
 *
 * Revision 1.6  1996/09/04 20:21:41  jhill
 * removed operator -> and added member pv
 *
 * Revision 1.5  1996/07/01 19:56:11  jhill
 * one last update prior to first release
 *
 * Revision 1.4  1996/06/26 23:32:17  jhill
 * changed where caProto.h comes from (again)
 *
 * Revision 1.3  1996/06/26 21:18:54  jhill
 * now matches gdd api revisions
 *
 * Revision 1.2  1996/06/20 18:08:35  jhill
 * changed where caProto.h comes from
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

//
// EPICS
//
#include "tsDLList.h"
#include "resourceLib.h"
#include "caProto.h"
#include "smartGDDPointer.h"

typedef aitUint32 caResId;

class casEventSys;
class casChannelI;

//
// casEvent
//
class casEvent : public tsDLNode<casEvent> {
public:
        epicsShareFunc virtual ~casEvent();
        virtual caStatus cbFunc(casEventSys &)=0;
private:
};

class casChanDelEv : public casEvent {
public:
	casChanDelEv(caResId idIn) : id(idIn) {}
	~casChanDelEv();
	caStatus cbFunc(casEventSys &);
private:
	caResId	id;
};

enum casResType {casChanT=1, casClientMonT, casPVT};
 
class casRes : public uintRes<casRes>
{
public:
	epicsShareFunc virtual ~casRes();
	virtual casResType resourceType() const = 0;
	virtual void show (unsigned level) const = 0;
	virtual void destroy() = 0;
private:
};

class ioBlockedList;
class osiMutex;

//
// ioBlocked
//
class ioBlocked : public tsDLNode<ioBlocked> {
friend class ioBlockedList;
public:
	ioBlocked ();
	virtual ~ioBlocked ();
private:
	ioBlockedList	*pList;
	virtual void ioBlockedSignal ();
};

//
// ioBlockedList
//
class ioBlockedList : private tsDLList<ioBlocked> {
public:
	ioBlockedList ();
	epicsShareFunc virtual ~ioBlockedList ();
	void signal ();
	void removeItemFromIOBLockedList(ioBlocked &item);
	void addItemToIOBLockedList(ioBlocked &item);
};
 
class casMonitor;

//
// casMonEvent
//
class casMonEvent : public casEvent {
public:
	//
	// only used when this is part of another structure
	// (and we need to postpone true construction)
	//
	inline casMonEvent ();
	inline casMonEvent (casMonitor &monitor, gdd &newValue);
	inline casMonEvent (const casMonEvent &initValue);

	//
	// ~casMonEvent ()
	// (not inline because this is virtual in the base class)
	//
	~casMonEvent ();

	caStatus cbFunc(casEventSys &);

	//
	// casMonEvent::getValue()
	//
	inline gdd *getValue() const;

	inline void operator = (const class casMonEvent &monEventIn);
	
	inline void clear();

	void assign (casMonitor &monitor, gdd *pValueIn);
private:
	smartGDDPointer pValue;
	caResId id;
};


//
// casMonitor()
//
class casMonitor : public tsDLNode<casMonitor>, public casRes {
public:
	casMonitor(caResId clientIdIn, casChannelI &chan, 
	unsigned long nElem, unsigned dbrType,
	const casEventMask &maskIn, osiMutex &mutexIn);
	virtual ~casMonitor();

	caStatus executeEvent(casMonEvent *);

	inline void post(const casEventMask &select, gdd &value);

	virtual void show (unsigned level) const;
	virtual caStatus callBack(gdd &value)=0;

	caResId getClientId() const 
	{
		return this->clientId;
	}

	unsigned getType() const 
	{
		return this->dbrType;
	}

	unsigned long getCount() const 
	{
		return this->nElem;
	}
	casChannelI &getChannel() const 
	{	
		return this->ciu;
	}

private:
	casMonEvent overFlowEvent;
	unsigned long const nElem;
	osiMutex &mutex;
	casChannelI &ciu;
	const casEventMask mask;
	caResId const clientId;
	unsigned char const dbrType;
	unsigned char nPend;
	unsigned ovf:1;
	unsigned enabled:1;

	void enable();
	void disable();

	void push (gdd &value);
};

//
// casMonitor::post()
// (check for NOOP case in line)
//
inline void casMonitor::post(const casEventMask &select, gdd &value)
{
	casEventMask	result(select&this->mask);
	//
	// NOOP if this event isnt selected
	// or if it is disabled
	//
	if (result.noEventsSelected() || !this->enabled) {
		return;
	}

	//
	// else push it on the queue
	//
	this->push(value);
}

class caServer;
class casCoreClient;
class casChannelI;
class casCtx;
class caServer;
class casAsyncIO;
class casAsyncReadIO;
class casAsyncWriteIO;
class casAsyncPVExistIO;
class casAsyncPVCreateIO;

class casAsyncIOI : public casEvent, public tsDLNode<casAsyncIOI> {
public:
	casAsyncIOI (casCoreClient &client, casAsyncIO &ioExternal);
	epicsShareFunc virtual ~casAsyncIOI();

	//
	// place notification of IO completion on the event queue
	//
	caStatus postIOCompletionI();

	inline void lock();
	inline void unlock();

	virtual caStatus cbFuncAsyncIO()=0;
	epicsShareFunc virtual int readOP();

	void destroyIfReadOP();

	caServer *getCAS() const;

	inline void destroy();

	void reportInvalidAsynchIO(unsigned);

protected:
	casCoreClient &client;   
	casAsyncIO &ioExternal;

private:
	unsigned inTheEventQueue:1;
	unsigned posted:1;
	unsigned ioComplete:1;
	unsigned serverDelete:1;
	unsigned duplicate:1;
	//
	// casEvent virtual call back function
	// (called when IO completion event reaches top of event queue)
	//
	epicsShareFunc caStatus cbFunc(casEventSys &);

	inline casAsyncIO * operator -> ();
};

//
// casAsyncRdIOI
//
// (server internal asynchronous read IO class)
//
class casAsyncRdIOI : public casAsyncIOI { 
public:
	epicsShareFunc casAsyncRdIOI(const casCtx &ctx, casAsyncReadIO &ioIn);
	epicsShareFunc virtual ~casAsyncRdIOI();

	void destroyIfReadOP();

	epicsShareFunc caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();

	epicsShareFunc caStatus postIOCompletion(caStatus completionStatus,
		gdd &valueRead);
	epicsShareFunc int readOP();
private:
	caHdr const msg;
	casChannelI &chan; 
	smartGDDPointer pDD;
	caStatus completionStatus;
};

//
// casAsyncWtIOI
//
// (server internal asynchronous write IO class)
//
class casAsyncWtIOI : public casAsyncIOI { 
public:
	epicsShareFunc casAsyncWtIOI(const casCtx &ctx, casAsyncWriteIO &ioIn);
	epicsShareFunc virtual ~casAsyncWtIOI();

	//
	// place notification of IO completion on the event queue
	//
	epicsShareFunc caStatus postIOCompletion(caStatus completionStatus);

	epicsShareFunc caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();
private:
	caHdr const	msg;
	casChannelI	&chan; 
	caStatus	completionStatus;
};

class casDGIntfIO;

//
// casAsyncExIOI 
//
// (server internal asynchronous read IO class)
//
class casAsyncExIOI : public casAsyncIOI { 
public:
	epicsShareFunc casAsyncExIOI(const casCtx &ctx, casAsyncPVExistIO &ioIn);
	epicsShareFunc virtual ~casAsyncExIOI();

	//
	// place notification of IO completion on the event queue
	//
	epicsShareFunc caStatus postIOCompletion(const pvExistReturn retVal);

	epicsShareFunc caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();
private:
	caHdr const msg;
	pvExistReturn retVal;
	casDGIntfIO * const pOutDGIntfIO;
	const caNetAddr dgOutAddr;
};

//
// casAsyncPVCIOI 
//
// (server internal asynchronous read IO class)
//
class casAsyncPVCIOI : public casAsyncIOI { 
public:
	epicsShareFunc casAsyncPVCIOI(const casCtx &ctx, casAsyncPVCreateIO &ioIn);
	epicsShareFunc virtual ~casAsyncPVCIOI();

	//
	// place notification of IO completion on the event queue
	//
	epicsShareFunc caStatus postIOCompletion(const pvCreateReturn &retVal);

	epicsShareFunc caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();
private:
	caHdr const	msg;
	pvCreateReturn 	retVal;
};

class casChannel;
class casPVI;

//
// casChannelI
//
// this derives from casEvent so that access rights
// events can be posted
//
class casChannelI : public tsDLNode<casChannelI>, public casRes, 
				public casEvent {
public:
	casChannelI (const casCtx &ctx, casChannel &chanAdapter);
	epicsShareFunc virtual ~casChannelI();

	casCoreClient &getClient() const
	{	
		return this->client;
	}
	const caResId getCID() 
	{
		return this->cid;
	}
	//
	// fetch the unsigned integer server id for this PV
	//
	inline const caResId getSID();

	//
	// addMonitor()
	//
	inline void addMonitor(casMonitor &mon);

	//
	// deleteMonitor()
	//
	inline void deleteMonitor(casMonitor &mon);

	//
	// findMonitor
	// (it is reasonable to do a linear search here because
	// sane clients will require only one or two monitors 
	// per channel)
	//
	inline casMonitor *findMonitor(const caResId clientIdIn);

	casPVI &getPVI() const 
	{
		return this->pv;
	}

	inline void installAsyncIO(casAsyncIOI &);
	inline void removeAsyncIO(casAsyncIOI &);

	inline void postEvent (const casEventMask &select, gdd &event);

	epicsShareFunc virtual casResType resourceType() const;

	virtual void show (unsigned level) const;

	virtual void destroy();

	inline void lock() const;
	inline void unlock() const;

	inline void clientDestroy();

	inline casChannel * operator -> () const;

	void clearOutstandingReads();

	//
	// access rights event call back
	//
	epicsShareFunc caStatus cbFunc(casEventSys &);

	inline void postAccessRightsEvent();
protected:
	tsDLList<casMonitor>	monitorList;
	tsDLList<casAsyncIOI>	ioInProgList;
	casCoreClient		&client;
	casPVI 			&pv;
	casChannel		&chan;
	caResId const           cid;    // client id 
	unsigned		clientDestroyPending:1;
	unsigned		accessRightsEvPending:1;
};

//
// class hierarchy added here solely because we need two list nodes
//
class casPVListChan : public casChannelI, public tsDLNode<casPVListChan>
{
public:
        casPVListChan (const casCtx &ctx, casChannel &chanAdapter);
        epicsShareFunc virtual ~casPVListChan();
};

class caServerI;
class casCtx;
class casChannel;
class casPV;

//
// casPVI
//
class casPVI : 
	public tsSLNode<casPVI>,  // server resource table installation 
	public casRes,		// server resource table installation 
	public ioBlockedList	// list of clients io blocked on this pv
{
public:
	casPVI (casPV &pvAdapter);
	epicsShareFunc virtual ~casPVI(); 

	//
	// for use by the server library
	//
	inline caServerI *getPCAS() const;

	//
	// attach to a server
	//
	caStatus attachToServer (caServerI &cas);

	//
	// CA only does 1D arrays for now (and the new server
	// temporarily does only scalers)
	//
	inline aitIndex nativeCount();

	//
	// only for use by casMonitor
	//
	caStatus registerEvent ();
	void unregisterEvent ();

	//
	// only for use by casAsyncIOI 
	//
	inline void unregisterIO();

	//
	// only for use by casChannelI
	//
	inline void installChannel(casPVListChan &chan);

	//
	// only for use by casChannelI
	//
	inline void removeChannel(casPVListChan &chan);

	//
	// check for none attached and delete self if so
	//
	inline void deleteSignal();

	inline void postEvent (const casEventMask &select, gdd &event);

	inline casPV *interfaceObjectPointer() const;

	caServer *getExtServer() const;

	//
	// bestDBRType()
	//
	inline caStatus bestDBRType (unsigned &dbrType);

	inline casPV * operator -> () const;

	epicsShareFunc virtual casResType resourceType() const;

	virtual void show(unsigned level) const;

	virtual void destroy();

private:
	tsDLList<casPVListChan>	chanList;
	caServerI		*pCAS;
	casPV			&pv;
	unsigned		nMonAttached;
	unsigned		nIOAttached;
	unsigned		destroyInProgress:1;

	inline void lock() const;
	inline void unlock() const;
};

