/* hgTables - Get table data associated with tracks and intersect tracks. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: hgTables.c,v 1.1 2004/07/12 17:32:40 kent Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTables - Get table data associated with tracks and intersect tracks\n"
  "usage:\n"
  "   hgTables XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Global variables. */
struct cart *cart;      /* Variables carried from last time program run. */	
struct hash *oldVars;	/* The cart before new cgi stuff added. */

/* --------------- HTML Helpers ----------------- */

void hvPrintf(char *format, va_list args)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, format, args);
}

static void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
}

static void htmlClose()
/* Close down html format page. */
{
cartWebEnd();
}

/* --------------- Text Mode Helpers ----------------- */

static void textWarnHandler(char *format, va_list args)
/* Text mode error message handler. */
{
char *hLine =
"---------------------------------------------------------------------------\n";
if (format != NULL) {
    fflush(stdout);
    fprintf(stderr, "%s", hLine);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s", hLine);
    }
}

static void textAbortHandler()
/* Text mode abort handler. */
{
exit(-1);
}

static void textOpen()
/* Start up page in text format. (No need to close this). */
{
printf("Content-Type: text/plain\n\n");
pushWarnHandler(textWarnHandler);
pushAbortHandler(textAbortHandler);
}

/* --------- Test Page --------------------- */

void testPage()
/* Put up a page to see what happens. */
{
textOpen();
hPrintf("%s\n", cartUsualString(cart, "test", "n/a"));
hPrintf("All for now\n");
}

/* --------- Initial Page ------------------ */

void beginPage()
/* Put up the first page user sees. */
{
htmlOpen("Track Tables");
printf("Hello world!");
htmlClose();
}

void dispatch()
/* Check phase variable and based on that dispatch to 
 * appropriate page-generator. */
{
char *phase = cartUsualString(cart, hgtaPhase, "begin");
if (sameString(phase, "begin"))
    beginPage();
else if (sameString(phase, "test"))
    testPage();
else
    {
    warn("Unrecognized phase '%s'", phase);
    cartRemove(cart, hgtaPhase);
    return;
    }
}

char *excludeVars[] = {"Submit", "submit", hgtaPhase, NULL};

void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks. */
{
oldVars = hashNew(8);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);
dispatch();
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
cgiSpoof(&argc, argv);
hgTables();
return 0;
}
