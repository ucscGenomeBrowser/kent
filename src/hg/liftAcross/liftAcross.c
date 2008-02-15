/* liftAcross - convert one coordinate system to another, no overlapping items. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "genePred.h"
#include "sqlList.h"

static char const rcsid[] = "$Id: liftAcross.c,v 1.5 2008/02/15 00:41:07 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftAcross - convert one coordinate system to another, no overlapping items\n"
  "usage:\n"
  "   liftAcross [options] liftAcross.txt srcFile.gp dstOut.gp\n"
  "required arguments:\n"
  "   liftAcross.txt - six column file relating src to destination\n"
  "   srcFile.gp - genePred input file to be lifted\n"
  "   dstOut.gp - genePred output file result\n"
  "options:\n"
  "   -warn - warnings only on broken items, not the default error exit\n"
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
   {"warn", OPTION_BOOLEAN},
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
    int frame;
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
    hel = hashStore(result, row[0]);		/* srcName hash	*/
    AllocVar(liftSpec);
    liftSpec->start = sqlUnsigned(row[1]);	/* src start	*/
    liftSpec->end = sqlUnsigned(row[2]);	/* src end	*/
    liftSpec->dstName = cloneString(row[3]);	/* dstName	*/
    liftSpec->dstStart = sqlUnsigned(row[4]);	/* dst start	*/
    liftSpec->strand = '+';			/* dst strand	*/
    if ('-' == *row[5])
	liftSpec->strand = '-';
    /* accumulate list of lift specs under the srcName hash	*/
    slAddHead(&(hel->val), liftSpec);
    }

/*	Go through each srcName in the hash, and sort the list there by
 *	the start coordinate of each item
 */
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

struct liftSpec *findLiftSpec(struct liftSpec *lsList, char *name,
    int start, int end)
/* see if the given item: name:start-end can be found in the liftSpec lsList
 *	if it can, return the liftSpec for that item
 *	if not found, return NULL
 * Verify sanity of lifting this item.  It needs to fall fully within
 *	the liftSpec found.  If not, it will not lift.
 */
{
struct liftSpec *ret = NULL;
struct liftSpec *ls;
for (ls = lsList; ls != NULL; ls = ls->next)
    {
    if (start < ls->end)
	{
	if (end > ls->end)
	    errAbort("item %s:%d-%d not fully contained in %s:%d-%d",
		name, start, end, ls->dstName, ls->start, ls->end);
        if (end < ls->start)
	    {
	    if (optionExists("warn"))
		{
		warn("item %s:%d-%d can not be found in %s:%d-%d",
		    name, start, end, ls->dstName, ls->start, ls->end);
		break;
		}
	    else
		errAbort("item %s:%d-%d can not be found in %s:%d-%d",
		    name, start, end, ls->dstName, ls->start, ls->end);
	    }
	verbose(4,"# lift %s:%d-%d to %s:%d-%d\n", name, start, end,
		ls->dstName, ls->start, ls->end);
	ret = ls;
	break;
	}
    }
return (ret);
}

struct liftedItem *liftOne(struct liftSpec *lift,
	char *name, int start, int end)
/* Given an item: name:start-end
 * lift it via the given liftSpec to a liftedItem
 *	return NULL if not found in the liftSpec
 */
{
struct liftedItem *result = NULL;
struct liftSpec *ls = findLiftSpec(lift, name, start, end);
if (ls)
    {
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
	result->start = ls->end - 1 - end + ls->dstStart;
	result->end = ls->end - 1 - start + ls->dstStart;
	}
    verbose(3,"#\t%s:%d-%d -> %s:%d-%d %c\n", name, start, end,
	result->name, result->start, result->end, result->strand);
    }
return result;
}

