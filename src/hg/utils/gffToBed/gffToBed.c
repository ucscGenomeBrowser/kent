/* gffToBed - Convert a gff file (gff1 or gff2) to bed.  Not tested with gff3 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "gff.h"
#include "genePred.h"
#include "bed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gffToBed - Convert a gff file (gff1 or gff2) to bed.  Not tested with gff3.\n" 
  "Works with a wide range of gff files, but it's such a loose spec, likely not all.\n"
  "usage:\n"
  "   gffToBed in.gff out.bed\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *bestExonFeature(struct gffFile *gff)
/* Snoop through lines in gff, and figure out best feature to use for our exons. */
{
if (hashLookup(gff->featureHash, "exon"))
    return "exon";
if (hashLookup(gff->featureHash, "EXON"))
    return "EXON";
struct gffFeature *bestFeature = NULL, *feature;
int bestCount = 0;
for (feature = gff->featureList; feature != NULL; feature = feature->next)
    {
    if (feature->count > bestCount)
        {
	bestCount = feature->count;
	bestFeature = feature;
	}
    }
if (bestFeature == NULL)
    return "exon";
else
    {
    return bestFeature->name;
    }
}

void gffToBed(char *inGff, char *outBed)
/* gffToBed - Convert a gff file (gff1 or gff2) to bed.  Not tested with gff3 */
{
struct gffFile *gff = gffRead(inGff);
FILE *f = mustOpen(outBed, "w");
char *exonFeature = bestExonFeature(gff);
gffGroupLines(gff);
struct gffGroup *group;
for (group = gff->groupList; group != NULL; group = group->next)
    {
    struct genePred *gp;
    if (gff->isGtf)
        gp = genePredFromGroupedGtf(gff, group, group->name, FALSE, FALSE);
    else
        gp = genePredFromGroupedGff(gff, group, group->name, exonFeature, FALSE, FALSE);
    if (gp != NULL)
	{
	assert(gp->txStart == gp->exonStarts[0]);
	struct bed *bed = bedFromGenePred(gp);
	bedTabOutN(bed, 12, f);
	bedFree(&bed);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
gffToBed(argv[1], argv[2]);
return 0;
}
