/* stageMultiz - Stage input directory for Webb's multiple aligner. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "axt.h"
#include "portable.h"
#include "errAbort.h"
#include "dnautil.h"
#include "maf.h"


int winSize = 1010000;
int overlapSize = 10000;
boolean noDieMissing = FALSE;
boolean chromOut = FALSE;
char *hPrefix = "";
char *mPrefix = "";
char *rPrefix = "";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stageMultiz - Stage input directory for Webb's multiple aligner\n"
  "usage:\n"
  "   stageMultiz h.sizes m.sizes r.sizes hm.axt mrAxtDir outputDir\n"
  "where:\n"
  "   h.sizes - file with sizes of human sequences\n"
  "   m.sizes - file with sizes of mouse sequences\n"
  "   r.sizes - file with sizes of rat sequences\n"
  "   hm.axt - human/mouse alignments for one human chromosome\n"
  "   mrAxtDir - directory full of mouse/rat alignments with indexes\n"
  "              built by axtIndex\n"
  "   outputDir - this will be created, as will subdirectories, one\n"
  "               for each window on the human chromosome\n"
  "options:\n"
  "   -winSize=N Human bases in each directory.  Default %d\n"
  "   -overlapSize=N Amount to overlap each window. Default %d\n"
  "   -noDieMissing Don't die over missing mouse/rat alignments\n"
  "   -verbose=1 - If set be more verbose with diagnostic output\n"
  "   -hPrefix=hgN. - Prefix hgN. to each human sequence name in output\n"
  "   -mPrefix=mmN. - Prefix for mouse sequence names\n"
  "   -rPrefix=ratN. - Prefix for rat sequence\n"
  "   -chromOut - If set output is one per chromosome.\n"
  , winSize, overlapSize
  );
}

int maxChromSize = 500*1024*1024;	/* Largest size a binRange can handle */

void prefixAxt(struct axt *axt, char *qPrefix, char *tPrefix)
/* Add prefixes to axt->qName and ->tName */
{
char tName[256], qName[256];
snprintf(tName, sizeof(tName), "%s%s", tPrefix, axt->tName);
freeMem(axt->tName);
axt->tName = cloneString(tName);
snprintf(qName, sizeof(qName), "%s%s", qPrefix, axt->qName);
freeMem(axt->qName);
axt->qName = cloneString(qName);
}

struct binKeeper *loadAxtsIntoRange(char *fileName, char *tPrefix, char *qPrefix)
/* Read in an axt file and shove it into a bin-keeper. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct binKeeper *bk = binKeeperNew(0, maxChromSize);
struct axt *axt;
int count = 0;

while ((axt = axtRead(lf)) != NULL)
    {
    binKeeperAdd(bk, axt->tStart, axt->tEnd, axt);
    ++count;
    }
uglyf("LOaded %d from %s\n", count, fileName);
lineFileClose(&lf);
return bk;
}

boolean allDash(char *s)
/* Return TRUE if string is all '-'. */
{
char c;

while ((c = *s++) != 0)
    {
    if (c != '-')
        return FALSE;
    }
return TRUE;
}

boolean mafAllDash(struct mafAli *maf)
/* Return TRUE if maf has a row which is all dashes. */
{
struct mafComp *comp;

for (comp = maf->components; comp != NULL; comp = comp->next)
    {
    if (allDash(comp->text))
	return TRUE;
    }
return FALSE;
}

void mafWriteGood(FILE *f, struct mafAli *maf)
/* Write out maf if it's not all dash. */
{
if (!mafAllDash(maf))
    mafWrite(f, maf);
}

void outputAxtAsMaf(FILE *f, struct axt *axt, 
	struct hash *tSizeHash, char *tPrefix, struct hash *qSizeHash, char *qPrefix)
