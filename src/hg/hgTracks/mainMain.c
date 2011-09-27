/* hgTracks - Human Genome browser main cgi script. */

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
#include "search.h"
#include "imageV2.h"

static char const rcsid[] = "$Id: hgTracks.c,v 1.1651 2010/06/11 17:53:06 larrym Exp $";

int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();
measureTime(NULL);
browserName = hBrowserName();
organization = "UCSC";

/* change title if this is for GSID */
browserName = (hIsGsidServer() ? "Sequence View" : browserName);
organization = (hIsGsidServer() ? "GSID" : organization);
organization = (hIsGisaidServer() ? "GISAID" : organization);

/* Push very early error handling - this is just
 * for the benefit of the cgiVarExists, which
 * somehow can't be moved effectively into doMiddle. */
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
char * link = webTimeStampedLinkToResourceOnFirstCall("HGStyle.css",TRUE); // resource file link wrapped in html
if (link)
    htmlSetStyle(link);

oldVars = hashNew(10);
if (hIsGsidServer())
    cartHtmlShell("GSID Sequence View", doMiddle, hUserCookie(), excludeVars, oldVars);
else
    cartHtmlShell("UCSC Genome Browser v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, oldVars);
if (measureTiming)
    {
    fprintf(stdout, "<span class='timing'>Overall total time: %ld millis<br /></span>\n",
            clock1000() - enteredMainTime);
    }
return 0;
}
