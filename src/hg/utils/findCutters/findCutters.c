/* findCutters - Find REBASE restriction enzymes using their GCG file. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaLoad.h"
#include "bed.h"
#include "cutter.h"

/* Program parameters. */
char *justThis = NULL;
char *justThese = NULL;
boolean countsOnly = FALSE;
boolean consolidateCounts = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findCutters - Find REBASE restriction enzymes using their GCG file\n"
  "usage:\n"
  "   findCutters rebase.gcg sequence output.bed\n"
  "where \"sequence\" is a .fa, .nib, or .2bit file\n"
  "options:\n"
  "   -justThis=enzyme    Only search for this enzyme.\n"
  "   -justThese=file     File of enzymes (one per line) to restrict search.\n"
  "   -countsOnly         Only output the # of times each enzyme is found\n"
  "                       in the sequence in a simple 2 column file.\n"
  "   -consolidateCounts  This option is used in the situation that a bunch\n"
  "                       of output files have been created and cat'ed\n"
  "                       together (Like after a cluster run).  The program\n"
  "                       usage then changes to:\n"
  "   findCutters -consolidateCounts input.counts output.counts\n\n"
  "NOTE: a proper GCG file is the one available from NEB, using a command "
  "like: \n"
  "  curl -A \"Mozilla/4.0\" http://rebase.neb.com/rebase/link_gcgenz > rebase.gcg\n"
  );
}

static struct optionSpec options[] = {
   {"justThis", OPTION_STRING},
   {"justThese", OPTION_STRING},
   {"countsOnly", OPTION_BOOLEAN},
   {"consolidateCounts", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *initCutterCountHash(struct cutter *cutters)
/* Return one of those hashes (keyed on enz name) of ints all set to zero. */
{
struct hash *countHash = newHash(12);
struct cutter *cutter;
for (cutter = cutters; cutter != NULL; cutter = cutter->next)
    hashAddInt(countHash, cutter->name, 0);
return countHash;
}

void addCountsToHash(struct hash *countHash, struct bed *bedList)
/* Add up the ones we find in the bed to the count hash. */
{
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct hashEl *el = hashLookup(countHash, bed->name);
    int newCount = ptToInt(el->val) + 1;
    el->val = intToPt(newCount);
    }
}

void spitBedList(struct bed *bedList, FILE *output)
/* Simply output the beds to a file one at a time. */
{
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    bedTabOutN(bed, 6, output);
}

void findBeds(struct cutter *cutters, struct dnaSeq *seqs, char *outputFile)
/* Output all beds found to a file. */
{
struct dnaSeq *seq;
FILE *f = mustOpen(outputFile, "w");
for (seq = seqs; seq != NULL; seq = seq->next)
    {
    struct bed *bedList = matchEnzymes(cutters, seq, 0);
    if (bedList)
	{
	spitBedList(bedList, f);
	bedFreeList(&bedList);
	}
    }
carefulClose(&f);
}

void writeHashToFile(struct hash *countHash, char *outputFile)
/* Make an slPair list out of a hashEl list, sort it, output it. */
{
FILE *f = mustOpen(outputFile, "w");
struct hashEl *el, *list = hashElListHash(countHash);
slNameSort((struct slName **)&list);
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\t%d\n", el->name, ptToInt(el->val));
carefulClose(&f);
hashElFreeList(&list);
}

void findCounts(struct cutter *cutters, struct dnaSeq *seqs, char *outputFile)
/* Go through each sequence, and each time add the counts of the enzymes */
/* encountered to the hash of counts. */
{
struct dnaSeq *seq;
struct hash *countHash = initCutterCountHash(cutters);
for (seq = seqs; seq != NULL; seq = seq->next)
    {
    struct bed *bedList = matchEnzymes(cutters, seq, 0);
    if (bedList)
	{
	addCountsToHash(countHash, bedList);
	bedFreeList(&bedList);
	}    
    }
writeHashToFile(countHash, outputFile);
}

struct slName *getWhiteListFromFile()
/* Read in the -justThese file and store the names of the enzymes */
/* in a list of slNames. */
{
struct slName *whiteList = NULL;
struct lineFile *lf = lineFileOpen(justThese, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    struct slName *newName = newSlName(line);
    slAddHead(&whiteList, newName);
    }
lineFileClose(&lf);
return whiteList;
}

void findCutters(char *gcgFile, char *genome, char *outputFile)
/* findCutters - Find REBASE restriction enzymes using their GCG file. */
{
struct cutter *cutters = readGcg(gcgFile);
struct dnaSeq *seqs = dnaLoadAll(genome);
struct slName *whiteList = NULL;
if (justThis)
    whiteList = newSlName(justThis);
if (justThese)
    {
    struct slName *listFromJustThese = getWhiteListFromFile();
    whiteList = slCat(whiteList, listFromJustThese);
    }
if (justThese || justThis)
    cullCutters(&cutters, TRUE, whiteList, 0);
if (countsOnly)
    findCounts(cutters, seqs, outputFile);
else
    findBeds(cutters, seqs, outputFile);
cutterFreeList(&cutters);
freeDnaSeqList(&seqs);
slNameFreeList(&whiteList);
}

void consolidateTheCounts(char *inputFile, char *outputFile)
/* Read the cat'ed file in, and either make a new hash item for each enzyme */
/* encountered on each line, or add to an existing one. Then output the hash. */
{
struct lineFile *lf = lineFileOpen(inputFile, TRUE);
struct hash *countHash = newHash(12);
char *words[2];
while (lineFileRow(lf, words))
    {
    char *name = words[0];
    int count = lineFileNeedFullNum(lf, words, 1);
    struct hashEl *el = hashLookup(countHash, name);
    if (!el)
	hashAddInt(countHash, name, count);
    else
	el->val = intToPt(ptToInt(el->val) + count);
    }
writeHashToFile(countHash, outputFile);
freeHash(&countHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
justThis = optionVal("justThis", NULL);
justThese = optionVal("justThese", NULL);
countsOnly = optionExists("countsOnly");
consolidateCounts = optionExists("consolidateCounts");
if ((consolidateCounts) && (argc == 3))
    consolidateTheCounts(argv[1], argv[2]);
else if ((!consolidateCounts) && (argc == 4))
    findCutters(argv[1], argv[2], argv[3]);
else 
    usage();
return 0;
}