/* Write out an axt in maf format. */
{
struct mafAli temp;
char *oldQ = axt->qName;
char *oldT = axt->tName;
char tName[256], qName[256];
snprintf(tName, sizeof(tName), "%s%s", tPrefix, axt->tName);
axt->tName = tName;
snprintf(qName, sizeof(qName), "%s%s", qPrefix, axt->qName);
axt->qName = qName;
mafFromAxtTemp(axt, hashIntVal(tSizeHash, oldT),
	hashIntVal(qSizeHash, oldQ), &temp);
axt->qName = oldQ;
axt->tName = oldT;
mafWriteGood(f, &temp);
}

struct hash *loadSizeHash(char *fileName)
/* Load up file with lines of name<space>size into hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(0);

while (lineFileRow(lf, row))
    {
    hashAddInt(hash, row[0], lineFileNeedNum(lf, row, 1));
    }
return hash;
}

struct mouseChromCache
/* Hold cashe for one mouse chromosome. */
    {
    struct mouseChromCache *next;
    char *name;	/* Chromosome name. */
    int size;	/* Mouse chromosome size. */
    struct lineFile *lf; /* Open lineFile containing alignments */
    struct binKeeper *bk; /* Interval index containing positions in lf. */
    };

struct mouseChromCache *newMouseChromCache(char *chrom, int chromSize, 
	char *ratMouseDir)
/* Create a new chromCache. */
{
struct mouseChromCache *mcc;
char fileName[512];
struct lineFile *lf;
char *row[3];
int start,end;
long long *pPos;

/* Open up file with actual alignments.  Warn and return NULL
 * if it doesn't exist. */
sprintf(fileName, "%s/%s.axt", ratMouseDir, chrom);
lf = lineFileMayOpen(fileName, TRUE);

/* Allocate structure and store basic info in it. */
AllocVar(mcc);
mcc->name = cloneString(chrom);
mcc->size = chromSize;
mcc->lf = lf;
if (lf == NULL)
    {
    warn("%s doesn't exist", fileName);
    if (!noDieMissing)
        noWarnAbort(); 
    return mcc;
    }

/* Read index file into bk. */
sprintf(fileName, "%s/%s.axt.ix", ratMouseDir, chrom);
mcc->bk = binKeeperNew(0, chromSize);
lf = lineFileOpen(fileName, TRUE);
verbose(1, "Reading %s\n", fileName);
while (lineFileRow(lf, row))
    {
    start = lineFileNeedNum(lf, row, 0);
    end = lineFileNeedNum(lf, row, 1) + start;
    AllocVar(pPos);
    *pPos = atoll(row[2]);
    binKeeperAdd(mcc->bk, start, end, pPos);
    }
lineFileClose(&lf);

/* Return initialized object. */
return mcc;
}

void writeMousePartsAsMaf(FILE *f, struct hash *mouseHash, 
	char *ratMouseDir, char *mouseChrom,
	int mouseStart, int mouseEnd, int mouseChromSize, 
	struct hash *rSizeHash, struct hash *dupeHash)
/* Write out mouse/rat alignments that intersect given region of mouse.
 * This gets a little involved because we need to do random access on
 * the mouse/rat alignment files, which are too big to fit into memory.
 * On disk we have a mouse/rat alignment file for each mouse chromosome,
 * and an index of it.  When we first access a mouse chromosome we load
 * the index for that chromosome into memory, and open the alignment file.
 * We then do a seek and read to load a particular alignment. */
{
struct mouseChromCache *mcc = NULL;
struct binElement *list = NULL, *el;
char aliName[512];

/* Get cache for this mouse chromosome */
mcc = hashFindVal(mouseHash, mouseChrom);
if (mcc == NULL)
    {
    mcc = newMouseChromCache(mouseChrom, mouseChromSize, ratMouseDir);
    hashAdd(mouseHash, mouseChrom, mcc);
    }
if (mcc->lf == NULL)
    return;

/* Get list of positions and process one axt into a maf for each */
list = binKeeperFindSorted(mcc->bk, mouseStart, mouseEnd);
for (el = list; el != NULL; el = el->next)
    {
    struct axt *axt;
    struct mafAli temp;
    long long *pPos, pos;
    pPos = el->val;
    pos = *pPos;
    sprintf(aliName, "%s.%lld", mouseChrom, pos);
    if (!hashLookup(dupeHash, aliName))
	{
	int rChromSize;
	hashAdd(dupeHash, aliName, NULL);
	lineFileSeek(mcc->lf, pos, SEEK_SET);
	axt = axtRead(mcc->lf);
	rChromSize = hashIntVal(rSizeHash, axt->qName);
	prefixAxt(axt, rPrefix, mPrefix);
	mafFromAxtTemp(axt, mouseChromSize, rChromSize, &temp);
	mafWriteGood(f, &temp);
	axtFree(&axt);
	}
    }
slFreeList(&list);
}

