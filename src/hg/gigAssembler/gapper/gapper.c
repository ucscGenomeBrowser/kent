/* gapper - try to find unsequenced gaps in the draft human genome
 * by looking at BAC end pair alignments between clones and
 * between fingerprint contigs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "errabort.h"
#include "obscure.h"
#include "localmem.h"
#include "hCommon.h"
#include "psl.h"
#include "jksql.h"


FILE *logFile;		/* Report stuff here. */
struct lm *lm;		/* Fast local memory pool. */
int version = 3;	/* Version number. */

void warnHandler(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) 
    {
    fprintf(stderr, "[warning] ");
    fprintf(logFile, "[warning] ");
    vfprintf(stderr, format, args);
    vfprintf(logFile, format, args);
    fprintf(stderr, "\n");
    fprintf(logFile, "\n");
    }
}

void status(char *format, ...)
/* Print status message to stdout and to log file. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
vfprintf(stdout, format, args);
va_end(args);
}

void logIt(char *format, ...)
/* Print message to log file. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
va_end(args);
}


#undef uglyf
#define uglyf status

struct gChromosome
/* A chromosome (or something like it) */
    {
    struct gChromosome *next;    /* next in list. */
    char *name;                  /* name of chromesome. */
    struct gContigRef *contigList;  /* contigList. */
    };

struct gContig
/* A fingerprint map contig. */
    {
    struct gContig *next;	/* Next in list. */
    char *name;                 /* Name of contig. */
    struct gChromosome *chrom;                /* Chromosome this is on. */
    boolean isOrdered;          /* Position in chromosome known? */
    int orderIx;                /* Relative position in chromosome if ordered. */
    struct gCloneRef *cloneList; /* List of all clones. */
    struct gBargeRef *bargeList; /* List of all barged. */
    };

struct gContigRef
/* Reference to a contig. */
    {
    struct gContigRef *next;    /* Next in list. */
    struct gContig *contig;     /* Contig reference. */
    };

struct gClone
/* Information about one clone. */
    {
    struct gClone *next;	/* Next in list. */
    char *acc;			/* Genbank accession. */
    char *bac;                  /* Bac name. */
    char *center;		/* Sequencing center. */
    struct gContig *contig;     /* Contig this is in. */
    struct gBarge *barge;       /* Barge this is in. */
    int pos;			/* Position in rough kilobase units. */
    char *ordering;             /* Ordering info. */
    int size;                   /* Estimated size in bases. */
    int phase;                  /* 3 (finished) 2 (ordered) 1 (draft) 0 (survey) */
    bool unmasked;		/* Not masked? */
    };

struct gCloneRef
/* Reference to a clone. */
    {
    struct gCloneRef *next;    /* Next in list. */
    struct gClone *clone;      /* Clone refers to. */
    };

struct gBarge
/* Info about a barge. */
    {
    struct gBarge *next;	/* Next in list. */
    char *name;			/* Barge name (chrome+contig+ix) */
    int ix;			/* Barge index inside contig. */
    struct gContig *contig;     /* Contig it is in. */
    struct gCloneRef *cloneList; /* List of clones */
    int doubleConnCount;        /* Number of connections .*/
    boolean bridgedToNext;      /* Bridge exists to next in list? */
    };

struct gBargeRef
/* Reference to a barge. */
    {
    struct gBargeRef *next;	/* Next in list. */
    struct gBarge *barge;	/* Barge. */
    };

struct bacEndPair
/* Info on bacEndPairs. */
    {
    struct bacEndPair *next;	/* Next in list. */
    char *a;			/* Accession of one. */
    char *b;			/* Accession of other. */
    char *clone;		/* Name of clone. */
    struct bConnect *aList;     /* Connections to a. */
    struct bConnect *bList;     /* Conenctions to b. */
    };

struct bConnect
/* Bac end connection */
    {
    struct bConnect *next;	  /* Next in list. */
    struct gClone *clone;         /* Clone this hits. */
    char *bacEnd;                 /* Bac end name. */
    short score;                  /* Overall score. */
    short startTail, endTail;     /* Unaligned "tails". */
    short match, repMatch;        /* Unique and repeat matches. */
    short misMatch;               /* Mismatches. */
    short sizeDiff;               /* Size difference between two parts of alignment. */
    short numInserts;             /* Number of inserts total. */
    char *tFrag;                  /* Name of target fragment. */
    int tStart,tEnd;              /* Target position of alignment. */
    };

struct connectPair
/* pair of bConnect used to link two clones or two
 * contigs together. */
    {
    struct connectPair *next;   /* Next in list. */
    struct bacEndPair *bep;     /* Bac end pair involved in join. */
    struct bConnect *a,*b;      /* Connections on either side. */
    };

struct joinedClones
/* A pair of clones that are joined. */
    {
    struct joinedClones *next;	/* Next in list. */
    char *name;                 /* Name (a,b alphabetical) */
    struct gClone *a, *b;       /* The clones. */
    struct connectPair *cpList; /* Connections joining pair. */
    float probOk;	        /* Probability join is real. */
    };


struct joinedBarges
/* A pair of barges that are joined. */
    {
    struct joinedBarges *next;   /* Next in list. */
    char *name;                  /* (a,b alphabetical) */
    struct gBarge  *a, *b;       /* The two barges. */
    struct connectPair *cpList; /* Connections joining pair. */
    float probOk;
    };

struct hash *contigHash;     /* A hash with contig names for key and gContigs for value. */
struct gContig *gContigList; /* List of all contigs. */
struct hash *accHash;	     /* A hash with clone accessions for key and gClones's for value. */
struct hash *bacHash;        /* Hash with bac names for key and gClones for value. */
struct hash *tFragHash;      /* Hash of clone fragments.  No value. */
struct gClone *gCloneList;   /* List of all clones. */
struct gChromosome *gChromList; /* List of all chromosomes. */

struct hash *centerHash;     /* Hash with names of all sequencing centers */
struct hash *orderingHash;   /* Hash with names of all orderings .*/

struct hash *bepHash;        /* Hash of bacEndPairs.  Keyed by accessions of both members. */
struct hash *bepCloneHash;   /* Hash of bacEndPairs keyed by bep->cloneName. */
struct bacEndPair *bepList;  /* List of all bac end pairs. */

struct hash *joinedClonesHash;   /* Hash of joined clones. */
struct joinedClones *joinedClonesList;  /* List of joined clones. */
struct hash *joinedBargesHash;  /* Hash of joined contigs. */
struct joinedBarges *joinedBargesList; /* List of joined contigs. */

struct gBarge *bargeList;        /* List of all barges. */

