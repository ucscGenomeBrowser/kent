/* edwSamRepeatAnalysis - Analyze result of alignment vs. RepeatMasker type libraries.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSamRepeatAnalysis - Analyze result of alignment vs. RepeatMasker type libraries.\n"
  "usage:\n"
  "   edwSamRepeatAnalysis in.sam out.ra\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwSamRepeatAnalysis(char *inSam, char *outRa)
/* edwSamRepeatAnalysis - Analyze result of alignment vs. RepeatMasker type libraries.. */
{
/* Go through sam file, filling in hiLevelHash with count of each hi level repeat class we see. */
struct hash *hiLevelHash = hashNew(0);
samfile_t *sf = samopen(inSam, "r", NULL);
bam_hdr_t *bamHeader = sam_hdr_read(sf);
bam1_t one;
ZeroVar(&one);
int err;
long long hit = 0, miss = 0;
while ((err = sam_read1(sf, bamHeader, &one)) >= 0)
    {
    int32_t tid = one.core.tid;
    if (tid < 0)
	{
	++miss;
        continue;
	}
    ++hit;

    /* Parse out hiLevel classification from target,  which is something like 7SLRNA#SINE/Alu 
     * from which we'd want to extract SINE.  The '/' is not present in all input. */
    char *target = bamHeader->target_name[tid];
    char *hashPos = strchr(target, '#');
    if (hashPos == NULL)
        errAbort("# not found in target %s", target);
    char *hiLevel = cloneString(hashPos + 1);
    char *slashPos = strchr(hiLevel, '/');
    if (slashPos != NULL)
        *slashPos = 0;

    hashIncInt(hiLevelHash, hiLevel);
    }
samclose(sf);

/* Output some basic stats as well as contents of hash */
FILE *f = mustOpen(outRa, "w");
double invTotal = 1.0 / (hit + miss);
double mapRatio = (double)hit * invTotal;
struct hashEl *hel, *helList = hashElListHash(hiLevelHash);
slSort(&helList, hashElCmp);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    double hitRatio = ptToInt(hel->val) * invTotal;
    fprintf(f, "%s %g\n", hel->name, hitRatio);
    }
fprintf(f, "total %g\n", mapRatio);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwSamRepeatAnalysis(argv[1], argv[2]);
return 0;
}
