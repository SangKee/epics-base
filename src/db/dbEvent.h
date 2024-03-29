/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 *      Author: Jeff Hill 
 *      Date: 	030393 
 */

#ifndef INCLdbEventh
#define INCLdbEventh

/*
 * collides with db_access.h used in the CA client
 */
#ifndef caClient
#include <dbCommon.h>
#endif /*caClient*/

#include <db_field_log.h>

struct event_block{
        ELLNODE                 node;
        struct dbAddr		*paddr;
	void 			(*user_sub)(
					void *user_arg,
					struct dbAddr *paddr,
					int eventsRemaining,
					db_field_log *pfl);
        void                    *user_arg;
        struct event_que        *ev_que;
	db_field_log		*pLastLog;
        unsigned long           npend;  /* n times this event is on the que */
        unsigned char           select;
        char                    valque;
		char					callBackInProgress;
};

typedef void			EVENTFUNC(
					void *user_arg, 
					struct dbAddr *paddr,
					int eventsRemaining,
					db_field_log *pfl);


#define EVENTSPERQUE	32
#define EVENTQUESIZE    (EVENTENTRIES  * EVENTSPERQUE)
#define EVENTENTRIES    4      /* the number of que entries for each event */
#define EVENTQEMPTY     ((struct event_block *)NULL)


/*
 * really a ring buffer
 */
struct event_que{
        /* lock writers to the ring buffer only */
        /* readers must never slow up writers */
        FAST_LOCK               writelock;
        db_field_log            valque[EVENTQUESIZE];
        struct event_block      *evque[EVENTQUESIZE];
        struct event_que        *nextque;       /* in case que quota exceeded */
        struct event_user       *evUser;        /* event user parent struct */
        unsigned short  putix;
        unsigned short  getix;
        unsigned short  quota;          /* the number of assigned entries*/
	    unsigned short	nDuplicates;	/* N events duplicated on this q */ 
        unsigned short  nCanceled;      /* the number of canceled entries */
};

struct event_user{
        struct event_que        firstque;       /* the first event que */

        FAST_LOCK               lock;
        SEM_ID                  ppendsem;       /* Wait while empty */
        SEM_ID             	pflush_sem;	/* wait for flush */

	void			(*overflow_sub)(/* called when overflow detect */
					void *overflow_arg, unsigned count);
        void                    *overflow_arg;  /* parameter to above   */

	void			(*extralabor_sub)/* off load to event task */
					(void *extralabor_arg);	
	void			*extralabor_arg;/* parameter to above */

        int                     taskid;         /* event handler task id */
        unsigned 		queovr;		/* event que overflow count */
        char                    pendlck;        /* Only one task can pend */
        unsigned char           pendexit;       /* exit pend task */
	unsigned char		extra_labor;	/* if set call extra labor func */
	unsigned char		flowCtrlMode;	/* replace existing monitor */
    unsigned char       extra_labor_busy;
};

typedef void 		OVRFFUNC(void *overflow_arg, unsigned count);
typedef void 		EXTRALABORFUNC(void *extralabor_arg);

int 			db_event_list(char *name);
struct event_user 	*db_init_events(void);
int			db_close_events(struct event_user *evUser);
unsigned		db_sizeof_event_block(void);

int db_add_event(
struct event_user       *evUser,
struct dbAddr           *paddr,
EVENTFUNC		*user_sub,
void                    *user_arg,
unsigned int            select,
struct event_block      *pevent  /* ptr to event blk (not required) */
);

int     		db_cancel_event(struct event_block      *pevent);

int db_add_overflow_event(
struct event_user       *evUser,
OVRFFUNC		*overflow_sub,
void                    *overflow_arg 
);

int db_add_extra_labor_event(
struct event_user       *evUser,
EXTRALABORFUNC		*func,
void			*arg);

int db_flush_extra_labor_event(
struct event_user       *evUser
);

int 			db_post_single_event(struct event_block *pevent);

int db_post_extra_labor(struct event_user *evUser);

int db_post_events(
void			*precord,
void			*pvalue,
unsigned int            select 
);

int db_start_events(
struct event_user       *evUser,
char                    *taskname,      /* defaulted if NULL */
int			(*init_func)(int),
int                     init_func_arg,
int                     priority_offset
);

int event_task(
struct event_user       *evUser,
int			(*init_func)(int),
int                     init_func_arg
);

int db_event_enable(struct event_block      *pevent);
int db_event_disable(struct event_block      *pevent);

void db_event_flow_ctrl_mode_on(struct event_user *evUser);
void db_event_flow_ctrl_mode_off(struct event_user *evUser);

#endif /*INCLdbEventh*/

