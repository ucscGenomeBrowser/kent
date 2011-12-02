/* ppWashU - process Wash U paired reads. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "dnautil.h"
#include "fa.h"
#include "qaSeq.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ppWashU - process Wash U paired reads\n"
  "usage:\n"
  "   ppWashU dataDir washu.pairs washu.fa\n");
}

struct seqPair
/* A pair of sequences on same template. */
    {
    struct seqPair *next;
    struct qaSeq *fRead;
    struct qaSeq *rRead;
    };

void seqPairFree(struct seqPair **pSp)
/* Free a single seqPair. */
{
struct seqPair *sp = *pSp;
if (sp != NULL)
    {
    qaSeqFree(&sp->fRead);
    qaSeqFree(&sp->rRead);
    freez(pSp);
    }
}

void seqPairFreeList(struct seqPair **pList)
/* Free a list of seqPairs. */
{
struct seqPair *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    seqPairFree(&el);
    }
*pList = NULL;
}

void parseWashuReadName(char *name, char *retTemplate, boolean *retIsRev, int *retVersion)
/* Parse WashU formatted read name. */
{
char *s = strchr(name, '.');
char c;
int len;

if (s == NULL)
    errAbort("Expecting dot in %s", name);
len = s-name;
if (len > 63)
    errAbort("Name %s too long", name);
memcpy(retTemplate, name, len);
retTemplate[len] = 0;
s += 1;
c = *s++;
if (c == 'f' || c == 's' || c == 'x' || c == 'z' || c == 'i' || c == 'b')
    *retIsRev = FALSE;
else if (c == 'r' || c == 'y' || c == 'g')
    *retIsRev = TRUE;
else
    errAbort("Unknown type letter after dot in %s", name);
if (!isdigit(s[0]))
    errAbort("Expecting version number in %s", name);
*retVersion = atoi(s);
}

void pairReads(struct qaSeq **pInList, struct seqPair **retPairList, 
    struct qaSeq **retSingleList, struct qaSeq **retDupeList)
/* Pair up *pInList into *retPairList, cannabalizing *pInList in 
 * the process.  Unpaired reads go into *retSingleList.  Reads
 * from previous versions go into *retDupeList. */
{
struct qaSeq *seq, *nextSeq, **pList, *oldSeq;
struct hash *tempHash = newHash(19);
char template[64];
boolean isRev;
int version, oldVer;
struct seqPair *pair, *nextPair, *pairList = NULL;

*retPairList = NULL;
*retSingleList = *retDupeList = NULL;

/* Make pairs. */
for (seq = *pInList; seq != NULL; seq = nextSeq)
    {
    nextSeq = seq->next;
    seq->next = NULL;
    parseWashuReadName(seq->name, template, &isRev, &version);
    if ((pair = hashFindVal(tempHash, template)) == NULL)
        {
	AllocVar(pair);
	hashAdd(tempHash, template, pair);
	slAddHead(&pairList, pair);
	}
    pList = (isRev ? &pair->rRead : &pair->fRead);
    if ((oldSeq = *pList) == NULL)
       *pList = seq;
    else	
        {
	/* Put old version on dupe list and new version on pair->xReads. */
	parseWashuReadName(oldSeq->name, template, &isRev, &oldVer);
	if (version > oldVer)
	    {
	    slAddHead(retDupeList, oldSeq);
	    *pList = seq;
	    }
	else
	    {
	    slAddHead(retDupeList, seq);
	    }
	}
    }

/* Remove singletons from pair list. */
for (pair = pairList; pair != NULL; pair = nextPair)
    {
    nextPair = pair->next;
    if (pair->rRead && pair->fRead)
        {
	slAddHead(retPairList, pair);
	}
    else
        {
	if (pair->rRead)
	    {
	    slAddHead(retSingleList, pair->rRead);
	    }
	else
	    {
	    slAddHead(retSingleList, pair->fRead);
	    }
	freeMem(pair);
	}
    }
freeHash(&tempHash);
}

int tilNextGood(UBYTE *q, int size, int minQual)
/* Return number of bases until next one that meets or exceeds minQual. */
{
int i;

for (i=0; i<size; ++i)
    {
    if (q[i] >= minQual)
	break;
    }
return i;
}

int tilNextBad(UBYTE *q, int size, int minQual)
/* Return number of bases until next one that is less than minQual. */
{
int i;

for (i=0; i<size; ++i)
    {
    if (q[i] < minQual)
	break;
    }
return i;
}

int revTilNextGood(UBYTE *q, int size, int minQual)
/* Return number of bases in reverse direction until next one that meets or exceeds minQual. */
{
int i;

q += size-1;
for (i=0; i<size; ++i)
    {
    if (q[-i] >= minQual)
	break;
    }
return i;
}

int revTilNextBad(UBYTE *q, int size, int minQual)
/* Return number of bases in reverse direction until next one that is less than minQual. */
{
int i;

q += size-1;
for (i=0; i<size; ++i)
    {
    if (q[-i] < minQual)
	break;
    }
return i;
}

int skipX(DNA *dna, int size)
/* Skip over X's if any. */
{
int lastAfterX = 0;
int i;

for (i=0; i<size; ++i)
    {
    if (dna[i] == 'X')
       lastAfterX = i+1;
    }
return lastAfterX;
}


