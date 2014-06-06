/* gffToBed - Convert a gff file (gff1 or gff2) to bed.  Not tested with gff3 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
    verbose(2, "bestFeature is %s with %d count\n", bestFeature->name, bestFeature->count);
    return bestFeature->name;
    }
}

boolean allSameSeq(struct gffGroup *group)
/* Return TRUE if all members of group are on same chromosome. */
{
struct gffLine *gl;
char *seq = group->lineList->seq;
for (gl = group->lineList->next; gl != NULL; gl = gl->next)
    {
    if (!sameString(gl->seq, seq))
        return FALSE;
    }
return TRUE;
}

struct gffGroup *splitGroupByChrom(struct gffFile *gff, struct gffGroup *oldGroup)
/* Split up a group into multiple groups,  each one chromosome specific. */
{
struct gffGroup *groupList = NULL, *group;
struct hash *seqHash = hashNew(0);

verbose(2, "Regrouping %s with %d elements\n", oldGroup->name, slCount(oldGroup->lineList));
struct gffLine *gl, *nextGl;
for (gl = oldGroup->lineList; gl != NULL; gl = nextGl)
    {
    nextGl = gl->next;
    group = hashFindVal(seqHash, gl->seq);
    if (group == NULL)
        {
	AllocVar(group);
	group->name = oldGroup->name;
	group->seq = gl->seq;
	group->source = oldGroup->source;
	group->start = gl->start;
	group->end = gl->end;
	group->strand = gl->strand;
	slAddHead(&groupList, group);
	hashAdd(seqHash, group->seq, group);
	}
    else
        {
	group->start = min(gl->start, group->start);
	group->end = max(gl->end, group->end);
	}
    slAddHead(&group->lineList, gl);
    }
hashFree(&seqHash);
for (group = groupList; group != NULL; group = group->next)
    slReverse(&group->lineList);
return groupList;
}

void separateGroupsByChromosome(struct gffFile *gff)
/* The genePredFromGroupedGtf has trouble with groups that span chromosomes.
 * So here we go through and split up groups by chromosome. */
{
struct gffGroup *newList = NULL, *nextGroup, *group, *splitGroup;
for (group = gff->groupList; group != NULL; group = nextGroup)
    {
    nextGroup = group->next;
    if (allSameSeq(group))
        {
	slAddHead(&newList, group);
	}
    else
        {
	splitGroup = splitGroupByChrom(gff, group);
	newList = slCat(splitGroup, newList);
	}
    }
slReverse(&newList);
gff->groupList = newList;
}

void gffToBed(char *inGff, char *outBed)
/* gffToBed - Convert a gff file (gff1 or gff2) to bed.  Not tested with gff3 */
{
struct gffFile *gff = gffRead(inGff);
FILE *f = mustOpen(outBed, "w");
char *exonFeature = bestExonFeature(gff);
gffGroupLines(gff);
separateGroupsByChromosome(gff);
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
