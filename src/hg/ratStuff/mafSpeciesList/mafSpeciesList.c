/* mafSpeciesList - Scan maf and output all species used in it.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"


boolean ignoreFirst = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafSpeciesList - Scan maf and output all species used in it.\n"
  "usage:\n"
  "   mafSpeciesList in.maf out.lst\n"
  "options:\n"
  "   -ignoreFirst - If true ignore first species in each maf, useful when this\n"
  "                  is a mafFrags result that puts gene id there.\n"
  );
}

static struct optionSpec options[] = {
   {"ignoreFirst", OPTION_BOOLEAN},
   {NULL, 0},
};

void mafSpeciesList(char *inFile, char *outFile)
/* mafSpeciesList - Scan maf and output all species used in it.. */
{
/* Scan through maf file saving species in hash. */
struct mafFile *mf = mafOpen(inFile);
struct mafAli *maf;
struct hash *speciesHash = hashNew(0);
char *dot;
while ((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *comp = maf->components;
    if (ignoreFirst)
        comp = comp->next;
    for (; comp != NULL; comp = comp->next)
        {
        dot = strchr(comp->src, '.'); /* The species name is found before the first dot */
        if (dot != NULL)
            *dot = 0;
        hashStore(speciesHash, comp->src);
        }
    mafAliFree(&maf);
    }

/* Get all species, sort, and output. */
struct hashEl *el, *list = hashElListHash(speciesHash);
slSort(&list, hashElCmp);
FILE *f = mustOpen(outFile, "w");
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\n", el->name);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
ignoreFirst = optionExists("ignoreFirst");
mafSpeciesList(argv[1], argv[2]);
return 0;
}
