/* segExpandToAnchors - Expand segments out to alignment anchors. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "options.h"
#include "seg.h"

static char const rcsid[] = "$Id: segExpandToAnchors.c,v 1.1 2008/04/25 17:35:58 rico Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "segExpandToAnchors - Expand segments out to alignment anchors\n"
  "usage:\n"
  "   segExpandToAnchors in.seg out.seg\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
	{"minAncLen", OPTION_INT},
	{"ref", OPTION_STRING},
	{"debug", OPTION_BOOLEAN},
	{NULL, 0},
};

static int minAncLen = 100;
static char *ref  = NULL;
static boolean debug = FALSE;

static struct segFile *anchorFile = NULL;
struct slName *anchorSpecies = NULL;
char *anchorChrom = NULL;

static char *anchorFileName(struct segComp *refComp, struct slName *speciesList)
/* Generate anchor file name.  Will be replaced with something more robust
   in the future. */
{
char *fileName, *refChrom = refComp->src + strlen(ref) + 1;
int fileNameLen, speciesListLen = slCount(speciesList);

if (speciesListLen != 1)
	errAbort("No support for speciesList of length != 1 [%d]", speciesListLen);

fileNameLen = 48 + strlen(ref) + strlen(speciesList->name) + strlen(refChrom);
AllocArray(fileName, fileNameLen);

safef(fileName, fileNameLen,
	"/cluster/data/%s/bed/blastz.%s/mafSynNetAnc/%s.anc.gz",
	ref, speciesList->name, refChrom);

return(fileName);
}

static boolean nameSubset(struct slName *a, struct slName *b)
/* Return TRUE if b is a subset of a. */
{
int aSize = slCount(a), bSize = slCount(b);
struct slName *el;

if (aSize < bSize)
	return(FALSE);

for (el = b; el != NULL; el = el->next)
	if (! slNameInList(a, el->name))
		return(FALSE);

return(TRUE);
}

static void loadAnchors(struct segComp *refComp, struct slName *speciesList)
/* Load appropriate anchors.  Will be replaced with something more robust
   in the future. */
{
char *afName, *refChrom = refComp->src + strlen(ref) + 1;
struct segBlock *block, *tail = NULL;
struct segComp *comp, *loadRefComp;
char *p;

if (anchorFile == NULL || ! sameString(anchorChrom, refChrom)
	|| ! nameSubset(anchorSpecies, speciesList))
	{
	/* Free current set of anchors. */
	segFileFree(&anchorFile);
	slNameFreeList(&anchorSpecies);
	freeMem(anchorChrom);

	afName = anchorFileName(refComp, speciesList);
	anchorFile = segOpen(afName);
	anchorChrom = cloneString(refChrom);

	if (debug)
		fprintf(stderr, "Loading anchors from %s\n", afName);

	/* Load anchors and maintain a list of secondary species */
	while ((block = segNext(anchorFile)) != NULL)
		{
		if (block->val < minAncLen)
			continue;

		loadRefComp = segFindCompSpecies(block, ref, '.');

		for (comp = block->components; comp != NULL; comp = comp->next)
			{
			if (comp == loadRefComp)
				continue;

			if ((p = strchr(comp->src, '.')) != NULL)
				*p = '\0';

			slNameStore(&anchorSpecies, comp->src);

			if (p != NULL)
				*p = '.';
			}

		if (anchorFile->blocks == NULL)
			anchorFile->blocks = block;
		else
			{
			tail->next = block;
			block->prev = tail;
			}
		tail = block;
		}

	freeMem(afName);
	}
}

static void segWarnF(struct segBlock *sb, const char *format, ...)
{
va_list ap;

fprintf(stderr, "# ");
va_start(ap, format);
vfprintf(stderr, format, ap);
va_end(ap);
fputc('\n', stderr);
segWrite(stderr, sb);
}

static void segWarn(struct segBlock *sb, const char *message)
{
segWarnF(sb, "%s", message);
}

static boolean anchorListEmpty(struct segBlock *sb, struct segBlock *left,
	struct segBlock *right)
/* Return TRUE if either left or right is NULL.  Return FALSE otherwise. */
{
if (left != NULL && right != NULL)
	return(FALSE);

if (left == NULL && right == NULL)
	segWarn(sb, "There are no anchors on either side of this block.");
else if (left == NULL)
	segWarn(sb, "There are no anchors to the left of this block.");
else
	segWarn(sb, "There are no anchors to the right of this block.");

return(TRUE);
}

static void chromBreakSpecies(struct segBlock *sb, struct segBlock *anchor,
	struct slName **speciesList)
