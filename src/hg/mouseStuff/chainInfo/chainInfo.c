#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "dnautil.h"
#include "chain.h"
#include "axt.h"
#include "axtInfo.h"

#define MINGAP 20

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainInfo - parse chains to determine number of introns on query and target\n"
  "usage:\n"
  "   chainInfo chainFile output\n"
  "options:\n"
  "   -xxx=\n"
  );
}

struct chain *chainReadIntrons(struct lineFile *lf, FILE *f)
/* Read next chain from file.  Return NULL at EOF. 
 * count introns on query and target side , blocks with < MINGAP bases between will be combined. 
 * Note that chain block scores are not filled in by
 * this. */
{
char *row[13];
int wordCount;
struct chain *chain;
int q,t;
static int id = 0;
int qIntron = 0, tIntron = 0, tGap = 0, qGap = 0, total = 0;
float score1, score2;

wordCount = lineFileChop(lf, row);
if (wordCount == 0)
    return NULL;
if (wordCount < 12)
    errAbort("Expecting at least 12 words line %d of %s", 
    	lf->lineIx, lf->fileName);
if (!sameString(row[0], "chain"))
    errAbort("Expecting 'chain' line %d of %s", lf->lineIx, lf->fileName);
AllocVar(chain);
chain->score = atof(row[1]);
chain->tName = cloneString(row[2]);
chain->tSize = lineFileNeedNum(lf, row, 3);
if (wordCount >= 13)
    chain->id = lineFileNeedNum(lf, row, 12);
else
    chainIdNext(chain);

/* skip tStrand for now, always implicitly + */
chain->tStart = lineFileNeedNum(lf, row, 5);
chain->tEnd = lineFileNeedNum(lf, row, 6);
chain->qName = cloneString(row[7]);
chain->qSize = lineFileNeedNum(lf, row, 8);
chain->qStrand = row[9][0];
chain->qStart = lineFileNeedNum(lf, row, 10);
chain->qEnd = lineFileNeedNum(lf, row, 11);
if (chain->qStart >= chain->qEnd || chain->tStart >= chain->tEnd)
    errAbort("End before start line %d of %s", lf->lineIx, lf->fileName);
if (chain->qStart < 0 || chain->tStart < 0)
    errAbort("Start before zero line %d of %s", lf->lineIx, lf->fileName);
if (chain->qEnd > chain->qSize || chain->tEnd > chain->tSize)
    errAbort("Past end of sequence line %d of %s", lf->lineIx, lf->fileName);

/* Now read in block list. */
q = chain->qStart;
t = chain->tStart;
for (;;)
    {
    int tg,qg;
    int wordCount = lineFileChop(lf, row);
    int size = lineFileNeedNum(lf, row, 0);
    struct boxIn *b;
    total += size;
    AllocVar(b);
    slAddHead(&chain->blockList, b);
    b->qStart = q;
    b->tStart = t;
    q += size;
    t += size;
    b->qEnd = q;
    b->tEnd = t;
    if (wordCount == 1)
        break;
    else if (wordCount < 3)
        errAbort("Expecting 1 or 3 words line %d of %s\n", 
		lf->lineIx, lf->fileName);
    tg = lineFileNeedNum(lf, row, 1);
    qg = lineFileNeedNum(lf, row, 2);
    t += tg;
    q += qg;
    tGap += tg;
    qGap += qg;
    if (tg >= MINGAP)
        tIntron++;
    if (qg >= MINGAP)
        qIntron++;
    }
if (q != chain->qEnd)
    errAbort("q end mismatch %d vs %d line %d of %s\n", 
    	q, chain->qEnd, lf->lineIx, lf->fileName);
if (t != chain->tEnd)
    errAbort("t end mismatch %d vs %d line %d of %s\n", 
    	t, chain->tEnd, lf->lineIx, lf->fileName);
slReverse(&chain->blockList);
assert(tIntron>=0);
assert(qIntron>=0);
/*if (tIntron>1000000) tIntron=1000000);
if (qIntron>1000000) qIntron=1000000);*/
score1 = (tIntron > qIntron ? (tGap+1)/(qGap+1) : -(qGap+1)/(tGap+1))/10;
score2 = tIntron > qIntron ? (tIntron+1.0)/(qIntron+1.0) : -(qIntron+1.0)/(tIntron+1.0) ;
    if (chain->qStrand == '+')
    fprintf(f,"%5.1f %5.1f %d %d %d %d %d %d %s %d %d %s %d %d %d\n",score1, score2, chain->tEnd-chain->tStart, tIntron, qIntron, tGap, qGap, total, chain->tName, chain->tStart, chain->tEnd, chain->qName, chain->qStart, chain->qEnd, chain->id);
else
    fprintf(f,"%5.1f %5.1f %d %d %d %d %d %d %s %d %d %s %d %d %d\n",score1, score2, chain->tEnd-chain->tStart, tIntron, qIntron, tGap, qGap, total, chain->tName, chain->tStart, chain->tEnd, chain->qName, chain->qSize-chain->qEnd, chain->qSize-chain->qStart, chain->id);
return chain;
}
struct hash *allChains(char *fileName, FILE *f)
/* read all chains in a chain file */
{
    struct hash *hash = newHash(0);
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct chain *chain;
    char *chainId;

    while ((chain = chainReadIntrons(lf,f)) != NULL)
        {
        AllocVar(chainId);
        sprintf(chainId, "%d",chain->id);
        if (hashLookup(hash, chainId) == NULL)
                {
                hashAddUnique(hash, chainId, chain);
                }
        }
    lineFileClose(&lf);
    return hash;
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *chainHash;
FILE *outFile; 

optionHash(&argc, argv);
if (argc != 3)
    usage();
outFile = mustOpen(argv[2], "w");
/*hSetDb(argv[1]);
hSetDb2(argv[2]);
db = hGetDb();
db2 = hGetDb2();*/
chainHash = allChains(argv[1], outFile);
return 0;
}
