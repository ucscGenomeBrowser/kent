/* Handle details pages for maf tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "maf.h"
#include "obscure.h"

static void mafPrettyHeader(FILE *f, struct mafAli *maf)
/* Write out summary. */
{
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    char dbOnly[128];
    char *chrom;
    int s = mc->start;
    int e = s + mc->size;
    safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
    chrom = chopPrefix(dbOnly);
    if (mc->strand == '-')
        reverseIntRange(&s, &e, mc->srcSize);
    linkToOtherBrowser(dbOnly, chrom, s, e);
    fprintf(f, "%s:%d-%d</A>, strand %c, size %d\n",
    	mc->src, s+1, e, mc->strand, mc->size);
    }
}

static void mafPrettyBody(FILE *f, struct mafAli *maf, int lineSize)
/* Print MAF base by base with line-breaks. */
{
int srcChars = 0;
struct mafComp *mc;
int i, lineStart, lineEnd;

/* Figure out length of source field. */
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    int len = strlen(mc->src);
    if (srcChars < len)
        srcChars = len;
    }

for (lineStart = 0; lineStart < maf->textSize; lineStart = lineEnd)
    {
    lineEnd = lineStart + lineSize;
    if (lineEnd >= maf->textSize)
        lineEnd = maf->textSize;
    for (mc = maf->components; mc != NULL; mc = mc->next)
        {
	fprintf(f, "%-*s ", srcChars, mc->src);
	mustWrite(f, mc->text + lineStart, lineEnd - lineStart);
	fprintf(f, "\n");
	}
    fprintf(f, "\n");
    }
}



static void mafPrettyOut(FILE *f, struct mafAli *maf, int lineSize)
/* Output MAF in human readable format. */
{
mafPrettyHeader(f, maf);
fprintf(f, "\n");
mafPrettyBody(f, maf, lineSize);
}

void genericMafClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for MAF tracks. */
{
if (winEnd - winStart > 20000)
    {
    printf("Zoom so that window is 20000 bases or less to see base-by-base alignments\n");
    }
else
    {
    struct mafAli *mafList, *maf, *subList = NULL;
    int aliIx = 0, realCount = 0;
    char dbChrom[64];
    struct slName *dbList = NULL, *dbEl;

    mafList = mafLoadInRegion(conn, tdb->tableName, seqName, winStart, winEnd);
    safef(dbChrom, sizeof(dbChrom), "%s.%s", database, seqName);
    printf("<TT><PRE>");
    for (maf = mafList; maf != NULL; maf = maf->next)
        {
	struct mafAli *subset = mafSubset(maf, dbChrom, winStart, winEnd);
	if (subset != NULL)
	    {
	    struct mafComp *mc;
	    subset->score = mafScoreMultiz(subset);
	    slAddHead(&subList, subset);

	    /* Get a list of all databases used in any maf. */
	    for (mc = subset->components; mc != NULL; mc = mc->next)
	        {
		char dbOnly[64];
		strncpy(dbOnly, mc->src, sizeof(dbOnly));
		chopPrefix(dbOnly);
		slNameStore(&dbList, dbOnly);
		}
	    ++realCount;
	    }
	}
    slReverse(&subList);
    slReverse(&dbList);
    mafAliFreeList(&mafList);
    if (subList != NULL)
	{
	printf("Multiple alignments between");
	for (dbEl = dbList; dbEl != NULL; dbEl = dbEl->next)
	    {
	    char *org = hOrganism(dbEl->name);
	    tolowers(org);
	    if (dbEl != dbList)
	       {
	       if (dbEl->next == NULL)
		   printf(" and");
	       else
		   printf(",");
	       }
	    printf(" %s", org);
	    freez(&org);
	    }
	printf(".  The versions of each genome used are:");
	printf("<UL>\n");
	for (dbEl = dbList; dbEl != NULL; dbEl = dbEl->next)
	    {
	    char *db = dbEl->name;
	    char *org = hOrganism(db);
	    char *freeze = hFreezeFromDb(db);
	    printf("<LI><B>%s</B> - %s (%s)", org, freeze, db);
	    freez(&org);
	    freez(&freeze);
	    }
	printf("</UL>\n");
	for (maf = subList; maf != NULL; maf = maf->next)
	    {
	    printf("<B>Alignment %d of %d in window, score %0.1f</B>\n",
		    ++aliIx, realCount, maf->score);
	    mafPrettyOut(stdout, maf, 70);
	    }
	mafAliFreeList(&subList);
	}
    else
	{
        printf("No multiple alignment in browser window");
	}
    printf("</PRE></TT>");
    }
}

