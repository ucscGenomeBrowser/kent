static char const rcsid[] = "$Id: gfNet.c,v 1.6 2003/05/05 06:45:33 kate Exp $";

/* gfNet.c - Network dependent stuff for blat server. */

#include "common.h"
#include "errabort.h"
#include "genoFind.h"
#include "net.h"

int gfConnect(char *hostName, char *portName)
/* Start connection with server. */
{
/* Connect to server. */
int sd = netConnect(hostName, atoi(portName));
if (sd < 0)
    {
    errnoAbort("Sorry, the BLAT server seems to be down.  Please try "
               "again later.");
    }
return sd;
}

