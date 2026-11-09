/* regionsToChain - lay a list of regions end to end and write the bigChain that
 * maps the result back to where the regions came from.
 *
 * The output is the whole definition of a synthetic multi-region coordinate
 * space: the chain's target side is the synthetic sequence, its query side is the
 * source assembly, and because the regions are laid down with nothing between
 * them the chain's blocks tile the synthetic sequence completely.  So sequence for
 * the space can be read out of the source assembly through this same chain, and no
 * synthetic 2bit is needed.
 *
 * One synthetic sequence per source sequence, keeping the source name.  A whole
 * genome concatenated into one sequence would put a gene-level layout of hg38
 * within a few percent of the signed 32 bit coordinate limit.
 *
 * refs #37788 */

#include <limits.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "pipeline.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regionsToChain - lay regions end to end and write the bigChain mapping the\n"
  "result back to the source assembly.\n"
  "usage:\n"
  "   regionsToChain regions.bed chrom.sizes output.bb\n"
  "Writes output.bb and output.link.bb, the pair a bigChain is made of.\n"
  "\n"
  "regions.bed is bed3 in source coordinates, with an optional fourth column of +\n"
  "or -.  A - region is laid down reverse-complemented.  Regions are laid down in\n"
  "the order given and used exactly as given:  overlapping regions are laid down\n"
  "twice and simply start a new chain, so merge them first if that is not what you\n"
  "want.\n"
  "options:\n"
  "   -keepTemp   leave the intermediate text files next to the output\n"
  );
}

static struct optionSpec options[] = {
   {"keepTemp", OPTION_BOOLEAN},
   {NULL, 0},
};

/* The synthetic sequence built from regions on more than one source sequence.  The
 * same name the virtual chromosome uses, so anything that already special-cases it
 * keeps working. */
#define MULTI_TARGET "multi"

/* The largest synthetic sequence a bigBed can describe. */
#define MAX_SYNTHETIC_SIZE 0x7fffffffLL

static boolean keepTemp = FALSE;

struct region
/* One region of the source assembly, and where it lands in the synthetic one. */
    {
    struct region *next;
    char *chrom;	/* SOURCE sequence name.  In the sizes hash. */
    char *tName;	/* SYNTHETIC sequence name, chosen by layOut(). */
    int qStart;		/* Source start, half open. */
    int qEnd;		/* Source end. */
    int tStart;		/* Synthetic start, filled in by layOut(). */
    int tEnd;		/* Synthetic end. */
    int chainId;	/* Which chain this region is a block of. */
    char strand;	/* '-' when laid down reverse-complemented, else '+'. */
    };

static struct hash *readChromSizes(char *fileName)
/* Read a two column chrom size file into a hash of ints. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRowTab(lf, row))
    hashAddInt(hash, row[0], lineFileNeedFullNum(lf, row, 1));
lineFileClose(&lf);
if (hash->elCount == 0)
    errAbort("no chrom sizes in %s", fileName);
return hash;
}

static struct region *readRegions(char *fileName, struct hash *sizeHash)
/* Read a bed3 of source regions, with an optional fourth column of strand.  The chrom
 * name is the one interned in sizeHash, so every region of a sequence shares one
 * string. */
{
struct region *list = NULL, *region;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
int wordCount;
while ((wordCount = lineFileChopNextTab(lf, row, ArraySize(row))) > 0)
    {
    if (wordCount != 3 && wordCount != 4)
        errAbort("expecting 3 or 4 columns, got %d, line %d of %s", wordCount,
                 lf->lineIx, fileName);
    char strand = '+';
    if (wordCount == 4)
        {
        if (!sameString(row[3], "+") && !sameString(row[3], "-"))
            errAbort("strand must be + or -, got '%s', line %d of %s", row[3],
                     lf->lineIx, fileName);
        strand = row[3][0];
        }
    struct hashEl *hel = hashLookup(sizeHash, row[0]);
    if (hel == NULL)
        errAbort("%s is not in the chrom sizes, line %d of %s", row[0], lf->lineIx, fileName);
    int size = ptToInt(hel->val);
    int start = lineFileNeedFullNum(lf, row, 1);
    int end = lineFileNeedFullNum(lf, row, 2);
    if (start < 0)
        errAbort("negative start %s:%d-%d, line %d of %s", row[0], start, end,
                 lf->lineIx, fileName);
    if (start >= end)
        errAbort("empty region %s:%d-%d, line %d of %s", row[0], start, end,
                 lf->lineIx, fileName);
    if (end > size)
        errAbort("%s:%d-%d runs off the end of %s (%d), line %d of %s", row[0], start, end,
                 row[0], size, lf->lineIx, fileName);
    AllocVar(region);
    region->chrom = hel->name;
    region->qStart = start;
    region->qEnd = end;
    region->strand = strand;
    slAddHead(&list, region);
    }
lineFileClose(&lf);
if (list == NULL)
    errAbort("no regions in %s", fileName);
slReverse(&list);
return list;
}

