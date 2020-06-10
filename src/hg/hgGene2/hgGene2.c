/* hgGene2 - hgGene support for gencodeAttr table set. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "genbank.h"

#include "assoc.h"
#include "sections.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
char *database = NULL;



static void doPage(struct cart *cart, char *id)
{
struct sqlConnection *conn = hAllocConn(database);

char *table = cartUsualString(cart, "hgg_type" , NULL);
struct trackDb *tdb = hTrackDbForTrack(database, table);
char *fileName = bbiNameFromSettingOrTable(tdb, NULL, tdb->table);
struct bbiFile *bbi = bigBedFileOpen(fileName);
char *gene = cartUsualString(cart, "hgg_gene" , 0);
char *chrom = cartUsualString(cart, "hgg_chrom" , 0);
int start = cartUsualInt(cart, "hgg_start" , 0);
int end = cartUsualInt(cart, "hgg_end" , 0);
struct lm *lm = lmInit(0);
struct asObject *as = asForDb(tdb, database);


struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
 char startBuf[128], endBuf[128];
 char *bedRow[40];

 
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
        continue;
    int bbFieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, bedRow, ArraySize(bedRow));

    char *name = cloneFirstWordByDelimiterNoSkip(bb->rest, '\t');
    boolean match = (isEmpty(name) && isEmpty(gene)) || sameOk(name, gene);
    freez(&name);
    if (!match)
        continue;
    int ii;
    struct asColumn *col = as->columnList;
    for(ii=0; ii < bbFieldCount; ii++, col = col->next)
        {
        if (ii < 20)
            continue;
        printf("<b>%s</b>: %s<BR>", col->comment, bedRow[ii]);
        }
    }

doAssociations(conn, tdb, gene);
doSections(conn, tdb, gene);
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
char *genome = NULL;
getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *id = cartUsualString(cart, "hgg_gene" , NULL);

cartWebStart(cart, database, "%s Gene %s (%s) Description and Page Index",
            genome, id, id);

doPage(cart, id);
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
