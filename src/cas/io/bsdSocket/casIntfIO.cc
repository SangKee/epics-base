//
// $Id$
//
// Author Jeff Hill
//


#include "server.h"


//
// 5 appears to be a TCP/IP built in maximum
//
const unsigned caServerConnectPendQueueSize = 5u;

//
// casIntfIO::casIntfIO()
//
casIntfIO::casIntfIO (const caNetAddr &addrIn) : 
	sock (INVALID_SOCKET),
    addr (addrIn.getSockIP())
{
	int status;
	osiSocklen_t addrSize;
    bool portChange;

	if ( ! osiSockAttach () ) {
		throw S_cas_internal;
	}

	/*
	 * Setup the server socket
	 */
	this->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->sock==INVALID_SOCKET) {
		printf("No socket error was %s\n", SOCKERRSTR(SOCKERRNO));
		throw S_cas_noFD;
	}

    /*
     * Note: WINSOCK appears to assign a different functionality for 
     * SO_REUSEADDR compared to other OS. With WINSOCK SO_REUSEADDR indicates
     * that simultaneously servers can bind to the same TCP port on the same host!
     * Also, servers are always enabled to reuse a port immediately after 
     * they exit ( even if SO_REUSEADDR isnt set ).
     */
#   ifndef SO_REUSEADDR_ALLOWS_SIMULTANEOUS_TCP_SERVERS_TO_USE_SAME_PORT
		int yes = TRUE;
	    status = setsockopt (
					    this->sock,
					    SOL_SOCKET,
					    SO_REUSEADDR,
					    (char *) &yes,
					    sizeof (yes));
	    if (status<0) {
		    errlogPrintf("CAS: server set SO_REUSEADDR failed? %s\n",
			    SOCKERRSTR(SOCKERRNO));
            socket_close (this->sock);
		    throw S_cas_internal;
	    }
#   endif

	status = bind(this->sock,
                      reinterpret_cast <sockaddr *> (&this->addr),
                      sizeof(this->addr));
	if (status<0) {
		if (SOCKERRNO == SOCK_EADDRINUSE) {
			//
			// enable assignment of a default port
			// (so the getsockname() call below will
			// work correctly)
			//
			this->addr.sin_port = ntohs (0);
			status = bind(this->sock,
                                      reinterpret_cast <sockaddr *> (&this->addr),
                                      sizeof(this->addr));
		}
		if (status<0) {
			char buf[64];
            int errnoCpy = SOCKERRNO;

			ipAddrToA (&this->addr, buf, sizeof(buf));
			errPrintf(S_cas_bindFail,
				__FILE__, __LINE__,
				"- bind TCP IP addr=%s failed because %s",
				buf, SOCKERRSTR(errnoCpy));
            socket_close (this->sock);
			throw S_cas_bindFail;
		}
        portChange = true;
	}
    else {
        portChange = false;
    }

	addrSize = ( osiSocklen_t ) sizeof (this->addr);
	status = getsockname (this->sock, 
                              reinterpret_cast <sockaddr *> (&this->addr)
                              , &addrSize);
	if (status) {
		errlogPrintf("CAS: getsockname() error %s\n", 
			SOCKERRSTR(SOCKERRNO));
        socket_close (this->sock);
		throw S_cas_internal;
	}

	//
	// be sure of this now so that we can fetch the IP 
	// address and port number later
	//
    assert (this->addr.sin_family == AF_INET);

    if ( portChange ) {
        errlogPrintf ( "cas warning: Configured TCP port was unavailable.\n");
        errlogPrintf ( "cas warning: Using dynamically assigned TCP port %hu,\n", 
            ntohs (this->addr.sin_port) );
        errlogPrintf ( "cas warning: but now two or more servers share the same UDP port.\n");
        errlogPrintf ( "cas warning: Depending on your IP kernel this server may not be\n" );
        errlogPrintf ( "cas warning: reachable with UDP unicast (a host's IP in EPICS_CA_ADDR_LIST)\n" );
    }

    status = listen(this->sock, caServerConnectPendQueueSize);
    if(status < 0) {
		errlogPrintf("CAS: listen() error %s\n", SOCKERRSTR(SOCKERRNO));
        socket_close (this->sock);
		throw S_cas_internal;
    }
}

//
// casIntfIO::~casIntfIO()
//
casIntfIO::~casIntfIO()
{
	if (this->sock != INVALID_SOCKET) {
		socket_close(this->sock);
	}

	osiSockRelease ();
}

//
// newStreamIO::newStreamClient()
//
casStreamOS *casIntfIO::newStreamClient(caServerI &cas) const
{
    static bool oneMsgFlag = false;
    struct sockaddr	newAddr;
    SOCKET newSock;
    osiSocklen_t length;
    casStreamOS	*pOS;
    
    length = ( osiSocklen_t ) sizeof(newAddr);
    newSock = accept(this->sock, &newAddr, &length);
    if ( newSock == INVALID_SOCKET ) {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy!=SOCK_EWOULDBLOCK && ! oneMsgFlag ) {
            errlogPrintf ("CAS: %s accept error \"%s\"\n",
                __FILE__,SOCKERRSTR(errnoCpy));
            oneMsgFlag = true;
        }
        return NULL;
    }
    else if ( sizeof (newAddr) > (size_t) length ) {
        socket_close(newSock);
        errlogPrintf("CAS: accept returned bad address len?\n");
        return NULL;
    }
    oneMsgFlag = false;
    ioArgsToNewStreamIO args;
    args.addr = newAddr;
    args.sock = newSock;
    pOS = new casStreamOS(cas, args);
    if (!pOS) {
        errMessage(S_cas_noMemory, "unable to create data structures for a new client");
        socket_close(newSock);
    }
    else {
        if ( cas.getDebugLevel()>0u) {
            char pName[64u];
            
            pOS->hostName (pName, sizeof (pName));
            errlogPrintf("CAS: allocated client object for \"%s\"\n", pName);
        }
    }
    return pOS;
}

//
// casIntfIO::setNonBlocking()
//
void casIntfIO::setNonBlocking()
{
        int status;
        osiSockIoctl_t yes = TRUE;
 
        status = socket_ioctl(this->sock, FIONBIO, &yes); // X aCC 392
        if (status<0) {
                errlogPrintf(
                "%s:CAS: server non blocking IO set fail because \"%s\"\n",
                                __FILE__, SOCKERRSTR(SOCKERRNO));
        }
}
 
//
// casIntfIO::getFD()
//
int casIntfIO::getFD() const
{
        return this->sock;
}

//
// casIntfIO::show()
//
void casIntfIO::show(unsigned level) const
{
	if (level>2u) {
		printf(" casIntfIO::sock = %d\n", this->sock);
	}
}

//
// casIntfIO::serverAddress ()
// (avoid problems with GNU inliner)
//
caNetAddr casIntfIO::serverAddress () const
{
    return caNetAddr (this->addr);
}

