/* regionAgp - generate an AGP file for an ENCODE region described in
 *              a bed file, containing lines of the format:
 *                      <chrom> <start> <end> <region>
 *              based on the chromosome AGP's.              
 */

#include "common.h"
#include "options.h"
#include "bed.h"
#include "agpFrag.h"
#include "agpGap.h"

static char const rcsid[] = "$Id: regionAgp.c,v 1.4 2004/08/18 21:48:01 kate Exp $";

#define DIR_OPTION              "dir"
#define NAME_PREFIX_OPTION      "namePrefix"

static struct optionSpec options[] = {
        {DIR_OPTION, OPTION_BOOLEAN},
        {NAME_PREFIX_OPTION, OPTION_STRING},
        {NULL, 0}
};

void usage()
/* Print usage instructions and exit. */
{
errAbort("regionAgp - generate an AGP file for a region\n"
     "\nusage:\n"
     "    regionAgp region.bed chrom.agp out\n\n"
     " region.bed describes the region with lines formatted:\n"
     "          <chrom> <start> <end> <region> <seq no>\n"
     " chrom.agp is the intput AGP file\n"
     " out is the output AGP file, or directory if -dir option used\n"
     "\noptions:\n"
     "    -%s   directory for output AGP files, to be named <region>.agp",
                DIR_OPTION);
}

struct agp {
    /* represents a line of an AGP file -- either a fragment or a gap */
    struct agp *next;
    bool isFrag;
    void *entry;        /* agpFrag or agpGap */
} agp;

bool dirOption = FALSE;
char *namePrefix = "";

struct hash *agpLoadAll(char *agpFile)
/* load AGP entries into a hash of AGP lists, one per chromosome */
{
struct hash *chromAgpHash = newHash(0);
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lastPos = 0;
int wordCount, lineSize;
struct agpFrag *agpFrag;
struct agpGap *agpGap;
char *chrom;
struct agp *agpList, *agp;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#' || line[0] == '\n')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Bad AGP line %d of %s\n", lf->lineIx, lf->fileName);
    AllocVar(agp);
    chrom = words[0];
    if (!hashFindVal(chromAgpHash, chrom))
        lastPos = 1;
    if (words[4][0] != 'N' && words[4][0] != 'U')
        {
        /* not a gap */
        lineFileExpectWords(lf, 9, wordCount);
        agpFrag = agpFragLoad(words);
        if (agpFrag->chromStart != lastPos)
            errAbort(
               "Frag start (%d, %d) doesn't match previous end line %d of %s\n",
                     agpFrag->chromStart, lastPos, lf->lineIx, lf->fileName);
        if (agpFrag->chromEnd - agpFrag->chromStart != 
                        agpFrag->fragEnd - agpFrag->fragStart)
            errAbort("Sizes don't match in %s and %s line %d of %s\n",
                    agpFrag->chrom, agpFrag->frag, lf->lineIx, lf->fileName);
        lastPos = agpFrag->chromEnd + 1;
        agp->entry = agpFrag;
        agp->isFrag = TRUE;
        }
    else
        {
        /* gap */
        lineFileExpectWords(lf, 8, wordCount);
        agpGap = agpGapLoad(words);
        if (agpGap->chromStart != lastPos)
            errAbort("Gap start (%d, %d) doesn't match previous end line %d of %s\n",
                     agpGap->chromStart, lastPos, lf->lineIx, lf->fileName);
        lastPos = agpGap->chromEnd + 1;
        agp->entry = agpGap;
        agp->isFrag = FALSE;
        }
    if ((agpList = hashFindVal(chromAgpHash, chrom)) == NULL)
        {
        /* new chrom */
        /* add to hashes of chrom agp lists and sizes */
        agpList = agp;
        }
    else
        {
        slAddHead(&agpList, agp);
        hashRemove(chromAgpHash, chrom);
        }
    hashAdd(chromAgpHash, chrom, agpList);
    }
/* reverse AGP lists */
hashTraverseVals(chromAgpHash, slReverse);
return chromAgpHash;
}

