/* genePredToMafFrames - create mafFrames tables from genePreds  */
#include "common.h"
#include "options.h"
#include "mafFrames.h"
#include "mkMafFrames.h"
#include "geneBins.h"
#include "chromBins.h"
#include "binRange.h"
#include "verbose.h"

static char const rcsid[] = "$Id: genePredToMafFrames.c,v 1.3 2005/10/15 00:35:05 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"bed", OPTION_STRING},
    {NULL, 0}
};

static void outputExonFrames(struct cdsExon *exon, FILE *frameFh, FILE *bedFh)
/* output mafFrames for an exon */
{
struct exonFrames *ef;
for (ef = exon->frames; ef != NULL; ef = ef->next)
    {
    struct mafFrames *mf = &ef->mf;
    mafFramesTabOut(mf, frameFh);
    if (bedFh != NULL)
        fprintf(bedFh, "%s\t%d\t%d\t%s\t%d\t%s\n",
                mf->chrom, mf->chromStart, mf->chromEnd,
                mf->name, mf->frame, mf->strand);
    }
}

static void outputChromFrames(struct binKeeper *chromBins, FILE *frameFh, FILE *bedFh)
/* output frames from a chromosome's bins */
{
struct binKeeperCookie cookie = binKeeperFirst(chromBins);
struct binElement* exonRef;
while ((exonRef = binKeeperNext(&cookie)) != NULL)
    outputExonFrames((struct cdsExon*)exonRef->val, frameFh, bedFh);
}

static void outputFrames(struct geneBins *genes, char *mafFramesFile, char *bedFile)
/* output all mafFrames rows */
{
FILE *frameFh = mustOpen(mafFramesFile, "w");
FILE *bedFh = (bedFile != NULL) ? mustOpen(bedFile, "w") : NULL;
struct slName *chroms = chromBinsGetChroms(genes->bins); 
struct slName *chrom;

for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    {
    struct binKeeper *chromBins = chromBinsGet(genes->bins, chrom->name, FALSE);
    if (chromBins != NULL)
        outputChromFrames(chromBins, frameFh, bedFh);
    }

slFreeList(&chroms);
carefulClose(&frameFh);
carefulClose(&bedFh);
}

static void genePredToMafFrames(char *geneDb, char *targetDb, char *genePredFile,
                                char *mafFramesFile, char *bedFile,
                                int numMafFiles, char **mafFiles)
/* create mafFrames tables from genePreds  */
{
struct geneBins *genes = geneBinsNew(genePredFile);
int i;

for (i = 0; i < numMafFiles; i++)
    mkMafFramesForMaf(geneDb, targetDb, genes, mafFiles[i]);

mkMafFramesFinish(genes);
outputFrames(genes, mafFramesFile, bedFile);

geneBinsFree(&genes);
}

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
    "\n"
    "genePredToMafFrames - create mafFrames tables from a genePreds\n"
    "\n"
    "genePredToMafFrames [options] geneDb targetDb genePred mafFrames maf1 [maf2..]\n"
    "\n"
    "Create frame annotations for a component of aa set of MAFs.  The\n"
    "resulting file maybe combined with mafFrames for other components.\n"
    "\n"
    "Arguments:\n"
    "  o geneDb - db in MAF that corresponds to genePred's organism.\n"
    "  o targetDb - db of target track in MAF\n"
    "  o genePred - genePred file.  Overlapping annotations ahould have\n"
    "    be removed.  This file may optionally include frame annotations\n"
    "  o mafFrames - output file\n"
    "  o maf1,... - MAF files\n"
    "Options:\n"
    "  -bed=file - output a bed of for each mafFrame region, useful for debugging.\n"
    "  -verbose=level - enable verbose tracing, the following levels are implemented:\n"
    "     3 - print information about data used to compute each record.\n"
    "\n", msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 6)
    usage("wrong # args");
genePredToMafFrames(argv[1], argv[2], argv[3], argv[4], optionVal("bed", NULL),
                    argc-5, argv+5);

return 0;
}