void readContigs(struct lineFile *in, struct gChromosome *chrom, boolean isOrdered)
/* Read in all contigs up to next "end human SUPERLINK" */
{
int lineSize, wordCount;
char *line, *words[20];
boolean gotEnd = FALSE;
char *lastCtgName = "";
char *ctgName;
char *acc, *bac;
struct gContig *contig = NULL;
struct gContigRef *contigRef;
struct hashEl *hel;
struct gClone *clone;
struct gCloneRef *cloneRef;
int orderIx = 0;
bool isCommitted;

while (lineFileNext(in, &line, &lineSize))
    {
    if (startsWith("end human", line))
	{
	gotEnd = TRUE;
	break;
	}
    else if (startsWith("start human", line))
        {
        gotEnd = TRUE;
        lineFileReuse(in);
        break;
        }
    wordCount = chopLine(line, words);
    if (words[0][0] == '@' || words[0][0] == '*')
	continue;	
    acc = words[0];
    isCommitted = sameWord(acc, "COMMITTED");
    if (!isCommitted && !(wordCount <= 11 && wordCount >= 9))
	errAbort("Expecting 11 words line %d of %s", in->lineIx, in->fileName);
    ctgName = words[3];
    if (!sameString(ctgName, lastCtgName))
	{
	AllocVar(contig);
	if ((hel = hashLookup(contigHash, ctgName)) != NULL)
	    warn("Duplicate contig %s line %d of %s\n", ctgName, in->lineIx, in->fileName);
	else
	    hel = hashAdd(contigHash, ctgName, contig);
	slAddHead(&gContigList, contig);
	contig->name = hel->name;
	lastCtgName = contig->name;
	contig->chrom = chrom;
	contig->isOrdered = isOrdered;
	contig->orderIx = ++orderIx;
	AllocVar(contigRef);
	contigRef->contig = contig;
	slAddTail(&chrom->contigList, contigRef);
	}
    acc = words[0];
    bac = words[1];
    AllocVar(clone);
    slAddHead(&gCloneList, clone);
    AllocVar(cloneRef);
    cloneRef->clone = clone;
    slAddTail(&contig->cloneList, cloneRef);
    if ((hel = hashLookup(bacHash, bac)) != NULL)
	{
	logIt("Duplicate BAC %s line %d of %s\n", bac, in->lineIx, in->fileName);
	}
    else
	hel = hashAdd(bacHash, bac, clone);
    clone->bac = hel->name;
    clone->center = hashStoreName(centerHash, words[2]);
    clone->pos = atoi(words[4]);
    if (!isCommitted)
	{
	if ((hel = hashLookup(accHash, acc)) != NULL)
	    logIt("Duplicate accession %s line %d of %s\n", acc, in->lineIx, in->fileName);
	else
	    hel = hashAdd(accHash, acc, clone);
	clone->ordering = hashStoreName(orderingHash, words[5]);
	clone->size = atoi(words[6]);
	clone->phase = atoi(words[7]);
	clone->acc = hel->name;
	}
    }
if (!gotEnd)
    warn("%s seems to be truncated\n", in->fileName);
}

void readCommitContigs(struct lineFile *in, struct gChromosome *chrom, boolean isOrdered)
/* Skip to "end human SUPERLINK" */
{
int lineSize;
char *line;

while (lineFileNext(in, &line, &lineSize))
    {
    if (startsWith("end human", line))
	break;
    }
}

void readNaContigs(struct lineFile *in, struct gChromosome *chrom, boolean isOrdered)
/* Read in all unplaced contigs up to next "end human SUPERLINK" */
{
int lineSize, wordCount;
char *line, *words[20];
boolean gotEnd = FALSE;
char *acc, *bac;
struct gContig *contig = NULL;
struct gContigRef *contigRef;
struct hashEl *hel;
struct gClone *clone;
struct gCloneRef *cloneRef;
char *ctgName = "ctg_na";

AllocVar(contig);
hel = hashAdd(contigHash, ctgName, contig);
contig->name = hel->name;
contig->chrom = chrom;
contig->isOrdered = FALSE;
slAddHead(&gContigList, contig);
AllocVar(contigRef);
contigRef->contig = contig;
slAddTail(&chrom->contigList, contigRef);

while (lineFileNext(in, &line, &lineSize))
    {
    if (startsWith("end human", line))
	{
	gotEnd = TRUE;
	break;
	}
    wordCount = chopLine(line, words);
    if (words[0][0] == '@' || words[0][0] == '*')
	continue;	
    if (wordCount != 5 && wordCount != 4)
	errAbort("Expecting 5 words line %d of %s", in->lineIx, in->fileName);
    acc = words[0];
    if (wordCount == 5)
	bac = words[1];
    else
	bac = NULL;
    AllocVar(clone);
    slAddHead(&gCloneList, clone);
    AllocVar(cloneRef);
    cloneRef->clone = clone;
    slAddTail(&contig->cloneList, cloneRef);
    if (bac != NULL)
	{
	if ((hel = hashLookup(bacHash, bac)) != NULL)
	    {
	    logIt("Duplicate BAC %s line %d of %s\n", bac, in->lineIx, in->fileName);
	    }
	else
	    hel = hashAdd(bacHash, bac, clone);
	clone->bac = hel->name;
	}
    clone->center = hashStoreName(centerHash, words[wordCount-3]);
    if ((hel = hashLookup(accHash, acc)) != NULL)
	logIt("Duplicate accession %s line %d of %s\n", acc, in->lineIx, in->fileName);
    else
	hel = hashAdd(accHash, acc, clone);
    clone->size = atoi(words[wordCount-1]);
    clone->phase = atoi(words[wordCount-2]);
    clone->acc = hel->name;
    }
if (!gotEnd)
    warn("%s seems to be truncated\n", in->fileName);
}

void readMap(char *fileName)
/* Read WashU style map. */
{
int lineSize, wordCount;
char *line, *words[20];
char chromName[32];
char lastChromName[32];
char *s;
struct gChromosome *chrom = NULL;
struct lineFile *in = lineFileOpen(fileName, TRUE);
boolean isOrdered;

contigHash = newHash(0);
accHash = newHash(16);
tFragHash = newHash(18);
bacHash = newHash(16);
centerHash = newHash(8);
orderingHash = newHash(8);

status("reading map:");
fflush(stdout);
while (lineFileNext(in, &line, &lineSize))
    {
    if (!startsWith("start human SUPERLINK", line))
	continue;
    wordCount = chopLine(line, words);
    if (wordCount != 5 && wordCount != 4)
	errAbort("Odd start line %d of %s\n", in->lineIx, in->fileName);
    s = strrchr(words[2], '.');
    if (s == NULL)
	errAbort("Couldn't find chromosome line %d of %5s\n", in->lineIx, in->fileName);
    s += 1;
    strncpy(chromName, s, sizeof(chromName));
    if (!sameString(chromName, lastChromName))
	{
	strcpy(lastChromName, chromName);
	status(" %s", chromName);
	fflush(stdout);
	AllocVar(chrom);
	chrom->name = cloneString(chromName);
	slAddHead(&gChromList, chrom);
	}
    isOrdered = sameWord(words[3], "ORDERED");
    if (sameString(chromName, "NA"))
	readNaContigs(in, chrom, isOrdered);
    else if (sameString(chromName, "COMMIT"))
	readCommitContigs(in, chrom, isOrdered);
    else
	readContigs(in, chrom, isOrdered);
    }
status("\n");
slReverse(&gChromList);
slReverse(&gContigList);
slReverse(&gCloneList);
lineFileClose(&in);
}

