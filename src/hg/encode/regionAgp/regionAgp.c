/* regionAgp - generate an AGP file for an ENCODE region described in
 *              a bed file, containing lines of the format:
 *                      <chrom> <start> <end> <region>
 *              based on the chromosome AGP's.              
 */

#include "common.h"
#include "bed.h"
#include "agpFrag.h"
#include "agpGap.h"

static char const rcsid[] = "$Id: regionAgp.c,v 1.1 2004/08/12 00:37:57 kate Exp $";

void usage()
/* Print usage instructions and exit. */
{
errAbort("regionAgp - generate an AGP file for a region\n\n"
     "usage:\n"
     "    regionAgp region.bed chrom.agp region.agp\n\n"
     " region.bed describes the region with lines formatted:\n"
     "          <chrom> <start> <end> <region> <seq no>\n"
     " chrom.agp is the intput AGP file\n"
     " region.agp is the output AGP file\n");
}

struct agp {
    /* represents a line of an AGP file -- either a fragment or a gap */
    struct agp *next;
    bool isFrag;
    void *entry;        /* agpFrag or agpGap */
} agp;

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
            errAbort("Start doesn't match previous end line %d of %s\n",
                         lf->lineIx, lf->fileName);
        if (agpFrag->chromEnd - agpFrag->chromStart != 
                        agpFrag->fragEnd - agpFrag->fragStart)
            errAbort("Sizes don't match in %s and %s line %d of %s\n",
                    agpFrag->chrom, agpFrag->frag, lf->lineIx, lf->fileName);
        if ((agpList = hashFindVal(chromAgpHash, chrom)) == NULL)
            {
            /* new chrom */
            /* add to hashes of chrom agp lists and sizes */
            AllocVar(agpList);
            hashAdd(chromAgpHash, chrom, agpList);
            }
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
            errAbort("Start doesn't match previous end line %d of %s\n",
                         lf->lineIx, lf->fileName);
        if ((agpList = hashFindVal(chromAgpHash, chrom)) == NULL)
            {
            /* new chrom */
            AllocVar(agpList);
            /* add to hashes of chrom agp lists and sizes */
            hashAdd(chromAgpHash, chrom, agpList);
            }
        lastPos = agpGap->chromEnd + 1;
        agp->entry = agpGap;
        agp->isFrag = FALSE;
        }
    /* TODO: speed up by adding to head, then at the end, go through
     * hashes, reversing each list */
    slAddTail(&agpList, agp);
    }
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
FILE *fout = mustOpen(agpOut, "w");
int start = 1;
int seqNum = 1;

/* read in BED file with chromosome coordinate ranges */
fprintf(stderr, "Loading bed file\n");
posList = bedLoadNAll(bedFile, 5);

/* read chrom AGP file into a hash of AGP's, one per chrom */
fprintf(stderr, "Loading AGP's\n");
agpHash = agpLoadAll(agpIn);

/* process BED lines, emitting an AGP file */
for (pos = posList; pos != NULL; pos = pos->next)
    {
    if (pos->score == 1)
        start = 1;
#ifdef DEBUG
    fprintf(stderr, "chr=%s, start=%d, end=%d, region=%s, seqnum=%d\n",
            pos->chrom, pos->chromStart, pos->chromEnd, pos->name, pos->score);
#endif
    agpList = (struct agp *)hashMustFindVal(agpHash, pos->chrom);
    for (agp = agpList->next; agp != NULL; agp = agp->next)
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
            frag.chrom = pos->name;
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
            gap.chrom = pos->name;
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
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *regionBedFile = argv[1];
char *chromAgpFile = argv[2];
char *regionAgpFile = argv[3];

if (argc != 4)
    usage();
regionAgp(regionBedFile, chromAgpFile, regionAgpFile);
return 0;
}
