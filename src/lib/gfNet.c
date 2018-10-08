/* gfNet.c - Network dependent stuff for blat server. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "genoFind.h"
#include "net.h"


int gfMayConnect(char *hostName, char *portName)
/* Start connection with server or return -1. */
{
/* Connect to server. */
int sd = netConnect(hostName, atoi(portName));
// if error, sd == -1
return sd;
}

int gfConnect(char *hostName, char *portName)
/* Start connection with server. Abort on error. */
{
/* Connect to server. */
int sd = gfMayConnect(hostName, portName);
if (sd < 0)
    {
    errnoAbort("Sorry, the BLAT/iPCR server seems to be down.  Please try "
               "again later.");
    }
return sd;
}

