/* axtToChain - Convert axt to chain format. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "axt.h"
#include "chain.h"
#include "dnautil.h"


static boolean softMask = TRUE;

static struct optionSpec optionSpecs[] =  
/* option for pslToChain */
{
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToChain - Convert axt to chain format\n"
  "usage:\n"
  "   axtToChain in.axt tSizes qSizes out.chain\n"
  "Where tSizes and qSizes are tab-delimited files with\n"
  "       <seqName>\t<size>\n"
  "columns.\n"
/*
  "options:\n"
  "   -xxx=XXX\n"
*/
  );
}

struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
	hashAdd(hash, name, intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

/* BEGIN code copied from psl.c...
 * maybe it should live in common or dnautil?? */

static int countInitialChars(char *s, char c)
/* Count number of initial chars in s that match c. */
{
int count = 0;
char d;
while ((d = *s++) != 0)
    {
    if (c == d)
        ++count;
    else
        break;
    }
return count;
}

static int countNonInsert(char *s, int size)
/* Count number of characters in initial part of s that
 * are not '-'. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (*s++ != '-')
        ++count;
return count;
}

static int countTerminalChars(char *s, char c)
/* Count number of initial chars in s that match c. */
{
int len = strlen(s), i;
int count = 0;
for (i=len-1; i>=0; --i)
    {
    if (c == s[i])
        ++count;
    else
        break;
    }
return count;
}
/* END code copied from psl.c */

static void trimAlignment(struct axt *axt)
/* remove leading or trailing indels from alignment (axtBest sometimes 
 * outputs those) */
{
int qStartInsert = countInitialChars(axt->qSym, '-');
int tStartInsert = countInitialChars(axt->tSym, '-');
int qEndInsert = countTerminalChars(axt->qSym, '-');
int tEndInsert = countTerminalChars(axt->tSym, '-');
int startInsert = max(qStartInsert, tStartInsert);
int endInsert = max(qEndInsert, tEndInsert);
int qNonCount, tNonCount;

if (startInsert > 0)
    {
    qNonCount = countNonInsert(axt->qSym, startInsert);
    tNonCount = countNonInsert(axt->tSym, startInsert);
    axt->qSym += startInsert;
    axt->tSym += startInsert;
    axt->symCount -= startInsert;
    axt->qStart += qNonCount;
    axt->tStart += tNonCount;
    }
if (endInsert > 0)
    {
    axt->symCount -= endInsert;
    qNonCount = countNonInsert(axt->qSym+axt->symCount, endInsert);
    tNonCount = countNonInsert(axt->tSym+axt->symCount, endInsert);
    axt->qSym[axt->symCount] = 0;
    axt->tSym[axt->symCount] = 0;
    axt->qEnd -= qNonCount;
    axt->tEnd -= tNonCount;
    }
}

struct chain* chainFromAxt(struct axt *axt, int qSize, int tSize,
			   boolean softMask)
/* Create a chain from an AXT.  Returns NULL if alignment is empty after
 * triming leading and trailing indels.*/
{
static int id = 0;
struct chain* chain = NULL;

/* Trim; Don't create if either query or target is zero length */
trimAlignment(axt);
if ((axt->qStart == axt->qEnd) || (axt->tStart == axt->tEnd))
     {
     return NULL;
     }

/* initialize chain */
AllocVar(chain);
chain->qName = cloneString(axt->qName);
chain->qSize = qSize;
chain->qStart = axt->qStart;
chain->qEnd = axt->qEnd;
chain->qStrand = axt->qStrand;
chain->tName = cloneString(axt->tName);
chain->tSize = tSize;
chain->tStart = axt->tStart;
chain->tEnd = axt->tEnd;
chain->id = id++;
chain->score = axt->score;

/* Add blocks (and put in correct order). */
axtAddBlocksToBoxInList(&(chain->blockList), axt);
slReverse(&(chain->blockList));

return chain;
}


void axtToChain(char *inName, char *tSizeFile, char *qSizeFile, char *outName)
/* axtToChain - Convert axt to chain format. */
{
struct hash *tSizeHash = readSizes(tSizeFile);
struct hash *qSizeHash = readSizes(qSizeFile);
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt = NULL;

while ((axt = axtRead(lf)) != NULL)
    {
    struct chain *chain = chainFromAxt(axt,
				       findSize(qSizeHash, axt->qName),
				       findSize(tSizeHash, axt->tName),
				       softMask);
    if (chain != NULL)
	{
	chainWrite(chain, f);
	chainFree(&chain);
	}
    axtFree(&axt);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
axtToChain(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
