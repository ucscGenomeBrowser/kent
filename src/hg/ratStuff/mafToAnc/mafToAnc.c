/* mafToAnc - Find and report anchors in a maf file.  Anchors are defined
 *   as ungapped alignments with a minimum length of 100bp.  The user
 *   can utilize the -minLen paramater to change the minimum ungapped
 *   length required to define an anchor. */

#include "common.h"
#include "errabort.h"
#include "hash.h"
#include "linefile.h"
#include "maf.h"
#include "obscure.h"
#include "options.h"
#include "seg.h"

#include <limits.h>	/* For CHAR_BIT */


static char const rcsid[] = "$Id: mafToAnc.c,v 1.7 2008/04/25 17:25:15 rico Exp $";

struct aliCont
/* A container for an alignment block. */
{
	struct aliCont *next;
	struct mafAli *ali;		/* The alignment block. */
	int compCount;			/* Count of components in the alignment. */
};

struct speciesInfo {
/* Species information used for determining breaks. */
	char *chrom;	/* Name of chromosome. */
	int start;		/* Start of segment. Zero based. If strand is - it is
					   relative to source end. */
	int size;		/* Size of segment. */
	char strand;	/* Strand of segment. Either + or -. */
	int srcSize;	/* Size of segment chromosome. */
	char order;		/* Relative order. Either + or - or 0. */
};

static struct optionSpec options[] = {
	{"coreSpecies", OPTION_STRING},
	{"minLen", OPTION_INT},
	{"noCheckSrc", OPTION_BOOLEAN},
	{"noCheckStrand", OPTION_BOOLEAN},
	{"noCheckCoverage", OPTION_BOOLEAN},
	{NULL, 0},
};

static char    *coreSpecies    = NULL;
static int     minLen          = 100;
static boolean noCheckSrc      = FALSE;
static boolean noCheckStrand   = FALSE;
static boolean noCheckCoverage = FALSE;

static struct hash *coreSpeciesHash = NULL;

static unsigned int uint_bits = sizeof(unsigned int) * CHAR_BIT;
static unsigned int uint_div  = 0U;
static unsigned int uint_rem  = 0U;
static unsigned int int_bits  = sizeof(int) * CHAR_BIT;
static unsigned int *bitMap   = NULL;
static unsigned int emptyUInt = 0U;
static unsigned int fullUInt  = ~0U;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToAnc - Find anchors in a maf file\n"
  "usage:\n"
  "   mafToAnc in.maf out.anc\n"
  "arguments:\n"
  "   in.maf   name of the input maf file\n"
  "   out.anc  name of the output anchor file\n"
  "options:\n"
  "   -coreSpecies=spe1,...,speN\n"
  "                     Use only the listed species when building anchors\n"
  "   -minLen=<D>       Minimum length of an ungapped alignment to define an\n"
  "                     anchor.  (default = 100)\n"
  "   -noCheckSrc       Don't check that the src of the first component of\n"
  "                     every alignment block is the same.\n"
  "   -noCheckStrand    Don't check that the first component of every\n"
  "                     alignment block is on the + strand.\n"
  "   -noCheckCoverage  Don't check for single coverage of the first\n"
  "                     component.\n"
  );
}

static void freeSpeciesInfo(void **pObj)
/* Free a speciesInfo struct. */
{
struct speciesInfo *si = *pObj;

freeMem(si->chrom);
freeMem(si);

*pObj = NULL;
}

static struct segBlock *buildAnchorBlock(int len, struct mafAli *ma, int *cp)
/* Build and anchor from the argument data. */
{
struct mafComp *mc;
struct segBlock *sb;
struct segComp *sc, *tail = NULL;
int compIdx;

/* Initialize the anchor block. */
AllocVar(sb);
sb->val = len;

for (mc = ma->components, compIdx = 0; mc != NULL; mc = mc->next, compIdx++)
	{
	/* Initialize a new anchor component. */
	AllocVar(sc);
	sc->src     = cloneString(mc->src);
	sc->start   = cp[compIdx] - len;
	sc->size    = len;
	sc->strand  = mc->strand;
	sc->srcSize = mc->srcSize;

	/* Add the new component to the current list. */
	if (sb->components == NULL)
		sb->components = sc;
	else
		tail->next = sc;
	tail = sc;
	}

return(sb);
}


