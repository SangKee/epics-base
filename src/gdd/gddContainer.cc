/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Author:	Jim Kowalkowski
// Date:	3/97
//
// $Id$

#define epicsExportSharedSymbols
#include "gdd.h"

// --------------------The gddContainer functions---------------------

gddCursor gddContainer::getCursor(void) const
{
	gddCursor ec(this);
	return ec;
}

gddContainer::gddContainer(void):gdd(0,aitEnumContainer,1) { }
gddContainer::gddContainer(int app):gdd(app,aitEnumContainer,1) { }

gddContainer::gddContainer(int app,int tot) : gdd(app,aitEnumContainer,1)
	{ cInit(tot); }

void gddContainer::cInit(int tot)
{
	int i;
	gdd* dd_list;
	gdd* temp;

	setBound(0,0,tot);
	dd_list=NULL;

	for(i=0;i<tot;i++)
	{
		temp=new gdd;
		temp->noReferencing();
		temp->setNext(dd_list);
		dd_list=temp;
	}
	setData(dd_list);
}

gddContainer::gddContainer(gddContainer* ec)
{
	unsigned i;
	gdd* dd_list;
	gdd* temp;

	copy(ec);
	dd_list=NULL;

	// this needs to recursively add to the container, only copy the
	// info and bounds information and scaler data, not arrays

	for(i=0;i<dimension();i++)
	{
		temp=new gdd(getDD(i));
		temp->noReferencing();
		temp->setNext(dd_list);
		dd_list=temp;
	}
	setData(dd_list);
}

gdd* gddContainer::getDD(aitIndex index)
{
	if (index==0u) {
		return this;
	}
	else if(index>getDataSizeElements())
		return NULL;
	else
	{
		aitIndex i;
		gdd* dd=(gdd*)dataPointer();
		for(i=1u;i<index;i++) {
			dd=(gdd*)dd->next();
		}
		return dd;
	}
}

gddStatus gddContainer::insert(gdd* dd)
{
	dd->setNext(cData());
	setData(dd);
	bounds->setSize(bounds->size()+1);
	return 0;
}

gddStatus gddContainer::remove(aitIndex index)
{
	gddCursor cur = getCursor();
	gdd *dd,*prev_dd;
	aitIndex i;

	prev_dd=NULL;

	for(i=0; (dd=cur[i]) && i!=index; i++) prev_dd=dd;

	if(i==index && dd)
	{
		if(prev_dd)
			prev_dd->setNext(dd->next());
		else
			setData(dd->next());

		dd->unreference();
		bounds->setSize(bounds->size()-1);
		return 0;
	}
	else
	{
		gddAutoPrint("gddContainer::remove()",gddErrorOutOfBounds);
		return gddErrorOutOfBounds;
	}
}

// ------------------------cursor functions-------------------------------

gdd* gddCursor::operator[](int index)
{
	int i,start;
	gdd* dd;

	if(index>=curr_index)
	{
		start=curr_index;
		dd=curr;
	}
	else
	{
		start=0;
		dd=list->cData();
	}

	for(i=start;i<index;i++) dd=dd->next();
	curr_index=index;
	curr=dd;
	return dd;
}

