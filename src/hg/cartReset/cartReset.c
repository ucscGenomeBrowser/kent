/* cartReset - Reset cart. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hui.h"
#include "cart.h"



static char *defaultDestination = "../cgi-bin/hgGateway";

void doMiddle()
/* cartReset - Reset cart. */
{

cartResetInDb(hUserCookie());
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
struct dyString *headText = newDyString(512);
char *destination = cgiUsualString("destination", defaultDestination);
if (strstr(destination, "//"))
    errAbort("To stop Open Redirect abuse, only relative URLs are supported. "
	    "Request for destination=[%s] rejected.\n", destination);

char *meta = getCspMetaHeader();  // ContentSecurityPolicy stops XSS js in destination

dyStringPrintf(headText, "%s"
	       "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0;URL=%s\">"
	       "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
	       "<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">"
	       ,meta,destination);

htmShellWithHead("Reset Cart", headText->string, doMiddle, NULL);

freeMem(meta);
dyStringFree(&headText);

cgiExitTime("cartReset", enteredMainTime);
return 0;
}
