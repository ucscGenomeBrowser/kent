/* liftAcross - convert one coordinate system to another, no overlapping items. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "genePred.h"

static char const rcsid[] = "$Id: liftAcross.c,v 1.3 2008/02/13 23:08:27 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftAcross - convert one coordinate system to another, no overlapping items\n"
  "usage:\n"
  "   liftAcross liftAcross.txt srcFile.gp dstOut.gp\n"
  "options:\n"
  "   liftAcross.txt - six column file relating src to destination\n"
  "   srcFile.gp - genePred input file to be lifted\n"
  "   dstOut.gp - genePred output file result\n"
  "The six columns in the liftAcross.txt file are:\n"
  "srcName  srcStart  srcEnd  dstName  dstStart dstStrand\n"
  "    the destination regions are the same size as the source regions.\n"
  "First incarnation only works with items that are fully contained in a\n"
  "   single source region.  First incarnation only lifts genePred files.\n"
  "Items not found in the liftAcross.txt file are merely passed along\n"
  "   unchanged."
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct liftSpec
/* define one region lift */
    {
    struct liftSpec *next;
    int start;
    int end;
    char *dstName;
    int dstStart;
    char strand;
    };

struct liftedItem
/* the result of lifting a single item */
    {
    struct liftedItem *next;
    char *name;
    int start;
    int end;
    char strand;
    };

static int lsStartCmp(const void *va, const void *vb)
/* Compare a sort based on start for a liftSpec item. */
{
const struct liftSpec *a = *((struct liftSpec **)va);
const struct liftSpec *b = *((struct liftSpec **)vb);
int dif;
dif = a->start - b->start;
return dif;
}

static int liStartCmp(const void *va, const void *vb)
/* Compare a sort based on start for a liftedItem. */
{
const struct liftedItem *a = *((struct liftedItem **)va);
const struct liftedItem *b = *((struct liftedItem **)vb);
int dif;
dif = a->start - b->start;
return dif;
}

static struct hash *readLift(char *liftAcross)
/* read in liftAcross file, create hash of srcName as hash key,
 *	hash elements are simple lists of coordinate relationships
 *	return them all sorted by start position
 */
{
char *row[6];
struct hash *result = newHash(8);
struct hashEl *hel = NULL;
struct lineFile *lf = lineFileOpen(liftAcross, TRUE);
while (lineFileNextRow(lf, row, ArraySize(row)))
    {
    struct liftSpec *liftSpec;
    hel = hashStore(result, row[0]);
    AllocVar(liftSpec);
    liftSpec->start = sqlUnsigned(row[1]);
    liftSpec->end = sqlUnsigned(row[2]);
    liftSpec->dstName = cloneString(row[3]);
    liftSpec->dstStart = sqlUnsigned(row[4]);
    liftSpec->strand = '+';
    if ('-' == *row[5]) liftSpec->strand = '-';
    slAddHead(&(hel->val), liftSpec);
    }
int hashItemCount = 0;
int liftItemCount = 0;
struct hashCookie cookie = hashFirst(result);
while ((hel = hashNext(&cookie)) != NULL)
    {
    slSort(&(hel->val), lsStartCmp);
    ++hashItemCount;
    liftItemCount += slCount(hel->val);
    if (verboseLevel() > 2)
	{
	struct liftSpec *ls;
	for (ls = hel->val; ls != NULL; ls = ls->next)
	    verbose(3, "# %s\t%d\t%d\t%s\t%d\t%c\n", hel->name, ls->start,
		ls->end, ls->dstName, ls->dstStart, ls->strand);
	}
    }
verbose(2,"#\tsource names count: %d, lift item count: %d\n",
	hashItemCount, liftItemCount);

return result;
}

struct liftSpec *findLiftSpec(struct liftSpec *lift, char *name,
    int start, int end)
{
struct liftSpec *ret = NULL;
struct liftSpec *ls;
for (ls = lift; ls != NULL; ls = ls->next)
    {
    if (start < ls->end)
	{
	if (end > ls->end)
	    errAbort("item %s:%d-%d not fully contained in %s:%d-%d",
		name, start, end, ls->dstName, ls->start, ls->end);
	verbose(3,"# lift %s:%d-%d to %s:%d-%d\n", name, start, end,
		ls->dstName, ls->start, ls->end);
	ret = ls;
	break;
	}
    }
return (ret);
}

struct liftedItem *liftOne(struct liftSpec *lift,
	char *name, int start, int end)
{
struct liftedItem *result;
struct liftSpec *ls = findLiftSpec(lift, name, start, end);
AllocVar(result);
result->name = cloneString(ls->dstName);
result->strand = ls->strand;
if ('+' == ls->strand)
    {
    result->start = start - ls->start + ls->dstStart;
    result->end = end - ls->start + ls->dstStart;
    }
else
    {
    result->start = ls->end - 1 - start + ls->dstStart;
    result->end = ls->end - 1 - end + ls->dstStart;
    }
return result;
}

