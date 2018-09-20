/* axtToMaf - Convert from axt to maf format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "axt.h"
#include "maf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToMaf - Convert from axt to maf format\n"
  "usage:\n"
  "   axtToMaf in.axt tSizes qSizes out.maf\n"
  "Where tSizes and qSizes is a file that contains\n"
  "the sizes of the target and query sequences.\n"
  "Very often this with be a chrom.sizes file\n"
  "Options:\n"
  "    -qPrefix=XX. - add XX. to start of query sequence name in maf\n"
  "    -tPrefex=YY. - add YY. to start of target sequence name in maf\n"
  "    -tSplit Create a separate maf file for each target sequence.\n"
  "            In this case output is a dir rather than a file\n"
  "            In this case in.maf must be sorted by target.\n"
  "    -score       - recalculate score \n"
  "    -scoreZero   - recalculate score if zero \n"
  );
}

static struct optionSpec options[] = {
   {"qPrefix", OPTION_STRING},
   {"tPrefix", OPTION_STRING},
   {"tSplit", OPTION_BOOLEAN},
   {"score", OPTION_BOOLEAN},
   {"scoreZero", OPTION_BOOLEAN},
   {NULL, 0},
};


char *qPrefix = "";
char *tPrefix = "";
boolean tSplit = FALSE;
boolean score = FALSE;
boolean scoreZero = FALSE;

struct hash *loadIntHash(char *fileName)
/* Read in a file full of name/number lines into a hash keyed
 * by name with number values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = newHash(0);

while (lineFileRow(lf, row))
    {
    int num = lineFileNeedNum(lf, row, 1);
    hashAdd(hash, row[0], intToPt(num));
    }
return hash;
}

int findInt(struct hash *hash, char *name)
/* Return integer value corresponding to name. */
{
return ptToInt(hashMustFindVal(hash, name));
}

void axtToMaf(char *in, char *tSizes, char *qSizes, char *out)
/* axtToMaf - Convert from axt to maf format. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = NULL;
char *tName = cloneString("");
struct hash *tSizeHash = loadIntHash(tSizes);
struct hash *qSizeHash = loadIntHash(qSizes);
struct axt *axt;
struct hash *uniqHash = newHash(18);

if (tSplit)
    {
    makeDir(out);
    }
else
    {
    f = mustOpen(out, "w");
    lineFileSetMetaDataOutput(lf, f);
    mafWriteStart(f, "blastz");
    }
while ((axt = axtRead(lf)) != NULL)
    {
    struct mafAli *ali;
    struct mafComp *comp;
    char srcName[256];
    AllocVar(ali);
    ali->score = axt->score;
    if ((ali->score == 0 && scoreZero) || score)
        ali->score = axtScoreDnaDefault(axt);
    AllocVar(comp);
    safef(srcName, sizeof(srcName), "%s%s", qPrefix, axt->qName);
    comp->src = cloneString(srcName);
    comp->srcSize = findInt(qSizeHash, axt->qName);
    comp->strand = axt->qStrand;
    comp->start = axt->qStart;
    comp->size = axt->qEnd - axt->qStart;
    comp->text = axt->qSym;
    axt->qSym = NULL;
    slAddHead(&ali->components, comp);
    AllocVar(comp);
    safef(srcName, sizeof(srcName), "%s%s", tPrefix, axt->tName);
    comp->src = cloneString(srcName);
    comp->srcSize = findInt(tSizeHash, axt->tName);
    comp->strand = axt->tStrand;
    comp->start = axt->tStart;
    comp->size = axt->tEnd - axt->tStart;
    comp->text = axt->tSym;
    axt->tSym = NULL;
    slAddHead(&ali->components, comp);
    if (tSplit && !sameString(axt->tName, tName))
        {
	char path[PATH_LEN];
	if (f != NULL)
	    mafWriteEnd(f);
	freez(&tName);
	tName = cloneString(axt->tName);
	if (hashLookup(uniqHash, tName) != NULL)
	    errAbort("%s isn't sorted, which is necessary when -tSplit option is used",
	        lf->fileName);
	hashAdd(uniqHash, tName, NULL);
	safef(path, sizeof(path), "%s/%s.maf", out, tName);
	carefulClose(&f);
	f = mustOpen(path, "w");
	mafWriteStart(f, "blastz");
	}
    mafWrite(f, ali);
    mafAliFree(&ali);
    axtFree(&axt);
    }
mafWriteEnd(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
qPrefix = optionVal("qPrefix", qPrefix);
tPrefix = optionVal("tPrefix", tPrefix);
tSplit = optionExists("tSplit");
score = optionExists("score");
scoreZero = optionExists("scoreZero");
if (argc != 5)
    usage();
axtToMaf(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
