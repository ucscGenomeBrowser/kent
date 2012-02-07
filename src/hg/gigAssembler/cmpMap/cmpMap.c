/* cmpMap - compare maps. */
#include "common.h"
#include "hash.h"
#include "linefile.h"


struct clone
/* A single clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Name (allocated in hash) */
    int phase;		/* HGS phase - 3 finished, 2 ordered, 1 draft, 0 small frags. */
    int size;		/* Size. */
    struct wuBin *wuBin;	/* Bin in Wash U map. */
    struct jkBin *jkBin;	/* Bin in JK map. */
    };

struct wuBin
/* A map contig. */
    {
    struct wuBin *next;	  /* Next in list. */
    int ix;			  /* Order in file. */
    char *name;			  /* Contig name. Allocated in hash. */
    char *chrom;		  /* Chromosome name. Allocated in hash. */
    boolean gotPos;		  /* TRUE if have STS info. */
    float pos;		          /* STS position. */
    struct cloneRef *cloneList;   /* List of clones. */
    };

struct cloneRef
/* Reference to clone. */
    {
    struct cloneRef *next;	/* Next in list. */
    struct clone *clone;	/* Clone reference. */
    };

struct jkBin
/* A map contig. */
    {
    struct jkBin *next;	  /* Next in list. */
    char *name;			  /* Bin name. Allocated in hash. */
    char *chrom;		  /* Chromosome name. Allocated in hash. */
    boolean gotPos;		  /* TRUE if have STS info. */
    float startPos,endPos;        /* STS position. */
    struct clonePos *cloneList;   /* List of clones. */
    };

struct clonePos
/* Clone position in jk map. */
    {
    struct clonePos *next;	/* Next in list. */
    int offset;			/* Base offset in contig. */
    struct clone *clone;	/* Clone. */
    int nextOverlap;            /* Overlap with next clone. */
    struct clone *maxOverlapClone;  /* Clone overlaps with most. */
    int maxOverlap;                 /* Maximum overlap. */
    char *stsChrom;		    /* NULL if no sts position. */
    float stsPos;                   /* STS position if have any. */
    };

struct clone *storeClone(struct hash *cloneHash, char *name, int phase, int size)
/* Allocate clone and put it in hash table. Squawk but don't die if a dupe. */
{
struct hashEl *hel;
struct clone *clone;

if ((hel = hashLookup(cloneHash, name)) != NULL)
    {
    warn("Duplicate clone %s, ignoring all but first", name);
    return NULL;
    }
else
    {
    AllocVar(clone);
    hel = hashAdd(cloneHash, name, clone);
    clone->name = hel->name;
    clone->phase = phase;
    clone->size = size;
    return clone;
    }
}

boolean isAccession(char *s)
/* Return TRUE if s is of form to be a Genbank accession. */
{
int len = strlen(s);
if (len < 6 || len > 9)
    return FALSE;
if (!isalpha(s[0]))
    return FALSE;
if (!isdigit(s[len-1]))
    return FALSE;
return TRUE;
}

struct clone *readSeqInfo(char *fileName, struct hash *cloneHash)
/* Read in sequence.inf file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
char cloneName[128];
struct clone *clone, *cloneList = NULL;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
	continue;
    wordCount = chopLine(line, words);
    if (wordCount < 8)
	errAbort("Expecting at least 8 words line %d of %s", lf->lineIx, lf->fileName);
    strncpy(cloneName, words[0], sizeof(cloneName));
    chopSuffix(cloneName);
    if (!isAccession(cloneName))
	{
	warn("Expecting an accession in first field line %d of %s", lf->lineIx, lf->fileName);
	continue;
	}
    if ((clone = storeClone(cloneHash, cloneName, atoi(words[3]), atoi(words[2]))) != NULL)
	{
	slAddHead(&cloneList, clone);
	}
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}

struct wuBin *getBin(struct hash *hash, char *name, struct wuBin **pList)
/* Get existing bin or make up a new one. */
{
struct hashEl *hel;
struct wuBin *bin;
static int ix = 0;

if ((hel = hashLookup(hash, name)) != NULL)
    return hel->val;
AllocVar(bin);
hel = hashAdd(hash, name, bin);
bin->name = hel->name;
bin->ix = ++ix;
slAddHead(pList, bin);
return bin;
}

