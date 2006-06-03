/* genePredToMafFrames - create mafFrames tables from genePreds  */
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

static char const rcsid[] = "$Id: genePredToMafFrames.c,v 1.9 2006/06/03 23:18:26 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"org", OPTION_STRING|OPTION_MULTI},
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

static void genePredToMafFrames(char *targetDb, int numGeneDbs, char **geneDbs,
                                char **genePreds, char *mafFramesFile,
                                char *bedFile, int numMafFiles, char **mafFiles)
/* create mafFrames tables from genePreds  */
{
/* get list of organisms and their genes */
struct orgGenes *orgs = loadGenePreds(numGeneDbs, geneDbs, genePreds);
struct orgGenes *genes;
int i;

/* process all organisms at once to avoid multiple maf reads */
for (i = 0; i < numMafFiles; i++)
    mkMafFramesForMaf(targetDb, orgs, mafFiles[i]);

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
    "genePredToMafFrames [options] -org=geneDb1,genePred1 [...] targetDb mafFrames maf1 [maf2..]\n"
    "or\n"
    "genePredToMafFrames [options] geneDb targetDb genePred mafFrames maf1 [maf2..]\n"
    "\n"
    "Create frame annotations for a component of aa set of MAFs.  The\n"
    "resulting file maybe combined with mafFrames for other components.\n"
    "The first form allows specifying multiple organisms at once.  This\n"
    "is significanly faster, as most of the first is spent reading the MAF\n"
    "\n"
    "Arguments:\n"
    "  o geneDb - db in MAF that corresponds to genePred's organism.\n"
    "  o targetDb - db of target genome\n"
    "  o genePred - genePred file.  Overlapping annotations ahould have\n"
    "    be removed.  This file may optionally include frame annotations\n"
    "  o mafFrames - output file\n"
    "  o maf1,... - MAF files\n"
    "Options:\n"
    "  -org=geneDb,genePred - specify a pair of a geneDb and a genePred file.\n"
    "   these arguments maybe comma or whitespace separated (in which case they\n"
    "   must be quoted).  Maybe specified multiple times.\n"
    "  -bed=file - output a bed of for each mafFrame region, useful for debugging.\n"
    "  -verbose=level - enable verbose tracing, the following levels are implemented:\n"
    "     3 - print information about data used to compute each record.\n"
    "     4 - dump information about the gene mappings that were constructed\n"
    "     5 - dump information about the gene mappings after split processing\n"
    "     6 - dump information about the gene mappings after frame linking\n"
    "\n", msg);
}

static void parseOrgOpt(char *value, char **org, char **genePred)
/* parse an org argument */
{
int i;
char *sep = NULL;
/* find first whitespace or comma (this will allow commas in file names */
for (i = 0; (value[i] != '\0') && (sep == NULL); i++)
    if (isspace(value[i]) || (value[i] == ','))
        sep = &(value[i]);
if (sep == NULL)
    errAbort("-opt value should be whitespace or comma separate pair: %s", value);
*sep = '\0';
*org = trimSpaces(value);
*genePred = trimSpaces(sep+1);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (optionExists("org"))
    {
    struct slName *orgGps, *orgGp;
    int numGeneDbs, i;
    char **geneDbs, **genePreds;
    if (argc < 4)
        usage("wrong # args");
    orgGps = optionMultiVal("org", NULL);
    numGeneDbs = slCount(orgGps);
    AllocArray(geneDbs, numGeneDbs);
    AllocArray(genePreds, numGeneDbs);
    i = 0;
    for (orgGp = orgGps; orgGp != NULL; orgGp = orgGp->next, i++)
        parseOrgOpt(orgGp->name, &(geneDbs[i]), &(genePreds[i]));
    genePredToMafFrames(argv[1], numGeneDbs, geneDbs, genePreds, argv[2],
                        optionVal("bed", NULL), argc-3, argv+3);
    }
else
    {
    if (argc < 6)
        usage("wrong # args");
    genePredToMafFrames(argv[2], 1, &(argv[1]), &(argv[3]), argv[4],
                        optionVal("bed", NULL), argc-5, argv+5);
    }
return 0;
}
