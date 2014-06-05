/* gfNet.c - Network dependent stuff for blat server. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errAbort.h"
#include "genoFind.h"
#include "net.h"


int gfConnect(char *hostName, char *portName)
/* Start connection with server. */
{
/* Connect to server. */
int sd = netConnect(hostName, atoi(portName));
if (sd < 0)
    {
    errnoAbort("Sorry, the BLAT/iPCR server seems to be down.  Please try "
               "again later.");
    }
return sd;
}

