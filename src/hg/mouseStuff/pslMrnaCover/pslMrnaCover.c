/* pslMrnaCover - Make histogram of coverage percentage of mRNA in psl. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslMrnaCover - Make histogram of coverage percentage of mRNA in psl.\n"
  "usage:\n"
  "   pslMrnaCover mrna.psl mrna.fa\n"
  "options:\n"
  "   -minSize=N  - default 100.  Minimum size of mRNA considered\n"
  "   -listZero=zero.tab - List accessions that don't align in zero.tab\n"
  );
}

int minSize = 100;
char *listZero = NULL;

struct rnaCover
/* Keep track of coverage of one mrna. */
    {
    struct rnaCover *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    int qSize;			/* Size mrna. */
    int qMaxAli;		/* Size of biggest alignment */ 
    };

void readFa(char *fileName, struct rnaCover **retList, struct hash **retHash)
/* Read in an FA file and store name and size of every record in hash/list. */
{
struct rnaCover *list = NULL, *rc;
struct hash *hash = newHash(18);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
DNA *dna;
int size;
char *name;

while (faSpeedReadNext(lf, &dna, &size, &name))
    {
    if (size >= minSize)
	{
	AllocVar(rc);
	slAddHead(&list, rc);
	if (hashLookup(hash, name))
	    {
	    warn("Duplicate %s line %d of %s, skipping", name, lf->lineIx, lf->fileName);
	    continue;
	    }
	hashAddSaveName(hash, name, rc, &rc->name);
	rc->qSize = size;
	}
    }
slReverse(&list);
*retList = list;
*retHash = hash;
}

void pslMrnaCover(char *pslFile, char *faFile)
/* pslMrnaCover - Make histogram of coverage percentage of mRNA in psl. */
{
static int histogram[101];
int i;
int qAli;
struct hash *hash;
struct rnaCover *rcList = NULL, *rc;
struct lineFile *lf = pslFileOpen(pslFile);
struct psl *psl;

/* Build up list of all sequences. */
readFa(faFile, &rcList, &hash);

/* Scan psls and see maximum amount each is aligned. */
while ((psl = pslNext(lf)) != NULL)
    {
    if (psl->qSize >= minSize)
	{
	if ((rc = hashFindVal(hash, psl->qName)) == NULL)
	    errAbort("%s is in %s but not %s", psl->qName, pslFile, faFile);
	if (rc->qSize != psl->qSize)
	    errAbort("%s is %d bytes in %s but %d in %s", psl->qName,
		rc->qSize, faFile, psl->qSize, pslFile);
	qAli = psl->match + psl->repMatch + psl->misMatch;
	if (qAli > rc->qMaxAli)
	   rc->qMaxAli = qAli;
	}
    pslFree(&psl);
    }
lineFileClose(&lf);

/* Open file to keep track of non-aligners */
if (listZero != NULL)
    {
    FILE *f = mustOpen(listZero, "w");
    for (rc = rcList; rc != NULL; rc = rc->next)
	{
	if (rc->qMaxAli == 0)
	    fprintf(f, "%s\t%d\n", rc->name, rc->qSize);
	}
    }

/* Talley up percentage aligning in histogram. */
for (rc = rcList; rc != NULL; rc = rc->next)
    {
    int histIx = roundingScale(100, rc->qMaxAli, rc->qSize);
    assert(histIx <= 100);
    histogram[histIx] += 1;
    }

/* Print out histogram. */
for (i=0; i<=100; ++i)
    {
    printf("%3d%% %6d\n", i, histogram[i]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minSize = optionInt("minSize", 100);
listZero = optionVal("listZero", listZero);
if (argc != 3)
    usage();
pslMrnaCover(argv[1], argv[2]);
return 0;
}
