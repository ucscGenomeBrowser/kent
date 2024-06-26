/* chainArrangeCollect - collect overlapping beds into a single bed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "dystring.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainArrangeCollect - collect overlapping beds into a chainArrange.as structure\n. Names are made with prefix.\n"
  "usage:\n"
  "   chainArrangeCollect prefix input.bed output.bed\n"
  "note: input beds need to be sorted with bedSort\n"
  "options:\n"
  "   -exact       overlapping blocks must be exactly the same range and score\n"
  );
}

boolean exact;  // overlapping blocks must be exactly the same range and score

/* Command line validation table. */
static struct optionSpec options[] = {
   {"exact", OPTION_BOOLEAN},
   {NULL, 0},
};

static void outBed(FILE *f, char *prefix, struct bed *bed, struct hash *nameHash)
{
static int count = 0;
struct slName *names = hashSlNameFromHash(nameHash);
int sizeQuery = bed->score;
bed->score = slCount(names);
struct dyString *dy = newDyString(100);
for(; names; names = names->next)
    {
    dyStringAppend(dy, names->name);
    if (names->next)
        dyStringAppend(dy, ",");
    }
bed->name = dy->string;

// we're actually not outputting chainArrange structure because the label is coming
// from an external program currently
fprintf(f, "%s\t%d\t%d\t%s%d.1\t%d\t+\t%d\t%d\t0\t%s\t%d\n", bed->chrom, bed->chromStart, bed->chromEnd, prefix, count++, bed->score, bed->chromStart, bed->chromEnd, bed->name, sizeQuery);
}

void chainArrangeCollect(char *prefix, char *inFile, char *outFile)
/* chainArrangeCollect - collect overlapping beds into a single bed. */
{
struct bed *allBeds = bedLoadAll(inFile);
FILE *f = mustOpen(outFile, "w");
struct bed *bed, *prevBed = allBeds;
prevBed->score = 1;
struct hash *nameHash = newHash(0);
hashStore(nameHash, prevBed->name);

if (exact)
    {
    for(bed = prevBed->next; bed;  bed = bed->next)
        {
        if (differentString(prevBed->chrom, bed->chrom) || (prevBed->chromStart != bed->chromStart) || (prevBed->chromEnd != bed->chromEnd) || (prevBed->score != bed->score))
            {
            outBed(f, prefix, prevBed, nameHash);

            freeHash(&nameHash);
            nameHash = newHash(0);
            prevBed = bed;
            hashStore(nameHash, bed->name);
            }
        else
            {
            hashStore(nameHash, bed->name);
            }
        }
    }
else
    {
    for(bed = prevBed->next; bed;  bed = bed->next)
        {
        if (differentString(prevBed->chrom, bed->chrom) || (prevBed->chromEnd <= bed->chromStart))
            {
            outBed(f, prefix, prevBed, nameHash);

            freeHash(&nameHash);
            nameHash = newHash(0);
            prevBed = bed;
            hashStore(nameHash, bed->name);
            }
        else
            {
            hashStore(nameHash, bed->name);
            prevBed->chromEnd = (bed->chromEnd > prevBed->chromEnd) ?  bed->chromEnd : prevBed->chromEnd;
            }
        }
    outBed(f, prefix, prevBed, nameHash);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
exact = optionExists("exact");
chainArrangeCollect(argv[1], argv[2], argv[3]);
return 0;
}
