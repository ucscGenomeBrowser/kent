/* mafStats - Calculate basic stats on maf file including species-by-species coverage and percent ID.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"
#include "maf.h"


boolean meFirst = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafStats - Calculate basic stats on maf file including species-by-species coverage and percent ID.\n"
  "usage:\n"
  "   mafStats ref.2bit mafDir output\n"
  "options:\n"
  "   -meFirst - Treat first component as reference species regardless of name.\n"
  "                Useful for mafFrag results where we use this for the gene name\n"
  );
}

static struct optionSpec options[] = {
   {"meFirst", OPTION_BOOLEAN},
   {NULL, 0},
};

struct speciesAcc
/* Accumulate counts on one species. */
    {
    struct speciesAcc *next;	/* Next in list. */
    char *name;	/* Name - not allocated here. */
    long covCount;	/* Bases covered in reference, including dashes in other species. */
    long aliCount;	/* Bases covered in ref, not including dashes in other species. */
    long idCount;	/* Bases identical in ref or and other species. */
    };

void addComp(struct mafComp *ref, struct mafComp *other, char *name, int textSize,
	struct hash *speciesHash, struct speciesAcc **pList)
/* Add counts from a single component. */
{
char *refText = ref->text;
char *otherText = other->text;
if (otherText == NULL)
    return;
struct speciesAcc *acc = hashFindVal(speciesHash, name);
if (acc == NULL)
    {
    AllocVar(acc);
    hashAddSaveName(speciesHash, name, acc, &acc->name);
    slAddHead(pList, acc);
    }
int i;
for (i=0; i<textSize; ++i)
    {
    char refBase = refText[i];
    char otherBase = otherText[i];
    if (refBase != '-' && refBase != '.' && refBase != 'N')
        {
	if (otherBase != '.' && otherBase != 'N')
	    {
	    acc->covCount += 1;
	    if (otherBase != '-')
	        {
		acc->aliCount += 1;
		if (otherBase == refBase)
		    acc->idCount += 1;
		}
	    }
	}
    }
}


char *cloneUpToDot(char *s)
/* Return copy of string up to dot. */
{
char *x = cloneString(s);
char *e = strchr(x, '.');
if (e != NULL)
    *e = 0;
return x;
}

void addCounts(struct mafAli *maf, struct hash *speciesHash, struct speciesAcc **pList)
/* Add counts from a single maf. */
{
struct mafComp *ref = maf->components;
struct mafComp *mc;
char *refName = NULL;
if (meFirst)
    refName = cloneString("ref");
else
    refName = cloneUpToDot(ref->src);
addComp(ref, ref, refName, maf->textSize, speciesHash, pList);
for (mc = ref->next; mc != NULL; mc = mc->next)
     {
     char *name = cloneUpToDot(mc->src);
     addComp(ref, mc, name, maf->textSize, speciesHash, pList);
     freeMem(name);
     }
freeMem(refName);
}

void mafStats(char *twoBitFile, char *mafDir, char *outFile)
/* mafStats - Calculate basic stats on maf file including species-by-species 
 * coverage and percent ID. */
{
struct twoBitFile *tbf = twoBitOpen(twoBitFile);
FILE *f = mustOpen(outFile, "w");
struct twoBitIndex *ix;
long genomeSize = 0;
struct hash *speciesHash = hashNew(0);
struct speciesAcc *speciesList = NULL, *species;
for (ix = tbf->indexList; ix != NULL; ix = ix->next)
    {
    unsigned chromSize = twoBitSeqSizeNoNs(tbf, ix->name);
    genomeSize += chromSize;
    char mafFileName[PATH_LEN];
    safef(mafFileName, sizeof(mafFileName), "%s/%s.maf", mafDir, ix->name);
    struct mafFile *mf = mafMayOpen(mafFileName);
    verbose(1, "processing %s\n", ix->name);
    if (mf == NULL)
        {
	warn("%s doesn't exist", mafFileName);
	continue;
	}
    struct mafAli *maf;
    while ((maf = mafNext(mf)) != NULL)
        {
	struct mafComp *mc;
	for (mc = maf->components; mc != NULL; mc = mc->next)
	    {
	    if (mc->text != NULL)
		toUpperN(mc->text, maf->textSize);
	    }
	addCounts(maf, speciesHash, &speciesList);
	mafAliFree(&maf);
	}
    mafFileFree(&mf);
    }
slReverse(&speciesList);

for (species = speciesList; species != NULL; species = species->next)
    {
    fprintf(f, "counts: %s\t%ld\t%ld\t%ld\n", species->name, species->covCount, species->aliCount, species->idCount);
    fprintf(f, "precents: %s\t%4.2f%%\t%4.2f%%\t%4.2f%%\n", 
    	species->name, 100.0 * species->covCount/genomeSize,
	100.0 * species->aliCount/genomeSize,
	100.0 * species->idCount/species->aliCount);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
meFirst = optionExists("meFirst");
mafStats(argv[1], argv[2], argv[3]);
return 0;
}