/* Return a list of species that have a chrom break */
{
struct segComp *comp, *acomp;
char *p;

for (comp = sb->components; comp != NULL; comp = comp->next)
	if ((acomp = segMayFindCompSpecies(anchor, comp->src, '.')) != NULL)
		if (! sameString(acomp->src, comp->src))
			{
			if ((p = strchr(comp->src, '.')) != NULL)
				*p = '\0';

			slNameStore(speciesList, comp->src);

			if (p != NULL)
				*p = '.';
			}
}

static void chromBreakWarnF(struct segBlock *sb, struct slName **speciesList,
	const char *format, ...)
{
va_list ap;
char *names;

names = slNameListToString(*speciesList, ',');

fprintf(stderr, "# ");
va_start(ap, format);
vfprintf(stderr, format, ap);
va_end(ap);
fprintf(stderr, " %s.", names);
fputc('\n', stderr);
segWrite(stderr, sb);

freeMem(names);
slNameFreeList(speciesList);
}

static void chromBreakWarn(struct segBlock *sb, struct slName **speciesList,
	char *msg)
{
chromBreakWarnF(sb, speciesList, "%s", msg);
}

static boolean regionHasChromBreaks(struct segBlock *sb,
	struct segComp *refComp, struct segBlock *left, struct segBlock *right)
/* Return TRUE if there is a chromosomal break in a secondary species between
   left and right. */
{
struct segBlock *anchor;
struct segComp *aRef;
struct slName *speciesList = NULL;

for (anchor = left; anchor != right->next; anchor = anchor->next)
	{
	/* We only need to check at the beginning and if there is a chrom break. */
	if ((anchor->name != NULL && sameString(anchor->name, "chromBreak"))
		|| anchor == left)
		{
		aRef = segFindCompSpecies(anchor, ref, '.');
		/* Only check if this anchor overlaps the argument block. */
		if ((aRef->start + aRef->size > refComp->start)
			&& (refComp->start + refComp->size > aRef->start))
			chromBreakSpecies(sb, anchor, &speciesList);
		}
	}

if (speciesList != NULL)
	{
	chromBreakWarn(sb, &speciesList, "This block has chromosomal breaks in");
	return(TRUE);
	}

return(FALSE);
}

static void outputRegion(FILE *of, struct segBlock *sb, struct segBlock *left,
	struct segBlock *right)
{
struct segComp *comp, *lComp, *rComp;

for (comp = sb->components; comp != NULL; comp = comp->next) {
	lComp = segFindCompSpecies(left, comp->src, '.');
	rComp = segFindCompSpecies(right, comp->src, '.');

	comp->start = lComp->start;
	comp->size  = rComp->start + rComp->size - lComp->start;
}
segWrite(of, sb);
}

static boolean anchorsConsistent(struct segBlock *l, struct segBlock *r)
{
struct segComp *lComp, *rComp;

for (lComp = l->components; lComp != NULL; lComp = lComp->next)
	{
	/* Anchors must have all the same components. */
	if ((rComp = segMayFindCompSpecies(r, lComp->src, '.')) == NULL)
		return(FALSE);

	/* Orientation must match. */
	if (lComp->strand != rComp->strand)
		return(FALSE);
	}

return(TRUE);
}


static boolean anchorChromMismatch(struct segBlock *sb, struct segBlock *anchor,
	char direction)
{
struct slName *speciesList = NULL;
static char *right = "right";
static char *left = "left";
char *word = NULL;

chromBreakSpecies(sb, anchor, &speciesList);
if (speciesList != NULL)
	{ 
	if (direction == 'l')
		word = left;
	else if (direction == 'r')
		word = right;
	else
		errAbort("Invalid direction: '%c'", direction);

	chromBreakWarnF(sb, &speciesList,
		"Chromosomal breaks on the way to the %s anchor in", word);

	return(TRUE);
	}

return(FALSE);
}


static int leftDistance(struct segComp *a, struct segComp *b) {
	if (a->start >= b->start)
		return(a->start - b->start);
	else
		return(b->start - a->start);
}

static int rightDistance(struct segComp *a, struct segComp *b) {
	if (a->start + a->size >= b->start + b->size)
		return(a->start + a->size - b->start - b->size);
	else
		return(b->start + b->size - a->start - a->size);
}

static int anchorDistance(struct segComp *l, struct segComp *r) {
	return(r->start + r->size - l->start);
}