static struct genePred *liftGenePred(struct genePred *gp, struct liftSpec *ls)
{
struct genePred *result = NULL;
struct hash *items = newHash(8);
struct hashEl *hel = NULL;
int exonCount = gp->exonCount;
struct liftedItem *txStart = liftOne(ls, gp->chrom, gp->txStart, gp->txStart+1);
struct liftedItem *txEnd = liftOne(ls, gp->chrom, gp->txEnd, gp->txEnd-1);
struct liftedItem *cdsStart = liftOne(ls, gp->chrom, gp->cdsStart,
	gp->cdsStart+1);
struct liftedItem *cdsEnd = liftOne(ls, gp->chrom, gp->cdsEnd, gp->cdsEnd-1);
int i;

for (i = 0; i < exonCount; ++i)
    {
    struct liftedItem *itemLift = liftOne(ls, gp->chrom,
	gp->exonStarts[i], gp->exonEnds[i]);
    hel = hashStore(items, itemLift->name);
    slAddHead(&(hel->val), itemLift);
    }

struct hashCookie cookie = hashFirst(items);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct liftedItem *li;
    struct liftedItem *itemList = (struct liftedItem *)hel->val;
    struct genePred *gpItem;
    slSort(&itemList, liStartCmp);
    int exonCount = slCount(itemList);
    unsigned *exonStarts = NULL;
    unsigned *exonEnds = NULL;
    AllocArray(exonStarts, exonCount);
    AllocArray(exonEnds, exonCount);
    AllocVar(gpItem);
    gpItem->name = cloneString(gp->name);
    gpItem->chrom = cloneString(hel->name);
    gpItem->exonCount = exonCount;
    gpItem->exonStarts = exonStarts;
    gpItem->exonEnds = exonEnds;
    int i = 0;
    li = itemList;
    gpItem->strand[0] = li->strand;
    gpItem->strand[1] = 0;
    for ( ; li != NULL; li = li->next)
	{
	exonStarts[i] = li->start;
	exonEnds[i++] = li->end;
	}
    if (sameWord(txStart->name, hel->name))
	gpItem->txStart = txStart->start;   /* this guy has the txStart */
    else
	gpItem->txStart = itemList->start;  /* first exon */
    if (sameWord(txEnd->name, hel->name))
	gpItem->txEnd = txEnd->end;   /* this guy has the txEnd */
    else
	gpItem->txEnd = exonEnds[exonCount-1];    /* last exon */
    if (sameWord(cdsStart->name, hel->name))
	gpItem->cdsStart = cdsStart->start;   /* this guy has the cdsStart */
    else
	gpItem->cdsStart = itemList->start;  /* first exon */
    if (sameWord(cdsEnd->name, hel->name))
	gpItem->cdsEnd = cdsEnd->end;   /* this guy has the cdsEnd */
    else
	gpItem->cdsEnd = exonEnds[exonCount-1];    /* last exon */
#ifdef NOT
    gpItem->optFields = gp->optFields;
    if (gpItem->optFields & genePredName2Fld)
	{
	if (gp->name2)
	    gpItem->name2 = cloneString(gp->name2);
	else
	    gpItem->name2 = cloneString("");
	}
    if (gpItem->optFields & genePredCdsStatFld)
	{
	gpItem->cdsStartStat = gp->cdsStartStat;
	gpItem->cdsEndStat = gp->cdsEndStat;
	}
#endif

    slAddHead(&result, gpItem);
    }
/* would be good to free the lifted items here */

/*result = gp;*/
return (result);
}	/*	static struct genePred *liftGenePred()	*/


void liftAcross(char *liftAcross, char *srcFile, char *dstOut)
/* liftAcross - convert one coordinate system to another, no overlapping items. */
{
struct hash *lftHash = readLift(liftAcross);
struct genePred *gpList = genePredExtLoadAll(srcFile);
struct genePred *gp = NULL;
FILE *out = mustOpen(dstOut, "w");

int genePredItemCount = 0;
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    struct liftSpec *lsFound = hashFindVal(lftHash, gp->chrom);
    if (lsFound)
	{
	struct genePred *gpLifted = liftGenePred(gp, lsFound);
	struct genePred *gpl;
	for (gpl = gpLifted; gpl != NULL; gpl = gpl->next)
	    genePredTabOut(gpl, out);
	verbose(1,"# found: %s\n", gp->chrom);
	}
    else
	{
	verbose(1,"# not found: %s\n", gp->chrom);
	genePredTabOut(gp, out);
	}
    ++genePredItemCount;
    }
verbose(2,"#\tgene pred item count: %d\n", genePredItemCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
liftAcross(argv[1], argv[2], argv[3]);
return 0;
}
