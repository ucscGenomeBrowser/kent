/* gfNet.c - Network dependent stuff for blat server. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "genoFind.h"
#include "net.h"


struct gfConnection *gfMayConnect(char *hostName, char *portName, boolean isDynamic)
/* Start connection with server or return -1. */
{
/* Connect to server. */
int port = atoi(portName);
int sd = netConnect(hostName, port);
// if error, sd == -1
if (sd < 0)
    return NULL;
struct gfConnection *conn;
AllocVar(conn);
conn->fd = sd;
conn->hostName = cloneString(hostName);
conn->port = port;
conn->isDynamic = isDynamic;
return conn;
}

struct gfConnection *gfConnect(char *hostName, char *portName, boolean isDynamic)
/* Start connection with server. Abort on error. */
{
/* Connect to server. */
struct gfConnection *conn = gfMayConnect(hostName, portName, isDynamic);
if (conn == NULL)
    {
    errnoAbort("Sorry, the BLAT/iPCR server seems to be down.  Please try "
               "again later: %s %s", hostName, portName);
    }
return conn;
}

void gfBeginRequest(struct gfConnection *conn)
/* called before a request is started.  If the connect is not open, reopen
 * it. */
{
if (conn->fd < 0)
    {
    conn->fd = netConnect(conn->hostName, conn->port);
    if (conn->fd < 0)
        errnoAbort("Sorry, the BLAT/iPCR server seems to be down.  Please try "
                   "again later: %s %d", conn->hostName, conn->port);
    }
}

void gfEndRequest(struct gfConnection *conn)
/* End a request that might be followed by another requests. For
 * a static server, this closed the connection.  A dynamic server
 * it is left open. */
{
if (!conn->isDynamic)
    {
    close(conn->fd);
    conn->fd = -1;
    }
}


void gfDisconnect(struct gfConnection **pConn)
/* Disconnect from server */
{
struct gfConnection *conn = *pConn;
if (conn != NULL)
    {
    if (conn->fd >= 0)
        close(conn->fd);
    freeMem(conn->hostName);
    freez(pConn);
    }
}