static void anchorSearch(struct segComp *refComp, struct segBlock *sb,
	struct segBlock *left_start, struct segBlock *right_start, FILE *of)
{
struct segBlock *left = left_start;
struct segBlock *right = right_start;
struct segBlock *best_left = NULL;
struct segBlock *best_right = NULL;
int best_distance = -1;
int distance;
int done = 0;
struct segComp *lRef;
struct segComp *rRef;

if (anchorListEmpty(sb, left_start, right_start))
	return;

if (regionHasChromBreaks(sb, refComp, left_start, right_start))
	return;

if (anchorChromMismatch(sb, left_start, 'l'))
	return;

if (anchorChromMismatch(sb, right_start, 'r'))
	return;

for (left = left_start; left != NULL; left = left->prev)
	{
	if (left->name != NULL && sameString(left->name, "chromBreak"))
		{
		if (anchorChromMismatch(sb, left->prev, 'l'))
			return;
		else
			continue;
		}

	lRef = segFindCompSpecies(left, ref, '.');
	if (best_distance != -1 && leftDistance(lRef, refComp) >= best_distance)
		break;

	for (right = right_start; right != NULL; right = right->next)
		{
		if (right->name != NULL && sameString(right->name, "chromBreak"))
			{
			if (anchorChromMismatch(sb, right, 'r'))
				return;
			else
				continue;
			}

			rRef = segFindCompSpecies(left, ref, '.');
			if (best_distance != -1
				&& rightDistance(rRef, refComp) >= best_distance)
				{
				done =1;
				break;
				}
	
			if (anchorsConsistent(left, right))
				{
				distance = anchorDistance(lRef, rRef);
				if (best_distance == -1 || distance < best_distance)
					{
					best_distance = distance;
					best_left = left;
					best_right = right;
					}
				else
					{
					done = 1;
					break;
					}
				}
		}

		if (done == 1)
			break;
	}

if (best_left == NULL)
	{
	segWarn(sb, "There are no consistent anchors for this block.");
	return;
	}

outputRegion(of, sb, left, right);
}


static void findAnchors(struct segBlock *sb, struct segComp *refComp,
	struct slName *speciesList, FILE *of)
{
struct segBlock *anchor, *prev_anchor, *left_start, *right_start;
struct segComp *aRef;

loadAnchors(refComp, speciesList);

/* Get the starting point for anchor searches to the left. */
prev_anchor = NULL;
for (anchor = anchorFile->blocks; anchor != NULL; anchor = anchor->next)
	{
	aRef = segFindCompSpecies(anchor, ref, '.');

	if (aRef->start > refComp->start)
		break;

	prev_anchor = anchor;
	}
left_start = prev_anchor;

/* Advance to the first anchor after the alignment block.  We're doing this
   because of the way chromBreak records are inserted into an anchor file. */
if (left_start == NULL)
	anchor = anchorFile->blocks;

prev_anchor = NULL;
for( ; anchor != NULL; anchor = anchor->next)
	{
	aRef = segFindCompSpecies(anchor, ref, '.');

	if (aRef->start >= refComp->start + refComp->size)
		break;

	prev_anchor = anchor;
	}
if (anchor == NULL)
	anchor = prev_anchor;

/* Get the starting point for anchor searches to the right. */
prev_anchor = NULL;
for ( ; anchor != NULL; anchor = anchor->prev)
	{
	aRef = segFindCompSpecies(anchor, ref, '.');

	if (aRef->start + aRef->size < refComp->start + refComp->size)
		break;

	prev_anchor = anchor;
	}
right_start = prev_anchor;

anchorSearch(refComp, sb, left_start, right_start, of);
}

void segExpandToAnchors(char *inSeg, char *outSeg)
/* segExpandToAnchors - Expand segments out to alignment anchors. */
{
struct segFile *sf = segOpen(inSeg);
FILE *of = mustOpen(outSeg, "w");
struct segBlock *sb;
struct segComp *refComp;
struct slName *speciesList;

segWriteStart(of);

while ((sb = segNext(sf)) != NULL)
	{
	/* Set ref if it wasn't set on the command line. */
	if (ref == NULL && ((ref = segFirstCompSpecies(sb, '.')) == NULL))
		errAbort("ref is not set and the first segment block has no "
			"components.");

	refComp = segFindCompSpecies(sb, ref, '.');
	speciesList = segSecSpeciesList(sb, refComp, '.');

	findAnchors(sb, refComp, speciesList, of);

	slNameFreeList(&speciesList);
	segBlockFree(&sb);
	}

segWriteEnd(of);
carefulClose(&of);
segFileFree(&sf);
segFileFree(&anchorFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 3)
    usage();

minAncLen = optionInt("minAncLen", minAncLen);
ref       = optionVal("ref", ref);
debug     = optionExists("debug");

segExpandToAnchors(argv[1], argv[2]);

return(EXIT_SUCCESS);;
}