boolean trimQa(struct qaSeq *qa, int minQual, int minQualRun, int *retStart, int *retSize)
/* Find part of sequence that survives trimming according to minQual
 * and qual run specs.  Return FALSE if nothing left after trimming.
 * This trims sequence on both ends until it comes to a run of sequences
 * of length minQualRun or more which meets or exceeds minQual. */
{
int qaSize = qa->size;
UBYTE *q = qa->qa;
int start, end;
int runSize;

for (start = skipX(qa->dna, qaSize); start < qaSize; )
    {
    start += tilNextGood(q+start, qaSize-start, minQual);
    runSize = tilNextBad(q+start, qaSize-start, minQual);
    if (runSize >= minQualRun)
        break;
    start += runSize;
    }
*retStart = start;
if (start >= qaSize)
    {
    *retSize = 0;
    return FALSE;
    }
for (end = qaSize; end > start;)
    {
    runSize = revTilNextGood(q+start, end - start,  minQual);
    end -= runSize;
    runSize = revTilNextBad(q+start, end - start,  minQual);
    if (runSize >= minQualRun)
        break;
    end -= runSize;
    }
*retSize = end - start;
return (end > start);
}


void writeClipped(FILE *f, struct qaSeq *seq)
/* Write out part of sequence that excludes any long runs of N's. */
{
int start, size;
if (trimQa(seq, 19, 15, &start, &size))
    {
    faWriteNext(f, seq->name, seq->dna + start, size);
    }
}

void readMatchingQuals(char *fileName, struct qaSeq *seqList)
/* Fill in qa members of seqList from file name, insuring data
 * matches. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct qaSeq *qs = seqList;
UBYTE *qa;
int size;
char *name;

printf("Processing %s\n", fileName);
while (qaFastReadNext(lf, &qa, &size, &name))
    {
    if (qs == NULL)
        errAbort("More sequences in %s than in corresponding fasta file", fileName);
    if (qs->size != size)
        errAbort("Size mismatch between fasta (%d) and qual (%d) in %s near line %d of %s", 
		qs->size, size, name, lf->lineIx, lf->fileName);
    if (!sameString(name, qs->name))
        errAbort("Name mismatch between fasta (%s) and qual (%s) near line %d of %s", 
		qs->name, name, lf->lineIx, lf->fileName);
    qs->qa = cloneMem(qa, size+1);
    qs = qs->next;
    }
lineFileClose(&lf);
if (qs != NULL)
    errAbort("Less sequences in %s than in corresponding fasta file", fileName);
}

struct qaSeq *readWashSeqs(char *faName, char *qaName)
/* Return a list of qaSeqs read from a WashU format
 * .fa /.qa file.  Allow both X's and N's to pass through. 
 * on .fa side. */
{
struct qaSeq *seqList = NULL, *seq = NULL;
struct lineFile *lf = lineFileOpen(faName, TRUE);
static DNA seqBuf[16*1024];
int seqSize = 0;
int lineSize;
char *line;
boolean atEof;

for (;;)
    {
    atEof = !lineFileNext(lf, &line, &lineSize);
    if (atEof || line[0] == '>')
        {
	if (seq != NULL)
	    {
	    seqBuf[seqSize] = 0;
	    seq->dna = cloneString(seqBuf);
	    seq->size = seqSize;
	    slAddHead(&seqList, seq);
	    }
	if (atEof)
	    break;
	AllocVar(seq);
	seq->name = cloneString(firstWordInLine(line+1));
	seqSize = 0;
	}
    else if (seq != NULL)
        {
	lineSize = strlen(line);
	if (lineSize + seqSize >= sizeof(seqBuf))
	    errAbort("Sequence %s too long, more than %d bases",
	         seq->name,  sizeof(seqBuf));
	memcpy(seqBuf + seqSize, line, lineSize);
	seqSize += lineSize;
	}
    else
        {
	errAbort("%s doesn't start with a '>' line", lf->fileName);
	}
    }
slReverse(&seqList);
lineFileClose(&lf);

readMatchingQuals(qaName, seqList);
return seqList;
}

void ppWashU(char *dataDir, char *pairFile, char *faFile)
/* ppWashU - process Wash U paired reads. */
{
struct fileInfo *fiList, *fi;
struct hash *templateHash = newHash(20);
struct qaSeq *seqList = NULL, *seq, *singles = NULL, *dupes = NULL;
struct seqPair *pairList, *pair;
char template[64];
boolean isRev;
int version;
UBYTE *qaBytes;
int qaSize;
char *qaName;

FILE *pairOut = mustOpen(pairFile, "w");
FILE *faOut = mustOpen(faFile, "w");

dnaUtilOpen();
fiList = listDirX(dataDir, "*.fasta.screen", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    char *fileName = fi->name;
    char qaFileName[512];
    printf("Processing %s\n", fileName);
    sprintf(qaFileName, "%s.qual", fileName);
    seqList = readWashSeqs(fileName, qaFileName);
    printf(" Read %d raw reads\n", slCount(seqList));
    pairReads(&seqList, &pairList, &singles, &dupes);
    printf(" %d pairs, %d singles, %d dupes\n", 
    	slCount(pairList), slCount(singles), slCount(dupes));
    

    for (pair = pairList; pair != NULL; pair = pair->next)
        {
	parseWashuReadName(pair->fRead->name, template, &isRev, &version);
	hashAddUnique(templateHash, template, pair);
	fprintf(pairOut, "%s\t%s\t%s\t800\t8000\n", 
	    pair->fRead->name, pair->rRead->name, template);
	if (ferror(pairOut))
	    {
	    perror("Error writing pair file\n");
	    errAbort("\n");
	    }
	writeClipped(faOut, pair->fRead);
	writeClipped(faOut, pair->rRead);
	if (ferror(faOut))
	    {
	    perror("Error writing fa file\n");
	    errAbort("\n");
	    }
	}

    qaSeqFreeList(&singles);
    qaSeqFreeList(&dupes);
    seqPairFreeList(&pairList);
    }
freeHash(&templateHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
ppWashU(argv[1], argv[2], argv[3]);
return 0;
}
