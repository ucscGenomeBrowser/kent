/* bedCollect - collect overlapping beds into a single bed. */
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
  "bedCollect - collect overlapping beds into a single bed\n"
  "usage:\n"
  "   bedCollect input.bed output.bed\n"
  "note: input beds need to be sorted with bedSort\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void outBed(FILE *f, struct bed *bed, struct hash *nameHash)
{
static int count = 0;
struct slName *names = hashSlNameFromHash(nameHash);
bed->score = slCount(names);
struct dyString *dy = newDyString(100);
for(; names; names = names->next)
    {
    dyStringAppend(dy, names->name);
    dyStringAppend(dy, ",");
    }
bed->name = dy->string;
fprintf(f, "%s %d %d arr%d %d + %d %d 0 %s\n", bed->chrom, bed->chromStart, bed->chromEnd, count++, bed->score, bed->chromStart, bed->chromEnd, bed->name);
//bedOutputN(bed, 5, f, '\t', '\n');
}

void bedCollect(char *inFile, char *outFile)
/* bedCollect - collect overlapping beds into a single bed. */
{
struct bed *allBeds = bedLoadAll(inFile);
FILE *f = mustOpen(outFile, "w");
struct bed *bed, *prevBed = allBeds;
prevBed->score = 1;
struct hash *nameHash = newHash(0);

for(bed = allBeds; bed;  bed = bed->next)
    {
    bed->score = 1;

    if (differentString(prevBed->chrom, bed->chrom) || (prevBed->chromEnd <= bed->chromStart))
        {
        outBed(f, prevBed, nameHash);

        nameHash = newHash(0);
        prevBed = bed;
        hashStore(nameHash, bed->name);
        }
    else
        {
        hashStore(nameHash, bed->name);
        prevBed->chromEnd = (bed->chromEnd > prevBed->chromEnd) ?  bed->chromEnd : prevBed->chromEnd;
        prevBed->score++;
        //printf("merging %d %d %d %d %d %s\n", prevBed->chromStart, prevBed->chromEnd, bed->chromStart, bed->chromEnd, prevBed->score, bed->name);
        }
    }
outBed(f, prevBed, nameHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedCollect(argv[1], argv[2]);
return 0;
}
