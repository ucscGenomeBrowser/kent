/* axtForEst - Generate file of mouse/human alignments corresponding to MGC EST's. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "hdb.h"
#include "binRange.h"
#include "axt.h"
#include "psl.h"

static char const rcsid[] = "$Id: axtForEst.c,v 1.5.116.1 2008/07/31 02:23:59 markd Exp $";

char *clChrom = "all";
char *track = "est";
char *estLibrary = NULL;
boolean isRefSeq = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtForEst - Generate file of mouse/human alignments corresponding to MGC EST's\n"
  "usage:\n"
  "   axtForEst database axtDir output.axt\n"
  "options:\n"
  "   -chrom=chrN - restrict to a specific chromosome\n"
  "   -track=track - Use a track other than est\n"
  "   -lib=libWildCard (SQL format where %% is like * normally)\n"
  "   -refSeq - Don't correct indels in EST, treat as refSeq\n"
  "To get MGC est's for Dec 2001 human do:\n"
  "axtForEst hg10 ~/bz/axtBest mgcEst.axt -track=tightEst -lib=NIH_MGC%%\n"
  );
}

struct mrnaBlock
/*  A single block of an mRNA alignment. */
    {
    struct mrnaBlock *next;  /* Next in singly linked list. */
    int qStart; /* Start of block in query */
    int qEnd;   /* End of block in query */
    int tStart; /* Start of block in target */
    int tEnd;   /* End of block in target */
    };

struct mrnaBlock *mrnaBlockFromPsl(struct psl *psl)
/* Convert psl to mrna blocks - merging small gaps. 
 * Free result with slFreeList. */
{
struct mrnaBlock *mbList = NULL, *mb = NULL;
int i;
int maxGap = 5;
if (isRefSeq)
    maxGap = 0;
for (i=0; i<psl->blockCount; ++i)
    {
    int qStart = psl->qStarts[i];
    int tStart = psl->tStarts[i];
    int size = psl->blockSizes[i];
    if (mb == NULL || qStart - mb->qEnd > maxGap 
    	|| tStart - mb->tEnd > maxGap)
        {
	AllocVar(mb);
	slAddHead(&mbList, mb);
	mb->qStart = qStart;
	mb->tStart = tStart;
	}
    mb->qEnd = qStart + size;
    mb->tEnd = tStart + size;
    }
slReverse(&mbList);
return mbList;
}


boolean isSpliced(struct mrnaBlock *mbList, struct dnaSeq *genoSeq, 
	int genoOffset)
/* Return TRUE if mrna looks spliced. */
{
struct mrnaBlock *lastMb = mbList, *mb;
DNA *dna = genoSeq->dna, *iStart, *iEnd;

if (lastMb == NULL)
    return FALSE;
for (mb = lastMb->next; mb != NULL; mb = mb->next)
    {
    iStart = dna + lastMb->tEnd - genoOffset;
    iEnd = dna + mb->tStart - genoOffset;
    if (
    	(iStart[0] == 'g'  || iStart[0] == 'G')
	&& (iStart[1] == 't' || iStart[1] == 'T')
	&& (iEnd[-2] == 'a' || iEnd[-2] == 'A')
	&& (iEnd[-1] == 'g' || iEnd[-1] == 'G'))
	return TRUE;
    lastMb = mb;
    }
return FALSE;
}

struct axt *readInAxt(char *fileName, struct binKeeper *bk)
/* Read all axt's from file.   Return them as a list and
 * add them to bk. */
{
struct axt *axtList = NULL, *axt;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while ((axt = axtRead(lf)) != NULL)
    {
    slAddHead(&axtList, axt);
    binKeeperAdd(bk, axt->tStart, axt->tEnd, axt);
    }
slReverse(&axtList);
return axtList;
}

struct hash *makeLibEstHash(struct sqlConnection *conn, char *lib)
/* Make hash of all MGC est's. */
{
struct hash *hash = newHash(20);
struct sqlResult *sr;
char **row;
int count = 0;
struct dyString *query = newDyString(512);

dyStringPrintf(query, "select mrna.acc from mrna,library "
		   "where mrna.library = library.id "
		   "and library.name like '%s'", lib);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++count;
    hashStore(hash, row[0]);
    }
