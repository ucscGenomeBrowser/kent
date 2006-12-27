/* testSearch - test the search functionality */
#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hgFind.h"
#include "hdb.h"
#include "hui.h"

static char const rcsid[] = "$Id: testSearch.c,v 1.1 2006/12/27 01:09:47 heather Exp $";

/* Need to get a cart in order to use hgFind. */
struct cart *cart = NULL;
char *excludeVars[] = { NULL };

void usage()
{
errAbort(
"testSearch - test search functionality.\n"
"usage:\n"
"  testSearch\n"
);
}

int main(int argc, char *argv[])
{
char *termToSearch = "rs3";
struct hgPositions *hgp = NULL;
struct hgPositions *hgpList = NULL;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

cgiSpoof(&argc, argv);
cart = cartAndCookie(hUserCookie(), excludeVars, NULL);
hSetDb("hg18");
hgpList = hgPositionsFind(termToSearch, "", "hgTracks", cart, FALSE);
if (hgpList == NULL || hgpList->posCount == 0)
    {
    printf("no matches found\n");
    return 1;
    }
printf("query = %s\n", hgpList->query);
printf("database = %s\n", hgpList->database);
printf("posCount = %d\n\n", hgpList->posCount);
for (hgp = hgpList; hgp != NULL; hgp = hgp->next)
    {
    if (hgp->tableList != NULL)
        {
        for (table = hgp->tableList; table != NULL; table = table->next)
            {
            printf("  tableName = %s\n", table->name);
	    // printf("  tableDescription = %s\n", table->description);
	    if (table->posList != NULL)
	        {
	        for (pos = table->posList; pos != NULL; pos = pos->next)
	            {
	            printf("    chrom = %s\n", pos->chrom);
	            printf("    chromStart = %d\n", pos->chromStart);
	            printf("    chromEnd = %d\n", pos->chromEnd);
	            // printf("    name = %s\n", pos->name);
	            // printf("    description = %s\n", pos->description);
	            printf("    browserName = %s\n\n", pos->browserName);
		    }
	        }
	    }
	}
    }
return 0;
}
