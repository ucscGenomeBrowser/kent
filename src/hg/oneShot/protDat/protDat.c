/* findExons - This program reads in a list of psls. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "protDat - this program reads in a protein psl as argv[1] and modifies qName to include\n"
  "          the position of the protein (using argv[2]) and various alias as defined by argv[3]\n"
  "usage:\n"
  "   protDat protein.psl proteinBlat.psl aliases outFile\n"
  "options:\n"
  "   -fb use FlyBase defaults\n"
  "   -kg use KnownGene defaults\n"
  );
}

static struct optionSpec options[] = {
   {"fb", OPTION_BOOLEAN},
   {"kg", OPTION_BOOLEAN},
   {NULL, 0},
};


struct alias
{
    char *kgName;
    char *spName;
};

void protDat(char *protName, char *blatName, char *aliasFile, char *outName)
{
FILE *outFile = mustOpen(outName, "w");
struct hash *protHash = newHash(10);
struct hash *blatHash = newHash(10);
struct hash *aliasHash = newHash(10);
struct psl *psls, *pslPtr, *protPsls, *blatPsl;
struct lineFile *lf = lineFileOpen(aliasFile, TRUE);
struct alias *alPtr;
char buffer[1024];
char *words[3];
int numWords = optionExists("fb") ? 2 : 3;

while (lineFileNextRow(lf, words, numWords))
    {
    AllocVar(alPtr);
    alPtr->kgName = cloneString(words[1]);
    if (numWords == 3)
	alPtr->spName = cloneString(words[2]);
    hashAdd(aliasHash, cloneString(words[0]), alPtr);
    }

protPsls = pslLoadAll(protName);

pslPtr = psls = pslLoadAll(blatName);
for(; pslPtr; pslPtr = pslPtr->next)
    hashAdd(blatHash, pslPtr->qName, pslPtr);

for(pslPtr = protPsls; pslPtr; pslPtr = pslPtr->next)
    {
    if ((blatPsl = hashFindVal(blatHash, pslPtr->qName)) != NULL)
	{
	if ((alPtr = hashFindVal(aliasHash, pslPtr->qName)) != NULL)
	    {
	    if (numWords == 3)
		sprintf(buffer,"%s.%s:%d-%d.%s.%s",pslPtr->qName,blatPsl->tName, 
		    blatPsl->tStart, blatPsl->tEnd,alPtr->kgName, alPtr->spName); 
	    else
		sprintf(buffer,"%s.%s:%d-%d.%s",pslPtr->qName,blatPsl->tName, 
		    blatPsl->tStart, blatPsl->tEnd,alPtr->kgName); 
	    pslPtr->qName = buffer;
	    pslTabOut(pslPtr, outFile);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 5)
    usage();
protDat(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
