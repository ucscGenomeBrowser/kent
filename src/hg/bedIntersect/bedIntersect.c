/* bedIntersect - Intersect two bed files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"

boolean aHitAny = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedIntersect - Intersect two bed files\n"
  "usage:\n"
  "   bedIntersect a.bed b.bed output.bed\n"
  "options:\n"
  "   -aHitAny output all of a if any of it is hit by b\n"
  );
}

struct bed3 
/* A three field bed. */
    {
    struct bed3 *next;
    char *chrom;	/* Allocated in hash. */
    int start;		/* Start (0 based) */
    int end;		/* End (non-inclusive) */
    };

struct hash *readBed(char *fileName)
/* Read bed and return it as a hash keyed by chromName
 * with binKeeper values. */
{
char *row[3];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct binKeeper *bk;
struct hash *hash = newHash(0);
struct hashEl *hel;
struct bed3 *bed;

while (lineFileRow(lf, row))
    {
    hel = hashLookup(hash, row[0]);
    if (hel == NULL)
       {
       bk = binKeeperNew(0, 511*1024*1024);
       hel = hashAdd(hash, row[0], bk);
       }
    bk = hel->val;
    AllocVar(bed);
    bed->chrom = hel->name;
    bed->start = lineFileNeedNum(lf, row, 1);
    bed->end = lineFileNeedNum(lf, row, 2);
    if (bed->start > bed->end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    binKeeperAdd(bk, bed->start, bed->end, bed);
    }
lineFileClose(&lf);
return hash;
}

void bedIntersect(char *aFile, char *bFile, char *outFile)
/* bedIntersect - Intersect two bed files. */
{
struct hash *bHash = readBed(bFile);
struct lineFile *lf = lineFileOpen(aFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[3];
char *name;
int start, end;
struct binElement *hitList = NULL, *hit;
struct binKeeper *bk;

while (lineFileRow(lf, row))
    {
    name = row[0];
    start = lineFileNeedNum(lf, row, 1);
    end = lineFileNeedNum(lf, row, 2);
    if (start > end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    bk = hashFindVal(bHash, name);
    if (bk != NULL)
	{
	hitList = binKeeperFind(bk, start, end);
	if (aHitAny)
	    {
	    if (hitList != NULL)
	        fprintf(f, "%s\t%d\t%d\n", name, start, end);
	    }
	else
	    {
	    for (hit = hitList; hit != NULL; hit = hit->next)
	        {
		int s = max(start, hit->start);
		int e = min(end, hit->end);
		if (s < e)
		    fprintf(f, "%s\t%d\t%d\n", name, s, e);
		}
	    }
	slFreeList(&hitList);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
aHitAny = optionExists("aHitAny");
if (argc != 4)
    usage();
bedIntersect(argv[1], argv[2], argv[3]);
return 0;
}