static struct hash *layOut(struct region *regionList)
/* Give every region its place in a synthetic sequence and number the chains.
 * Returns a hash of synthetic sizes.
 *
 * The regions are laid down in the order given.  That order is the layout:  an alt
 * haplotype shown in place is its chromosome's flank, then the alt, then the rest of
 * the chromosome, and sorting it would put the alt after the end.
 *
 * A region continues the chain before it when it moves forward on both sides.  It
 * always moves forward on the synthetic side, so a new chain starts only where the
 * source sequence changes or a region does not move forward on the source side:  a
 * chain is an alignment, and an alignment cannot use the same source base twice or
 * go backward.
 *
 * Regions from ONE source sequence lay into a synthetic sequence that takes that
 * sequence's name, which is what lets the browser ask for it by the name the user
 * typed.  Regions from SEVERAL lay into a single sequence called "multi", because
 * there is no source name that describes it:  the same answer, and the same name,
 * the virtual chromosome uses.  A browser can only display one sequence at a time,
 * so laying them out separately would silently show one and drop the rest. */
{
struct hash *synthSizes = hashNew(0);
struct region *region, *prev = NULL;
int offset = 0;
int chainId = 0;

boolean manyChroms = FALSE;
for (region = regionList;  region != NULL;  region = region->next)
    if (differentString(region->chrom, regionList->chrom))
        {
        manyChroms = TRUE;
        break;
        }

for (region = regionList;  region != NULL;  region = region->next)
    {
    region->tName = manyChroms ? MULTI_TARGET : region->chrom;
    boolean newSeq = (prev == NULL || differentString(region->tName, prev->tName));
    if (newSeq)
        {
        if (prev != NULL)
            hashAddInt(synthSizes, prev->tName, offset);
        offset = 0;
        }
    /* A new chain at a new synthetic sequence, at a source-side overlap, and when the
     * source sequence changes, since one chain cannot span two query sequences.  A reversed
     * region is a chain of its own, since a chain has one strand. */
    if (newSeq || differentString(region->chrom, prev->chrom) || region->qStart < prev->qEnd
        || region->strand == '-' || prev->strand == '-')
        chainId += 1;

    /* bigBed coordinates are 32 bits, and a synthetic sequence laid out from a
     * genome-wide region list runs past that.  Say so here:  the alternative is the
     * signed overflow going on to bedToBigBed, which rejects the negative number with
     * "invalid unsigned integer" and says nothing about what actually happened. */
    if ((long long)offset + (region->qEnd - region->qStart) > MAX_SYNTHETIC_SIZE)
        errAbort("regions do not fit in one synthetic sequence:  past %s they would "
                 "need more than the %lld bases a 32-bit coordinate can address.  A "
                 "genome-wide region list cannot be laid into one sequence.",
                 region->chrom, (long long)MAX_SYNTHETIC_SIZE);
    region->tStart = offset;
    region->tEnd = offset + (region->qEnd - region->qStart);
    region->chainId = chainId;
    offset = region->tEnd;
    prev = region;
    }
hashAddInt(synthSizes, prev->tName, offset);
return synthSizes;
}

static void writeSizes(char *fileName, struct hash *synthSizes)
/* Write the synthetic sizes, which is what bedToBigBed indexes against. */
{
FILE *f = mustOpen(fileName, "w");
struct hashEl *el, *list = hashElListHash(synthSizes);
slSort(&list, hashElCmp);
for (el = list;  el != NULL;  el = el->next)
    fprintf(f, "%s\t%d\n", el->name, ptToInt(el->val));
hashElFreeList(&list);
carefulClose(&f);
}

static void writeChainAndLink(char *chainFile, char *linkFile, struct region *regionList,
                              struct hash *sizeHash, struct hash *synthSizes)
/* Write the two bed files bedToBigBed will index.  The region list is in layout
 * order, which is synthetic start order on the one synthetic sequence, and that is
 * the order both files need. */
{
FILE *cf = mustOpen(chainFile, "w");
FILE *lf = mustOpen(linkFile, "w");
struct region *region = regionList;

while (region != NULL)
    {
    /* Gather the run of regions making up one chain, and write a link for each. */
    /* A reversed region is a chain of one block on the minus strand, whose query
     * coordinates count from the end of the source sequence, as every chain's do. */
    struct region *first = region, *last = region;
    int qSize = hashIntVal(sizeHash, first->chrom);
    while (region != NULL && region->chainId == first->chainId)
        {
        int linkQStart = (region->strand == '-') ? qSize - region->qEnd : region->qStart;
        fprintf(lf, "%s\t%d\t%d\t%d\t%d\n",
                region->tName, region->tStart, region->tEnd, region->chainId, linkQStart);
        last = region;
        region = region->next;
        }

    int chainQStart = first->qStart, chainQEnd = last->qEnd;
    if (first->strand == '-')
        {
        chainQStart = qSize - first->qEnd;
        chainQEnd = qSize - first->qStart;
        }
    /* bigChain: chrom start end name score strand tSize qName qSize qStart qEnd chainScore */
    fprintf(cf, "%s\t%d\t%d\t%d\t1000\t%c\t%d\t%s\t%d\t%d\t%d\t1000\n",
            first->tName, first->tStart, last->tEnd, first->chainId, first->strand,
            hashIntVal(synthSizes, first->tName),
            first->chrom, qSize, chainQStart, chainQEnd);
    }
carefulClose(&cf);
carefulClose(&lf);
}

