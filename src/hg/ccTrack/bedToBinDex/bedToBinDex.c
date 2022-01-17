/* bedToBinDex - Convert bed file to binary and index it. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedToBinDex - Convert bed file to binary and index it.\n"
  "usage:\n"
  "   bedToBinDex in.bed out.bin out.dex\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


int bedCmpStartEnd(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart,-chromEnd. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = b->chromStart - a->chromStart;
return dif;
}


void bedToBinDex(char *input, char *binary, char *index)
/* bedToBinDex - Convert bed file to binary and index it. */
{
struct bed *bed, *bedList = bedLoadAll(input);
int bedCount = slCount(bedList);
verbose(1, "%d records loaded from %s\n", bedCount, input);
slSort(&bedList, bedCmpStartEnd);
verbose(1, "sorted\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedToBinDex(argv[1], argv[2], argv[3]);
return 0;
}