void wuFormatError(struct lineFile *lf)
/* Put generic error message from unparsable WU map. */
{
errAbort("Line %d of %s appears badly formatted", lf->lineIx, lf->fileName);
}

struct wuBin *readWuMap(char *fileName, struct hash *binHash, struct hash *cloneHash, struct hash *chromHash)
/* Read in a WashU map to a wuBin list. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[32], *parts[5];
int lineSize, wordCount, partCount;
int i;
struct wuBin *binList = NULL, *bin;
char *curChrom = NULL;
char *firstWord;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount < 1)
	continue;
    firstWord = words[0];
    if (sameString("start", firstWord))
	{
	if (wordCount < 3)
	    errAbort("Short start line %d of %s", lf->lineIx, lf->fileName);
	partCount = chopByChar(words[2], '.', parts, ArraySize(parts));
	if (partCount != 3)
	    wuFormatError(lf);
	curChrom = hashStoreName(chromHash, parts[2]);
	}
    else if (sameString("end", firstWord))
	{
	}
    else if (sameString("@", firstWord))
	{
	if (wordCount >= 3)
	    {
	    char *numString = words[2];
	    char *name = words[1];
	    if (!startsWith("ctg", name)) 
		wuFormatError(lf); 
	    if (isdigit(numString[0])) 
	        { 
		bin = getBin(binHash, name, &binList);
		bin->chrom = curChrom;
		bin->gotPos = TRUE;
		bin->pos = atof(numString);
		}
	    }
	}
    else if (sameString("COMMITTED", firstWord))
	{
	}
    else /* An accession we hope! */
	{
	struct cloneRef *cr;
	struct clone *clone;

	if (curChrom != NULL && sameString(curChrom, "NA"))
	    continue;
	if (!isAccession(firstWord))
	    errAbort("Expecting accession in first word line %d of %s", 
	    	lf->lineIx, lf->fileName);
	if (wordCount < 9)
	    errAbort("Expecting at least 9 words line %d of %s",
	    	lf->lineIx, lf->fileName);
	bin = getBin(binHash, words[3], &binList);
	bin->chrom = curChrom;
	AllocVar(cr);
	cr->clone = clone = hashMustFindVal(cloneHash, firstWord);
	slAddTail(&bin->cloneList, clone);
	clone->wuBin = bin;
	}
    }
slReverse(&binList);
lineFileClose(&lf);
return binList;
}


struct jkBin *readJkMap(char *fileName, struct hash *binHash, struct hash *cloneHash, struct hash *chromHash)
/* Read in a JI map to a jkBin list.  The records looks like so, and are separated by
 * blank lines. 
	barge 1 clones 4 chrom 1 pos 694.51 to 694.51
	0	AC016192 157851  50298  (AL356353  50354  319)
	107553	AL356353 142433  75363  (AC016047  75347  529)	1 694.51
	174623	AC016047 218568 179960  (AL356310 179881  823)	1 694.51
	213231	AL356310 211693      0  (AC016047 179939  850)	1 694.51
 */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[32], *parts[5];
int lineSize, wordCount, partCount;
int i;
struct jkBin *binList = NULL, *bin = NULL;
char *firstWord;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    firstWord = words[0];
    if (firstWord[0] == '#')
	continue;
    if (sameString(firstWord, "barge"))
	{
	char *name = words[2];
	struct hashEl *hel;
	if (wordCount != 10)
	    errAbort("Expecting 10 words line %d of %s\n", lf->lineIx, lf->fileName);
	AllocVar(bin);
	hel = hashAdd(binHash, name, bin);
	bin->name = hel->val;
	bin->chrom = hashStoreName(chromHash, words[5]);
	if (!sameString(bin->chrom, "NA"))
	    {
	    bin->gotPos = TRUE;
	    bin->startPos = atoi(words[7]);
	    bin->endPos = atoi(words[9]);
	    }
	slAddHead(&binList, bin);
	}
    else
	{
	char *acc = words[1];
	struct clonePos *pos;
	struct clone *clone;

	if (bin == NULL)
	    errAbort("Expecting barge line %d of %s", lf->lineIx, lf->fileName);
	if (wordCount < 7)
	    errAbort("Expecting at least 7 words line %d of %s\n", lf->lineIx, lf->fileName);
	if (!isAccession(acc))
	    errAbort("Expecting accession word 2 line %d of %s\n", lf->lineIx, lf->fileName);
	clone = hashMustFindVal(cloneHash, acc);
	clone->jkBin = bin;
	AllocVar(pos);
	pos->offset = atoi(words[0]);
	pos->clone = clone;
	pos->nextOverlap = atoi(words[3]);
	pos->maxOverlapClone = hashMustFindVal(cloneHash, words[4]+1);
	pos->maxOverlap = atoi(words[5]);
	if (wordCount >= 9)
	    {
	    pos->stsChrom = hashStoreName(chromHash, words[7]);
	    pos->stsPos = atof(words[8]);
	    }
	slAddTail(&bin->cloneList, pos);
	}
    }