void readPairs(char *fileName)
/* Read in bac end pairs into hashes and list. */
{
struct lineFile *in = lineFileOpen(fileName, TRUE);
struct bacEndPair *bep;
struct hashEl *hel;
int lineSize, wordCount;
char *line, *words[4];

bepHash = newHash(19);
bepCloneHash = newHash(18);
while (lineFileNext(in, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
    if (wordCount != 3)
	errAbort("Expecting three tab separated fields line %d of %s\n", in->lineIx, in->fileName);
    AllocVar(bep);
    hel = hashAdd(bepHash, words[0], bep);
    bep->a = hel->name;
    hel = hashAdd(bepHash, words[1], bep);
    bep->b = hel->name;
    hel = hashAdd(bepCloneHash, words[2], bep);
    bep->clone = hel->name;
    slAddHead(&bepList, bep);
    }
slReverse(&bepList);
lineFileClose(&in);
}

struct psl *pslShortStatic(char **row)
/* Return static psl made up from row that doesn't include
 * comma separated lists. */
{
static struct psl p;

p.match = sqlUnsigned(row[0]);
p.misMatch = sqlUnsigned(row[1]);
p.repMatch = sqlUnsigned(row[2]);
p.nCount = sqlUnsigned(row[3]);
p.qNumInsert = sqlUnsigned(row[4]);
p.qBaseInsert = sqlSigned(row[5]);
p.tNumInsert = sqlUnsigned(row[6]);
p.tBaseInsert = sqlSigned(row[7]);
p.strand[0] = row[8][0];
p.qName = row[9];
p.qSize = sqlUnsigned(row[10]);
p.qStart = sqlUnsigned(row[11]);
p.qEnd = sqlUnsigned(row[12]);
p.tName = row[13];
p.tSize = sqlUnsigned(row[14]);
p.tStart = sqlUnsigned(row[15]);
p.tEnd = sqlUnsigned(row[16]);
p.blockCount = sqlUnsigned(row[17]);
return &p;
}

void findPslTails(struct psl *psl, int *retStartTail, int *retEndTail)
/* Find the length of "tails" (rather than extensions) implied by psl. */
{
int qFloppyStart, qFloppyEnd;
int tFloppyStart, tFloppyEnd;

if (psl->strand[0] == '+')
    {
    qFloppyStart = psl->qStart;
    qFloppyEnd = psl->qSize - psl->qEnd;
    }
else
    {
    qFloppyStart = psl->qSize - psl->qEnd;
    qFloppyEnd = psl->qStart;
    }
tFloppyStart = psl->tStart;
tFloppyEnd = psl->tSize - psl->tEnd;
*retStartTail = min(qFloppyStart, tFloppyStart);
*retEndTail = min(qFloppyEnd, tFloppyEnd);
}


struct bConnect *newConnect()
/* Allocate a fresh bConnect. */
{
return lmAlloc(lm, sizeof(struct bConnect));
}

int calcMilliBad(struct psl *psl, boolean isMrna)
/* Calculate badness in parts per thousand. */
{
int qAliSize, tAliSize, aliSize;
int milliBad;
int sizeDif;
int insertFactor;

qAliSize = psl->qEnd - psl->qStart;
tAliSize = psl->tEnd - psl->tStart;
aliSize = min(qAliSize, tAliSize);
sizeDif = qAliSize - tAliSize;
if (sizeDif < 0)
    {
    if (isMrna)
	sizeDif = 0;
    else
	sizeDif = -sizeDif;
    }
insertFactor = psl->qNumInsert;
if (!isMrna)
    insertFactor += psl->tNumInsert;

milliBad = (1000 * (psl->misMatch + insertFactor + round(log(1+sizeDif)))) / aliSize;
return milliBad;
}

struct bConnect *createConnect(struct psl *psl)
/* Create connection based on psl. */
{
char acc[128];
struct bConnect *bc;
struct hashEl *hel;
struct gClone *clone;
int qSize, tSize, sizeDiff;
int milliBad;
int score;
int startTail, endTail;

fragToCloneName(psl->tName, acc);
if ((hel = hashLookup(accHash, acc)) == NULL)
    {
    warn("Can't find clone %s in map\n", acc);
    return NULL;
    }
clone = hel->val;
if (clone->unmasked)
    {
    logIt("Filtering out bConnect %s to unmasked %s\n", psl->qName, clone->acc);
    return NULL;
    }

/* Filter out so-so matches. */
if (psl->match < 40)
    return NULL;
milliBad = calcMilliBad(psl, FALSE);
if (milliBad > 60)
    return NULL;
if (psl->match + psl->misMatch < 100)
    return NULL;
findPslTails(psl, &startTail, &endTail);
score = round(25.0*log(1.0+psl->match) + log(1+psl->repMatch)) 
	- 15*milliBad - 4*(startTail+endTail);
if (score < -500)
    return NULL;

bc = newConnect();
bc->clone = clone;
bc->score = score;
bc->startTail = startTail;
bc->endTail = endTail;
bc->match = psl->match;
bc->repMatch = psl->repMatch;
bc->misMatch = psl->misMatch;
qSize = psl->qEnd - psl->qStart;
tSize = psl->tEnd - psl->tStart;
if ((sizeDiff = qSize-tSize) < 0)
    sizeDiff = -sizeDiff;
bc->sizeDiff = sizeDiff;
bc->numInserts = psl->blockCount-1;
bc->tFrag = hashStoreName(tFragHash, psl->tName);
bc->tStart = psl->tStart;
bc->tEnd = psl->tEnd;
return bc;
}

void readPsl(char *fileName)
/* Read in psl */
{
struct lineFile *in = pslFileOpen(fileName);
int lineSize, wordCount;
char *line, *words[18];
struct psl *psl;
struct bConnect *bc = NULL;
int bcCount = 0;
int pslCount = 0;
char *bacEndAcc;
struct hashEl *hel;
struct bacEndPair *bep;
int dotMaxMod = 10000;
int dotMod = dotMaxMod;

status("Processing %s", fileName);
fflush(stdout);
while (lineFileNext(in, &line, &lineSize))
    {
    if (--dotMod <= 0)
	{
	status(".");
	fflush(stdout);
	dotMod = dotMaxMod;
	}
    wordCount = chopLine(line, words);
    if (wordCount != 18)
	errAbort("Expecting at least 18 words line %d of %s\n", in->lineIx, in->fileName);
    psl = pslShortStatic(words);
    ++pslCount;
    hel = hashLookup(bepHash, psl->qName);
    if (hel != NULL)
	{
	bep = hel->val;
	bc = createConnect(psl);
	if (bc != NULL)
	    {
	    ++bcCount;
	    bc->bacEnd = hel->name;
	    bacEndAcc = psl->qName;
	    if (hel->name == bep->a)
		{
		slAddHead(&bep->aList, bc);
		}
	    else
		{
		assert(hel->name == bep->b);
		slAddHead(&bep->bList, bc);
		}
	    }
	}
    }
status("\nGot %d connections from %d psls in %s\n", bcCount, pslCount, fileName);
lineFileClose(&in);
}

bits32 bcTempSig = 0xBEBDBCBB - 2;

void saveBcList(struct bConnect *bcList, FILE *f)
/* Write out list of bConnects to a file. */
{
struct bConnect *bc;
bits32 count;

count = slCount(bcList);
writeOne(f, count);
for (bc = bcList; bc != NULL; bc = bc->next)
    {
    writeString(f, bc->clone->acc);
    writeOne(f, bc->score);
    writeOne(f, bc->startTail);
    writeOne(f, bc->endTail);
    writeOne(f, bc->match);
    writeOne(f, bc->repMatch);
    writeOne(f, bc->misMatch);
    writeOne(f, bc->sizeDiff);
    writeOne(f, bc->numInserts);
    writeString(f, bc->tFrag);
    writeOne(f, bc->tStart);
    writeOne(f, bc->tEnd);
    }
}

struct bConnect *readBcList(FILE *f, char *bacEnd)
/* Read list of bConnects from file. */
{
struct bConnect *bcList = NULL, *bc;
bits32 count;
bits32 i;
char s[256];
struct hashEl *hel;

mustReadOne(f, count);
for (i=0; i<count; ++i)
    {
    bc = newConnect();
    fastReadString(f, s);
    if ((hel = hashLookup(accHash, s)) == NULL)
	errAbort("Can't find clone %s while reading from preprocessed file\n", s);
    bc->clone = hel->val;
    bc->bacEnd = bacEnd;
    mustReadOne(f, bc->score);
    mustReadOne(f, bc->startTail);
    mustReadOne(f, bc->endTail);
    mustReadOne(f, bc->match);
    mustReadOne(f, bc->repMatch);
    mustReadOne(f, bc->misMatch);
    mustReadOne(f, bc->sizeDiff);
    mustReadOne(f, bc->numInserts);
    fastReadString(f, s);
    bc->tFrag = hashStoreName(tFragHash, s);
    mustReadOne(f, bc->tStart);
    mustReadOne(f, bc->tStart);
    slAddHead(&bcList, bc);
    }
slReverse(&bcList);
return bcList;
}

struct bConnect *bestBcForClone(struct bConnect *bcList, struct gClone *clone)
/* Return the bConnect on list that has highest scoring connection to
 * clone. */
{
struct bConnect *bc;
int bestScore = -0x3fffffff;
struct bConnect *bestConnect = NULL;

for (bc = bcList; bc != NULL; bc = bc->next)
    {
    if (bc->clone == clone && bc->score > bestScore)
	{
	bestScore = bc->score;
	bestConnect = bc;
	}
    }
return bestConnect;
}

void nullOutSecondBest(struct bConnect *bcList, 
	struct gClone *clone, struct bConnect *best)
/* Null out clone field on all members of bcList that connect to clone
 * except for  the best. */
{
struct bConnect *bc;
for (bc = bcList; bc != NULL; bc = bc->next)
    if (bc->clone == clone && bc != best)
	bc->clone = NULL;
}
    
struct bConnect *filterBcList(struct bConnect *oldList)
/* Filter out members of bcList that point same clone. (Just
 * retain best of these.) */
{
struct bConnect *bc, *newList = NULL, *next, *best ;
struct gClone *clone;

for (bc = oldList; bc != NULL; bc = bc->next)
    {
    if ((clone = bc->clone) != NULL)
	{
	best = bestBcForClone(bc, clone);
	nullOutSecondBest(bc, clone, best);
	}
    }
for (bc = oldList; bc != NULL; bc = next)
    {
    next = bc->next;
    if (bc->clone != NULL)
	{
	slAddHead(&newList, bc);
	}
    }
slReverse(&newList);
return newList;
}


void savePreProc(char *fileName)
/* Save preprocessed file so I don't get so impatient during development. */
{
struct bacEndPair *bep;
FILE *f = mustOpen(fileName, "wb");
bits32 count = slCount(bepList);

writeOne(f, bcTempSig);
writeOne(f, count);
for (bep = bepList; bep != NULL; bep = bep->next)
    {
    writeString(f, bep->a);
    saveBcList(bep->aList, f);
    saveBcList(bep->bList, f);
    }
fclose(f);
}

void readPreProc(char *fileName)
/* Read preprocessed file. */
{
struct bacEndPair *bep;
FILE *f = mustOpen(fileName, "rb");
bits32 sig, count;
bits32 i;
char s[256];
struct hashEl *hel;
int tot = 0;

mustReadOne(f, sig);
if (sig != bcTempSig)
    errAbort("%s isn't a valid gapPreProc file\n", fileName);
mustReadOne(f, count);
for (i=0; i<count; ++i)
    {
    fastReadString(f, s);
    if ((hel = hashLookup(bepHash, s)) == NULL)
	{
	errAbort("Couldn't find %s while reading %s\n", s, fileName);
	}
    bep = hel->val;
    bep->aList = readBcList(f, bep->a);
    tot += slCount(bep->aList);
    bep->bList = readBcList(f, bep->b);
    tot += slCount(bep->bList);
    }
status("Read %d beps and %d bConnects in %s\n", (int)count, tot, fileName);
}

int realConn(struct bacEndPair *bep)
/* Return TRUE if bep could possibly connect anything. */
{
bool gotFirst = FALSE;
if (bep->aList != NULL)
    {
    if (bep->aList->next != NULL)
	return TRUE;
    gotFirst = TRUE;
    }
if (bep->bList != NULL)
    {
    if (gotFirst)
	return TRUE;
    if (bep->bList->next != NULL)
	return TRUE;
    }
return FALSE;
}

struct bacEndPair *filterBepList(struct bacEndPair *oldList)
/* Return only bits of bepList that are significant. */
{
struct bacEndPair *newList = NULL, *bep, *next;

for (bep = oldList; bep != NULL; bep = next)
    {
    next = bep->next;
    bep->aList = filterBcList(bep->aList);
    bep->bList = filterBcList(bep->bList);
    if (realConn(bep))
	{
	slAddHead(&newList, bep);
	}
    }
slReverse(&newList);
return newList;
}

char *joinedName(char *a, char *b)
/* Return unique name for a+b.  Doesn't matter order of a & b. */
{
static char buf[256];
int len;

if (strcmp(a,b) < 0)
    {
    char *temp;
    temp = a;
    a = b;
    b = temp;
    }
len = strlen(a);
memcpy(buf, a, len);
buf[len] = '&';
strcpy(buf+len+1, b);
return buf;
}

float scoreToProb(int score)
/* Convert from score -500 to 200 or so to a probability. */
{
static float minProb = 0.9;
static float maxProb = 1.0;
static float probRange = 0.1;
static int minScore = -500;
static int maxScore = 200;
int scoreRange = maxScore - minScore;
float prob;

if (score < -500)
    return minProb;
if (score > 200)
    return maxProb;
prob = minProb + probRange*((float)(score-minScore)/scoreRange);
assert(prob >= 0.0 && prob <= 1.00);
return prob;
}

void cpProbs(struct connectPair *cp, float *retA, float *retB)
/* Calc probs associated with a and b side of cp. */
{
float a, b;
struct bacEndPair *bep;

bep = cp->bep;
a = scoreToProb(cp->a->score) * 3.0/(2.0+slCount(bep->aList));
b = scoreToProb(cp->b->score) * 3.0/(2.0+slCount(bep->bList));
*retA = a;
*retB = b;
}


float combinedCpProb(struct connectPair *cpList, double placeFactor)
/* Return probability of multiple connections being "real" */
{
float cumProbNot = 1.0;		/* Prob joined clones not ok. */
float probOne, probNotOne;
struct connectPair *cp;
boolean first = TRUE;
struct bacEndPair *bep;

for (cp = cpList; cp != NULL; cp = cp->next)
    {
    bep = cp->bep;
    if (first)
	{
	float aP, bP;
	cpProbs(cp, &aP, &bP);
	probOne = 0.75 * aP * bP;
	probNotOne = 1.0 - probOne;
	}
    else
	{
	probNotOne = (double)slCount(bep->bList)*placeFactor;
	}
    cumProbNot *= probNotOne;
    first = FALSE;
    }
return 1.0 - cumProbNot;
}

float joinedClonesProb(struct joinedClones *jc)
/* Figure probability that joined clones are ok. */
{
return combinedCpProb(jc->cpList, 1.0/20000);
}

int cmpJoinedClones(const void *va, const void *vb)
/* Compare two joinedClones to sort by default position . */
{
const struct joinedClones *a = *((struct joinedClones **)va);
const struct joinedClones *b = *((struct joinedClones **)vb);
int d;
float diff;

d = slCount(b->cpList) - slCount(a->cpList);
if (d != 0)
    return d;
diff = b->probOk - a->probOk;
if (diff == 0.0)
    return 0;
else if (diff < 0)
    return -1;
else
    return 1;
}

int cmpJoinedBarges(const void *va, const void *vb)
/* Compare two joinedBarges to sort by default position . */
{
const struct joinedBarges *a = *((struct joinedBarges **)va);
const struct joinedBarges *b = *((struct joinedBarges **)vb);
int d;
float diff;

d = slCount(b->cpList) - slCount(a->cpList);
if (d != 0)
    return d;
diff = b->probOk - a->probOk;
if (diff == 0.0)
    return 0;
else if (diff < 0)
    return -1;
else
    return 1;
}

boolean inCpList(struct connectPair *cpList, struct bacEndPair *bep)
/* Return TRUE if cpList already has a member pointing to bep. */
{
struct connectPair *cp;
for (cp = cpList; cp != NULL; cp = cp->next)
    if (cp->bep == bep)
	return TRUE;
return FALSE; 
}

void connectClones(struct bConnect *a, struct bConnect *b, struct bacEndPair *bep)
/* Form a connection between two clones .*/
{
struct connectPair *cp;
struct joinedClones *jc;
char *jcName;
struct hashEl *hel;

if (a->clone == b->clone)
    return;	/* Hey, that was easy... */
jcName = joinedName(a->clone->acc, b->clone->acc);
if ((hel = hashLookup(joinedClonesHash, jcName)) == NULL)
    {
    AllocVar(jc);
    slAddHead(&joinedClonesList, jc);
    hel = hashAdd(joinedClonesHash, jcName, jc);
    jc->name = hel->name;
    jc->a = a->clone;
    jc->b = b->clone;
    }
else
    jc = hel->val;
if (!inCpList(jc->cpList, bep))
    {
    AllocVar(cp);
    cp->bep  = bep;
    cp->a = a;
    cp->b = b;
    slAddHead(&jc->cpList, cp);
    }
}

void nailClones(struct bConnect *bcList, struct bacEndPair *bep)
/* Nail together all clones in bcList. */
{
}

void dumpBep(struct bacEndPair *bep, FILE *f)
/* Print out info on bep to file. */
{
struct bConnect *bc;

fprintf(f, "%s&%s at %s\n", bep->a, bep->b, bep->clone);
fprintf(f, " aList\n");
for (bc = bep->aList; bc != NULL; bc = bc->next)
    {
    fprintf(f, "   %s %d tails %d %d match %d repMatch %d misMatch %d sizeDiff %d inserts %d\n",
	bc->clone->acc, bc->score, bc->startTail, bc->endTail, bc->match, bc->repMatch,
	bc->misMatch, bc->sizeDiff, bc->numInserts);
    }
fprintf(f, " bList\n");
for (bc = bep->bList; bc != NULL; bc = bc->next)
    {
    fprintf(f, "   %s %d tails %d %d match %d repMatch %d misMatch %d sizeDiff %d inserts %d\n",
	bc->clone->acc, bc->score, bc->startTail, bc->endTail, bc->match, bc->repMatch,
	bc->misMatch, bc->sizeDiff, bc->numInserts);
    }
}

void bridgeClones(struct bacEndPair *bep)
/* Bridge together clone pair connected by opposite
 * sides of bep. */
{
struct bConnect *aBc, *bBc;
int aCount = slCount(bep->aList);
int bCount = slCount(bep->bList);
int product = aCount*bCount;

if (aCount >= 10 || bCount >= 10)
    return;	/* Repeats! */

logIt("Bridging %s (%d) %s (%d)\n", bep->a, aCount, bep->b, bCount);
if (product > 25)
    {
    logIt("[warning] Making %d bridges with pair %s and %s", product, bep->a, bep->b);
    dumpBep(bep, logFile);
    }

for (aBc = bep->aList; aBc != NULL; aBc = aBc->next)
    {
    for (bBc = bep->bList; bBc != NULL; bBc = bBc->next)
	{
	connectClones(aBc, bBc, bep);
	}
    }
}

void joinClones()
/* Join together clones. */
{
struct bacEndPair *bep;
int count;
struct joinedClones *jc;
int doubleCount = 0;



joinedClonesHash = newHash(18);
for (bep = bepList, count=1; bep != NULL; bep = bep->next,++count)
    {
    if (count<10)
	{
	logIt("~~~join clones~~~\n");
	dumpBep(bep, logFile);
	}
    nailClones(bep->aList, bep);
    nailClones(bep->bList, bep);
    bridgeClones(bep);
    }
status("Got %d joinedClones\n", slCount(joinedClonesList));

for (jc = joinedClonesList; jc != NULL; jc = jc->next)
    {
    jc->probOk = joinedClonesProb(jc);    
    if (slCount(jc->cpList) > 1)
	++doubleCount;
    }
slSort(&joinedClonesList, cmpJoinedClones);
status("Got %d clones joined by two or more BAC end  pairs\n", 
	doubleCount);
}

void writeClonePairs(struct joinedClones *jcList, char *fileName)
/* Write out clone pairs to file. */
{
FILE *f = mustOpen(fileName, "w");
struct joinedClones *jc;
struct connectPair *cp;
struct bacEndPair *bep;
float aP, bP;

for (jc = jcList; jc != NULL; jc = jc->next)
    {
    fprintf(f, "%s\t%f\t%d\n", jc->name, jc->probOk, slCount(jc->cpList));
    for (cp = jc->cpList; cp != NULL; cp = cp->next)
	{
	bep = cp->bep;
	cpProbs(cp, &aP, &bP);
	fprintf(f, "\t%s\t%4.3f\t%s\t%4.3f\t%s\n",
	    bep->a, aP, bep->b, bP, bep->clone);
	}
    }
fclose(f);
}

void writeBep(struct bacEndPair *bepList, char *fileName)
/* Save out bac end pairs in text format */
{
struct bacEndPair *bep;
struct bConnect *bc;
FILE *f = mustOpen(fileName, "w");

for (bep = bepList; bep  != NULL; bep = bep->next)
    {
    fprintf(f, "%s %s %s\n", bep->a, bep->b, bep->clone);
    for (bc = bep->aList; bc != NULL; bc = bc->next)
	fprintf(f, "  %s %s %d tails %d %d match %d %d mis %d szDf %d ins %d tPos %d %d\n", 
		bep->a, bc->clone->acc, bc->score,
		bc->startTail, bc->endTail, bc->match, bc->repMatch,
		bc->misMatch, bc->sizeDiff, bc->numInserts,
		bc->tStart, bc->tEnd);
    if (bep->aList != NULL && bep->bList != NULL)
	fprintf(f, "\n");
    for (bc = bep->bList; bc != NULL; bc = bc->next)
	fprintf(f, "  %s %s %d tails %d %d match %d %d mis %d szDf %d ins %d tPos %d %d\n", 
		bep->b, bc->clone->acc, bc->score,
		bc->startTail, bc->endTail, bc->match, bc->repMatch,
		bc->misMatch, bc->sizeDiff, bc->numInserts,
		bc->tStart, bc->tEnd);
    }
fclose(f);
}

struct lineFile *bargeFileOpen(char *fileName)
/* Open up barge file and read header. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize;
char *line;
int i;

if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s is empty", fileName);
if (!startsWith("Barge",  line))
    {
    errAbort("%s is not a barge file", fileName);
    }
for (i=0; i<3; ++i)
    {
    if (!lineFileNext(lf, &line, &lineSize))
	errAbort("%s is truncated", fileName);
    }
return lf;
}

void readOneBargeFile(char *fileName, char *chrom, char *ctg)
/* Read in one barge file. */
{
struct gBarge *barge = NULL;
struct gBargeRef *bargeRef;
struct gContig *contig;
int lineSize, wordCount;
char *line, *words[16];
struct hashEl *hel;
char bargeName[64];
int bargeIx = 0;
char *cloneName;
struct gClone *clone;
struct gCloneRef *cloneRef;
struct lineFile *lf;

if ((hel = hashLookup(contigHash, ctg)) == NULL)
    {
    warn("Can't find contig %s in map", ctg);
    return;
    }
lf = bargeFileOpen(fileName);
contig = hel->val;
while (lineFileNext(lf, &line, &lineSize))
    {
    if (barge == NULL)
	{
	AllocVar(barge);
	barge->ix = ++bargeIx;
	sprintf(bargeName, "%s/%s.%d", chrom, ctg, bargeIx);
	barge->name = cloneString(bargeName);
	barge->contig = contig;
	slAddHead(&bargeList, barge);
	AllocVar(bargeRef);
	bargeRef->barge = barge;
	slAddHead(&contig->bargeList, bargeRef);
	}
    if (startsWith("-----", line))
	{
	if (strstr(line, "bridged"))
	    barge->bridgedToNext = TRUE;
	barge = NULL;
	}
    else
	{
	wordCount = chopLine(line, words);
	if (wordCount != 4 && wordCount != 6)
	    errAbort("Expecting four words line %d of %s", lf->lineIx, lf->fileName);
	cloneName = words[1];
	if ((hel = hashLookup(accHash, cloneName)) == NULL)
	    warn("%s is in %s but not in map", cloneName, fileName);
	else
	    {
	    clone = hel->val;
	    clone->barge = barge;
	    AllocVar(cloneRef);
	    cloneRef->clone = clone;
	    slAddTail(&barge->cloneList, cloneRef);
	    }
	}
    }
lineFileClose(&lf);
slReverse(&contig->bargeList);
}

void readBarges(char *ooDir, char *version)
/* Read in barges from ooDir. */
{
struct slName *chromList, *chromEl, *ctgList, *ctgEl;
char *chromName, *ctgName;
char path[512];
int len;
int singletonCount = 0;
struct gBarge *barge;

status("Reading barges from %s\n", ooDir);
chromList = listDir(ooDir, "*");
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chromName = chromEl->name;
    len = strlen(chromName);
    if (len == 1 || len == 2)
	{
	sprintf(path, "%s/%s", ooDir, chromName);
	ctgList = listDir(path, "ctg*");
	for (ctgEl = ctgList; ctgEl != NULL; ctgEl = ctgEl->next)
	    {
	    ctgName = ctgEl->name;
	    sprintf(path, "%s/%s/%s/barge.%s", ooDir, chromName, ctgName, version);
	    if (fileExists(path))
		readOneBargeFile(path, chromName, ctgName);
	    else
		logIt("[warning]%s doesn't exist\n", path);
	    }
	}
    }
slReverse(&bargeList);

for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    if (barge->cloneList->next == NULL)
	++singletonCount;
    }
