/* mafMeFirst - Move component to top if it is one of the named ones.  Useful 
 * in conjunction with mafFrags when you don't want the one with the gene name 
 * to be in the middle.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafMeFirst - Move component to top if it is one of the named ones.  \n"
  "Useful in conjunction with mafFrags when you don't want the one with \n"
  "the gene name to be in the middle.\n"
  "usage:\n"
  "   mafMeFirst in.maf me.list out.maf\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct mafComp *compInHash(struct mafAli *maf, struct hash *hash)
/* Return first component that is in hash. */
{
struct mafComp *comp;
for (comp = maf->components; comp != NULL; comp = comp->next)
   if (hashLookup(hash, comp->src))
       break;
return comp;
}

void mafMeFirst(char *inMaf, char *meFile, char *outMaf)
/* mafMeFirst - Move component to top if it is one of the named ones.  Useful 
 * in conjunction with mafFrags when you don't want the one with the gene name 
 * to be in the middle.. */
{
struct hash *meHash = hashWordsInFile(meFile, 18);
struct mafFile *mf = mafOpen(inMaf);
FILE *f = mustOpen(outMaf, "w");
mafWriteStart(f, mf->scoring);
struct mafAli *maf;
while ((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *comp = compInHash(maf, meHash);
    if (comp == NULL)
        errAbort("No components in %s in maf ending line %d of %s",
		meFile, mf->lf->lineIx, mf->lf->fileName);
    slRemoveEl(&maf->components, comp);
    slAddHead(&maf->components, comp);
    mafWrite(f, maf);
    mafAliFree(&maf);
    }

mafWriteEnd(f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mafMeFirst(argv[1], argv[2], argv[3]);
return 0;
}
