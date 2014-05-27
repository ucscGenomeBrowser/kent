/* liftAcross - convert one coordinate system to another, no overlapping items. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "genePred.h"
#include "sqlList.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftAcross - convert one coordinate system to another, no overlapping items\n"
  "usage:\n"
  "   liftAcross [options] liftAcross.lft srcFile.gp dstOut.gp\n"
  "required arguments:\n"
  "   liftAcross.lft - six column file relating src to destination\n"
  "   srcFile.gp - genePred input file to be lifted\n"
  "   dstOut.gp - genePred output file result\n"
  "options:\n"
  "   -bedOut=<regionMap.bed> - mappings from liftAcross.lft in bed format\n"
  "	          - this is a type bed 6+ file, where thickStart and thickEnd\n"
  "               - are the GeneScaffold coordinates, unrelated to the\n"
  "               - scaffold target coordinates."
  "   -warn - warnings only on broken items, not the default error exit\n"
  "The six columns in the liftAcross.lft file are:\n"
  "srcName  srcStart  srcEnd  dstName  dstStart dstStrand\n"
  "    the destination regions are the same size as the source regions.\n"
  "This only works with items that are fully contained in a\n"
  "   single source region.  First incarnation only lifts genePred files.\n"
  "Items not found in the liftAcross.lft file are merely passed along\n"
  "   unchanged."
  );
}

char *bedOut = NULL;	/*	default NULL == not happening */

static struct optionSpec options[] = {
   {"bedOut", OPTION_STRING},
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
    boolean outOfOrder;
    };

static void freeLiftedItem(struct liftedItem **pLi)
/* free up the space for a single item */
{
struct liftedItem *li = *pLi;
if (NULL == li)
    return;
freeMem(li->name);
freez(pLi);
}

static void freeLiftedItemList(struct liftedItem **pLi)
/* free up the space for all items on the list */
{
struct liftedItem *liList = *pLi;
if (NULL == liList)
    return;
struct liftedItem *li;
struct liftedItem *next;
for (li = liList; li; li = next)
    {
    next = li->next;
    freeLiftedItem(&li);
    }
}

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
 *	the start coordinate of each item.  The searching will expect
 *	them to be in order.
 */
struct hashCookie cookie = hashFirst(result);
while ((hel = hashNext(&cookie)) != NULL)
    {
    slSort(&(hel->val), lsStartCmp);
    if (verboseLevel() > 2)
	{
	struct liftSpec *ls;
	for (ls = hel->val; ls != NULL; ls = ls->next)
	    verbose(3, "# %s\t%d\t%d\t%s\t%d\t%c\n", hel->name, ls->start,
		ls->end, ls->dstName, ls->dstStart, ls->strand);
	}
    }

return result;
}

static int verifyCoordinates(int start1, int end1, int start2, int end2,
    char *name1, char *name2)
/* verify start1-end1 is completely contained in start2-end2
 *	return (when 'warn'):
 *	-1 == no overlap at all, completely disjoint
 *	1 == partially overlap
 *	0 == perfectly OK
 * name1 and name2 are along for the ride only for printout
 */
{
if ((end1 <= start2) || (start1 >= end2))
    {
    if (optionExists("warn"))
	warn("item %s:%d-%d can not be found in %s:%d-%d",
	    name1, start1, end1, name2, start2, end2);
    else
	errAbort("item %s:%d-%d can not be found in %s:%d-%d",
	    name1, start1, end1, name2, start2, end2);
    return -1;
    }
if ((start1 < start2) || (end1 > end2))
    {
    if (optionExists("warn"))
	warn("item %s:%d-%d not fully contained in %s:%d-%d",
	    name1, start1, end1, name2, start2, end2);
    else
	errAbort("item %s:%d-%d not fully contained in %s:%d-%d",
	    name1, start1, end1, name2, start2, end2);
    return 1;
    }
return 0;
}

static struct liftSpec *findLiftSpec(struct liftSpec *lsList, char *srcName,
    int start, int end)
