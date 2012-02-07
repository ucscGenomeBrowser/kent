/* ooChains - make chains (partially ordered clone fragments) for oo dir. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"
#include "obscure.h"
#include "hCommon.h"


char *finfFiles[] = {"predraft.finf", "draft.finf", "finished.finf", "extras.finf"};
char *infoFile = "sequence.inf";

struct clone
/* Clone sequence info structure.  */
    {
    struct clone *next;		/* Next in list. */
    char *name;			/* Clone name - stored in hash.  No version # */
    int size;     		/* Number of bases. */
    int phase;                  /* HTG phase 0-3. */
    struct finf *fragList;	/* List of fragments. */
    struct chain *chainList;    /* List of chains. */
    };

struct finf
/* Fragment info structure.  Stores a
 * A parsed line from a Greg Schuler .finf file.
 * Contains clone name, fragment name, etc. */
   {
   struct finf *next;	/* Next in list. */
   char *name;		/* Fragment name. */
   int start, end;      /* Position in clone as submitted. */
   short chainId;       /* Zero if not part of chain. */
   short linkIx;        /* Number of link in chain. */
   short linkCount;     /* Total number of links. */
   char endInfo[9];     /* Sp6,R  T7,L   etc. */
   };

struct link
/* A link in a chain of fragments. */
    {
    struct link *next;	/* Next in list. */
    int ix;		/* Index of link in list. */
    struct finf *finf;  /* Reference to a fragment. */
    };

struct chain
/* A chain of fragments. */
    {
    struct chain *next;    /* Next in list. */
    int id;                /* Id of chain. */
    struct link *linkList; /* List of links. */
    };

int linkCmpIx(const void *va, const void *vb)
/* Compare two links to sort by link index. */
{
const struct link *a = *((struct link **)va);
const struct link *b = *((struct link **)vb);
return a->finf->linkIx - b->finf->linkIx;
}

struct chain *findChain(struct chain *chainList, int id)
/* Find chain with given id in list. */
{
struct chain *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
   if (chain->id == id)
       return chain;
return NULL;
}


struct finf *finfReadNext(struct lineFile *lf)
/* Read in next finf from file, or NULL at EOF. */
{
char ucscName[32];
char *parts[4], *words[16];
int partCount, wordCount;
struct finf *finf;

wordCount = lineFileChop(lf, words);
if (wordCount <= 0)
    return NULL;
lineFileExpectWords(lf, 7, wordCount);
AllocVar(finf);
gsToUcsc(words[0], ucscName);
finf->name = cloneString(ucscName);
finf->start = atoi(words[2])-1;
finf->end = atoi(words[3]);
if (words[5][0] != '?')
    {
    partCount = chopString(words[5], ",/", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Misformed field 6 line %d of %s\n", lf->lineIx, lf->fileName);
    finf->chainId = atoi(parts[0]);
    finf->linkIx = atoi(parts[1]);
    finf->linkCount = atoi(parts[2]);
    }
strncpy(finf->endInfo, words[6], sizeof(finf->endInfo));
return finf;
}


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooChains - make chains (partially ordered clone fragments) for oo dir\n"
  "usage:\n"
  "   ooChains ffaDir ooDir\n");
}