void regionAgp(char *bedFile, char *agpIn, char *agpOut)
/* regionAgp - generate an AGP file for a region described in a bed file,
 * based on a chromosome-based AGP */
{
struct bed *pos, *posList;
struct hash *agpHash;
struct agp *agpList, *agp;
struct lineFile *lf = lineFileOpen(agpIn, TRUE);
FILE *fout = NULL;
int start = 1;
int seqNum = 1;
char regionName[64];
char outFile[64];

/* read in BED file with chromosome coordinate ranges */
//fprintf(stderr, "Loading bed file\n");
fprintf(stderr, "Loading bed file...\n");
posList = bedLoadNAll(bedFile, 5);

/* read chrom AGP file into a hash of AGP's, one per chrom */
fprintf(stderr, "Loading AGP's...\n");
agpHash = agpLoadAll(agpIn);

fprintf(stderr, "Creating new AGP's...\n");
if (!dirOption)
    fout = mustOpen(agpOut, "w");

/* process BED lines, emitting an AGP file */
for (pos = posList; pos != NULL; pos = pos->next)
    {
    if (pos->score == 1)
        /* score field of the BED is actually the sequence number
         * of the segment in the region */
        start = 1;
    verbose(2, "chr=%s, start=%d, end=%d, region=%s, seqnum=%d\n",
            pos->chrom, pos->chromStart, pos->chromEnd, pos->name, pos->score);
    safef(regionName, ArraySize(regionName), "%s%s_%d", 
                namePrefix, pos->name, pos->score);
    if (dirOption)
        {
        start = 1;
        seqNum = 1;
        safef(outFile, ArraySize(outFile), "%s/%s.agp", 
                        agpOut, regionName);
        fout = mustOpen(outFile, "w");
        }
    agpList = (struct agp *)hashMustFindVal(agpHash, pos->chrom);
    for (agp = agpList; agp != NULL; agp = agp->next)
    //for (agp = agpList->next; agp != NULL; agp = agp->next)
        {
        if (agp->isFrag)
            {
            /* fragment */
            int chromStart, chromEnd;
            struct agpFrag frag, *agpFrag = (struct agpFrag *)agp->entry;

            /* determine if this AGP entry intersects the range */
            if (pos->chromEnd < agpFrag->chromStart ||
                pos->chromStart > agpFrag->chromEnd)
                    continue;

            chromStart = max(pos->chromStart, agpFrag->chromStart);
            chromEnd = min(pos->chromEnd, agpFrag->chromEnd);

            /* populate the fragment */
            frag.chrom = regionName;
            frag.chromStart = start - 1;  // agpFragOutput adds 1
            frag.chromEnd = start + chromEnd - chromStart;
            frag.fragStart = agpFrag->fragStart +
                                chromStart - agpFrag->chromStart - 1;
            frag.fragEnd = agpFrag->fragEnd -
                                (agpFrag->chromEnd - chromEnd);
            start = frag.chromEnd + 1;
            frag.ix = seqNum++;;
            frag.type[0] = agpFrag->type[0];
            frag.type[1] = 0;
            frag.frag = agpFrag->frag;
            frag.strand[0] = agpFrag->strand[0];
            frag.strand[1] = 0;
            agpFragOutput(&frag, fout, '\t', '\n');
            }
        else
            {
            /* gap */
            struct agpGap gap, *agpGap = (struct agpGap *)agp->entry;
            int chromEnd = agpGap->chromStart + agpGap->size - 1;

            /* determine if this AGP entry intersects the range */
            if (pos->chromEnd < agpGap->chromStart ||
                pos->chromStart > agpGap->chromEnd)
                    continue;
            gap.chrom = regionName;
            gap.chromStart = start;
            gap.chromEnd = gap.chromStart + agpGap->size - 1;
            start = gap.chromEnd + 1;
            gap.ix = seqNum++;;
            gap.n[0] = 'N';
            gap.n[1] = 0;
            gap.size = agpGap->size;
            gap.type = agpGap->type;
            gap.bridge = agpGap->bridge;
            agpGapOutput(&gap, fout, '\t', '\n');
            }
        }
    if (dirOption)
        carefulClose(&fout);
    }
if (!dirOption)
    carefulClose(&fout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *regionBedFile;
char *chromAgpFile;
char *out;

optionInit(&argc, argv, options);
dirOption = optionExists(DIR_OPTION);
namePrefix = optionVal(NAME_PREFIX_OPTION, "");

if (argc != 4)
    usage();
regionBedFile = argv[1];
chromAgpFile = argv[2];
out = argv[3];
regionAgp(regionBedFile, chromAgpFile, out);
return 0;
}
