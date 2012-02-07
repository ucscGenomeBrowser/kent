/* ppStats - collect stats on paired plasmid alignments. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "psl.h"
#include "pPair.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ppStats - collect stats on paired plasmid alignments\n"
  "usage:\n"
  "   ppStats pairs.psl pairs.pair finDir read.head missMatch.out\n");
}

struct readInfo
/* Information on a single read. */
    {
    struct readInfo *next;	/* Next in list. */
    char *name;                 /* Read name - allocated in hash. */
    struct pairInfo *pair;      /* Associated pair info. */
    struct shortAli *aliList;   /* List of alignments this is in. */
    char strand;	        /* Direction of read '+' or '-'. */
    int aliCount;               /* Number of alignment it's in. */
    int goodAliCount;           /* Number of good alignments it's in. */
    };

struct pairInfo
/* Info on a read pair. */
    {
    struct pairInfo *next;	/* Next in list. */
    char *name;                 /* Pair name - allocated in hash. */
    struct projectInfo *project;    /* Project - determines insert size. */
    struct readInfo *fRead;     /* Forward read. */
    struct readInfo *rRead;     /* Reverse read. */
    };

struct shortAli
/* Short alignment info. */
    {
    struct shortAli *next;
    char *tName;           /* Target name - allocated in hash */
    int tStart, tEnd;      /* Target start and end position. */
    int tSize;             /* Target size. */
    char strand;           /* Relative oriention. */
    };

struct projectInfo
/* Info on a project. */
    {
    struct projectInfo *next;	/* Next in list. */
    char *name;			/* Allocated in hash. */
    int minSize;		/* Minimum insert size. */
    int maxSize;		/* Maximum insert size. */
    };

boolean filter(struct psl *psl)
/* Return TRUE if psl passes filter. */
{
int startTail, endTail;
int maxTail = 60;
pslTailSizes(psl, &startTail, &endTail);
if (startTail > maxTail || endTail > maxTail)
    return FALSE;
return TRUE;
}

int aliSizes[20];

int readAlignments(char *pairsPsl, struct hash *readHash, struct hash *fragHash, boolean tolerateNew)
/* Read in alignments and process them into the read->aliList. 
 * Returns number of alignments altogether. */
{
struct lineFile *lf = pslFileOpen(pairsPsl);
struct shortAli *ali;
struct psl *psl;
struct readInfo *rd;
int aliCount = 0;
int dotEvery = 20*1024;
int dotty = dotEvery;
int aliSize;
int totalAliCount = 0;

printf("Reading and processing %s\n", pairsPsl);
for (;;)
    {
    if (--dotty <= 0)
	{
	dotty = dotEvery;
        printf(".");
	fflush(stdout);
	}
    AllocVar(ali);     /* Allocate this first to reduce memory fragmentation. */
    if ((psl = pslNext(lf)) == NULL)
        {
	freeMem(ali);
	break;
	}
    ++totalAliCount;
    rd = hashFindVal(readHash, psl->qName);
    if (rd == NULL)
	{
	if (!tolerateNew)
	    errAbort("%s is in psl file but not in pairs file", psl->qName);
	else
	    {
	    pslFree(&psl);
	    freeMem(ali);
	    continue;
	    }
	}
    ++rd->aliCount;
    if (filter(psl))
	{
	++rd->goodAliCount;
	aliSize = psl->match + psl->repMatch;
	aliSize /= 100;
	if (aliSize < 0) aliSize = 0;
	if (aliSize >= ArraySize(aliSizes)) aliSize = ArraySize(aliSizes)-1;
	aliSizes[aliSize] += 1;
	ali->tName = hashStoreName(fragHash, psl->tName);
	ali->tStart = psl->tStart;
	ali->tEnd = psl->tEnd;
	ali->tSize = psl->tSize;
	ali->strand = psl->strand[0];
	slAddHead(&rd->aliList, ali);
	pslFree(&psl);
	++aliCount;
	}
    else
        {
	pslFree(&psl);
	freeMem(ali);
	}
    }
printf("\n");
printf("%d of %d alignments passed filter\n", aliCount, totalAliCount);
return aliCount;
}

