/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

int maxMap = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslMaxMap - filter out psls that map more than N times\n"
  "usage:\n"
  "   pslMaxMap inPsl outPsl\n"
  "options:\n"
  "   -maxMap=10 (default)\n"
  );
}

static struct optionSpec options[] = {
   {"maxMap", OPTION_INT},
   {NULL, 0},
};

struct name
{
    int count;
};

void pslMaxMap(char *pslIn, char *pslOut)
/* pslMaxMap - make clusters out of a psl file. */
{
FILE *out = mustOpen(pslOut, "w");
struct psl *pslList = pslLoadAll(pslIn);
struct hash *nameHash = newHash(0);  
struct psl *psl;
struct name *name;

for(psl = pslList; psl ; psl = psl->next)
    {
    name = hashFindVal(nameHash, psl->qName);
    if (name == NULL)
	{
	AllocVar(name);
	hashAdd(nameHash, psl->qName, name);
	}
    name->count++;
    }
for(psl = pslList; psl ; psl = psl->next)
    {
    name = hashFindVal(nameHash, psl->qName);

    if (name->count <= maxMap)
	pslTabOut(psl, out);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxMap = optionInt("maxMap", maxMap);
if (argc != 3)
    usage();
pslMaxMap(argv[1], argv[2]);
return 0;
}