status("Read %d barges from %s including %d singletons\n", slCount(bargeList), ooDir,
	singletonCount);
}

int bargeUses(struct gBarge *barge)
/* Count the number of times barge is connected not including 
 * self connection. */
{
struct joinedBarges *jb;
int useCount = 0;

for (jb = joinedBargesList; jb != NULL; jb = jb->next)
    {
    if (jb->a != jb->b)
	{
	if (barge == jb->a  || barge == jb->b)
	    {
	    if (jb->cpList->next != NULL)
		{
		++useCount;
		}
	    }
	}
    }
return useCount;
}

void countDoubles(struct gBarge *bargeList)
/* Fill in doubleConnCount of all barges in list. */
{
struct gBarge *barge;
for (barge = bargeList; barge != NULL; barge = barge->next)
    barge->doubleConnCount = bargeUses(barge);
}


void writeOneBridge(struct gBarge *aBarge, struct gBarge *bBarge, FILE *f)
/* Write out bridges if any between aBarge and bBarge */
{
int aCloneCount;
int aIx;
struct gCloneRef *aRef, *bRef;
struct gClone *aClone, *bClone;
char *jcName;
struct hashEl *hel;
struct joinedClones *jc;
struct connectPair *cp;
struct hash *uniq = newHash(8);
int becCount = 0;

fprintf(f, "%s/%s barge %d (used %d clones %d) and %s/%s barge %d (used %d clones %d):\n",
    aBarge->contig->chrom->name, aBarge->contig->name, aBarge->ix, 
    aBarge->doubleConnCount, slCount(aBarge->cloneList),
    bBarge->contig->chrom->name, bBarge->contig->name, bBarge->ix, 
    bBarge->doubleConnCount, slCount(bBarge->cloneList));
aCloneCount = slCount(aBarge->cloneList);
for (aIx = aCloneCount-1; aIx >= 0; --aIx)
    {
    aRef = slElementFromIx(aBarge->cloneList, aIx);
    aClone = aRef->clone;
    for (bRef = bBarge->cloneList; bRef != NULL; bRef = bRef->next)
	{
	bClone = bRef->clone;
	jcName = joinedName(aClone->acc, bClone->acc);
	if ((hel = hashLookup(joinedClonesHash, jcName)) != NULL)
	    {
	    jc = hel->val;
	    for (cp = jc->cpList; cp != NULL; cp = cp->next)
		{
		struct bacEndPair *bep = cp->bep;
		struct bConnect *ac, *bc;
		char bepName[16];
		sprintf(bepName, "%p", bep);
		if (!hashLookup(uniq, bepName))
		    {
		    ac = cp->a;
		    bc = cp->b;
		    hashAdd(uniq, bepName, bep);
		    fprintf(f, " %s %s '%s' %s@%d %s@%d\n", 
		    	bep->a, bep->b, bep->clone, 
			ac->tFrag, (ac->tStart+ac->tEnd)/2,
			bc->tFrag, (bc->tStart+bc->tEnd)/2);
		    ++becCount;
		    }
		}
	    }
	}
    }
freeHash(&uniq);
}