struct clone *readCloneList(char *fileName, struct hash *cloneHash)
/* Read clone list from sequence.inf file and save it in list/hash. */
{
struct clone *cloneList = NULL, *clone;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[8];
struct hashEl *hel;

while (lineFileRow(lf, words))
    {
    AllocVar(clone);
    chopSuffix(words[0]);
    hel = hashAddUnique(cloneHash, words[0], clone);
    clone->name = hel->name;
    clone->size = lineFileNeedNum(lf, words, 2);
    clone->phase = lineFileNeedNum(lf, words, 3);
    slAddHead(&cloneList, clone);
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}

void readFinf(char *fileName, struct hash *cloneHash)
/* Read fragment info file and add frags to appropriate clone. */
{
struct clone *clone;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char nameBuf[32];
struct finf *finf;

while ((finf = finfReadNext(lf)) != NULL)
    {
    strncpy(nameBuf, finf->name, sizeof(nameBuf));
    chopSuffix(nameBuf);
    clone = hashMustFindVal(cloneHash, nameBuf);
    slAddHead(&clone->fragList, finf);
    }
lineFileClose(&lf);
}

int clonesWithChainsCount = 0;

void makeChains(struct clone *clone)
/* Make chains out of clone fragments when possible. */
{
struct chain *chain = NULL;
struct link *link;
struct finf *finf;

for (finf = clone->fragList; finf != NULL; finf = finf->next)
    {
    int chainId = finf->chainId;
    if (chainId != 0)
        {
	if (chain == NULL || chain->id != chainId)
	    {
	    if ((chain = findChain(clone->chainList, chainId)) == NULL)
	        {
		AllocVar(chain);
		chain->id = chainId;
		slAddHead(&clone->chainList, chain);
		}
	    }
	AllocVar(link);
	link->ix = finf->linkIx;
	link->finf = finf;
	slAddHead(&chain->linkList, link);
	}
    }
if (clone->chainList != NULL)
    {
    clonesWithChainsCount++;
    slReverse(&clone->chainList);
    for (chain = clone->chainList; chain != NULL; chain = chain->next)
        {
	slSort(&chain->linkList, linkCmpIx);
	}
    }
}

int linkCount = 0;

void writeLink(struct finf *af, struct finf *bf, 
	int maxGap, int score, FILE *f)
/* Write link between af and bf. */
{
int midDistance = (af->end - af->start + bf->end - bf->start)/2;
/* write a,b,minDistance,maxDistance,score */
fprintf(f, "%s\t+\t%s\t%d\t%d\t%d\n",
    af->name, bf->name, midDistance-100, midDistance+maxGap, score);
++linkCount;
}

void writeChains(struct clone *clone, FILE *f)
/* Write info about chains in clone if any. */
{
struct chain *chain;
struct link *al, *bl;
struct finf *af, *bf;

if (clone->phase == 2)
    {
    if ((bf = clone->fragList) != NULL)
	{
	for (;;)
	    {
	    af = bf;
	    if ((bf = bf->next) == NULL)
		break;
	    writeLink(af, bf, 1000, 20, f);
	    }
	}
    }
else
    {
    for (chain = clone->chainList; chain != NULL; chain = chain->next)
        {
	if ((bl = chain->linkList) != NULL)
	    {
	    for (;;)
	        {
		al = bl;
		if ((bl = bl->next) == NULL)
		    break;
		writeLink(al->finf, bl->finf, 3000, 0, f);
		}
	    }
	}
    }
}

void writeRelevantChains(char *ctgDir, struct hash *cloneHash)
/* Read in geno.lst and write chains on relevant clones to
 * fragChains. */
{
char inName[512];
char outName[512];
char *wordBuf, **faNames;
int faCount;
int i;
char dir[256], cloneName[128], ext[64];
FILE *f;
struct clone *clone;

sprintf(inName, "%s/geno.lst", ctgDir);
sprintf(outName, "%s/fragChains", ctgDir);
readAllWords(inName, &faNames, &faCount, &wordBuf);
f = mustOpen(outName, "w");
for (i=0; i<faCount; ++i)
   {
   splitPath(faNames[i], dir, cloneName, ext);
   if (!startsWith("NT_", cloneName))
       {
       clone = hashMustFindVal(cloneHash, cloneName);
       writeChains(clone, f);
       }
   }
freeMem(wordBuf);
fclose(f);
}

void ooChains(char *ffaDir, char *ooDir)
/* ooChains - make chains (partially ordered clone fragments) for oo dir. */
{
struct hash *cloneHash = newHash(16);
struct clone *cloneList, *clone;
char fileName[512];
int i;
struct fileInfo *chromDir = NULL, *ctgDir = NULL, *chrom, *ctg;

/* Read in input from ffaDir. */
sprintf(fileName, "%s/%s", ffaDir, infoFile);
printf("Reading %s\n", fileName);
cloneList = readCloneList(fileName, cloneHash);
for (i=0; i<ArraySize(finfFiles); ++i)
    {
    sprintf(fileName, "%s/%s", ffaDir, finfFiles[i]);
    printf("Reading %s\n", fileName);
    readFinf(fileName, cloneHash);
    }
printf("Making chains\n");
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    slReverse(&clone->fragList);
    makeChains(clone);
    }

/* Make output in each of contig dirs. */
chromDir = listDirX(ooDir, "*", FALSE);
for (chrom = chromDir; chrom != NULL; chrom = chrom->next)
    {
    char *chromName = chrom->name;
    if (chrom->isDir && strlen(chromName) <= 2 && chromName[0] != '.')
        {
	printf("Processing %s\n", chromName);
	sprintf(fileName, "%s/%s", ooDir, chromName);
	ctgDir = listDirX(fileName, "ctg*", TRUE);
	for (ctg = ctgDir; ctg != NULL; ctg = ctg->next)
	    {
	    printf("."); 
	    fflush(stdout);
	    if (ctg->isDir)
	        writeRelevantChains(ctg->name, cloneHash);
	    }
	printf("\n");
	slFreeList(&ctgDir);
	}
    }
printf("Got chains in %d of %d files.  %d total links (including phase2)\n",
	clonesWithChainsCount, slCount(cloneList), linkCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ooChains(argv[1], argv[2]);
return 0;
}