static struct genePred *liftGenePred(struct genePred *gp, struct liftSpec *ls)
/* lift the specified genePred gp via the given liftSpec ls
 *	result is a list of genePred items on various other scaffolds
 */
{
struct genePred *result = NULL;
struct hash *items = newHash(8);
struct hashEl *hel = NULL;
int exonCount = gp->exonCount;
struct liftedItem *txStart = liftOne(ls, gp->chrom, gp->txStart, gp->txStart+1);
struct liftedItem *txEnd = liftOne(ls, gp->chrom, gp->txEnd-1, gp->txEnd);
struct liftedItem *cdsStart = liftOne(ls, gp->chrom, gp->cdsStart,
	gp->cdsStart+1);
struct liftedItem *cdsEnd = liftOne(ls, gp->chrom, gp->cdsEnd-1, gp->cdsEnd);
int i;
boolean frames = FALSE;
boolean noCds = FALSE;

if (NULL == txStart)
    warn("#\tWARNING: missing txStart for %s:%d\n", gp->chrom, gp->txStart);
if (NULL == txEnd)
    warn("#\tWARNING: missing txEnd for %s:%d\n", gp->chrom, gp->txEnd);
if (NULL == cdsStart)
    warn("#\tWARNING: missing cdsStart for %s:%d\n", gp->chrom, gp->cdsStart);
else if ((0 == gp->cdsStart) && (0 == gp->cdsEnd))
    noCds = TRUE;
if (NULL == cdsEnd)
    warn("#\tWARNING: missing cdsEnd for %s:%d\n", gp->chrom, gp->cdsEnd);
else if ((0 == gp->cdsStart) && (0 == gp->cdsEnd))
    noCds = TRUE;

if (gp->optFields & genePredExonFramesFld)
    frames = TRUE;

/* lift each exon.  Some do not lift, their lift specification may be
 * missing, ignore them.  For each lifted exon, keep it on a hash list by the
 *	lifted scaffold name
 */
for (i = 0; i < exonCount; ++i)
    {
    struct liftedItem *itemLift = liftOne(ls, gp->chrom,
	gp->exonStarts[i], gp->exonEnds[i]);
    if (itemLift)
	{
	if (frames)
	    itemLift->frame = gp->exonFrames[i];
	hel = hashStore(items, itemLift->name);
	slAddHead(&(hel->val), itemLift);
	}
    }

/* Each scaffold name in the hash becomes a new genePred item all by itself */
struct hashCookie cookie = hashFirst(items);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct liftedItem *li;
    struct liftedItem *itemList = (struct liftedItem *)hel->val;
    struct genePred *gpItem;
    slSort(&itemList, liStartCmp);	/* sort the exons by start coord */
    int itemExonCount = slCount(itemList);
    unsigned *exonStarts = NULL;
    unsigned *exonEnds = NULL;
    AllocArray(exonStarts, itemExonCount);
    AllocArray(exonEnds, itemExonCount);
    AllocVar(gpItem);	/*	start this new genePred item	*/
    if (frames)
	AllocArray(gpItem->exonFrames, itemExonCount);
    gpItem->name = cloneString(gp->name);	/* gene name stays the same */
    gpItem->chrom = cloneString(hel->name);	/* new scaffold name */
    gpItem->exonCount = itemExonCount;		/* this many exons in this gp */
    gpItem->exonStarts = exonStarts;
    gpItem->exonEnds = exonEnds;
    int i = 0;
    li = itemList;
    gpItem->strand[0] = li->strand;
    /* gpItem->strand[1] = 0; is already zero from the AllocVar */
    /* copy each new exon start and end to the new gpItem */
    for ( ; li != NULL; li = li->next)
	{
	if (frames)
	    gpItem->exonFrames[i] = li->frame;
	exonStarts[i] = li->start;
	exonEnds[i++] = li->end;
	}
    /*	assign txStart,End and cdsStart,End appropriately
     *	First, assume this scaffold is not the one with these items, thus
     *	the first and last exon define these starts and ends.
     *	If this scaffold has one of those, then apply the actual items.
     *	The negative strand turns everything on its head.
     */
    if ('-' == gpItem->strand[0])
	{
	gpItem->txEnd = exonEnds[itemExonCount-1];  /* assume last exon */
	gpItem->txStart = exonStarts[0];  /* assume first exon */
	if (txStart && sameWord(txStart->name, hel->name))
	    gpItem->txEnd = txStart->start+1;   /* this guy has the txEnd */
	if (txEnd && sameWord(txEnd->name, hel->name))
	    gpItem->txStart = txEnd->start;   /* this guy has the txStart */

	if (noCds)
	    {
	    gpItem->cdsEnd = 0;
	    gpItem->cdsStart = 0;
	    }
	else
	    {
	    gpItem->cdsEnd = exonEnds[itemExonCount-1];/* assume last exon */
	    gpItem->cdsStart = exonStarts[0];/* assume first exon */
	    if (cdsStart && sameWord(cdsStart->name, hel->name))
		gpItem->cdsEnd = cdsStart->start+1; /* this has the cdsEnd */
	    if (cdsEnd && sameWord(cdsEnd->name, hel->name))
		gpItem->cdsStart = cdsEnd->start;   /* this has the cdsStart */
	    }
	}
    else
	{
	gpItem->txStart = exonStarts[0];  /* assume first exon */
	gpItem->txEnd = exonEnds[itemExonCount-1];/* assume last exon */
	if (txStart && sameWord(txStart->name, hel->name))
	    gpItem->txStart = txStart->start;   /* this guy has the txStart */
	if (txEnd && sameWord(txEnd->name, hel->name))
	    gpItem->txEnd = txEnd->end;   /* this guy has the txEnd */

	if (noCds)
	    {
	    gpItem->cdsEnd = 0;
	    gpItem->cdsStart = 0;
	    }
	else
	    {
	    gpItem->cdsStart = itemList->start;  /* assume first exon */
	    gpItem->cdsEnd = exonEnds[itemExonCount-1];/* assume last exon */
	    if (cdsStart && sameWord(cdsStart->name, hel->name))
		gpItem->cdsStart = cdsStart->start; /* this has the cdsStart */
	    if (cdsEnd && sameWord(cdsEnd->name, hel->name))
		gpItem->cdsEnd = cdsEnd->end;   /* this has the cdsEnd */
	    }
	}

    /*	if optional fields present, carry them along to the new item */
    gpItem->optFields = gp->optFields;
    if (gpItem->optFields & genePredScoreFld)
	gpItem->score = gp->score;
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

    /* accumulating a list of genePred items for the final result */
    slAddHead(&result, gpItem);
    }
/* XXX would be good to free the lifted items here, and their container hash */

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
	/* XXX it would be good to free the gpLifted item here */
	}
    else
	{
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