/* see if the given item: start-end can be found in the liftSpec lsList
 *	if it can, return the liftSpec for that item
 *	if not found, return NULL
 * Verify sanity of lifting this item.  It needs to fall fully within
 *	the liftSpec found.  If not, it will not lift.
 * The srcName is used here merely for debug and error output
 *	information.  The given liftSpec is the liftSpec that belongs
 *	to this srcName as already found by the caller to findLiftSpec.
 */
{
struct liftSpec *ret = NULL;
struct liftSpec *ls = NULL;
for (ls = lsList; ls != NULL; ls = ls->next)
    {
    if (start < ls->end)	/* lift found when start < end */
	{
	if (verifyCoordinates(start, end, ls->start, ls->end,
		srcName, ls->dstName))
	    break;	/* skip any illegal ones */
	ret = ls;	/* this one is it, remember it and */
	break;		/* get out of here */
	}
    }
return (ret);
}

struct liftedItem *liftOne(struct liftSpec *lift,
	char *srcName, int start, int end)
/* Given an item: start-end
 * lift it via the given liftSpec to a liftedItem
 *	return NULL if not found in the liftSpec
 * The srcName is used here merely for debug and error output
 *	information.  The given liftSpec is the liftSpec that belongs
 *	to this srcName as already found by the caller to liftOne.
 */
{
struct liftedItem *result = NULL;
struct liftSpec *ls = findLiftSpec(lift, srcName, start, end);
if (ls)
    {
    AllocVar(result);
    result->name = cloneString(ls->dstName);
    result->strand = ls->strand;
    result->outOfOrder = FALSE;	/* can only be determined later */
    if ('+' == ls->strand)
	{
	result->start = start - ls->start + ls->dstStart;
	result->end = end - ls->start + ls->dstStart;
	}
    else
	{
	result->start = ls->end - end + ls->dstStart;
	result->end = ls->end - start + ls->dstStart;
	}
    verbose(3,"#\t%s:%d-%d -> %s:%d-%d %c\n", srcName, start, end,
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
struct hash *prevLift = newHash(8);
struct hashEl *hel = NULL;
int exonCount = gp->exonCount;
struct liftedItem *cdsStart = liftOne(ls, gp->chrom, gp->cdsStart,
	gp->cdsStart+1);
struct liftedItem *cdsEnd = liftOne(ls, gp->chrom, gp->cdsEnd-1, gp->cdsEnd);
int i;
boolean frames = FALSE;
boolean noCds = FALSE;

if (((0 == gp->cdsStart) && (0 == gp->cdsEnd)) || (gp->cdsStart == gp->cdsEnd))
    noCds = TRUE;
else if (NULL == cdsStart)
    warn("#\tWARNING: missing cdsStart for %s:%d\n", gp->chrom, gp->cdsStart);

if (((0 == gp->cdsStart) && (0 == gp->cdsEnd)) || (gp->cdsStart == gp->cdsEnd))
    noCds = TRUE;
else if (NULL == cdsEnd)
    warn("#\tWARNING: missing cdsEnd for %s:%d\n", gp->chrom, gp->cdsEnd);

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
	/* see if exons are getting mixed up, compare result with previous lift
	 *	on this scaffold */
	struct hashEl *prev = hashStore(prevLift, itemLift->name);
	if (prev->val)
	    {
	    struct liftedItem *prevLift = prev->val;
	    /* things are out of order if the strands are different, or
	     *	the items are not marching along in order.  - strand
	     *	results reverse the sense of "marching along"
	     */
	    if ( (prevLift->strand != itemLift->strand) ||
		(('-' == itemLift->strand) ?
			(itemLift->end > prevLift->start) :
			(prevLift->end > itemLift->start) ) )
		{
    		prevLift->outOfOrder = itemLift->outOfOrder = TRUE;
		}
	    }
	prev->val = itemLift;
	}
    }
freeHash(&prevLift);

/* Each scaffold name in the hash becomes a new genePred item all by itself */
struct hashCookie cookie = hashFirst(items);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct liftedItem *li = NULL;
    struct liftedItem *itemList = (struct liftedItem *)hel->val;
    struct genePred *gpItem = NULL;
    unsigned *exonStarts = NULL;
    unsigned *exonEnds = NULL;
    int itemExonCount = slCount(itemList);

    slSort(&itemList, liStartCmp);	/* sort the exons by start coord */
    AllocArray(exonStarts, itemExonCount);
    AllocArray(exonEnds, itemExonCount);
    AllocVar(gpItem);	/*	start this new genePred item	*/
    if (frames)
	AllocArray(gpItem->exonFrames, itemExonCount);
    gpItem->name = cloneString(gp->name);	/* gene name stays the same */
    gpItem->chrom = cloneString(hel->name);	/* new scaffold name */
    gpItem->exonCount = itemExonCount;		/* this many exons in this gp */
    gpItem->exonStarts = exonStarts;		/* an array of unsigned ints */
    gpItem->exonEnds = exonEnds;		/* an array of unsigned ints */
    gpItem->strand[0] = gp->strand[0];		/* assume lifted to + result */
    /* set result strand properly, lifted to - may reverse it */
    if ('-' == itemList->strand)		/* assume all on same strand */
	gpItem->strand[0] = ('+' == gp->strand[0]) ? '-' : '+';

    /* no need to set gpItem->strand[1]=0, is already zero from the AllocVar */

    int i = 0;
    boolean exonsOutOfOrder = FALSE;
    /* copy each new exon start and end to the new gpItem */
    for (li = itemList; li != NULL; li = li->next)
	{
	if (frames)
	    gpItem->exonFrames[i] = li->frame;
	exonStarts[i] = li->start;
	exonEnds[i++] = li->end;
	if (li->outOfOrder)
	    exonsOutOfOrder = TRUE;
	}
    /*	assign cdsStart,End appropriately.  txStart/End are always first
     *	and last exon extents no matter what.
     *	First, assume this scaffold is not the one with the cds bits, thus
     *	the first and last exon define these cds starts and ends.
     *	If this scaffold has one of those, then apply the actual cds bit.
     *	A negative strand lift result turns everything on its head.
     */
    gpItem->txStart = exonStarts[0];  /* assume first exon */
    gpItem->txEnd = exonEnds[itemExonCount-1];  /* assume last exon */
    if ('-' == itemList->strand)	/* assume all on same strand */
	{
	if (noCds)
	    {
	    gpItem->cdsStart = gpItem->txEnd;
	    gpItem->cdsEnd = gpItem->txEnd;
	    }
	else
	    {
	    gpItem->cdsStart = exonStarts[0];/* assume first exon */
	    gpItem->cdsEnd = exonEnds[itemExonCount-1];/* assume last exon */
	    if (cdsStart && sameWord(cdsStart->name, hel->name))
		gpItem->cdsEnd = cdsStart->start+1; /* this has the cdsEnd */
	    if (cdsEnd && sameWord(cdsEnd->name, hel->name))
		gpItem->cdsStart = cdsEnd->start;   /* this has the cdsStart */
	    /*	fix unusual (mixed up) mappings onto a scaffold */
	    if (exonsOutOfOrder)
		{
		gpItem->cdsStart = min(gpItem->cdsStart, exonStarts[0]);
		gpItem->cdsEnd = max(gpItem->cdsEnd,exonEnds[itemExonCount-1]);
		}
	    }
	}
    else
	{
	if (noCds)
	    {
	    gpItem->cdsEnd = gpItem->txEnd;
	    gpItem->cdsStart = gpItem->txEnd;
	    }
	else
	    {
	    gpItem->cdsStart = exonStarts[0];  /* assume first exon */
	    gpItem->cdsEnd = exonEnds[itemExonCount-1];/* assume last exon */
	    if (cdsStart && sameWord(cdsStart->name, hel->name))
		gpItem->cdsStart = cdsStart->start; /* this has the cdsStart */
	    if (cdsEnd && sameWord(cdsEnd->name, hel->name))
		gpItem->cdsEnd = cdsEnd->end;   /* this has the cdsEnd */
	    /*	fix unusual (mixed up) mappings onto a scaffold */
	    if (exonsOutOfOrder)
		{
		gpItem->cdsStart = min(gpItem->cdsStart, exonStarts[0]);
		gpItem->cdsEnd = max(gpItem->cdsEnd, exonEnds[itemExonCount-1]);
		}
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
    freeLiftedItemList((struct liftedItem **)(&hel->val));
    }	/*	while ((hel = hashNext(&cookie)) != NULL)	*/

freeHash(&items);

return (result);
}	/*	static struct genePred *liftGenePred()	*/

static void bedRegionOutput(struct hash *lftHash)
/* write the lift spec out as a bed file to map the regions lifted
 *	scores of the items are set at 1000 when they are in order,
 *	or 200 when they are out of order.
*/
{
FILE *bedFile = mustOpen(bedOut, "w");
struct hashEl *hel = NULL;
struct hashCookie cookie = hashFirst(lftHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct liftSpec *lsList = hel->val;
    struct liftSpec *ls = NULL;
    int itemCount = 0;
    struct liftSpec *prevLs = lsList;
    struct liftSpec *nextLs = prevLs->next;
    int size = prevLs->end - prevLs->start;
    int prevStart = prevLs->dstStart;
    int prevEnd = prevStart + size;
    char prevStrand = prevLs->strand;
    char *prevDstName = NULL;
    int nextStart = prevStart;
    int nextEnd = prevEnd;
    char nextStrand = prevStrand;
    char *nextDstName = NULL;

    for (ls = lsList; ls != NULL; ls = ls->next)
	{
	size = ls->end - ls->start;
	int dstEnd = ls->dstStart+size;
	int score = 1000;	/* assume everything is in order */
	nextLs = ls->next;
	if (nextLs)		/* if there is a next item */
	    {
	    size = nextLs->end - nextLs->start;
	    nextStart = nextLs->dstStart;
	    nextEnd = nextStart + size;
	    nextStrand = nextLs->strand;
	    nextDstName = nextLs->dstName;
	    }
	/* verify we are properly in order from previous item 
	 *	strands must be the same, and start/end in order
	 *	negative strand reverses meaning of "in order"
	 */
	if ( ! differentStringNullOk(prevDstName,ls->dstName) &&
		( (prevStrand != ls->strand) ||
	    (('-' == ls->strand) ? (dstEnd > prevStart) :
				    (prevEnd > ls->dstStart) ) ) )
	    {
	    score = 200;
	    }
	/* when possible, verify next item properly follows this item */
	if (nextLs)
	    {
	    if ( ! differentStringNullOk(nextDstName,ls->dstName) &&
		    ( (nextStrand != ls->strand) ||
		(('-' == ls->strand) ? (nextEnd > ls->dstStart) :
					(dstEnd > nextStart) ) ) )
		{
		score = 200;
		}
	    }
	fprintf(bedFile, "%s\t%d\t%d\t%s.%d\t%d\t%c\t%d\t%d\n",
	    ls->dstName, ls->dstStart, dstEnd, hel->name,
	    ++itemCount, score, ls->strand, ls->start, ls->end);
	prevStart = ls->dstStart;
	prevEnd = dstEnd;
	prevStrand = ls->strand;
	prevDstName = ls->dstName;
	}
    }
}

void liftAcross(char *liftAcross, char *srcFile, char *dstOut)
/* liftAcross - convert one coordinate system to another, no overlapping items. */
{
struct hash *lftHash = readLift(liftAcross);
struct genePred *gpList = genePredExtLoadAll(srcFile);
struct genePred *gp = NULL;
FILE *out = mustOpen(dstOut, "w");

if (bedOut)
    bedRegionOutput(lftHash);

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
	genePredFreeList(&gpLifted);
	}
    else
	{
	genePredTabOut(gp, out);
	}
    ++genePredItemCount;
    }
/* lftHash and gpList are left allocated to disappear at exit */
verbose(2,"#\tgene pred item count: %d\n", genePredItemCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedOut = optionVal("bedOut", bedOut);
liftAcross(argv[1], argv[2], argv[3]);
return 0;
}
