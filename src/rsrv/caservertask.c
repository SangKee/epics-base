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
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 *
 */

static char *sccsId = "@(#) $Id$";
#include <stdio.h>
#include <unistd.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <sockLib.h>
#include <socket.h>
#include <in.h>
#include <logLib.h>
#include <string.h>
#include <usrLib.h>
#include <errnoLib.h>
#include <tickLib.h>
#include <sysLib.h>

#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "task_params.h"
#include "envDefs.h"
#include "freeList.h"
#include "server.h"
#include "bsdSocketResource.h"

LOCAL int terminate_one_client(struct client *client);
LOCAL void log_one_client(struct client *client, unsigned level);
LOCAL unsigned long delay_in_ticks(unsigned long prev);


/*
 *
 *	req_server()
 *
 *	CA server task
 *
 *	Waits for connections at the CA port and spawns a task to
 *	handle each of them
 *
 */
int req_server(void)
{
	struct sockaddr_in serverAddr;	/* server's address */
	int status;
	int i;

	taskwdInsert((int)taskIdCurrent,NULL,NULL);

	ca_server_port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

	if (IOC_sock != 0 && IOC_sock != ERROR)
		if ((status = close(IOC_sock)) == ERROR)
			logMsg( "CAS: Unable to close open master socket\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	if ((IOC_sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		logMsg("CAS: Socket creation error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		taskSuspend(0);
	}

	/* Zero the sock_addr structure */
	bfill((char *)&serverAddr, sizeof(serverAddr), 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(ca_server_port);

	/* get server's Internet address */
	if (bind(IOC_sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == ERROR) {
		logMsg("CAS: Bind error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(IOC_sock);
		taskSuspend(0);
	}

	/* listen and accept new connections */
	if (listen(IOC_sock, 10) == ERROR) {
		logMsg("CAS: Listen error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(IOC_sock);
		taskSuspend(0);
	}

	while (TRUE) {
		struct sockaddr	sockAddr;
		osiSocklen_t addLen = sizeof(sockAddr);

		if ((i = accept(IOC_sock, &sockAddr, &addLen)) == ERROR) {
			logMsg("CAS: Client accept error was \"%s\"\n",
				(int) strerror(errnoGet()),
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			taskDelay(15*sysClkRateGet());
			continue;
		} else {
			status = taskSpawn(CA_CLIENT_NAME,
					   CA_CLIENT_PRI,
					   CA_CLIENT_OPT,
					   CA_CLIENT_STACK,
					   (FUNCPTR) camsgtask,
					   i,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL);
			if (status == ERROR) {
				logMsg("CAS: task creation for new client failed because \"%s\"\n",
					(int) strerror(errnoGet()),
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
				taskDelay(15*sysClkRateGet());
				continue;
			}
		}
	}
}


/*
 *
 *	free_client()
 *
 */
int free_client(struct client *client)
{
	if (!client) {
		return ERROR;
	}

	terminate_one_client(client);

	freeListFree(rsrvClientFreeList, client);

	return OK;
}


/* 
 * TERMINATE_ONE_CLIENT
 */
LOCAL int terminate_one_client(struct client *client)
{
    int                     servertid;
    int                     tmpsock;
    int                     status;
    struct event_ext        *pevext;
    struct channel_in_use   *pciu;

	if (client->proto != IPPROTO_TCP) {
		logMsg("CAS: non TCP client delete ignored\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		return ERROR;
	}

	tmpsock = client->sock;

	if(CASDEBUG>0){
		logMsg("CAS: Connection %d Terminated\n", 
			tmpsock,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

    /*
     * turn off extra labor callbacks from the event thread
     */
    status = db_add_extra_labor_event ( client->evuser, NULL, NULL );
    assert ( status == OK );

    /*
     * wait for extra labor in progress to comple
     */
    status = db_flush_extra_labor_event ( client->evuser );
    assert(status==OK);

	/*
	 * Server task deleted first since close() is not reentrant
	 */
	servertid = client->tid;
	taskwdRemove(servertid);
	if (servertid != taskIdSelf()){
		if (taskIdVerify(servertid) == OK){
			if (taskDelete(servertid) == ERROR) {
				logMsg("CAS: Client shut down task delete failed because \"%s\"\n", 
					(int) strerror(errnoGet()),
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
			}
		}
	}

	while(TRUE){
        FASTLOCK(&client->addrqLock);
		pciu = (struct channel_in_use *) ellGet ( &client->addrq );
        FASTUNLOCK(&client->addrqLock);
		if(!pciu){
			break;
		}

		while (TRUE){
			/*
			 * AS state change could be using this list
			 */
			FASTLOCK(&client->eventqLock);
			pevext = (struct event_ext *) ellGet(&pciu->eventq);
			FASTUNLOCK(&client->eventqLock);
			if(!pevext){
				break;
			}

			if(pevext->pdbev){
				status = db_cancel_event(pevext->pdbev);
				assert(status == OK);
			}
			freeListFree(rsrvEventFreeList, pevext);
		}

		if(pciu->pPutNotify){
            if ( pciu->pPutNotify->busy ) {
                dbNotifyCancel ( &pciu->pPutNotify->dbPutNotify );
            }
			free(pciu->pPutNotify);
		}

		FASTLOCK(&clientQlock);
		status = bucketRemoveItemUnsignedId (
				pCaBucket, 
				&pciu->sid);
		FASTUNLOCK(&clientQlock);
		if(status != S_bucket_success){
			errPrintf (
				status,
				__FILE__,
				__LINE__,
				"Bad id=%d at close",
				pciu->sid);
		}
		status = asRemoveClient(&pciu->asClientPVT);
		if(status!=0 && status != S_asLib_asNotActive){
			printf("And the status is %x \n", status);
			errPrintf(status, __FILE__, __LINE__, "asRemoveClient");
		}

		/*
		 * place per channel block onto the
		 * free list
		 */
		freeListFree (rsrvChanFreeList, pciu);
	}

	if (client->evuser) {
		status = db_close_events(client->evuser);
		if (status == ERROR)
			taskSuspend(0);
	}
	if (close(tmpsock) == ERROR)	/* close socket	 */
		logMsg("CAS: Unable to close socket\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);

	if(FASTLOCKFREE(&client->eventqLock)<0){
		logMsg("CAS: couldnt free sem\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	if(FASTLOCKFREE(&client->addrqLock)<0){
		logMsg("CAS: couldnt free sem\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	if(FASTLOCKFREE(&client->putNotifyLock)<0){
		logMsg("CAS: couldnt free sem\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	if(FASTLOCKFREE(&client->lock)<0){
		logMsg("CAS: couldnt free sem\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	status = semDelete(client->blockSem);
	if(status != OK){
		logMsg("CAS: couldnt free block sem\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	if(client->pUserName){
		free(client->pUserName);
	}

	if(client->pHostName){
		free(client->pHostName);
	}

	client->minor_version_number = CA_UKN_MINOR_VERSION;

	return OK;
}


/*
 *	client_stat()
 */
int client_stat(unsigned level)
{
	printf ("\"client_stat\" has been replaced by \"casr\"\n");
	return ellCount(&clientQ);
}

/*
 *	casr()
 */
void casr (unsigned level)
{
	size_t bytes_reserved;
	struct client 	*client;

	printf( "Channel Access Server V%d.%d\n",
		CA_PROTOCOL_VERSION,
		CA_MINOR_VERSION);

	LOCK_CLIENTQ;
	client = (struct client *) ellNext(&clientQ);
	if (!client) {
		printf("No clients connected.\n");
	}
	while (client) {

		log_one_client(client, level);

		client = (struct client *) ellNext(&client->node);
	}
	UNLOCK_CLIENTQ;

	if (level>=2 && prsrv_cast_client) {
		log_one_client(prsrv_cast_client, level);
	}
	
	if (level>=2u) {
		bytes_reserved = 0u;
		bytes_reserved += sizeof (struct client) * 
					freeListItemsAvail (rsrvClientFreeList);
		bytes_reserved += sizeof (struct channel_in_use) *
					freeListItemsAvail (rsrvChanFreeList);
		bytes_reserved += (sizeof(struct event_ext)+db_sizeof_event_block()) *
					freeListItemsAvail (rsrvEventFreeList);
		printf(	"There are currently %u bytes on the server's free list\n",
			bytes_reserved);
		printf(	"%u client(s), %u channel(s), and %u event(s) (monitors)\n",
			freeListItemsAvail (rsrvClientFreeList),
			freeListItemsAvail (rsrvChanFreeList),
			freeListItemsAvail (rsrvEventFreeList));

		if(pCaBucket){
			printf(	"The server's resource id conversion table:\n");
			FASTLOCK(&clientQlock);
			bucketShow (pCaBucket);
			FASTUNLOCK(&clientQlock);
		}

		caPrintAddrList (&beaconAddrList);
	}
}



/*
 *	log_one_client()
 *
 */
LOCAL void log_one_client(struct client *client, unsigned level)
{
	int			i;
	struct channel_in_use	*pciu;
	char			*pproto;
	float			send_delay;
	float			recv_delay;
	unsigned long		bytes_reserved;
	char			*state[] = {"up", "down"};
    char            clientHostName[256];

	if(client->proto == IPPROTO_UDP){
		pproto = "UDP";
	}
	else if(client->proto == IPPROTO_TCP){
		pproto = "TCP";
	}
	else{
		pproto = "UKN";
	}

	send_delay = delay_in_ticks(client->ticks_at_last_send);
	recv_delay = delay_in_ticks(client->ticks_at_last_recv);

	printf(	
"Client Name=\"%s\", Client Host=\"%s\", V%d.%u, Channel Count=%d\n", 
		client->pUserName,
		client->pHostName,
		CA_PROTOCOL_VERSION,
		client->minor_version_number,
		ellCount(&client->addrq));
	if (level>=1) {
		printf( "\tTId=0X%lX, Protocol=%3s, Socket FD=%d\n", 
			(unsigned long) client->tid,
			pproto, 
			client->sock); 
		printf( 
		"\tSecs since last send %6.2f, Secs since last receive %6.2f\n", 
			send_delay/sysClkRateGet(),
			recv_delay/sysClkRateGet());
		printf( 
		"\tUnprocessed request bytes=%lu, Undelivered response bytes=%lu\n", 
			client->send.stk,
			client->recv.cnt - client->recv.stk);	

        ipAddrToA (&client->addr, clientHostName, sizeof(clientHostName));

		printf(
		"\tClient's DNS Host Name %s State=%s\n",
            clientHostName, state[client->disconnect?1:0]);
	}

	if (level>=2u) {
		if (client->udpNoBuffCount>0u) {
			printf ("\tNumber of UDP response messages dropped due to ENOBUFs = %u\n",
				client->udpNoBuffCount);
		}

		bytes_reserved = 0;
		bytes_reserved += sizeof(struct client);

		FASTLOCK(&client->addrqLock);
		pciu = (struct channel_in_use *) client->addrq.node.next;
		while (pciu){
			bytes_reserved += sizeof(struct channel_in_use);
			bytes_reserved += 
				(sizeof(struct event_ext)+db_sizeof_event_block())*
					ellCount(&pciu->eventq);
			if(pciu->pPutNotify){
				bytes_reserved += sizeof(*pciu->pPutNotify);
				bytes_reserved += pciu->pPutNotify->valueSize;
			}
			pciu = (struct channel_in_use *) ellNext(&pciu->node);
		}
		FASTUNLOCK(&client->addrqLock);
		printf(	"\t%ld bytes allocated\n", bytes_reserved);


		FASTLOCK(&client->addrqLock);
		pciu = (struct channel_in_use *) client->addrq.node.next;
		i=0;
		while (pciu){
			printf(	"\t%s(%d%c%c)", 
				pciu->addr.precord->name,
				ellCount(&pciu->eventq),
				asCheckGet(pciu->asClientPVT)?'r':'-',
				rsrvCheckPut(pciu)?'w':'-');
			pciu = (struct channel_in_use *) ellNext(&pciu->node);
			if( ++i % 3 == 0){
				printf("\n");
            }
		}
		FASTUNLOCK(&client->addrqLock);
		printf("\n");
	}

	if (level >= 3u) {
		printf(	"\tSend Lock\n");
		semShow (client->lock.ppend, 1);
		printf(	"\tPut Notify Lock\n");
		semShow (client->putNotifyLock.ppend, 1);
		printf(	"\tAddress Queue Lock\n");
		semShow (client->addrqLock.ppend, 1);
		printf(	"\tEvent Queue Lock\n");
		semShow (client->eventqLock.ppend, 1);
		printf(	"\tBlock Semaphore\n");
		semShow (client->blockSem, 1);
	}
}


/*
 * delay_in_ticks()
 */
unsigned long delay_in_ticks(unsigned long prev)
{
	unsigned long delay;
	unsigned long current;

	current = tickGet();
	if (current >= prev) {
		delay = current - prev;
	} 
	else {
		delay = current + (ULONG_MAX - prev);
	}

	return delay;
}	


