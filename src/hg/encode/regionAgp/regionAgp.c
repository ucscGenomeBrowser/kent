/* regionAgp - generate an AGP file for an ENCODE region described in
 *              a bed file, containing lines of the format:
 *                      <chrom> <start> <end> <region>
 *              based on the chromosome AGP's.              
 */

#include "common.h"
#include "options.h"
#include "bed.h"
#include "agp.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "contigAcc.h"

static char const rcsid[] = "$Id: regionAgp.c,v 1.9 2004/10/06 17:17:08 kate Exp $";

#define DIR_OPTION              "dir"
#define NAME_PREFIX_OPTION      "namePrefix"
#define CONTIG_FILE_OPTION      "contigFile"

static struct optionSpec options[] = {
        {DIR_OPTION, OPTION_BOOLEAN},
        {NAME_PREFIX_OPTION, OPTION_STRING},
        {CONTIG_FILE_OPTION, OPTION_STRING},
        {NULL, 0}
};

bool dirOption = FALSE;
char *namePrefix = "";
char *contigFile = NULL;

void usage()
/* Print usage instructions and exit. */
{
fprintf(stderr, "regionAgp - generate an AGP file for a region\n"
     "\nusage:\n"
     "    regionAgp region.bed chrom.agp out\n\n"
     " region.bed describes the region with lines formatted:\n"
     "          <chrom> <start> <end> <region> <seq no>\n"
     " chrom.agp is the input AGP file\n"
     " out is the output AGP file, or directory if -dir option used\n"
     "\noptions:\n");
fprintf(stderr, 
    "    -%s   prefix for sequence name and AGP file (usually species_)\n",
                        NAME_PREFIX_OPTION);
fprintf(stderr,
    "    -%s   file name for contig to accession map (dump contigAcc table)\n
                    used if AGP contains contigs not accessioned.\n",
                        CONTIG_FILE_OPTION);
fprintf(stderr,
    "    -%s   directory for output AGP files, to be named <region>.agp",
                        DIR_OPTION);
errAbort("");
}

struct hash *contigAccLoadToHash(char *contigFile)
/* load accessions into  hash by contig name */
{
struct hash *contigAccHash = newHash(0);
struct contigAcc *contigAcc, *contigAccList;

contigAccList = contigAccLoadAll(contigFile);
for (contigAcc = contigAccList; contigAcc->next; contigAcc = contigAcc->next)
    hashAddUnique(contigAccHash, contigAcc->contig, contigAcc->acc);
return contigAccHash;
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
struct contigAcc *contigAcc, *contigAccList;
struct hash *contigAccHash = NULL;

/* read in BED file with chromosome coordinate ranges */
fprintf(stderr, "Loading bed file...\n");
posList = bedLoadNAll(bedFile, 5);

/* read chrom AGP file into a hash of AGP's, one per chrom */
fprintf(stderr, "Loading AGP's...\n");
agpHash = agpLoadAll(agpIn);

fprintf(stderr, "Creating new AGP's...\n");
if (!dirOption)
    fout = mustOpen(agpOut, "w");

if (contigFile != NULL)
    {
    /* read in contig to accession mapping */
    contigAccHash = contigAccLoadToHash(contigFile);
    }

/* process BED lines, emitting an AGP file */
for (pos = posList; pos != NULL; pos = pos->next)
    {
    /* convert BED start to 1-based, since AGP is */
    pos->chromStart++;
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
            if (contigFile)
                frag.frag = hashMustFindVal(contigAccHash, agpFrag->frag);
            else
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
contigFile = optionVal(CONTIG_FILE_OPTION, NULL);

if (argc != 4)
    usage();
regionBedFile = argv[1];
chromAgpFile = argv[2];
out = argv[3];
regionAgp(regionBedFile, chromAgpFile, out);
return 0;
}
