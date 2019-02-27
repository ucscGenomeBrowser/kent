/* twoBitMask - apply masking to a .2bit file, creating a new .2bit file. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "bits.h"
#include "localmem.h"
#include "memalloc.h"
#include "repMask.h"
#include "twoBit.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitMask - apply masking to a .2bit file, creating a new .2bit file\n"
  "usage:\n"
  "   twoBitMask input.2bit maskFile output.2bit\n"
  "options:\n"
  "   -add   Don't remove pre-existing masking before applying maskFile.\n"
  "   -type=.XXX   Type of maskFile is XXX (bed or out).\n"
  "maskFile can be a RepeatMasker .out file or a .bed file.  It must not\n"
  "contain rows for sequences which are not in input.2bit.\n"
);
}

/* Options: */
boolean add = FALSE;
char *type = NULL;

static struct optionSpec options[] = {
   {"add", OPTION_BOOLEAN},
   {"type", OPTION_STRING},
   {NULL, 0},
};


unsigned slurpInput(char *inName, struct hash *tbHash,
			  struct hash *bitmapHash, struct twoBit **list)
/* Read .2bit file inName into memory and return list of twoBit items.
 * Populate tbHash with twoBit items, and bitmapHash with bitmaps for
 * easy masking.  Both are hashed by twoBit sequence name. */
{
struct twoBit *twoBitList = NULL;
struct twoBit *twoBit = NULL;
struct twoBitFile *tbf = twoBitOpen(inName);
int version = tbf->version;
*list =  twoBitList = twoBitFromOpenFile(tbf);
/* Free and clear the masking data (unless -add).  Hash twoBits by name. */
for (twoBit = twoBitList;  twoBit != NULL;  twoBit = twoBit->next)
    {
    Bits *bits = bitAlloc(twoBit->size);
    if (add)
	{
	/* Store the currently masked bits: */
	int i;
	for (i = 0;  i < twoBit->maskBlockCount;  i++)
	    {
	    bitSetRange(bits, twoBit->maskStarts[i], twoBit->maskSizes[i]);
	    }
	}
    /* Free the current representation of masking -- it will be replaced. */
    twoBit->maskBlockCount = 0;
    freez(&(twoBit->maskStarts));
    freez(&(twoBit->maskSizes));
    /* Hash twoBit and our new bitmap by sequence name. */
    hashAddUnique(tbHash, twoBit->name, twoBit);
    hashAddUnique(bitmapHash, twoBit->name, bits);
    }
return version;
}


void addMasking(struct hash *twoBitHash, struct hash *bitmapHash, char *seqName,
		unsigned start, unsigned end)
/* Set bits in range. */
{
if (end > start)
    {
    struct twoBit *tb = (struct twoBit *)hashMustFindVal(twoBitHash, seqName);
    if ((end > tb->size) || (start >= tb->size))
	errAbort("bed range (%d - %d) is off the end of chromosome %s size %d",
	    start, end, seqName, tb->size);
    Bits *bits = (Bits *)hashMustFindVal(bitmapHash, seqName);
    bitSetRange(bits, start, (end - start));
    }
}


struct unsignedRange
    {
    struct unsignedRange *next;
    unsigned start;
    unsigned size;
    };

void bitmapToMaskArray(struct hash *bitmapHash, struct hash *tbHash)
/* Translate each bitmap in bitmapHash into an array of mask coordinates
 * in the corresponding twoBit in tbHash.  Assume tbHash's mask array is
 * empty at the start -- we allocate it here.  Free bitmap when done. */
{
struct hashCookie cookie = hashFirst(tbHash);
struct hashEl *hel = NULL;

while ((hel = hashNext(&cookie)) != NULL)
    {
    char *seqName = hel->name;
    struct twoBit *tb = (struct twoBit *)(hel->val);
    struct hashEl *bHel = hashLookup(bitmapHash, seqName);
    Bits *bits;
    unsigned start=0, end=0;

    assert(tb != NULL);
    assert(tb->maskBlockCount == 0);
    if (bHel == NULL)
	errAbort("Missing bitmap for seq \"%s\"", seqName);
    bits = (Bits *)bHel->val;
    if (bits != NULL)
	{
	struct lm *lm = lmInit(0);
	struct unsignedRange *rangeList = NULL, *range = NULL;
	int i;
	for (;;)
	    {
	    start = bitFindSet(bits, end, tb->size);
	    if (start >= tb->size)
		break;
	    end = bitFindClear(bits, start, tb->size);
	    if (end > start)
		{
		lmAllocVar(lm, range);
		range->start = start;
		range->size = (end - start);
		slAddHead(&rangeList, range);
		}
	    }
	slReverse(&rangeList);
	tb->maskBlockCount = slCount(rangeList);
	if (tb->maskBlockCount > 0)
	    {
	AllocArray(tb->maskStarts, tb->maskBlockCount);
	AllocArray(tb->maskSizes, tb->maskBlockCount);
	for (i = 0, range = rangeList;  range != NULL;
	     i++, range = range->next)
	    {
	    tb->maskStarts[i] = range->start;
	    tb->maskSizes[i] = range->size;
	    }
	    }
	lmCleanup(&lm);
	bitFree(&bits);
	bHel->val = NULL;
	}
    }
}


