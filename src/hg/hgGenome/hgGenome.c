/* hgGenome - Full genome (as opposed to chromosome) view of data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hui.h"
#include "dbDb.h"
#include "hdb.h"
#include "web.h"
#include "hgColors.h"

static char const rcsid[] = "$Id: hgGenome.c,v 1.1 2006/05/25 14:55:49 kent Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGenome - Full genome (as opposed to chromosome) view of data\n"
  "usage:\n"
  "   hgGenome XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void webMain(struct sqlConnection *conn)
/* Set up fancy web page with hotlinks bar and
 * sections. */
{
uglyf("Theoretically showing full genome view here.");
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
struct sqlConnection *conn = NULL;
cart = theCart;
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
conn = hAllocConn();

    {
    /* Default case - start fancy web page. */
    cartWebStart(cart, "%s Genome View", genome);
    webMain(conn);
    cartWebEnd();
    }
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
if (argc != 1)
    usage();
oldCart = hashNew(12);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
return 0;
}
