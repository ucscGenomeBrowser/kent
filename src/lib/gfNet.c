/* gfNet.c - Network dependent stuff for blat server. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "genoFind.h"
#include "net.h"


struct gfConnection *gfMayConnect(char *hostName, char *portName)
/* Start connection with server or return -1. */
{
/* Connect to server. */
int sd = netConnect(hostName, atoi(portName));
// if error, sd == -1
if (sd < 0)
    return NULL;
struct gfConnection *conn;
AllocVar(conn);
conn->fd = sd;
return conn;
}

struct gfConnection *gfConnect(char *hostName, char *portName)
/* Start connection with server. Abort on error. */
{
/* Connect to server. */
struct gfConnection *conn = gfMayConnect(hostName, portName);
if (conn == NULL)
    {
    errnoAbort("Sorry, the BLAT/iPCR server seems to be down.  Please try "
               "again later.");
    }
return conn;
}

void gfDisconnect(struct gfConnection **pConn)
/* Disconnect from server */
{
if (*pConn != NULL)
    {
    close((*pConn)->fd);
    freez(pConn);
    }
}
