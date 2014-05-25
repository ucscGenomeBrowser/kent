/* kvsSummary - Summarize output of a bunch of knownVsBlats. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "blatStats.h"


#define maxRuns 256
#define maxChroms 256

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kvsSummary - Summarize output of a bunch of knownVsBlats\n"
  "usage:\n"
  "   kvsSummary outputFile inputFile(s)\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chrom
/* Keep track of one chromosome's data. */
    {
    struct chrom *next;
    char *name;
    struct hash *statHash;	/* Filled with stats. */
    };

struct run
/* Keep tabs on a single run. */
    {
    struct run *next;
    char *name;		/* Derived from file name. */
    struct hash *chromHash;	/* Filled with chrom's. */
    };

void parseRatio(char *ratio, int *retP, int *retQ, char *fileName, int lineIx)
/* Parse something that looks like p/q or (p/q) */
{
char *words[3];
int wordCount;
wordCount = chopString(ratio, "()/", words, ArraySize(words));
if (wordCount != 2)
    errAbort("Malformed ratio line %d of %s", lineIx, fileName);
*retP = atoi(words[0]);
*retQ = atoi(words[1]);
}

struct hash *readChromStats(struct lineFile *lf, struct hash *catHash, struct slName **pCatList)
/* Read from next '-------' to blank line into a hash of stats and return. */
{
char *line, *words[32];
char *s, *e;
int i, wordCount;
char *catName;
struct oneStat *stat;
struct hash *hash = newHash(5);

for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        errAbort("Couldn't find '--------' line by end of %s.", lf->fileName);
    if (startsWith("--------", line))
        break;
    }
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
	{
	warn("Missing final blank line in %s", lf->fileName);
        break;
	}
    if ((line = skipLeadingSpaces(line)) == NULL || line[0] == 0)
        break;

    /* Parse line initially by inserting a few extra %'s */
    for (s = line; s != NULL && s[0] != 0; s = e)
        {
	e = strchr(s, '%');
	if (e != NULL)
	    {
	    /* Assume percentage is at end of number.  Put
	     * percentage in front of number too. */
	    char *p = e-1, c;
	    for (;;)
	        {
		c = *p;
		if (c != '.' && !isdigit(c))
		    break;
		--p;
		}
	    *p = '%';
	    e += 1;
	    }
	}

    /* Now parse more fully. */
    wordCount = chopByChar(line, '%', words, ArraySize(words));
    if (wordCount != 5)
        errAbort("Malformed line %d of %s", lf->lineIx, lf->fileName);
    for (i=0; i<wordCount; ++i)
	words[i] = trimSpaces(words[i]);

    catName = words[0];
    if (!hashLookup(catHash, catName))
        {
	struct slName *cat = newSlName(catName);
	slAddHead(pCatList, cat);
	hashAdd(catHash, catName, cat);
	}
    AllocVar(stat);
    hashAdd(hash, catName, stat);
    parseRatio(words[2], &stat->basesPainted, &stat->basesTotal, lf->fileName, lf->lineIx);
    parseRatio(words[4], &stat->hits, &stat->features, lf->fileName, lf->lineIx);
    }
return hash;
}

struct hash *readRun(char *fileName, 
	struct hash *allChromHash, struct chrom **pAllChromList,
	struct hash *catHash, struct slName **pCatList)
/* Read a knownVsBlat file into a hash filled with chromosomes. */
{
struct hash *chromHash = newHash(6);
struct chrom *chrom;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *chromName;

while (lineFileNext(lf, &line, NULL))
    {
    char *e;
    if ((e = stringIn(" stats:", line)) != NULL)
        {
	*e = 0;
	chromName = trimSpaces(line);
	if (!hashLookup(allChromHash, chromName))
	    {
	    AllocVar(chrom);
	    slAddHead(pAllChromList, chrom);
	    hashAddSaveName(allChromHash, chromName, chrom, &chrom->name);
	    }
	AllocVar(chrom);
	hashAddSaveName(chromHash, chromName, chrom, &chrom->name);
	chrom->statHash = readChromStats(lf, catHash, pCatList);
	}
    }
lineFileClose(&lf);
return chromHash;
}

void kvsSummary(char *outName, int inCount, char *inNames[])
/* kvsSummary - Summarize output of a bunch of knownVsBlats. */
{
int i;
struct hash *catHash = newHash(8);	/* Catagory hash */
struct slName *catList = NULL, *cat;	/* Catagory list. */
struct hash *allChromHash = newHash(0);	/* Chromosome hash. */
struct chrom *chromList = NULL, *chrom;	/* Chromosome list. */
struct hash *runHash = newHash(0);	/* Which run this is in. */
struct run *runList = NULL, *run;		/* Info about a run. */
char runNameBuf[64], *runName;
FILE *f = mustOpen(outName, "w");

/* Read input files into a bunch of hashes. */
for (i=0; i<inCount; ++i)
    {
    char *inName = inNames[i];
    /* Figure out name of run from fileName - prefer suffix. */
    runName = runNameBuf;
    runName[0] = 0;
    splitPath(inName, NULL, NULL, runNameBuf);
    if (runNameBuf[0] == 0)
	splitPath(inName, NULL, runNameBuf, NULL);
    else
	runName += 1;
    AllocVar(run);
    slAddHead(&runList, run);
    hashAddSaveName(runHash, runName, run, &run->name);
    run->chromHash = readRun(inName, allChromHash, &chromList, catHash, &catList);
    }
slReverse(&chromList);
slReverse(&runList);
slReverse(&catList);

/* Write output. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    /* Print header. */
    fprintf(f, ">>>>==== %s totals ====<<<<\n", chrom->name);
    fprintf(f, "%-15s", "region");
    for (run = runList; run != NULL; run = run->next)
	fprintf(f, " %10s", run->name);
    fprintf(f, "\n");
    fprintf(f, "---------------");
    for (run = runList; run != NULL; run = run->next)
	fprintf(f, "-----------");
    fprintf(f, "\n");

    /* Print fields. */
    for (cat = catList; cat != NULL; cat = cat->next)
	{
	char *catName = cat->name;
	struct oneStat *stat;
	fprintf(f, "%-15s", catName);

	/* Print all runs on this field. */
	for (run = runList; run != NULL; run = run->next)
	    {
	    struct chrom *c;
	    boolean printed = FALSE;
	    if ((c = hashFindVal(run->chromHash, chrom->name)) != NULL)
	        {
		if ((stat = hashFindVal(c->statHash, cat->name)) != NULL)
		    {
		    char buf[16];
		    sprintf(buf, "%4.1f%%", divAsPercent(stat->basesPainted, stat->basesTotal));
		    fprintf(f, " %10s", buf);
		    printed = TRUE;
		    }
		}
	    if (!printed)
	        fprintf(f, " %10s", "n/a");
	    }
	fprintf(f, "\n");
	}
    fprintf(f, "\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 3)
    usage();
kvsSummary(argv[1], argc-2, argv+2);
return 0;
}