void stageMultiz(char *hSizeFile, char *mSizeFile, char *rSizeFile,
	char *humanAxtFile, char *ratMouseDir, char *outputDir)
/* stageMultiz - Stage input directory for Webb's multiple aligner. */
{
struct binKeeper *humanBk = loadAxtsIntoRange(humanAxtFile, hPrefix, mPrefix);
struct hash *mouseHash = newHash(0);
struct hash *hSizeHash = loadSizeHash(hSizeFile);
struct hash *mSizeHash = loadSizeHash(mSizeFile);
struct hash *rSizeHash = loadSizeHash(rSizeFile);
struct hash *dupeHash = newHash(16);
int hStart;
char humanChromName[256];

makeDir(outputDir);
splitPath(humanAxtFile, NULL, humanChromName, NULL);
if (chromOut)
    winSize = hashIntVal(hSizeHash, humanChromName);

for (hStart = 0; hStart<maxChromSize - winSize; hStart += winSize - overlapSize)
    {
    int hEnd = hStart + winSize;
    struct binElement *humanList = binKeeperFindSorted(humanBk, hStart, hEnd);
    struct binElement *humanEl;
    char dirName[512], hmName[512], mrName[512];
    FILE *f;

    if (humanList != NULL)
	{
	/* Make output directory and create output file names. */
	if (chromOut)
	    sprintf(dirName, "%s", outputDir);
	else
	    sprintf(dirName, "%s/%s.%d", outputDir, humanChromName, hStart);
	verbose(1, "Making %s\n", dirName);
	makeDir(dirName);
	sprintf(hmName, "%s/12.maf", dirName);
	sprintf(mrName, "%s/23.maf", dirName);

	/* Write human/mouse file. */
	f = mustOpen(hmName, "w");
	mafWriteStart(f, "blastz");
	for (humanEl = humanList; humanEl != NULL; humanEl = humanEl->next)
	    {
	    struct axt *axt = humanEl->val;
	    outputAxtAsMaf(f, axt, hSizeHash, hPrefix, mSizeHash, mPrefix);
	    }
	mafWriteEnd(f);
	carefulClose(&f);

	/* Write mouse/rat file. */
	f = mustOpen(mrName, "w");
	mafWriteStart(f, "blastz");
	for (humanEl = humanList; humanEl != NULL; humanEl = humanEl->next)
	    {
	    struct axt *axt = humanEl->val;
	    int start = axt->qStart, end = axt->qEnd;
	    int mouseChromSize = hashIntVal(mSizeHash, axt->qName);
	    if (axt->qStrand == '-')
	        reverseIntRange(&start, &end, mouseChromSize);
	    writeMousePartsAsMaf(f, mouseHash, ratMouseDir, axt->qName, 
	    	start, end, mouseChromSize, rSizeHash, dupeHash);
	    }
	mafWriteEnd(f);
	carefulClose(&f);
	slFreeList(&humanList);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionHash(&argc, argv);
if (argc != 7)
    usage();
winSize = optionInt("winSize", winSize);
overlapSize = optionInt("overlapSize", overlapSize);
noDieMissing = optionExists("noDieMissing");
hPrefix = optionVal("hPrefix", hPrefix);
mPrefix = optionVal("mPrefix", mPrefix);
rPrefix = optionVal("rPrefix", rPrefix);
chromOut = optionExists("chromOut");

stageMultiz(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