int writeEasyBridges(struct gBarge *bargeList, char *fileName)
/* Write out bridges between barges that are bridged by ooGreedy. */
{
struct gBarge *aBarge;
FILE *f = mustOpen(fileName, "w");
int count = 0;

for (aBarge = bargeList; aBarge != NULL; aBarge = aBarge->next)
    {
    if (aBarge->bridgedToNext)
	{
	assert(aBarge->contig == aBarge->next->contig);
	writeOneBridge(aBarge, aBarge->next, f);
	++count;
	}
    }
fclose(f);
return count;
}

void joinBarges()
/* Join together connected barges */
{
struct joinedBarges *jb;
char *jbName;
struct joinedClones *jc;
struct gBarge *bargeA, *bargeB;
struct hashEl *hel;
struct connectPair *cp;

joinedBargesHash = newHash(16);
for (jc = joinedClonesList; jc != NULL; jc = jc->next)
    {
    bargeA = jc->a->barge;
    bargeB = jc->b->barge;
    if (bargeA == NULL || bargeB == NULL)
	warn("Joining clones that aren't part of barge %s (%p) and %s (%p)",
	    jc->a->acc, bargeA, jc->b->acc, bargeB);
    else
	{
	jbName = joinedName(bargeA->name, bargeB->name);
	if ((hel = hashLookup(joinedBargesHash, jbName)) == NULL)
	    {
	    AllocVar(jb);
	    hel = hashAdd(joinedBargesHash, jbName, jb);
	    jb->name = hel->name;
	    jb->a = bargeA;
	    jb->b = bargeB;
	    slAddHead(&joinedBargesList, jb);
	    }
	else
	    jb = hel->val;
	for (cp = jc->cpList; cp != NULL; cp = cp->next)
	    {
	    struct bacEndPair *bep = cp->bep;
	    if (!inCpList(jb->cpList, bep))
		{
		struct connectPair *newCp;
		AllocVar(newCp);
		*newCp = *cp;
		slAddHead(&jb->cpList, newCp);
		}
	    }
	}
    }
status("Got %d joined barges\n", slCount(joinedBargesList));
slReverse(&joinedBargesList);

for (jb = joinedBargesList; jb != NULL; jb = jb->next)
    {
    int aSize = slCount(jb->a->cloneList);
    int bSize = slCount(jb->b->cloneList);
    float maxSize = max(aSize, bSize);
    jb->probOk = combinedCpProb(jb->cpList, maxSize/20000);
    }
slSort(&joinedBargesList, cmpJoinedBarges);
}

