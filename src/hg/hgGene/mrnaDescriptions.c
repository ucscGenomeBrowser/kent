/* mRNA descriptions. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hdb.h"
#include "genePred.h"
#include "bed.h"
#include "hgGene.h"

struct genePred *curGenePred(struct sqlConnection *conn)
/* Return current gene in genePred. */
{
char *track = genomeSetting("knownGene");
char table[64];
boolean hasBin;
char query[256];
struct sqlResult *sr;
char **row;
struct genePred *gp = NULL;

hFindSplitTable(curGeneChrom, track, table, &hasBin);
safef(query, sizeof(query), 
	"select * from %s where name = '%s' "
	"and chrom = '%s' and txStart=%d and txEnd=%d"
	, table, curGeneId, curGeneChrom, curGeneStart, curGeneEnd);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    gp = genePredLoad(row + hasBin);
sqlFreeResult(&sr);
return gp;
}

static boolean mrnaDescriptionsExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if mrna
 * on this one. */
{
struct psl *list = NULL, *psl;
if (hTableExists("mrna"))
    {
    struct sqlResult *sr;
    char **row;
    struct psl *psl;
    int rowOffset;
    char extra[64];
    struct genePred *gp = curGenePred(conn);
    safef(extra, sizeof(extra), "strand='%c'", gp->strand[0]);
    genePredFree(&gp);
    sr = hRangeQuery(conn, "mrna", curGeneChrom, curGeneStart, curGeneEnd,
    	extra, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
         {
	 psl = pslLoad(row+rowOffset);
	 slAddHead(&list, psl);
	 }
    slReverse(&list);
    section->items = list;
    }
return slCount(list) > 0;
}

int gpRangeIntersection(struct genePred *gp, int start, int end)
/* Return number of bases range start,end shares with bed. */
{
int intersect = 0;
int i, exonCount = gp->exonCount;
for (i=0; i<exonCount; ++i)
    {
    intersect += positiveRangeIntersection(gp->exonStarts[i], gp->exonEnds[i],
    	start, end);
    }
return intersect;
}

int basesShared(struct genePred *gp, struct psl *psl)
/* Return number of bases a&b share. */
{
int intersect = 0;
int i, blockCount = psl->blockCount;
int s,e;
for (i=0; i<blockCount; ++i)
    {
    s = psl->tStarts[i];
    e = s + psl->blockSizes[i];
    if (psl->strand[1] < 0)
	reverseIntRange(&s, &e, psl->tSize);
    intersect += gpRangeIntersection(gp, s, e);
    }
return intersect;
}

static void mrnaDescriptionsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out mrna descriptions annotations. */
{
struct genePred *gp = curGenePred(conn);
struct psl *psl, *pslList = section->items;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (basesShared(gp, psl) > 12)	/* Filter out possible little noisy flecks. */
        {
	char query[512];
	char *description;
	safef(query, sizeof(query),
	    "select description.name from mrna,description"
	    " where mrna.acc='%s' and mrna.description = description.id"
	    , psl->qName);
	description = sqlQuickString(conn, query);
	if (description != NULL)
	    {
	    char *url = "http://www.ncbi.nlm.nih.gov/entrez/query.fcgi"
	    		"?cmd=Search&db=Nucleotide&term=%s&doptcmdl=GenBank"
			"&tool=genome.ucsc.edu";
	    hPrintf("<A HREF=\"");
	    hPrintf(url, psl->qName);
	    hPrintf("\">");
	    hPrintf("%s</A> - ", psl->qName);
	    hPrintf("%s<BR>", description);
	    }
	freeMem(description);
	}
    }
}

struct section *mrnaDescriptionsSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create mrnaDescriptions section. */
{
struct section *section = sectionNew(sectionRa, "mrnaDescriptions");
section->exists = mrnaDescriptionsExists;
section->print = mrnaDescriptionsPrint;
return section;
}