static struct segBlock *buildBreakBlock(struct mafAli *ma)
/* Build a break block. */
{
struct mafComp *mc;
struct segBlock *sb;
struct segComp *sc, *tail = NULL;

/* Initialize the break block. */
AllocVar(sb);
sb->name = cloneString("chromBreak");

for (mc = ma->components; mc != NULL; mc = mc->next)
	{
	/* Initialize a new segment component. */
	AllocVar(sc);
	sc->src     = cloneString(mc->src);
	sc->start   = mc->start;
	sc->size    = mc->size;
	sc->strand  = mc->strand;
	sc->srcSize = mc->srcSize;

	/* Add the new segment component to the current list. */
	if (sb->components == NULL)
		sb->components = sc;
	else
		tail->next = sc;
	tail = sc;
	}

return(sb);
}


int hashPOTSize(int count)
/* Return the power of two size needed for a hash with count elements. */
{
int left = count - 1, right = count - 1, shifts = 0;

if (count <= 0)
	return(0);

NEXT_SHIFT:
if (left < 0)
	return(int_bits - shifts);

if (right == 0)
	return(shifts);

shifts  += 1;
left   <<= 1;
right  >>= 1;

goto NEXT_SHIFT;
}

static int checkForBreak(struct mafComp *mc, struct hash *speciesHash)
/* Check for a break in chromosome.  I may expand this in the future to
   check fo breaks in order and orientation as well. */
{
char *p = NULL;
struct hashEl *hel;
struct speciesInfo *si;
int foundBreak = 0, copyData = 0;

/* Temporarily split src into species and chrom. */
if ((p = strchr(mc->src, '.')) != NULL)
	*p = '\0';

if ((hel = hashLookup(speciesHash, mc->src)) == NULL)
	{
	AllocVar(si);
	hel = hashAdd(speciesHash, mc->src, (void *) si);
	copyData = 1;
	}
else
	{
	si = (struct speciesInfo *) hel->val;

	/* Chromosomal break. */
	if ((p && !sameString(si->chrom, p + 1)) || (!p && si->chrom != NULL))
		{
		freeMem(si->chrom);
		si->chrom = NULL;
		foundBreak = 1;
		}
	}

if (foundBreak || copyData)
	{
	if (p)
		si->chrom = cloneString(p + 1);
	si->start   = mc->start;
	si->size    = mc->size;
	si->strand  = mc->strand;
	si->srcSize = mc->srcSize;
	}

/* Restore src. */
if (p)
	*p = '.';

return(foundBreak);
}


static void findAnchors(struct aliCont **acList, char *outAnc, int maxComps)
/* Find and report anchors. */
{
struct aliCont *ac, *next;
struct mafAli *ma;
struct mafComp *mc;
int idx, len, compIdx, alignBreak, gapCol = 0;
int *compPos;
struct segBlock *block;
struct hash *speciesHash = newHash( hashPOTSize(maxComps) );
FILE *f = mustOpen(outAnc, "w");

/* Allocate the compPos array. */
AllocArray(compPos, maxComps);

segWriteStart(f);

/* Find and report anchors. */
for (ac = *acList; ac != NULL; ac = next)
	{
	next = ac->next;
	ma = ac->ali;
	idx = len = 0;

	/* Initialize positions for each component; check for chromosomal
	   breaks. */
	alignBreak = 0;
	for (mc = ma->components, compIdx = 0; mc != NULL; mc = mc->next)
		{
		compPos[compIdx++] = mc->start;
		if (alignBreak == 0)
			alignBreak = checkForBreak(mc, speciesHash);
		}

	/* */
	if (alignBreak)
		{
		block = buildBreakBlock(ma);
		segWrite(f, block);
		segBlockFree(&block);
		}

	/* Check this container's alignment for anchors. */
	while (idx < ma->textSize)
		{
		/* Check to see if there is a gap in this column. */
		gapCol = 0;
		for (mc = ma->components; mc != NULL; mc = mc->next)
			if (mc->text[idx] == '-')
				{
				gapCol = 1;
				break;
				}

		if (! gapCol)
			len++;
		else
			{
			/* Possibly report an anchor. */
			if (len >= minLen)
				{
				block = buildAnchorBlock(len, ma, compPos);
				segWrite(f, block);
				segBlockFree(&block);
				}
			len = 0;
			}

		/* Update the positions for each component. */
		for (mc = ma->components, compIdx = 0; mc != NULL; mc = mc->next)
			{
			if (mc->text[idx] != '-')
				compPos[compIdx] += 1;
			compIdx++;
			}

		idx++;
		}

	/* Is the final region of this alignment an anchor? */
	if ((! gapCol) && (len >= minLen))
		{
		block = buildAnchorBlock(len, ma, compPos);
		segWrite(f, block);
		segBlockFree(&block);
		}

	mafAliFree(&ma);
	freeMem(ac);
	}

segWriteEnd(f);
carefulClose(&f);
freeMem(compPos);
hashFreeWithVals(&speciesHash, freeSpeciesInfo);
}


