/* hgNotAvail - Human Genome Browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "hgFind.h"
#include "hCommon.h"
#include "hui.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: hgNotAvail.c,v 1.2 2009/06/25 08:43:09 markd Exp $";

struct cart *cart = NULL;
struct hash *oldVars = NULL;
char *clade = NULL;
char *organism = NULL;
char *db = NULL;
char *progName;

void hgNotAvail()
/* hgNotAvail */
{
printf("The function you selected (%s) is currently not available.", progName);
return;
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
char *scientificName = NULL;
cart = theCart;

getDbGenomeClade(cart, &db, &organism, &clade, oldVars);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    clade = hClade(organism);
    }
scientificName = hScientificName(db);
if (hIsGsidServer())
    cartWebStart(theCart, db, "GSID %s Genome Browser \n", organism);
else
    {
    char buffer[128];
    char *browserName = hBrowserName();

    /* tell html routines *not* to escape htmlOut strings*/
    htmlNoEscape();
    buffer[0] = 0;
    if (*scientificName != 0)
	{
	if (sameString(clade,"ancestor"))
	    safef(buffer, sizeof(buffer), "(<I>%s</I> Ancestor) ", scientificName);
	else
	    safef(buffer, sizeof(buffer), "(<I>%s</I>) ", scientificName);
	}
    cartWebStart(theCart, db, "%s %s%s\n", organism, buffer, browserName);
    htmlDoEscape();
    }
hgNotAvail();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

progName = argv[0];
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
