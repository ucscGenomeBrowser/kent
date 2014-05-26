/* genePredToMafFrames - create mafFrames tables from genePreds  */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "mafFrames.h"
#include "mkMafFrames.h"
#include "splitMultiMappings.h"
#include "finishMafFrames.h"
#include "orgGenes.h"
#include "chromBins.h"
#include "binRange.h"
#include "verbose.h"


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

static void outputFrames(struct orgGenes *orgs, char *mafFramesFile, char *bedFile)
/* output all mafFrames rows */
{
FILE *frameFh = mustOpen(mafFramesFile, "w");
FILE *bedFh = (bedFile != NULL) ? mustOpen(bedFile, "w") : NULL;
struct orgGenes *genes;
struct gene *gene;
struct cdsExon *exon;
for (genes = orgs; genes != NULL; genes = genes->next)
    {
    for (gene = genes->genes; gene != NULL; gene = gene->next)
        {
        geneCheck(gene);
        for (exon = gene->exons; exon != NULL; exon = exon->next)
            outputExonFrames(exon, frameFh, bedFh);
        }
    }
carefulClose(&frameFh);
carefulClose(&bedFh);
}

static void dumpGeneInfo(char *desc, struct orgGenes *orgs)
/* dump information about genes */
{
FILE *fh = verboseLogFile();
int i;
struct orgGenes *genes;
struct gene *gene;
for (genes = orgs; genes != NULL; genes = genes->next)
    {
    fprintf(fh, "srcDb: %s\n", genes->srcDb);
    for (gene = genes->genes; gene != NULL; gene = gene->next)
        {
        geneSortFramesOffTarget(gene);
        fprintf(fh, "%s: ", desc);
        geneDump(fh, gene);
        }
    for (i = 0; i < 90; i++)
        fputc('-', fh);
    fputc('\n', fh);
    }
}

static struct orgGenes *loadGenePreds(int numGeneDbs, char **geneDbs,
                                      char **genePreds)
/* build list of genes for all organisms */
{
struct orgGenes *orgs = NULL;
int i;
for (i = 0; i < numGeneDbs; i++)
    slSafeAddHead(&orgs, orgGenesNew(geneDbs[i], genePreds[i]));
return orgs;
}

static void genePredToMafFrames(char *targetDb, char *mafFile, char *mafFramesFile,
                                int numGeneDbs, char **geneDbs, char **genePreds,
                                char *bedFile)
/* create mafFrames tables from genePreds  */
{
/* get list of organisms and their genes */
struct orgGenes *orgs = loadGenePreds(numGeneDbs, geneDbs, genePreds);
struct orgGenes *genes;

mkMafFramesForMaf(targetDb, orgs, mafFile);

if (verboseLevel() >= 4)
    dumpGeneInfo("after load", orgs);
for (genes = orgs; genes != NULL; genes = genes->next)
    splitMultiMappings(genes);
if (verboseLevel() >= 5)
    dumpGeneInfo("after split", orgs);
for (genes = orgs; genes != NULL; genes = genes->next)
    finishMafFrames(genes);
if (verboseLevel() >= 5)
    dumpGeneInfo("after finish", orgs);
outputFrames(orgs, mafFramesFile, bedFile);

orgGenesFree(&genes);
}

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
    "\n"
    "genePredToMafFrames - create mafFrames tables from a genePreds\n"
    "\n"
    "genePredToMafFrames [options] targetDb maf mafFrames geneDb1 genePred1 [geneDb2 genePred2...] \n"
    "\n"
    "Create frame annotations for one or more components of a MAF.\n"
    "It is significantly faster to process multiple gene sets in the same\""
    "run, as 95%% of the CPU time is spent reading the MAF\n"
    "\n"
    "Arguments:\n"
    "  o targetDb - db of target genome\n"
    "  o maf - input MAF file\n"
    "  o mafFrames - output file\n"
    "  o geneDb1 - db in MAF that corresponds to genePred's organism.\n"
    "  o genePred1 - genePred file.  Overlapping annotations ahould have\n"
    "    be removed.  This file may optionally include frame annotations\n"
    "Options:\n"
    "  -bed=file - output a bed of for each mafFrame region, useful for debugging.\n"
    "  -verbose=level - enable verbose tracing, the following levels are implemented:\n"
    "     3 - print information about data used to compute each record.\n"
    "     4 - dump information about the gene mappings that were constructed\n"
    "     5 - dump information about the gene mappings after split processing\n"
    "     6 - dump information about the gene mappings after frame linking\n"
    "\n", msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int numGeneDbs, i, j;
char **geneDbs, **genePreds;
optionInit(&argc, argv, optionSpecs);
if ((argc < 6) || ((argc % 2) != 0))
    usage("wrong # args");
numGeneDbs = (argc - 4) / 2;
AllocArray(geneDbs, numGeneDbs);
AllocArray(genePreds, numGeneDbs);

for (i = 0, j = 4; j < argc; i++, j += 2)
    {
    geneDbs[i] = argv[j];
    genePreds[i] = argv[j+1];
    }
/* try to detect old command line arguments, and warn user of change:
 * genePredToMafFrames geneDb   targetDb genePred  mafFrames maf1 [maf2..]\n"
 * 0                   1        2        3         4         5           
  "genePredToMafFrames targetDb maf      mafFrames geneDb1   genePred1 [geneDb2 genePred2...] \n"
 */

if (endsWith(argv[3], ".gp") || sameString(argv[3], "stdin")
    || endsWith(argv[4], "mafFrame") || sameString(argv[4], "stdout")
    || endsWith(argv[5], "maf") || endsWith(argv[5], "maf.gz")
    || sameString(argv[5], "stdin"))
    errAbort("Error: appear to be using old command arguments, please review usage");

genePredToMafFrames(argv[1], argv[2], argv[3], 
                    numGeneDbs, geneDbs, genePreds,
                    optionVal("bed", NULL));
return 0;
}