struct readInfo *addNewRead(char *name, struct hash *readHash, struct readInfo **pReadList)
/* Add read.  Insure that it's new. */
{
struct hashEl *hel;
struct readInfo *ri;

AllocVar(ri);
hel = hashAddUnique(readHash, name, ri);
ri->name = hel->name;
slAddHead(pReadList, ri);
return ri;
}

struct readInfo *strandRead(char *name, struct hash *readHash, char strand)
/* Look up read in hash and add strand info. */
{
struct readInfo *ri;

if ((ri = hashFindVal(readHash, name)) == NULL)
     return NULL;
ri->strand = strand;
return ri;
}

void readPairs(char *fileName, struct hash *pairHash, struct pairInfo **retPairList,
     struct hash *readHash, struct readInfo **retReadList,
     struct hash *projectHash, struct projectInfo **retProjectList)
/* Read in a paired file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[8];
struct pPairInfo pp;
struct readInfo *ri;
struct projectInfo *project;
struct pairInfo *pair;
char name[32];
struct hashEl *hel;


while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 5, wordCount);
    pPairInfoStaticLoad(words, &pp);
    AllocVar(pair);
    hel = hashAddUnique(pairHash, pp.clone, pair);
    pair->name = hel->name;
    slAddHead(retPairList, pair);
    sprintf(name, "%d-%d", pp.minSize, pp.maxSize);
    if ((hel = hashLookup(projectHash, name)) == NULL)
        {
	AllocVar(project);
	hel = hashAdd(projectHash, name, project);
	project->name = hel->name;
	project->minSize = pp.minSize;
	project->maxSize = pp.maxSize;
	slAddHead(retProjectList, project);
	}
    else
        project = hel->val;
    pair->project = project;
    pair->fRead = strandRead(pp.fRead, readHash, '+');
    pair->rRead = strandRead(pp.rRead, readHash, '-');
    }
lineFileClose(&lf);
slReverse(retPairList);
slReverse(retProjectList);
}

void readReads(char *fileName, struct hash *readHash, struct readInfo **pReadList)
/* Load up reads from file of:
    >readName
 */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct readInfo *ri;
char *line;
int lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
       ++line;
    line = firstWordInLine(line);
    ri = addNewRead(line, readHash, pReadList);
    }
lineFileClose(&lf);
}

int countRealPairs(struct pairInfo *pairList)
/* Count number of pairs where have both partners. */
{
struct pairInfo *pair;
int count = 0;
for (pair = pairList; pair != NULL; pair = pair->next)
    if (pair->rRead != NULL && pair->fRead != NULL)
        ++count;
return count;
}

int countTailessPairs(struct pairInfo *pairList)
/* Count number of pairs where have both partners. */
{
struct pairInfo *pair;
int count = 0;
for (pair = pairList; pair != NULL; pair = pair->next)
    if (pair->rRead != NULL && pair->fRead != NULL)
	if (pair->rRead->goodAliCount > 0 && pair->fRead->goodAliCount > 0)
	    ++count;
return count;
}

struct shortAli *findMidFinAli(struct shortAli *aliList, struct hash *finHash)
/* Find first in ali list that targets finished seq in hash. */
{
struct shortAli *ali;
char tName[64];
int minDist = 10000;

for (ali = aliList; ali != NULL; ali = ali->next)
    {
    strncpy(tName, ali->tName, sizeof(tName));
    chopSuffix(tName);
    if (hashLookup(finHash, tName))
        {
	if (ali->tStart >= minDist && ali->tSize - ali->tEnd >= minDist)
	    return ali;
	}
    }
return NULL;
}