sqlFreeResult(&sr);
printf("%d %s ESTs\n", count, lib);
dyStringFree(&query);
return hash;
}

void copyAxtIntersection(struct dyString *q, struct dyString *t, 
	int tStart, int tEnd, struct axt *axt, boolean isRc, int tSize)
/* Copy in part of alignment corresponding to target sequence
 * starting at tStart and covering tSize bases. */
{
int tPos = axt->tStart;
int i;

if (axt->tStrand == '-')
    errAbort("Can't handle negative target strand in axt files");

    
// dyStringAppendC(q, '[');
// dyStringAppendC(t, '[');
for (i=0; i<axt->symCount; ++i)
    {
    char tc = axt->tSym[i];
    char qc = axt->qSym[i];
    if (tPos >= tStart)
        {
	dyStringAppendC(t, tc);
	dyStringAppendC(q, qc);
	}
    if (tc != '-')
	{
	++tPos;
	if (tPos >= tEnd)
	    break;
	}
    }
// dyStringAppendC(q, ']');
// dyStringAppendC(t, ']');
}

void lookupGenoAli(struct dyString *q, struct dyString *t, 
	int tStart, int tEnd, 
	struct dnaSeq *genoSeq, int genoOffset, struct binKeeper *bk,
	boolean isRc, int tSize)
/* Add exon to q/t strings - taking it from the
 * alignments if possible, and if not from the genome. */
{
struct binElement *bList = NULL, *bEl;
int lastEnd = tStart;
struct axt *axt;
int uncovSize;

if (isRc)
    {
    int s = tStart, e = tEnd;
    reverseIntRange(&s, &e, tSize);
    bList = binKeeperFindSorted(bk, s, e);
    }
else
    bList = binKeeperFindSorted(bk, tStart, tEnd);
for (bEl = bList; bEl != NULL; bEl = bEl->next)
    {
    axt = bEl->val;
    if (isRc)
	{
	reverseComplement(axt->qSym, axt->symCount);
	reverseComplement(axt->tSym, axt->symCount);
	reverseIntRange(&axt->tStart, &axt->tEnd, tSize);
	}
    uncovSize = axt->tStart - lastEnd;
    if (uncovSize > 0)
	{
	dyStringAppendN(t, genoSeq->dna + lastEnd - genoOffset, uncovSize);
	dyStringAppendMultiC(q, '.', uncovSize);
	}
    copyAxtIntersection(q, t, tStart, tEnd, axt, isRc, tSize);
    lastEnd = axt->tEnd;
    if (isRc)
	{
	reverseComplement(axt->qSym, axt->symCount);
	reverseComplement(axt->tSym, axt->symCount);
	reverseIntRange(&axt->tStart, &axt->tEnd, tSize);
	}
    }
uncovSize = tEnd - lastEnd;
if (uncovSize > 0)
    {
    uncovSize = tEnd - lastEnd;
    dyStringAppendN(t, genoSeq->dna + lastEnd - genoOffset, uncovSize);
    dyStringAppendMultiC(q, '.', uncovSize);
    }
}

int countReal(char *s, int size)
/* Count characters other than '-' and '.' */
{
int i, count = 0;
char c;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c != '.' && c != '-')
        ++count;
    }
return count;
}

