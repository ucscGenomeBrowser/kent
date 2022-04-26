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

boolean problem = FALSE;
char *destination = NULL;

void doMiddle()
/* cartReset - Reset cart. */
{
if (problem)
    {	
    warn("To stop Open Redirect abuse, only relative URLs are supported. "
	   "Request for destination=[%s] rejected.\n", destination);
    }
cartResetInDb(hUserCookie());
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
struct dyString *headText = dyStringNew(512);
destination = cgiUsualString("destination", defaultDestination);
// Only allow relative URL that does not contain space or quote characters.
if (strstr(destination, "//") // absolute URL
   || strchr(destination, '\'') // single quote
   || strchr(destination, '"') // double quote
   || strchr(destination, ' ') // space
   || sameString(destination, "") // empty string
    )
    {
    problem = TRUE;
    }

char *csp = getCspMetaHeader();  // ContentSecurityPolicy stops XSS js in destination
dyStringPrintf(headText, "%s",csp);

if (!problem)
    {
    dyStringPrintf(headText, 
		   "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0;URL=%s\">"
		   "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
		   "<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">"
		   ,destination);
    }

htmShellWithHead("Reset Cart", headText->string, doMiddle, NULL);

freeMem(csp);
dyStringFree(&headText);

cgiExitTime("cartReset", enteredMainTime);
return 0;
}