lineFileClose(&lf);
slReverse(&binList);
return binList;
}

struct wuBinRef
/* Reference to a Wash U contig. */
    {
    struct wuBinRef *next;	/* Next in list. */
    struct wuBin *wuBin;	/* Reference. */
    int count;		        /* Count of references. */
    };

int cmpRefCount(const void *va, const void *vb)
/* Compare two wuBinRefCounts to sort largest counts first. */
{
const struct wuBinRef *a = *((struct wuBinRef **)va);
const struct wuBinRef *b = *((struct wuBinRef **)vb);
return b->count - a->count;
}

void findBeforeAndAfter(struct jkBin *jk, struct clonePos *center, struct clonePos **retBefore, 
	struct clonePos **retAfter)
/* Find clones before and after center in jk contig. */
{
struct clonePos *pos, *lastPos = NULL, *before = NULL, *after = NULL;

for (pos = jk->cloneList; pos != NULL; pos = pos->next)
    {
    if (pos == center)
	{
	*retBefore = lastPos;
	*retAfter = pos->next;
	return;
	}
    lastPos = pos;
    }
errAbort("Couldn't find %s in %s", center->clone->name, jk->name);
}

char *placementStrength(struct jkBin *jk, struct clonePos *pos, struct clonePos *before, struct clonePos *after)
/* Return a word describing strength of placement in map. */
{
int weight = 0;

if (before != NULL)
    {
    ++weight;
    if (before->clone == pos->maxOverlapClone)
	++weight;
    }
if (after != NULL)
    {
    ++weight;
    if (after->clone == pos->maxOverlapClone)
	++weight;
    }
if (weight <= 1)
    return "weak";
if (weight <= 2)
    return "medium";
return "strong";
}

struct stray
/* Describes a possibly stray clone. */
    {
    struct stray *next;
    int wuCtgIx;	/* Index of contig in WU map. */
    char *clone;	/* Name of clone. */
    char *wuChrom;	/* Chromosome in WU map. */
    boolean wuGotPos;   /* True if have WU position. */
    float wuPos;        /* STS position. */
    char *wuCtg;	/* Name of WU contig. */
    char *strength;     /* Strength of position. */
    char *before;	/* Name of clone before. */
    char *after;	/* Name of clone after. */
    char *jkChrom;	/* JK chromosome assignment. */
    float jkPos;        /* JK position. */
    };

int compareStrays(const void *va, const void *vb)
/* Compare two strays to sort largest counts first. */
{
const struct stray *a = *((struct stray **)va);
const struct stray *b = *((struct stray **)vb);
return a->wuCtgIx - b->wuCtgIx;
}

struct stray *makeStray(struct jkBin *jk, struct clonePos *pos, char *wuContigName, 
  int wuIx, char *wuChrom, boolean wuGotPos, float wuPos)
/* Make structure that describes a stray. */
{
struct clonePos *before, *after;
char *beforeName = "n/a", *afterName = "n/a";
char *strength;
struct stray *stray;

findBeforeAndAfter(jk, pos, &before, &after);
if (before)
    beforeName = before->clone->name;
if (after)
    afterName = after->clone->name;

AllocVar(stray);
stray->wuCtgIx = wuIx;
stray->clone = pos->clone->name;
stray->wuChrom = wuChrom;
stray->wuGotPos = wuGotPos;
stray->wuPos = wuPos;
stray->wuCtg = wuContigName;
stray->strength = placementStrength(jk, pos, before, after);
stray->before = beforeName;
stray->after = afterName;
if (jk->gotPos)
    {
    stray->jkChrom = jk->chrom;
    stray->jkPos = 0.5*(jk->startPos + jk->endPos);
    }
return stray;
}

