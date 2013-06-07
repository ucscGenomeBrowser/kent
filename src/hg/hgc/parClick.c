/* parClick - click handling for par track  */
#include "common.h"
#include "parClick.h"
#include "hgc.h"
#include "bed.h"
#include "hdb.h"
#include "web.h"
#include "hCommon.h"

static struct bed *loadParTable(struct trackDb *tdb)
/* load all records in the par table */
{
struct bed *pars = NULL;
struct sqlConnection *conn = hAllocConn(database);
char query[512];
sqlSafef(query, sizeof(query), "select * from %s", tdb->table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    slAddHead(&pars, bedLoadN(row, 4));
sqlFreeResult(&sr);
hFreeConn(&conn);
return pars;
}

static int parCmp(const void *va, const void *vb)
/* Compare by name, then by chrom */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif = strcmp(a->name, b->name);
if (dif == 0)
    dif = strcmp(a->chrom, b->chrom);
return dif;
}

static struct bed *getClickedPar(char *item, struct bed **pars)
/* find par record that was clicked on, removing it from the list d*/
{
struct bed *clickedPar = NULL, *otherPars = NULL, *par;
while ((par = slPopHead(pars)) != NULL)
    {
    if (sameString(par->chrom, seqName) && sameString(par->name, item) && positiveRangeIntersection(par->chromStart, par->chromEnd, winStart, winEnd))
        {
        if (clickedPar != NULL)
            errAbort("multiple par rows named %s overlapping %s:%d-%d", item, seqName, winStart, winEnd);
        clickedPar = par;
        }
    else
        slAddHead(&otherPars, par);
    }
if (clickedPar == NULL)
    errAbort("no par row %s overlapping %s:%d-%d", item, seqName, winStart, winEnd);
*pars = otherPars;
return clickedPar;
}

static struct bed *getHomologousPar(struct bed *clickedPar, struct bed **pars)
/* find par record that was clicked on, removing it from the list */
{
struct bed *homPar = NULL, *otherPars = NULL, *par;
while ((par = slPopHead(pars)) != NULL)
    {
    if (sameString(par->name, clickedPar->name))
        {
        if (homPar != NULL)
            errAbort("multiple homologous par rows for %s", clickedPar->name);
        homPar = par;
        }
    else
        slAddHead(&otherPars, par);
    }
if (homPar == NULL)
    errAbort("no homologous par row for %s", clickedPar->name);
*pars = otherPars;
return homPar;
}

static void printParLink(struct bed *par)
/* print one par link back to the browser */
{
webPrintLinkCellStart();
printf("<A HREF=\"%s?db=%s&position=%s:%d-%d\">%s:%d-%d</A>",
       hgTracksName(), database, 
       par->chrom, par->chromStart, par->chromEnd,
       par->chrom, par->chromStart+1, par->chromEnd);
webPrintLinkCellEnd();
}

static void printHomPairRow(struct bed *parA, struct bed *parB)
/* print a homologous pair */
{
if (!sameString(parA->name, parB->name))
    errAbort("invalid PAR homologous pair: %s and %s", parA->name, parB->name);
webPrintLabelCell(parA->name);
printParLink(parA);
printParLink(parB);
}

static void printOtherPars(struct bed *clickedPar, struct bed *pars)
/* pirnt out other pairs of pars, keeping chroms lined up that were not
 * selected */
{
webPrintLinkTableNewRow();
webPrintWideCenteredLabelCell("Other PARs", 3);
struct bed *par1;
for (par1 = pars; par1 != NULL; par1 = par1->next->next)
    {
    webPrintLinkTableNewRow();
    if (sameString(par1->chrom, clickedPar->chrom))
        printHomPairRow(par1, par1->next);
    else
        printHomPairRow(par1->next, par1);
    }
}

void doParDetails(struct trackDb *tdb, char *name)
/* show details of a PAR item. */
{
// load entire PAR table (t's tiny) and partition
struct bed *pars = loadParTable(tdb);
if (slCount(pars) & 1)
    errAbort("par items not paired in %s", tdb->table);

struct bed *clickedPar = getClickedPar(name, &pars);
struct bed *homPar = getHomologousPar(clickedPar, &pars);
slSort(&pars, parCmp);

cartWebStart(cart, database, "Pseudoautosomal regions");
webPrintLinkTableStart();

// header
webPrintLabelCell("");
webPrintLabelCell("Selected PAR");
webPrintLabelCell("Homologous PAR");

// selected
webPrintLinkTableNewRow();
printHomPairRow(clickedPar, homPar);
if (pars != NULL)
    printOtherPars(clickedPar, pars);

webPrintLinkTableEnd();
printTrackHtml(tdb);
webEnd();

bedFreeList(&pars);
bedFree(&clickedPar);
bedFree(&homPar);
}
