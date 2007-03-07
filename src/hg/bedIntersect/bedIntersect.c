/* bedIntersect - Intersect two bed files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"

static char const rcsid[] = "$Id: bedIntersect.c,v 1.7 2007/03/07 01:46:42 angie Exp $";

static boolean aHitAny = FALSE;
static boolean bScore = FALSE;
static float aCoverage = 0.00001;

static struct optionSpec optionSpecs[] = {
    {"aHitAny", OPTION_BOOLEAN},
    {"bScore", OPTION_BOOLEAN},
    {"aCoverage", OPTION_FLOAT},
    {NULL, 0}
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedIntersect - Intersect two bed files\n"
  "usage:\n"
  "   bedIntersect a.bed b.bed output.bed\n"
  "options:\n"
  "   -aHitAny output all of a if any of it is hit by b\n"
  "   -aCoverage=0.N min coverage of b to output match.  Default .00001\n"
  "   -bScore output score from b.bed (must be at least 5 field bed)\n"
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

struct bed5 
/* A five field bed. */
    {
    struct bed5 *next;
    char *chrom;	/* Allocated in hash. */
    int start;		/* Start (0 based) */
    int end;		/* End (non-inclusive) */
    char *name;	/* Name of item */
    int score; /* Score - 0-1000 */
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
       bk = binKeeperNew(0, 1024*1024*1024);
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

struct hash *readBed5(char *fileName)
/* Read bed and return it as a hash keyed by chromName
 * with binKeeper values. */
{
char *row[5];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct binKeeper *bk;
struct hash *hash = newHash(0);
struct hashEl *hel;
struct bed5 *bed;

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
    bed->score = lineFileNeedNum(lf, row, 4);
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
struct hash *bHash = NULL;
struct lineFile *lf = lineFileOpen(aFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[40];
char *name;
int start, end, i, wordCount;
struct binElement *hitList = NULL, *hit;
struct binKeeper *bk;
struct bed5 *bed;

if (bScore)
    bHash = readBed5(bFile);
else
    bHash = readBed(bFile);
while ((wordCount = lineFileChop(lf, row)) != 0)
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
                {
                for (hit = hitList; hit != NULL; hit = hit->next)
                    {
                    float overlap = (float)positiveRangeIntersection(start, end, hit->start, hit->end);
                    if (overlap/(float)(end-start) > aCoverage)
                        {
                        fprintf(f, "%s\t%d\t%d", name, start, end);
                        for (i = 3 ; i<wordCount; i++)
                            {
                            if (bScore && i==4)
                                {
                                bed = hit->val;
                                fprintf(f, "\t%d",bed->score);
                                }
                            else
                                fprintf(f, "\t%s",row[i]);
                            }
                        fprintf(f, "\n");
                        break;
                        }
                    else
                        {
                        printf("filter out %s %d %d %d %d overlap %.1f %d %d %.3f\n",
                                name,start,end, hit->start,hit->end, overlap, end-start, hit->end-hit->start,overlap/(float)(end-start));
                        }
                    }
                }
	    }
	else
	    {
	    for (hit = hitList; hit != NULL; hit = hit->next)
	        {
		int s = max(start, hit->start);
		int e = min(end, hit->end);
                float overlap = (float)positiveRangeIntersection(start, end, hit->start, hit->end);
		if ((s < e) && (overlap/(float)(hit->end-hit->start) > aCoverage))
                    {
		    fprintf(f, "%s\t%d\t%d", name, s, e);
                    for (i = 3 ; i<wordCount; i++)
                        {
                        if (bScore && i==4)
                            {
                            bed = hit->val;
                            fprintf(f, "\t%d",bed->score);
                            }
                        else
                            fprintf(f, "\t%s",row[i]);
                        }
                    fprintf(f, "\n");
                    }
		}
	    }
	slFreeList(&hitList);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

aHitAny = optionExists("aHitAny");
bScore = optionExists("bScore");
aCoverage = optionFloat("aCoverage", aCoverage);
if (argc != 4)
    usage();
bedIntersect(argv[1], argv[2], argv[3]);
return 0;
}