static void runBedToBigBed(char *type, char *inFile, char *sizesFile, char *outFile)
/* Index one of the two bed files.
 *
 * No -as: every reader of these files takes the fields by column number
 * (chainLoadIdRangeHub() in hg/lib/chainNetDbLoad.c is the one that matters), and
 * this pair defines a coordinate space rather than being drawn as a track.  That
 * keeps a copy of bigChain.as out of this program, where it could drift. */
{
char typeArg[64];
safef(typeArg, sizeof typeArg, "-type=%s", type);

/* Beside the CGI when run from one (cwd is the cgi-bin dir), on PATH when run by
 * hand.  Not a bare name in the CGI case:  execvp would resolve it against
 * whatever PATH apache hands us. */
char *bedToBigBed = fileExists("loader/bedToBigBed") ? "loader/bedToBigBed"
                                                     : "bedToBigBed";
char *cmd[7];
int i = 0;
cmd[i++] = bedToBigBed;
cmd[i++] = typeArg;
cmd[i++] = "-tab";
cmd[i++] = inFile;
cmd[i++] = sizesFile;
cmd[i++] = outFile;
cmd[i++] = NULL;
assert(i == ArraySize(cmd));

struct pipeline *pl = pipelineOpen1(cmd, pipelineRead | pipelineNoAbort,
                                    "/dev/null", NULL, 0);
if (pipelineClose(&pl) != 0)
    errAbort("%s failed on %s", bedToBigBed, inFile);
}

static char *tempBeside(char *outFile, char *suffix)
/* A working file beside the output, so the rename into place stays on one
 * filesystem, and unique, so two builds at once do not share one.  Free the result. */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "%s.%d%s", outFile, (int)getpid(), suffix);
return dyStringCannibalize(&dy);
}

static void keepAs(char *tempFile, char *outFile, char *suffix)
/* Put a working file where -keepTemp promises it, which is the same name without
 * the pid in it, so a test can say which files it means. */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "%s%s", outFile, suffix);
mustRename(tempFile, dy->string);
dyStringFree(&dy);
}

void regionsToChain(char *regionFile, char *chromSizes, char *outFile)
/* Lay the regions end to end and write the bigChain pair. */
{
if (!endsWith(outFile, ".bb"))
    errAbort("output must end in .bb: the link file is named from it");

struct hash *sizeHash = readChromSizes(chromSizes);
struct region *regionList = readRegions(regionFile, sizeHash);
struct hash *synthSizes = layOut(regionList);

char *synthSizesFile = tempBeside(outFile, ".sizes");
char *chainBed = tempBeside(outFile, ".chain.bed");
char *linkBed = tempBeside(outFile, ".link.bed");
char *chainTmp = tempBeside(outFile, ".chain.tmp");
char *linkTmp = tempBeside(outFile, ".link.tmp");

writeSizes(synthSizesFile, synthSizes);
writeChainAndLink(chainBed, linkBed, regionList, sizeHash, synthSizes);
runBedToBigBed("bed6+6", chainBed, synthSizesFile, chainTmp);
runBedToBigBed("bed4+1", linkBed, synthSizesFile, linkTmp);

/* Rename last, and the link file first: a reader that finds the chain file must
 * find the link file beside it, since bigChainGetLinkFile() only builds a name. */
char linkOut[PATH_MAX];
safef(linkOut, sizeof linkOut, "%.*s.link.bb", (int)(strlen(outFile) - 3), outFile);
mustRename(linkTmp, linkOut);
mustRename(chainTmp, outFile);

if (keepTemp)
    {
    keepAs(synthSizesFile, outFile, ".sizes");
    keepAs(chainBed, outFile, ".chain.bed");
    keepAs(linkBed, outFile, ".link.bed");
    }
else
    {
    remove(synthSizesFile);
    remove(chainBed);
    remove(linkBed);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
keepTemp = optionExists("keepTemp");
regionsToChain(argv[1], argv[2], argv[3]);
return 0;
}
