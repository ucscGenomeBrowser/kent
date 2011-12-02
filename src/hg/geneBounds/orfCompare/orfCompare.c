/* orfCompare - Compare two sets of ORF predictions.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "orfCompare - Compare two sets of ORF predictions.\n"
  "usage:\n"
  "   orfCompare a.cds b.cds\n"
  "where a.cds and b.cds are both three column files of form\n"
  "  <name> <start> <end>\n"
  "options:\n"
  "   -bad=bad.tab - put in ones where a&b really disagree here\n"
  );
}

FILE *fBad = NULL;

static struct optionSpec options[] = {
   {"bad", OPTION_STRING},
   {NULL, 0},
};

struct orf
/* Information about ORF. */
    {
    struct orf *next;
    char *name;		/* Name of associated cDNA. */
    int start,end;	/* Half open interval covering ORF. */
    };

int orfCmp(const void *va, const void *vb)
/* Compare to sort based name, start, end. */
{
const struct orf *a = *((struct orf **)va);
const struct orf *b = *((struct orf **)vb);
int diff = strcmp(a->name, b->name);
if (diff == 0)
    diff = a->start - b->start;
return diff;
}

struct orf *orfLoadAll(char *fileName)
/* Load all orfs in file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
struct orf *list = NULL;
while (lineFileRow(lf, row))
    {
    struct orf *orf;
    AllocVar(orf);
    orf->name = cloneString(row[0]);
    orf->start = lineFileNeedNum(lf, row, 1);
    orf->end = lineFileNeedNum(lf, row, 2);
    slAddHead(&list, orf);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct hash *hashOrfList(struct orf *list)
/* Hash all orfs, making sure no dupes. */
{
struct hash *hash = hashNew(18);
struct orf *orf;
for (orf = list; orf != NULL; orf = orf->next)
    hashAddUnique(hash, orf->name, orf);
return hash;
}

void badOut(struct orf *a, struct orf *b, char *why)
/* Print out why a/b don't match */
{
if (fBad)
    {
    fprintf(fBad, "%s\t%d\t%d\t%d\t%d\t%s\n", a->name, a->start, a->end,
    	b->start, b->end, why);
    }
}

void orfCompare(char *aFile, char *bFile)
/* orfCompare - Compare two sets of ORF predictions.. */
{
struct orf *a, *aList = orfLoadAll(aFile);
struct orf *b, *bList = orfLoadAll(bFile);
struct hash *aHash = hashOrfList(aList);
struct hash *bHash = hashOrfList(bList);
int sharedCount = 0;
int sameCount = 0;
int sameFrameCount = 0;
int sameFrameOverlap80 = 0;
for (a = aList; a != NULL; a = a->next)
    {
    int aSize = a->end - a->start;
    if ((b = hashFindVal(bHash, a->name)) != NULL)
        {
	++sharedCount;
	if (a->start == b->start && a->end == b->end)
	    ++sameCount;
	else
	    {
	    if (a->start%3 == b->start%3)
	        {
		++sameFrameCount;
		int bSize = b->end - b->start;
		double overlap = positiveRangeIntersection(a->start, a->end, b->start, b->end);
		double aRatio = overlap/aSize;
		double bRatio = overlap/bSize;
		double ratio = min(aRatio,bRatio);
		if (ratio >= 0.80)
		    ++sameFrameOverlap80;
		else
		    {
		    char why[64];
		    safef(why, sizeof(why), "overlap %4.1f%%", 100.0*ratio);
		    badOut(a, b, why);
		    }
		}
	    else
	        badOut(a, b, "framed");
	    }
	}
    }
int totalOrfs = aHash->elCount + bHash->elCount - sharedCount;
int aUnique = aHash->elCount - sharedCount;
int bUnique = bHash->elCount - sharedCount;
int sameFrameLess = sameFrameCount - sameFrameOverlap80;
int otherFrame = sharedCount - sameFrameCount - sameCount;
printf("Comparing %s (a)  vs %s (b)\n", aFile, bFile);
printf("Total: %d\n", totalOrfs);
printf("Exact match: %d %5.2f%%\n", sameCount, 100.0*sameCount/totalOrfs);
printf("Same frame >80%% overlap: %d %5.2f%%\n", sameFrameOverlap80, 100.0*sameFrameOverlap80/totalOrfs);
printf("Same frame <80%% overlap: %d %5.2f%%\n", sameFrameLess, 100.0*sameFrameLess/totalOrfs);
printf("Different frames: %d %5.2f%%\n", otherFrame, 100.0*otherFrame/totalOrfs);
printf("Unique to a: %d %5.2f%%\n", aUnique, 100.0*aUnique/totalOrfs);
printf("Unique to b: %d %5.2f%%\n", bUnique, 100.0*bUnique/totalOrfs);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *fileName = optionVal("bad", NULL);
if (fileName != NULL)
    fBad = mustOpen(fileName, "w");
orfCompare(argv[1], argv[2]);
carefulClose(&fBad);
return 0;
}