static void aliContFuse(struct aliCont *ac, int maxComps)
/* Fuse adjacent alignment containers. */
{
struct aliCont *ac1, *ac2;
struct mafComp *mc1, *mc2, **compArray;
char *text;
int compIdx;

/* Used to put the components of two alignments in the same order. */
AllocArray(compArray, maxComps);

ac1 = ac;
while (ac1->next != NULL)
	{
	ac2 = ac1->next;
	compIdx = 0;

	/* Prepare to fuse the two alignment containers. */
	if (ac1->compCount == ac2->compCount)
		{
		for (mc1 = ac1->ali->components; mc1 != NULL; mc1 = mc1->next)
			{
			if ((mc2 = mafMayFindComponent(ac2->ali, mc1->src)) == NULL)
				break;

			if (mc1->start + mc1->size != mc2->start
				|| mc1->strand != mc2->strand
				|| !sameString(mc1->src, mc2->src))
				break;

			compArray[compIdx++] = mc2;
			}
		}

	/* Fuse the alignment containers if it's possible. */
	if (ac1->compCount == ac2->compCount && compIdx == ac1->compCount)
		{
		compIdx = 0;
		for (mc1 = ac1->ali->components; mc1 != NULL; mc1 = mc1->next)
			{
			mc2 = compArray[compIdx++];
			mc1->size += mc2->size;
			AllocArray(text, ac1->ali->textSize + ac2->ali->textSize + 1);
			strcpy(text, mc1->text);
			strcat(text, mc2->text);
			freeMem(mc1->text);
			mc1->text = text;
			}
		ac1->ali->score += ac2->ali->score;
		ac1->ali->textSize += ac2->ali->textSize;
		ac1->next = ac2->next;
		mafAliFree(&ac2->ali);
		freeMem(ac2);
		}
	else
		{
		ac1 = ac2;
		}
	}

freeMem(compArray);
}


static int cmpOrder(const void *a, const void *b)
/* Comparison function for sorting alignment containers. */
{
struct mafComp *p, *q;

p = (*((struct aliCont **) a))->ali->components;
q = (*((struct aliCont **) b))->ali->components;

if (p->start != q->start)
	return(p->start - q->start);

return(p->start + p->size - q->start - q->size);
}


static void aliContSortByPos(struct aliCont **acList, int aliCount)
/* Sort the alignment containers by the position of the first component
 * in the contained alignment. */
{
struct aliCont **contArr, *ac;
int i;

/* Initialize the alignment container array. */
AllocArray(contArr, aliCount);
for (ac = *acList, i = 0; ac != NULL; ac = ac->next, i++)
	contArr[i] = ac;

qsort((void *) contArr, (size_t) aliCount, sizeof(struct aliCont *), cmpOrder);

/* Update the alignment countainer array next pointers. */
*acList = contArr[0];
for (i = 0; i < aliCount - 1; i++)
	contArr[i]->next = contArr[i + 1];
contArr[aliCount - 1]->next = NULL;

freeMem(contArr);
}