int writeHardBridges(struct joinedBarges *jbList, char *fileName, boolean singles)
/* Write out bridges between barges (that are not already written out) */
{
struct joinedBarges *jb;
FILE *f = mustOpen(fileName, "w");
struct gBarge *a,*b;
int count = 0;

for (jb = jbList; jb != NULL; jb = jb->next)
    {
    a = jb->a;
    b = jb->b;
    if (a != b)
	{
	if (!((a->bridgedToNext && a->next == b) ||
	    (b->bridgedToNext && b->next == a)))
	    {
	    int cpCount = slCount(jb->cpList);
	    if ((singles && cpCount == 1) || (!singles && cpCount != 1))
		{
		writeOneBridge(a, b, f);
		++count;
		}
	    }
	}
    }
fclose(f);
return count;
}

int writeSelfBridges(struct joinedBarges *jbList, char *fileName)
/* Write out bridges between clones in same barge. */
{
struct joinedBarges *jb;
FILE *f = mustOpen(fileName, "w");
struct gBarge *a,*b;
int selfCount = 0;

for (jb = jbList; jb != NULL; jb = jb->next)
    {
    a = jb->a;
    b = jb->b;
    if (a == b)
	{
	writeOneBridge(a, b, f);
	++selfCount;
	}
    }
fclose(f);
return selfCount;
}

