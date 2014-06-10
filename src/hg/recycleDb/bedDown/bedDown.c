/* bedDown - Make stuff to find a BED format submission in a new version. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "bed.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "agpFrag.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedDown - Make stuff to find a BED format submission in a new version\n"
  "usage:\n"
  "   bedDown database table output.fa output.tab\n");
}

struct agpFrag *loadChromAgp(struct sqlConnection *conn, char *chrom)
/* Load all AGP fragments for chromosome. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct agpFrag *fragList = NULL, *frag;

sqlSafef(query, sizeof query, "select * from %s_gold", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    frag = agpFragLoad(row);
    slAddHead(&fragList, frag);
    }
slReverse(&fragList);
return fragList;
}

boolean findCoveringFrag(int chromPos, struct agpFrag **pFragsLeft, struct agpFrag **retFrag, 
	int *retFragPos)
/* Find fragment holding chromPos. */
{
struct agpFrag *frag;

for (frag = *pFragsLeft; frag != NULL; frag = frag->next)
    {
    if (frag->chromStart <= chromPos && frag->chromEnd > chromPos)
        {
	*pFragsLeft = *retFrag = frag;
	*retFragPos = chromPos - frag->chromStart;
	return TRUE;
	}
    if (frag->chromStart > chromPos)
       {
       break;
       }
    }
return FALSE;
}

void bedDown(char *database, char *table, char *faName, char *tabName)
/* bedDown - Make stuff to find a BED format submission in a new version. */
{
char query[512];
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct bed bed;
static char nibChrom[64];
int nibStart = 0;
int nibSize = 0;
int nibEnd = 0;
int nibTargetSize = 512*1024;
struct dnaSeq *nibSeq = NULL;
int midPos;
int chromSize;
int s, e, sz;
FILE *fa = mustOpen(faName, "w");
FILE *tab = mustOpen(tabName, "w");
FILE *nib = NULL;
char nibFileName[512];
char seqName[512];
struct agpFrag *chromFragList = NULL, *frag, *fragsLeft = NULL;
int fragPos;
char *destName;
char *destStrand;


sqlSafef(query, sizeof query, "select chrom,chromStart,chromEnd,name from %s order by chrom,chromStart", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bedStaticLoad(row, &bed);

    /* Fix badly cased Exofish chromosomes mostly... */
    if (sameString(bed.chrom, "chrna_random"))
        bed.chrom = "chrNA_random";
    else if (sameString(bed.chrom, "chrul_random"))
        bed.chrom = "chrUL_random";
    else if (sameString(bed.chrom, "chrx_random"))
        bed.chrom = "chrX_random";
    else if (sameString(bed.chrom, "chry_random"))
        bed.chrom = "chrY_random";
    else if (sameString(bed.chrom, "chrx"))
        bed.chrom = "chrX";
    else if (sameString(bed.chrom, "chry"))
        bed.chrom = "chrY";

    if (!sameString(bed.chrom, nibChrom))
        {
	strcpy(nibChrom, bed.chrom);
	nibSize = nibStart = nibEnd = 0;
	sqlSafef(query, sizeof query, "select fileName from chromInfo where chrom = '%s'", bed.chrom);
	sqlQuickQuery(conn2, query, nibFileName, sizeof(nibFileName));
	carefulClose(&nib);
	nibOpenVerify(nibFileName, &nib, &chromSize);
	agpFragFreeList(&chromFragList);
	chromFragList = fragsLeft = loadChromAgp(conn2, bed.chrom);
	printf("%s has %d bases in %d fragments\n", nibFileName, chromSize, slCount(chromFragList));
	}
    midPos = (bed.chromStart + bed.chromEnd)/2;
    s = midPos - 200;
    if (s < 0) s = 0;
    e = midPos + 200;
    if (e > chromSize) e = chromSize;
    sz = e-s;
    if (rangeIntersection(s,e,nibStart,nibEnd) < sz)
        {
	freeDnaSeq(&nibSeq);
	nibStart = s;
	nibSize = nibTargetSize;
	if (nibSize < sz)
	    nibSize = sz;
	nibEnd = nibStart + nibSize;
	if (nibEnd > chromSize)
	    {
	    nibEnd = chromSize;
	    nibSize = nibEnd - nibStart;
	    }
	nibSeq = nibLdPart(nibFileName, nib, chromSize, nibStart, nibSize);
	}
    if (findCoveringFrag(midPos, &fragsLeft, &frag, &fragPos))
        {
	destName = frag->frag;
	destStrand = frag->strand;
	}
    else
	{
	destName = "?";
	fragPos = 0;
	destStrand = "+";
	warn("Couldn't find %s@%s:%d in agpFrag list", bed.name, bed.chrom, midPos);
	}
    fprintf(tab, "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\n", 
	    bed.name, database, bed.chrom, bed.chromStart, 
	    bed.chromEnd - bed.chromStart, s - bed.chromStart, e-bed.chromStart,
	    destName, destStrand, fragPos);
    sprintf(seqName, "%s.%s.%s.%d", bed.name, database, bed.chrom, 
	bed.chromStart);
    faWriteNext(fa, seqName, nibSeq->dna + s - nibStart, sz);
    }
freeDnaSeq(&nibSeq);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
sqlDisconnect(&conn2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
dnaUtilOpen();
bedDown(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
