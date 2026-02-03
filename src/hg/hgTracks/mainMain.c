/* hgTracks - Human Genome browser main cgi script. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "botDelay.h"
#include "hgConfig.h"
#include <sys/time.h>
#include <sys/resource.h>

boolean issueBotWarning;
long enteredMainTime = 0;

int main(int argc, char *argv[])
{
enteredMainTime = clock1000();
measureTime(NULL);

// This is generic CGI setup code: Should be moved one day into a generic function 
// combined with the code cart.c:genericCgiSetup() ?
cfgSetMaxMem(); // read hg.conf and set the maxMem if there
cfgSetLogCgiVars(); // set logging of the CGI vars

issueBotWarning = earlyBotCheck(enteredMainTime, "hgTracks", delayFraction, 0, 0, "html");
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
    // user has selected one of the tools in View > In external tools: Do not plot, just redirect.
    printf("Content-type: text/html\n\n");
    errAbortSetDoContentType(FALSE);
    cart = cartForSession(hUserCookie(), NULL, NULL);
    extToolRedirect(cart, cgiString("hgt.redirectTool"));
    }
else
    {
    httpHeaders = slPairNew("Cache-Control", "no-store");
    cartHtmlShell("UCSC Genome Browser v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, oldVars);
    }

// TODO: better place for this ?
webIncludeResourceFile("font-awesome.min.css");

if (measureTiming)
    measureTime("Time to write and close cart");
if (measureTiming)
    {
    fprintf(stdout, "<span class='timing'>Overall total time: %ld millis<br /></span>\n",
            clock1000() - enteredMainTime);
    }
cgiExitTime("hgTracks", enteredMainTime);

// print out some resource usage stats
struct rusage usage;
int stat = getrusage(RUSAGE_SELF, &usage);
if (stat == 0)
    // if you change this printf, then increment the number after RESOURCE:
    fprintf(stderr, "RESOURCE: 1 %ld %ld %ld\n",usage.ru_utime.tv_sec,usage.ru_stime.tv_sec, usage.ru_maxrss);
return 0;
}