static int foundOverlap(struct mafComp *mc)
{
int start = mc->start;
int stop  = mc->start + mc->size;
unsigned int start_idx, start_rem, stop_idx, stop_rem, pattern, i;

if (bitMap == NULL)
	AllocArray(bitMap, ((mc->srcSize + 7) >> uint_div));

start_idx = start >> uint_div;
start_rem = start  & uint_rem;
stop_idx  = stop  >> uint_div;
stop_rem  = stop   & uint_rem;

/* Special case if on same chunk. */
if (start_idx == stop_idx)
	{
	pattern = (fullUInt >> start_rem) & (fullUInt << (uint_bits - stop_rem));
	if ((pattern & bitMap[start_idx]) != emptyUInt)
		return(1);
	bitMap[start_idx] |= pattern;
	return(0);
	}

/* Check and set the start of the range. */
if (start_rem == 0)
	start_idx--;
else
	{
	pattern = fullUInt >> start_rem;
	if ((pattern & bitMap[start_idx]) != emptyUInt)
		return(1);
	bitMap[start_idx] |= pattern;
	}

/* Check and set the end of the range. */
if (stop_rem != 0)
	{
	pattern = fullUInt << (uint_bits - stop_rem);
	if ((pattern & bitMap[stop_idx]) != emptyUInt)
		printf("Stop Collision!\n");
	bitMap[stop_idx] |= pattern;
	}

/* Check and set the middle of the range. */
for (i = start_idx + 1; i < stop_idx; i++)
	{
	if (bitMap[i] != emptyUInt)
		return(1);
	bitMap[i] = fullUInt;
	}

return(0);
}


static int stripAndCountComponents(struct mafAli *ma)
/* Strip out data that results in "e", "q", and "i" lines. Return the
 * number of components in the alignment. */
{
struct mafComp *prev, *curr;
int compCount = 0;
int keep;
int dash = 0;
int i,j;
struct mafAli *a;
struct mafComp *c;
char *p;

prev = curr = ma->components;
while (curr != NULL)
	{
	keep = 1;

	/* Strip out non-core species. */
	if (coreSpeciesHash != NULL)
		{
		if ((p = strchr(curr->src, '.')) != NULL)
			*p = '\0';

		if (hashLookup(coreSpeciesHash, curr->src) == NULL)
			{
			keep = 0;
			dash = 1;
			}

		if (p != NULL)
			*p = '.';
		}

	/* Strip out "e" components. */
	if ((curr->size == 0) && (curr->leftStatus))
		keep = 0;

	if (keep)
		{
		compCount++;

		/* Strip out "q" data. */
		if (curr->quality)
			freeMem(curr->quality);

		/* Strip out "i" data. */
		curr->leftStatus = 0;

		prev = curr;
		curr = curr->next;
		}
	else
		{
		if (curr == ma->components)
			{
			ma->components = curr->next;
			mafCompFree(&curr);
			prev = curr = ma->components;
			}
		else
			{
			prev->next = curr->next;
			mafCompFree(&curr);
			curr = prev->next;
			}
		}
	}

/* remove columns containing only dashes */
if (dash)
	{
	a = ma;

	for (i = j = 0; i < a->textSize; i++)
		{
		for (c = a->components; c != NULL; c = c->next)
			if (c->text[i] != '-')
				break;

		if (c != NULL)
			{
			if (j != i)
				for (c = a->components; c != NULL; c = c->next)
					c->text[j] = c->text[i];
			j++;
			}
		}

	if (j != a->textSize)
		{
		a->textSize = j;

		for (c = a->components; c != NULL; c = c->next)
			c->text[j] = '\0';
		}

	}

return(compCount);
}

