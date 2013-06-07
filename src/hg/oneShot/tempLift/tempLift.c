/* tempLift - Program to lift BED files temporarily while putting in centromeres.. */
#include "common.h"
#include "jksql.h"
#include "hash.h"
#include "linefile.h"
#include "ctgPos.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tempLift - Program to lift BED files temporarily while putting in centromeres.\n"
  "usage:\n"
  "   tempLift in.bed inserts out.bed\n");
}

struct hugeGap 
/* A huge gap to be inserted. */
    {
    struct hugeGap *next;	/* Next in list. */
    int offset;                 /* Offset in ungapped chromosome. */
    int size;                   /* Size of gap. */
    };

int cmpHugeGap(const void *va, const void *vb)
/* Compare to sort biggest offset first. */
{
const struct hugeGap *a = *((struct hugeGap **)va);
const struct hugeGap *b = *((struct hugeGap **)vb);
return b->offset - a->offset;
}

struct chromGaps
/* A list of huge gaps on a chromosome. */
    {
    struct chromGaps *next;
    char *chrom;              /* Name of chromosome, not allocated here. */
    struct hugeGap *gapList;  /* Sorted biggest offset first. */
    };

int gapOffset(struct chromGaps *gaps, int pos)
/* Convert from ungapped to gapped position. */
{
struct hugeGap *gap;
int offset = 0;

if (gaps != NULL)
    {
    for (gap = gaps->gapList; gap != NULL; gap = gap->next)
	{
	if (pos >= gap->offset)
	    {
	    offset += gap->size;
	    pos += gap->size;
	    }
	}
    }
return offset;
}

struct hash *hugeHash;

void setupHugeGaps(char *insertFile)
/* Setup things to lookup gaps. */
{
struct lineFile *lf;
char *words[8];
int wordCount;
struct chromGaps *chromList = NULL, *cg;
struct hugeGap *gap;
char *chrom;
char query[512];
struct sqlResult *sr;
char **row;
struct ctgPos ctgPos;
int start, size;
struct hashEl *hel;
struct sqlConnection *conn = sqlConnect("hg4");

hugeHash = newHash(6);
lf = lineFileOpen(insertFile, TRUE);
while ((wordCount = lineFileChop(lf, words)) != 0)
     {
     chrom = words[0];
     if (sameString(words[2], "-"))
         continue;
     if ((cg = hashFindVal(hugeHash, chrom)) == NULL)
         {
	 AllocVar(cg);
	 slAddHead(&chromList, cg);
	 hel = hashAdd(hugeHash, chrom, cg);
	 cg->chrom = hel->name;
	 }
     size = atoi(words[3]);
     sqlSafef(query, sizeof query, "select * from ctgPos where contig = '%s'", words[2]);
     sr = sqlGetResult(conn, query);
     if ((row = sqlNextRow(sr)) == NULL)
        errAbort("Couldn't find %s from %s in database", words[2], lf->fileName);
     ctgPosStaticLoad(row, &ctgPos);
     if (!sameString(chrom, ctgPos.chrom))
         errAbort("%s is in %s in database and %s in %s", ctgPos.contig, ctgPos.chrom,
	 	chrom, lf->fileName);
     start = ctgPos.chromStart;
     uglyf("%s %s (%d size %d) %s \n", chrom, words[1], start, size, words[2]);
     sqlFreeResult(&sr);

     AllocVar(gap);
     slAddHead(&cg->gapList, gap);
     gap->offset = start;
     gap->size = size;
     }
lineFileClose(&lf);
sqlDisconnect(&conn);
for (cg = chromList; cg != NULL; cg = cg->next)
    slSort(&cg->gapList, cmpHugeGap);
}


void tempLift(char *inName, char *insertsFile, char *outName)
/* tempLift - Program to lift BED files temporarily while putting in centromeres.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
int wordCount, i;
int start, end, offset;
char *chrom;
char *words[128];
int count = 0, liftCount = 0;
struct chromGaps *cg;

setupHugeGaps(insertsFile);
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    chrom = words[0];
    start = atoi(words[1]);
    end = atoi(words[2]);

    cg = hashFindVal(hugeHash, chrom);
    if (cg != NULL)
        {
	offset = gapOffset(cg, start);
	if (offset != 0)
	    {
	    start += offset;
	    end += offset;
	    liftCount += 1;
	    }
	}

    fprintf(f, "%s\t%d\t%d", chrom, start, end);
    for (i=3; i<wordCount; ++i)
        fprintf(f, "\t%s", words[i]);
    fprintf(f, "\n");
    ++count;
    }
printf("Lifted %d of %d lines of %s to %s\n", liftCount, count, inName, outName);
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
tempLift(argv[1], argv[2], argv[3]);
return 0;
}