void makeCondorFile(char *conName)
{
warn("makeCondorFile not implemented");
}

#ifdef OLD_KLUDGE
void checkMaskedClones()
/* Check that clone files are masked. */
{
char *host = getenv("HOST");
if (host != NULL && sameWord(host, "Crunx"))
    {
    struct gClone *clone;
    warn("Skipping checkMaskedClones on %s", host);
    }
else
    {
    char **words, *buf;
    int wordCount;
    int i;
    char *acc;
    struct hashEl *hel;
    struct gClone *clone;
    char *uname = "/projects/compbio/experiments/hg/gs.1/unmaskedAcc";

    status("Hiding unmasked\n");
    if (!fileExists(uname))
	warn("Couldn't find %s", uname);
    else
	{
	readAllWords(uname, &words, &wordCount, &buf);
	for (i=0; i<wordCount; ++i)
	    {
	    acc = words[i];
	    if ((hel = hashLookup(accHash, acc)) == NULL)
		warn("Couldn't find unmasked %s", acc);
	    else
		{
		clone = hel->val;
		clone->unmasked = TRUE;
		}
	    }
	freeMem(buf);
	}
    }
}
#endif /* OLD_KLUDGE */

int countClonesWithAccession(struct gClone *cloneList)
/* Count number of clones in list with accession. */
{
struct gClone *clone;
int count = 0;
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->acc != NULL)
	++count;
    }
return count;
}

enum fuzzy {yes=1, no=0, maybe=-1};

int countAdjacent(int minConn, int maxUse, int minBargeSize,
	enum fuzzy selfOnly, enum fuzzy intraOnly, enum fuzzy inChromOnly,
	enum fuzzy ulNaOnly, int maxMapDif)
/* Count connections between barges in a number of ways including
 * whether are ordered and adjacent in map. */
{
struct joinedBarges *jb;
struct gBarge *a, *b;
char *acName, *bcName;
int count = 0;
bool self, intra, inChrom, ulNa;

for (jb = joinedBargesList; jb != NULL; jb = jb->next)
    {
    a = jb->a;
    b = jb->b;
    if (slCount(jb->cpList) >= minConn && 
	a->doubleConnCount <= maxUse && b->doubleConnCount <= maxUse && 
	slCount(a->cloneList) >= minBargeSize &&
	slCount(b->cloneList) >= minBargeSize)
	{
	self = (a == b);
	intra = (a->contig == b->contig);
	inChrom = (a->contig->chrom == b->contig->chrom);
	acName = a->contig->chrom->name;
	bcName = b->contig->chrom->name;
	ulNa = (sameWord(acName, "UL") || sameWord(bcName, "UL") ||
	       sameWord(acName, "NA") || sameWord(bcName, "NAUL"));
	     
	if (selfOnly == yes && !self)
	    continue;
	if (selfOnly == no && self)
	    continue;
	if (intraOnly == yes && !intra)
	    continue;
	if (intraOnly == no && intra)
	    continue;
	if (inChromOnly == yes && !inChrom)
	    continue;
	if (inChromOnly == no && inChrom)
	    continue;
	if (ulNaOnly == yes && !ulNa)
	    continue;
	if (ulNaOnly == no && ulNa)
	    continue;
	if (maxMapDif > 0)
	    {
	    int mapDif;
	    if (!a->contig->isOrdered || !b->contig->isOrdered)
		continue;
	    mapDif = a->contig->orderIx - b->contig->orderIx;
	    if (mapDif < 0) mapDif = -mapDif;
	    if (mapDif > maxMapDif)
		continue;
	    }
	++count;
	}
    }
return count;
}

int countUlNa(int minConn, int maxUse, int minBargeSize,
	enum fuzzy selfOnly, enum fuzzy intraOnly, enum fuzzy inChromOnly,
	enum fuzzy ulNaOnly)
/* Count connections between barges in a number of ways including
 * whether part of UL or NA chromosomes. */
{
return countAdjacent(minConn, maxUse, minBargeSize, 
	selfOnly, intraOnly, inChromOnly, ulNaOnly, 0);
}

int countBargeConns(int minConn, int maxUse, int minBargeSize,
	enum fuzzy selfOnly, enum fuzzy intraOnly, enum fuzzy inChromOnly)
