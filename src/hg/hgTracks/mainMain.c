/* hgTracks - Human Genome browser main cgi script. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "memalloc.h"
#include "localmem.h"
#include "hCommon.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "hgFind.h"
#include "hgTracks.h"
#include "versionInfo.h"
#include "net.h"
#include "search.h"
#include "imageV2.h"
#include "extTools.h"


int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();
measureTime(NULL);
browserName = hBrowserName();
organization = "UCSC";

/* Push very early error handling - this is just
 * for the benefit of the cgiVarExists, which
 * somehow can't be moved effectively into doMiddle. */
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
char * link = webTimeStampedLinkToResourceOnFirstCall("HGStyle.css",TRUE); // resource file link
if (link)                                                                  // wrapped in html
    htmlSetStyle(link);

oldVars = hashNew(10);

if (cgiVarExists("hgt.redirectTool"))
    {
    printf("Content-type: text/html\n\n");
    errAbortSetDoContentType(FALSE);
    cart = cartForSession(hUserCookie(), NULL, NULL);
    extToolRedirect(cart, cgiString("hgt.redirectTool"));
    }
else
    cartHtmlShell("UCSC Genome Browser v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, oldVars);

if (measureTiming)
    measureTime("Time to write and close cart");
if (measureTiming)
    {
    fprintf(stdout, "<span class='timing'>Overall total time: %ld millis<br /></span>\n",
            clock1000() - enteredMainTime);
    }
cgiExitTime("hgTracks", enteredMainTime);
return 0;
}