void outputOne(char *database, struct psl *psl, struct binKeeper *bk, FILE *f)
/* Output mouse/human version of one EST. */
{
struct dnaSeq *estSeq = hExtSeq(database, psl->qName);
struct dnaSeq *genoSeq = hChromSeq(database, psl->tName, psl->tStart, psl->tEnd);
struct dyString *q = newDyString(2*psl->qSize);
struct dyString *t = newDyString(2*psl->qSize);
int qSize = psl->qSize;
int tSize = psl->tSize;
static struct axt axt;
int qLastEnd = 0;
struct mrnaBlock *mbList, *mb;
int genoOffset = psl->tStart;
boolean isRc = FALSE;
char label[256];

toUpperN(genoSeq->dna, genoSeq->size);	/* This helps debug... */
mbList = mrnaBlockFromPsl(psl);
if (psl->strand[0] == '-')
    {
    reverseComplement(genoSeq->dna, genoSeq->size);
    genoOffset = tSize - psl->tEnd;
    for (mb = mbList; mb != NULL; mb = mb->next)
         {
	 reverseIntRange(&mb->tStart, &mb->tEnd, tSize);
	 reverseIntRange(&mb->qStart, &mb->qEnd, qSize);
	 }
    slReverse(&mbList);
    isRc = TRUE;
    }
if (isSpliced(mbList, genoSeq, genoOffset))
    {
    for (mb = mbList; mb != NULL; mb = mb->next)
	{
	int qStart = mb->qStart;
	int qEnd = mb->qEnd;
	int uncovSize = qStart - qLastEnd;
	if (uncovSize > 0)
	    {
	    dyStringAppendN(t, estSeq->dna + qLastEnd, uncovSize);
	    dyStringAppendMultiC(q, '.', uncovSize);
	    }
	lookupGenoAli(q, t, mb->tStart, mb->tEnd, 
		genoSeq, genoOffset, bk, isRc, tSize);
	qLastEnd = qEnd;
	}
    if (qLastEnd != qSize)
	{
	int uncovSize = qSize - qLastEnd;
	dyStringAppendN(t, estSeq->dna + qLastEnd, uncovSize);
	dyStringAppendMultiC(q, '.', uncovSize);
	}
    assert(q->stringSize == t->stringSize);
    axt.tName = psl->qName;
    axt.tStart = 0;
    axt.tEnd = qSize;
    axt.tStrand = '+';
    sprintf(label, "hm_%s_%d", psl->tName, psl->tStart);
    axt.qName = label;
    axt.qStart = 0;
    axt.qEnd = countReal(q->string, q->stringSize);
    axt.qStrand = psl->strand[0];
    axt.symCount = q->stringSize;
    axt.qSym = q->string;
    axt.tSym = t->string;
    axtWrite(&axt, f);
    }

/* Clean up time. */
slFreeList(&mbList);
freeDyString(&q);
freeDyString(&t);
freeDnaSeq(&estSeq);
freeDnaSeq(&genoSeq);
}

void oneChrom(char *database, char *chrom, struct sqlConnection *conn, 
	struct hash *libEstHash, char *inAxtFile, FILE *f)
/* Process one chromosome */
{
int chromSize = hChromSize(database, chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct axt *inAxtList = readInAxt(inAxtFile, bk);
struct psl *psl;
struct sqlResult *sr;
char **row;
int rowOffset;
int spliEstCount = 0, libCount = 0;

printf("Read in %d axts from %s\n", slCount(inAxtList), inAxtFile);
sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++spliEstCount;
    psl = pslLoad(row + rowOffset);
    if (libEstHash == NULL || hashLookup(libEstHash, psl->qName))
        {
	++libCount;
	outputOne(database,psl, bk, f); 
	}
    pslFree(&psl);
    }
sqlFreeResult(&sr);
printf("Read %d spliced EST's, %d in library\n", spliEstCount, libCount);
binKeeperFree(&bk);
axtFreeList(&inAxtList);
}

void axtForEst(char *database, char *axtDir, char *outFile)
/* axtForEst - Generate file of mouse/human alignments corresponding to 
 * MGC EST's. */
{
struct slName *chromList = NULL, *chromEl = NULL;
FILE *f = mustOpen(outFile, "w");
char inAxtFile[512];
struct sqlConnection *conn;
struct hash *libEstHash = NULL;

conn = hAllocConn(database);
if (estLibrary != NULL)
   libEstHash = makeLibEstHash(conn, estLibrary);
track = optionVal("track", track);
estLibrary = optionVal("lib", estLibrary);
clChrom = optionVal("chrom", clChrom);
isRefSeq = optionExists("refSeq");
if (sameWord(clChrom, "all"))
    chromList = hAllChromNamesDb(database);
else
    chromList = newSlName(clChrom);
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    char *chrom = chromEl->name;
    sprintf(inAxtFile, "%s/%s.axt", axtDir, chrom);
    oneChrom(database, chrom, conn, libEstHash, inAxtFile, f);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
axtForEst(argv[1], argv[2], argv[3]);
return 0;
}