void doInsertStats(struct pairInfo *pairList, struct hash *finHash, char *mismatchFile)
/* Calculate and print stats on pairs where fRead hits
 * near middle of finished clone. */
{
struct pairInfo *pair;
int possibly = 0, really = 0;
int minIns, maxIns;
double totalIns = 0;
FILE *miss = mustOpen(mismatchFile, "w");

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->rRead != NULL && pair->fRead != NULL)
	{
	if (pair->rRead->aliCount == 1 && pair->fRead->aliCount == 1 &&
	  pair->rRead->goodAliCount > 0 && pair->fRead->goodAliCount > 0)
	    {
	    struct shortAli *fAli, *rAli;
	    if ((fAli = findMidFinAli(pair->fRead->aliList, finHash)) != NULL)
	        {
		bool gotMatch = FALSE;
		++possibly;
		for (rAli = pair->rRead->aliList; rAli != NULL; rAli=rAli->next)
		    {
		    if (sameString(rAli->tName, fAli->tName) && rAli->strand != fAli->strand)
		        {
			minIns = min(rAli->tStart, fAli->tStart);
			maxIns = max(rAli->tEnd, fAli->tEnd);
			totalIns += (maxIns - minIns);
			++really;
			gotMatch = TRUE;
			break;
			}
		    }
		if (!gotMatch)
		    {
		    fprintf(miss, "%s\t%s\t%s\t%d\t%c\t%d\t%d\n", pair->fRead->name, pair->rRead->name, 
		    	fAli->tName, fAli->tSize, fAli->strand, fAli->tStart, fAli->tEnd);
		    }
		}
	    }
	}
    }
printf("%d of %d near middle of finished (%f%%) also align\n",
	really, possibly, 100.0*really/(double)possibly);
if (really > 0)
    printf("Average insert size %f\n", totalIns/really);
}

int countAliReads(struct readInfo *readList)
/* Count up number of reads with alignments. */
{
struct readInfo *ri;
int count = 0;
for (ri = readList; ri != NULL; ri = ri->next)
    if (ri->aliCount > 0)
        ++count;
return count;
}

int countGoodAliReads(struct readInfo *readList)
/* Count up number of reads with good alignments. */
{
struct readInfo *ri;
int count = 0;
for (ri = readList; ri != NULL; ri = ri->next)
    if (ri->goodAliCount > 0)
        ++count;
return count;
}


void ppStats(char *pairsPsl, char *pairFile, char *finDir, char *readHeads, char *mismatchFile)
/* ppStats - collect stats on paired plasmid alignments. */
{
struct hash *readHash = newHash(21);
struct hash *pairHash = newHash(20);
struct hash *fragHash = newHash(17);
struct hash *finHash = NULL;
struct hash *projectHash = newHash(8);
struct pairInfo *pairList = NULL, *measuredList, *pair;
struct readInfo *readList = NULL, *rd;
struct projectInfo *projectList = NULL, *project;
int readCount, readAli, goodReadAli;
int aliCount;
int finCount;
int i;
struct slName *finList, *name;
boolean isWhitehead = (strstr(pairFile, "hite") != NULL);

/* Make hash of finished clones. */
finHash = newHash(14);
finList = listDir(finDir, "*.fa");
if ((finCount = slCount(finList)) == 0)
    errAbort("No fa files in %s\n", finDir);
printf("Got %d (finished) .fa files in %s\n", finCount, finDir);
for (name = finList; name != NULL; name = name->next)
    {
    chopSuffix(name->name);
    hashStore(finHash, name->name);
    }

/* List Reads. */
readReads(readHeads, readHash, &readList);
readCount = slCount(readList);
printf("Got %d reads\n", readCount);

/* Read in pairs file. */
readPairs(pairFile, pairHash, &pairList, readHash, &readList, projectHash, &projectList);
printf("Got %d pairs in %d projects\n",
	countRealPairs(pairList), slCount(projectList));
slReverse(&readList);

aliCount = readAlignments(pairsPsl, readHash, fragHash, TRUE);

readAli = countAliReads(readList);
goodReadAli = countGoodAliReads(readList);
printf("%d of %d reads (%4.2f%%) align to genome\n", 
	readAli, readCount, 100.0*readAli/(double)readCount);
printf("%d of %d reads (%4.2f%%) align to genome without tails\n", 
	goodReadAli, readCount, 100.0*goodReadAli/(double)readCount);
printf("%d pairs have both ends aligning without tails\n",
        countTailessPairs(pairList));
doInsertStats(pairList, finHash, mismatchFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    usage();
ppStats(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