/* Count connections between barges in a number of ways. */
{
return countUlNa(minConn, maxUse, minBargeSize, selfOnly, intraOnly, inChromOnly, maybe);
}

int countBarges(int minSize, enum fuzzy selfOnly)
/* Count barges of given minimum size and self connect status. */
{
int count = 0;
struct gBarge *barge;
char *jbName;
bool self;

for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    if (slCount(barge->cloneList) >= minSize)
	{
	jbName = joinedName(barge->name, barge->name);
	self = (hashLookup(joinedBargesHash, jbName) != NULL);
	if (selfOnly == yes && !self)
	    continue;
	if (selfOnly == no && self)
	    continue;
	++count;
	}
    }
return count;
}

void writeStats(char *fileName)
/* Write summary statistics to file. */
{
FILE *f = mustOpen(fileName, "w");
int i;

fprintf(f, "gapper version %d summary statistics\n\n", version);
fprintf(f, "%5d clones\n", slCount(gCloneList));
fprintf(f, "%5d clones with accessions\n", countClonesWithAccession(gCloneList));
fprintf(f, "%5d connections between clones\n", slCount(joinedClonesList));
fprintf(f, "%5d barges\n", slCount(bargeList));
fprintf(f, "%5d barge connections\n", slCount(joinedBargesList));
fprintf(f, "%5d barge connections of two or more bac end pairs\n", 
	countBargeConns(2,100000,0,maybe,maybe,maybe));
fprintf(f, "%5d barge self connections\n", 
	countBargeConns(2,100000,0,yes,maybe,maybe));
fprintf(f, "%5d barge connections within a contig (not including self connections)\n",
	countBargeConns(2,100000,0,no,yes,maybe));
fprintf(f, "%5d barge connections between contigs within a chromosome\n",
	countBargeConns(2,100000,0,no,no,yes));
fprintf(f, "%5d barge connections between chromosomes\n",
	countUlNa(2,100000,0,no,no,no,no));
fprintf(f, "%5d connections between NA or UL and a barge in another chromosome\n",
	countUlNa(2,100000,0,no,no,no,yes));
fprintf(f, "\n");

fprintf(f, 
   "In the following table connections defined by a single BAC end pair are allowed.\n");
fprintf(f,
   "(In all other tables two or more BAC end pairs are required to connect barges.)\n");
fprintf(f, 
   "----------------------------------------------------------------------------------\n");
for (i=1; i<=10; ++i)
    {
    fprintf(f, "%4d barges with %2d or more clones, %4d with self connections\n", 
	    countBarges(i, maybe), i, countBarges(i, yes));
    }
fprintf(f, 
   "----------------------------------------------------------------------------------\n");
fprintf(f, 
   "Note that BAC end connections to the same clone are filtered out early in the\n"
   "process, which is why you see the same number of self connections in the first\n"
   "two lines above\n");

fprintf(f, "\n");

for (i=1; i<10; ++i)
    {
    fprintf(f, "%4d connections between barges with %d or less uses of each barge\n",
	countBargeConns(2, i, 0, no, maybe, maybe), i);
    }
fprintf(f, "%4d connections between barges total\n",
    countBargeConns(2, 100000, 0, no, maybe, maybe));
fprintf(f, "\n");


for (i=1; i<10; ++i)
    {
    fprintf(f, "%4d cross-chromosome barge connections with %d or less uses of each barge\n",
	countUlNa(2, i, 0, maybe, maybe, no, no), i);
    }
fprintf(f, "%4d cross-chromosome barge connections total\n",
	countUlNa(2, 100000, 0, maybe, maybe, no, no));
fprintf(f, "\n");

for (i=1; i<10; ++i)
    {
    fprintf(f, "%4d intra-chromosomal barge connections within %d on ordered part of map\n",
	countAdjacent(2, 100000, 0, no, no, yes, no, i), i);
    }
fprintf(f, "%4d intra-chromosomal barge connections anywhere on ordered part of map\n",
    countAdjacent(2, 100000, 0, no, no, yes, no, 100000));
fprintf(f, "\n");

fclose(f);
}

void gapper(char *mapName, char *pairName, char *pslName, char *ooDir, 
	char *ooVersion, char *outRoot)
/* gapper - try to find unsequenced gaps in the draft human genome
 * by looking at BAC end pair alignments between clones and
 * between fingerprint contigs. */
{
static char logName[512];
static char preProcName[512];
static char clopName[512];
static char conpName[512];
static char bepName[512];
static char ezName[512];
static char doubleName[512];
static char singleName[512];
static char selfName[512];
static char conName[512];
static char statsName[512];
int count;

sprintf(logName, "%s.log", outRoot);
sprintf(preProcName, "%s.gtmp", outRoot);
sprintf(clopName, "%s.clop", outRoot);
sprintf(conpName, "%s.conp", outRoot);
sprintf(bepName, "%s.bep", outRoot);
sprintf(ezName, "%s.bb", outRoot);
sprintf(doubleName, "%s.double", outRoot);
sprintf(singleName, "%s.single", outRoot);
sprintf(selfName, "%s.self", outRoot);
sprintf(statsName, "%s.stats", outRoot);
sprintf(conName, "%s.con", outRoot);

logFile = mustOpen(logName, "w");
pushWarnHandler(warnHandler);
lm = lmInit(0);
readMap(mapName);
status("Got %d chromosomes %d contigs %d clones in %s\n",
    slCount(gChromList), slCount(gContigList), slCount(gCloneList), mapName);
readPairs(pairName);
status("Got %d BAC end pairs\n", slCount(bepList));
if (!fileExists(preProcName))
    {
    // checkMaskedClones();
    readPsl(pslName);
    bepList = filterBepList(bepList);
    status("Saving %s\n", preProcName);
    savePreProc(preProcName);
    status("Saving %s\n", bepName);
    writeBep(bepList, bepName);
    }
else
    {
    readPreProc(preProcName);
    bepList = filterBepList(bepList);
    }
readBarges(ooDir, ooVersion);
status("Joining clones\n");
joinClones();
writeClonePairs(joinedClonesList, clopName);
count = writeEasyBridges(bargeList, ezName);
status("wrote %d intra-contig bridges\n", count);
joinBarges();
status("Counting doubles\n");
countDoubles(bargeList);
count = writeHardBridges(joinedBargesList, doubleName, FALSE);
status("wrote %d double bridges\n", count);
count = writeHardBridges(joinedBargesList, singleName, TRUE);
status("wrote %d single bridges\n", count);
count = writeSelfBridges(joinedBargesList, selfName);
status("Wrote %d self bridges\n", count);
writeStats(statsName);
makeCondorFile(conName);
lmCleanup(&lm);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
"gapper - try to find unsequenced gaps in the draft human genome\n"
"by looking at BAC end pair alignments between clones and\n"
"between fingerprint contigs.\n"
"usage:\n"
"   gapper mapFile pairFile bacEnds.psl ooDir ooVersion outRoot\n"
"This will look at the WashU style fingerprint map file in mapFile\n"
"the list of BAC end pairs in pairFile and the bacEnd/clone alignments\n"
"in bacEnd.psl, analyze them, and put the results in a series of files\n"
"named outRoot.xxx, where xxx includes:\n"
"    .bep  - pairs of clones connected by a single bac end\n");
}

int main(int argc, char *argv[])
/* Analyse command line and call main processing routines. */
{
if (argc != 7)
    usage();
gapper(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