void saveStrays(struct stray *strayList, FILE *f)
/* Save strays to a file. */
{
struct stray *stray;

for (stray = strayList; stray != NULL; stray = stray->next)
    {
    char buf[20];
    if (stray->wuGotPos)
	sprintf(buf, "%-6.2f", stray->wuPos);
    else
	sprintf(buf, "n/a");
    fprintf(f, "%-10s %-3s %6s  %-10s %-8s %-10s %-10s",
    	stray->clone, stray->wuChrom, buf, stray->wuCtg, stray->strength, stray->before, stray->after);
    if (stray->jkChrom)
	fprintf(f, " %3s %7.2f", stray->jkChrom, stray->jkPos);
    fprintf(f, "\n");
    }
}

void straysInJk(struct jkBin *jk, struct hash *cloneHash, struct stray **pStrayList)
/* Print info on strays within a single jk contig. */
{
struct wuBin *wu;
struct clone *clone;
struct clonePos *pos;
struct wuBinRef *refList = NULL, *ref;
struct hash *refHash = newHash(10);
int positioned = 0, unpositioned = 0;
struct stray *strayList = NULL, *stray;

for (pos = jk->cloneList; pos != NULL; pos = pos->next)
    {
    clone = pos->clone;
    if ((wu = clone->wuBin) == NULL)
	{
	stray = makeStray(jk, pos, "n/a", 1000000, "NA", FALSE, 0.0);
	slAddHead(pStrayList, stray);
	++unpositioned;
	}
    else
	{
	if ((ref = hashFindVal(refHash, wu->name)) == NULL)
	    {
	    AllocVar(ref);
	    hashAdd(refHash, wu->name, ref);
	    ref->wuBin = wu;
	    slAddHead(&refList, ref);
	    }
	++ref->count;
	++positioned;
	}
    }
slSort(&refList, cmpRefCount);
if ((ref = refList) != NULL)
    {
    int largestCount = ref->count;
    if (largestCount*2 > positioned)
	{
	for (ref = ref->next; ref != NULL; ref = ref->next)
	    {
	    wu = ref->wuBin;
	    for (pos = jk->cloneList; pos != NULL; pos = pos->next)
		{
		clone = pos->clone;
		if (clone->wuBin == wu)
		    {
		    if (!wu->gotPos || !jk->gotPos ||
			wu->chrom != jk->chrom ||
			fabs(wu->pos - 0.5*(jk->startPos+jk->endPos)) > 20)
			{
			stray = makeStray(jk, pos, wu->name, wu->ix, 
			    wu->chrom, wu->gotPos, wu->pos);
			slAddHead(pStrayList, stray);
			}
		    }
		}
	    }
	}
    }
freeHash(&refHash);
slFreeList(&refList);
}


void findStrays(struct jkBin *jkList, struct hash *cloneHash, FILE *f)
/* Print info on stuff where jk and wu binning are different. */
{
struct jkBin *jk;
struct stray *strayList = NULL;

for (jk = jkList; jk != NULL; jk = jk->next)
    straysInJk(jk, cloneHash, &strayList);
slSort(&strayList, compareStrays);
saveStrays(strayList, f);
slFreeList(&strayList);
}

void cmpMap(char *seqInfo, char *wuMap, char *jkMap)
/* Compare maps. */
{
struct hash *cloneHash = newHash(16);
struct hash *chromHash = newHash(6);
struct hash *wuHash = newHash(15);
struct hash *jkHash = newHash(15);
struct clone *cloneList;
struct wuBin *wuList;
struct jkBin *jkList;

printf("Reading %s...\n", seqInfo);
cloneList = readSeqInfo(seqInfo, cloneHash);
printf(" read %d clones.\n", slCount(cloneList));
printf("Reading %s...\n", wuMap);
fflush(stdout);
wuList = readWuMap(wuMap, wuHash, cloneHash, chromHash);
printf(" read %d contigs.\n", slCount(wuList));
printf("Reading %s...\n", jkMap);
jkList = readJkMap(jkMap, jkHash, cloneHash, chromHash);
printf(" read %d barges.\n", slCount(jkList));
findStrays(jkList, cloneHash, stdout);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
"cmpMap - compare maps.\n"
"usage:\n"
"  cmpMap sequence.inf map.wu map.jk");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
cmpMap(argv[1], argv[2], argv[3]);
return 0;
}
