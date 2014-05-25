/* mafSpeciesSubset - Extract a maf that just has a subset of species.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"


boolean keepFirst = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafSpeciesSubset - Extract a maf that just has a subset of species.\n"
  "usage:\n"
  "   mafSpeciesSubset in.maf species.lst out.maf\n"
  "Where:\n"
  "    in.maf is a file where the sequence source are either simple species\n"
  "           names, or species.something.  Usually actually it's a genome\n"
  "           database name rather than a species before the dot to tell the\n"
  "           truth.\n"
  "    species.lst is a file with a list of species to keep\n"
  "    out.maf is the output.  It will have columns that are all - or . in\n"
  "           the reduced species set removed, as well as the lines representing\n"
  "           species not in species.lst removed.\n"
  "options:\n"
  "   -keepFirst - If set, keep the first 'a' line in a maf no matter what\n"
  "                Useful for mafFrag results where we use this for the gene name\n"
  );
}

static struct optionSpec options[] = {
   {"keepFirst", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean prefixInHash(struct hash *hash, char *name)
/* Return true  if name, or name up to first dot is in hash. */
{
char *dot = strchr(name, '.');
if (dot == NULL)
    return hashLookup(hash, name) != NULL;
else
    {
    *dot = 0;
    boolean inHash = (hashLookup(hash, name) != NULL);
    *dot = '.';
    return inHash;
    }
}


void mafSpeciesSubset(char *inMafFile, char *orgFile, char *outMafFile)
/* mafSpeciesSubset - Extract a maf that just has a subset of species.. */
{
struct hash *orgHash = hashWordsInFile(orgFile, 0);
struct mafFile *mf = mafOpen(inMafFile);
FILE *f = mustOpen(outMafFile, "w");
struct mafAli *inMaf;
mafWriteStart(f, mf->scoring);

while ((inMaf = mafNext(mf)) != NULL)
    {
    /* Allocate output maf and fill in simple fields. */
    struct mafAli *outMaf;
    AllocVar(outMaf);
    outMaf->score =inMaf->score;
    outMaf->textSize = inMaf->textSize;

    /* Flip components we're interested in from inComp to outComp. */
    struct mafComp *comp, *nextComp, *remainder = NULL;
    for (comp = inMaf->components; comp != NULL; comp = nextComp)
        {
	nextComp = comp->next;
	if ((keepFirst && comp == inMaf->components) || prefixInHash(orgHash, comp->src))
	    {
	    slAddHead(&outMaf->components, comp);
	    }
	else
	    {
	    slAddHead(&remainder, comp);
	    }
	}
    slReverse(&outMaf->components);
    inMaf->components = remainder;

    /* Strip columns and write result if anything left. */
    mafStripEmptyColumns(outMaf);
    if (outMaf->textSize > 0)
	mafWrite(f, outMaf);

    /* Clean up. */
    mafAliFree(&outMaf);
    mafAliFree(&inMaf);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
keepFirst = optionExists("keepFirst");
mafSpeciesSubset(argv[1], argv[2], argv[3]);
return 0;
}