void maskWithBed(char *bedName, struct hash *tbHash, struct hash *bitmapHash)
/* Read coordinates from bedName and apply them to twoBits in tbHash. */
{
struct lineFile *lf = lineFileOpen(bedName, TRUE);
int wordCount;
char *words[13];
boolean alreadyWarned = FALSE;
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    struct bed bed;
    /* warn if bed has at least 12 fields -- no support for blocks */
    if (wordCount >= 12 && !alreadyWarned)
	{
	warn("Warning: BED file %s has >=%d fields which means it might "
	     "contain block coordinates, but this program uses only the "
	     "first three fields (the entire span -- no support for blocks).",
	     bedName, wordCount);
	alreadyWarned = TRUE;
	}
    bedStaticLoad(words, &bed);
    addMasking(tbHash, bitmapHash, bed.chrom, bed.chromStart, bed.chromEnd);
    }
bitmapToMaskArray(bitmapHash, tbHash);
}


void maskWithOut(char *outName, struct hash *tbHash, struct hash *bitmapHash)
/* Read coordinates from outName and apply them to twoBits in tbHash. */
{
struct lineFile *lf = lineFileOpen(outName, TRUE);
char *line;
int lineSize;

/* Make sure we have a .out header. */
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("Empty %s", lf->fileName);
if (!startsWith("   SW  perc perc", line))
    {
    if (!startsWith("   SW   perc perc", line))
	errAbort("%s doesn't seem to be a RepeatMasker .out file, first "
	    "line seen:\n%s", lf->fileName, line);
    }
lineFileNext(lf, &line, &lineSize);
lineFileNext(lf, &line, &lineSize);

/* Process line oriented records of .out file. */
while (lineFileNext(lf, &line, &lineSize))
    {
    struct repeatMaskOut rmo;
    char *words[24];
    int wordCount;
    char *seqName;
    int start, end;

    wordCount = chopLine(line, words);
    if (wordCount < 14)
        errAbort("Expecting 14 or 15 words line %d of %s", 
	    lf->lineIx, lf->fileName);
    repeatMaskOutStaticLoad(words, &rmo);
    seqName = rmo.qName;
    start = rmo.qStart - 1;
    end = rmo.qEnd;
    addMasking(tbHash, bitmapHash, seqName, start, end);
    }
bitmapToMaskArray(bitmapHash, tbHash);
}


void twoBitMask(char *inName, char *maskName, char *outName)
/* twoBitMask - apply masking to a .2bit file, creating a new .2bit file. */
{
struct hash *tbHash = hashNew(20);
struct hash *bitmapHash = hashNew(20);
struct twoBit *twoBitList = NULL;
struct twoBit *twoBit = NULL;
FILE *f = NULL;

if (! twoBitIsFile(inName))
    {
    if (twoBitIsSpec(inName))
	errAbort("Sorry, this works only on whole .2bit files, not specs.");
    else
	errAbort("Input %s does not look like a proper .2bit file.", inName);
    }

unsigned version = slurpInput(inName, tbHash, bitmapHash, &twoBitList);

/* Read mask data into bitmapHash, store it in twoBits: */
if ((type && endsWith(type, "bed")) || endsWith(maskName, ".bed"))
    maskWithBed(maskName, tbHash, bitmapHash);
else if ((type && endsWith(type, "out")) || endsWith(maskName, ".out"))
    maskWithOut(maskName, tbHash, bitmapHash);
else
    errAbort("Sorry, maskFile must end in \".bed\" or \".out\".");

/* Create a new .2bit file, write it out from twoBits. */
f = mustOpen(outName, "wb");
twoBitWriteHeaderExt(twoBitList, f, version == 1);
for (twoBit = twoBitList; twoBit != NULL; twoBit = twoBit->next)
    {
    twoBitWriteOne(twoBit, f);
    }
carefulClose(&f);

/* Don't bother freeing twoBitList and hashes here -- just exit. */
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
add = optionExists("add");
type = optionVal("type", NULL);
if (argc != 4)
    usage();
twoBitMask(argv[1], argv[2], argv[3]);
return 0;
}