void mafToAnc(char *inMaf, char *outAnc)
/* mafToAnc - Find anchors in a maf file. */
{
struct mafFile *mf = mafOpen(inMaf);
struct mafAli *ma;
struct aliCont *ac, *acList = NULL, *acListTail = NULL;
int aliCount = 0, maxComps = 0, preSorted = 1, lastEnd = 0, compCount;
char *firstCmpSrc = NULL;

/* Read in and count the alignment blocks from the maf. Determine the
 * maximum number of components in an alignment. Make sure that the
 * source of the first component of every alignment is the same. */
while ((ma = mafNext(mf)) != NULL)
	{
	/* Strip and count the components in the alignment. */
	compCount = stripAndCountComponents(ma);

	/* Skip alignments that contain only zero or one component. */
	if (ma->components == NULL || ma->components->next == NULL)
		{
		mafAliFree(&ma);
		continue;
		}

	/* Check that the source of the first components are all the same. */
	if (!noCheckSrc)
		{
		if (firstCmpSrc == NULL)
			firstCmpSrc = cloneString(ma->components->src);
		else if (!sameString(firstCmpSrc, ma->components->src))
			{
			warn("Expecting the first component source to be %s.", firstCmpSrc);
			mafWrite(stderr, ma);
			exit(EXIT_FAILURE);
			}
		}
	
	/* Make sure the first component is on the + strand. */
	if (!noCheckStrand && ma->components->strand != '+')
		{
		warn("Expecting the first component to be on the + strand:");
		mafWrite(stderr, ma);
		exit(EXIT_FAILURE);
		}

	/* Check for single coverage of bases in the first component. */
	if (!noCheckCoverage && foundOverlap(ma->components))
		{
		warn("Found overlap:");
		mafWrite(stderr, ma);
		exit(EXIT_FAILURE);
		}
	
	/* Check to see if we'll need to sort later. */
	if (preSorted && lastEnd > ma->components->start)
		preSorted = 0;
	lastEnd = ma->components->start + ma->components->size;

	/* Update maximum component count. */
	if (compCount > maxComps)
		maxComps = compCount;

	/* Update the alignment count. */
	aliCount++;

	/* Create a new container for this alignment. */
	AllocVar(ac);
	ac->ali = ma;
	ac->compCount = compCount;

	/* Add this new container to our list */
	if (acList == NULL)
		acList = ac;
	else
		acListTail->next = ac;
	acListTail = ac;
	}
mafFileFree(&mf);

if (!noCheckCoverage)
	freeMem(bitMap);

freeMem(firstCmpSrc);

/* Sort the alignment containers by the position of the first component. */
if (!preSorted)
	aliContSortByPos(&acList, aliCount);

/* Combine adjacent alignment blocks that can be fused. */
aliContFuse(acList, maxComps);

/* Find and report anchors. */
findAnchors(&acList, outAnc, maxComps);
}

static void buildCoreSpeciesHash(char *speciesList)
{
char delim = ',';
char *p, *d;
int speciesCount=1;
struct hashEl *hel;

/* We don't need a hash if there's nothing to store. */
if (speciesList == NULL)
	return;

/* Count the number of species in the list. */
for (p = speciesList; *p != '\0'; p++)
	if (*p == delim)
		speciesCount++;

/* Create a hash with enough space. */
coreSpeciesHash = newHash( hashPOTSize(speciesCount) );

/* Populate the hash. */
p = speciesList;
while (1)
	{
	if ((d = strchr(p, delim)) != NULL)
		*d = '\0';

	if ((hel = hashLookup(coreSpeciesHash, p)) == NULL)
		hashAddInt(coreSpeciesHash, p, 1);

	if (d == NULL)
		break;

	p = d + 1;
	}
}


int main(int argc, char *argv[])
/* Process command line. Search maf file for anchors. */
{
optionInit(&argc, argv, options);

if (argc != 3)
    usage();

coreSpecies     = optionVal("coreSpecies", NULL);
minLen          = optionInt("minLen", minLen);
noCheckSrc      = optionExists("noCheckSrc");
noCheckStrand   = optionExists("noCheckStrand");
noCheckCoverage = optionExists("noCheckCoverage");

if (!noCheckSrc)
	noCheckStrand = noCheckCoverage = TRUE;

if (!noCheckStrand)
	noCheckCoverage = TRUE;

if (!noCheckCoverage)
	{
	/* Is uint_bits a power of two? */
	if ((uint_bits & (uint_bits - 1)) != 0)
		{
		warn("Turning off checking the first component for single coverage.\n"
		     "unsigned int is not represented by a power of two bits.");
		noCheckCoverage = TRUE;
		}
	else
		{
		/* Calculate some values we'll need later. */
		uint_div = hashPOTSize(uint_bits);
		uint_rem = uint_bits - 1;
		}
	}

buildCoreSpeciesHash(coreSpecies);
mafToAnc(argv[1], argv[2]);
freeHash(&coreSpeciesHash);

return(EXIT_SUCCESS);
}
