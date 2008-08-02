/* bedCons - Look at conservation of a BED track vs. a refence (nonredundant) alignment track. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "fa.h"
#include "nib.h"
#include "hdb.h"
#include "binRange.h"

static char const rcsid[] = "$Id: bedCons.c,v 1.4.116.2 2008/08/02 04:06:19 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedCons - Look at conservation of a BED track vs. a refence (nonredundant) alignment track\n"
  "usage:\n"
  "   bedCons database refAliTrack bedTrack\n"
  "Bed track can also be a bed file (ending in .bed suffix)\n"
  "options:\n"
  "   -chrom=chrom  - restrict to a single chromosome.\n"
  );
}

struct stats
/* Summary statistics maker. */
    {
    struct stats *next;
    int bedCount;	/* Number of beds used. */
    int bedBaseCount;	/* Number of bases in beds. */
    int bedBaseMatch;	/* Number of bases that are aligned and match. */
    int bedBaseAli;	/* Number of bases in bed that are in alignments. */
    };

void printStats(struct stats *stats)
/* Print statistics. */
{
printf("%d bases in %d beds\n", stats->bedBaseCount, stats->bedCount);
printf("Aligning %d (%4.2f%%)\n", stats->bedBaseAli, 100.0*stats->bedBaseAli/stats->bedBaseCount);
printf("Matching %d (%4.2f%%)\n", stats->bedBaseMatch, 100.0*stats->bedBaseMatch/stats->bedBaseCount);
}

struct otherSeq
/* Non-human sequence. */
   {
   struct otherSeq *next;
   char *name;	/* Name of sequence. */
   int chromSize;	/* Size of chromosome. */
   char *nibFile;	/* Name of associated nib file. */
   FILE *f;	/* Handle on associated file. */
   };

struct hash *makeOtherHash(char *database, char *table)
/* Make otherSeq valued hash of other sequences */
{
char query[256];
struct hash *hash = newHash(7);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct otherSeq *os;

sprintf(query, "select chrom,fileName from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(os);
    hashAddSaveName(hash, row[0], os, &os->name);
    os->nibFile = cloneString(row[1]);
    nibOpenVerify(os->nibFile, &os->f, &os->chromSize);
    }
hFreeConn(&conn);
return hash;
}

struct dnaSeq *loadSomeSeq(struct hash *otherHash, char *chrom, int start, int end)
/* Load sequence from chromosome file referenced in chromTable. */
{
struct dnaSeq *seq = NULL;
struct otherSeq *os = hashFindVal(otherHash, chrom);

if (os != NULL)
    {
    seq = nibLdPart(os->nibFile, os->f, os->chromSize, start, end - start);
    }
else
    {
    warn("Sequence %s isn't a chromsome", chrom);
    }
return seq;
}

int baseMatch(DNA *a, DNA *b, int size)
/* Count how many match. */
{
int count = 0, i;
for (i=0; i<size; ++i)
    {
    if (a[i] == b[i])
        ++count;
    }
return count;
}

void foldPslIntoStats(struct psl *psl, struct dnaSeq *tSeq, 
   struct hash *otherHash, struct stats *stats)
/* Load sequence corresponding to bed and add alignment stats. */
{
struct dnaSeq *qSeq = loadSomeSeq(otherHash, 
	psl->qName, psl->qStart, psl->qEnd);
int i, bCount = psl->blockCount;
int qOffset;

// uglyf("%s:%d-%d %s %s:%d-%d\n", psl->qName, psl->qStart, psl->qEnd, psl->strand, psl->tName, psl->tStart, psl->tEnd);
if (qSeq != NULL && tSeq != NULL)
    {
    if (psl->strand[0] == '-')
	{
	reverseComplement(qSeq->dna, qSeq->size);
	qOffset = psl->qSize - psl->qEnd;
	}
    else
	qOffset = psl->qStart;
    if (psl->strand[1] == '-')
	errAbort("Can't yet handle reverse complemented targets");
    for (i=0; i<bCount; ++i)
	{
	int bSize  = psl->blockSizes[i];
	stats->bedBaseAli += bSize;
	stats->bedBaseMatch += baseMatch(qSeq->dna + psl->qStarts[i] - qOffset,
	    tSeq->dna + psl->tStarts[i],  bSize);
	}
    }
freeDnaSeq(&qSeq);
}

void oneChrom(char *database, char *chrom, char *refAliTrack, char *bedTrack, 
	struct hash *otherHash, struct stats *stats)
/* Process one chromosome. */
{
struct bed *bedList = NULL, *bed;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
int chromSize = hChromSize(database, chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct psl *pslList = NULL;
struct dnaSeq *chromSeq = NULL;

if (endsWith(bedTrack, ".bed"))
    {
    struct lineFile *lf = lineFileOpen(bedTrack, TRUE);
    char *row[3];
    while (lineFileRow(lf, row))
        {
	if (sameString(chrom, row[0]))
	    {
	    bed = bedLoad3(row);
	    slAddHead(&bedList, bed);
	    }
	}
    lineFileClose(&lf);
    }
else
    {
    sr = hChromQuery(conn, bedTrack, chrom, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoad3(row+rowOffset);
	slAddHead(&bedList, bed);
	}
    sqlFreeResult(&sr);
    }
slReverse(&bedList);
uglyf("Loaded beds\n");

sr = hChromQuery(conn, refAliTrack, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row + rowOffset);
    slAddHead(&pslList, psl);
    binKeeperAdd(bk, psl->tStart, psl->tEnd, psl);
    }
sqlFreeResult(&sr);
uglyf("Loaded psls\n");

chromSeq = hLoadChrom(database, chrom);
/* Fetch entire chromosome into memory. */
uglyf("Loaded human seq\n");

for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct binElement *el, *list = binKeeperFind(bk, bed->chromStart, bed->chromEnd);
    for (el = list; el != NULL; el = el->next)
        {
	struct psl *fullPsl = el->val;
	struct psl *psl = pslTrimToTargetRange(fullPsl, 
		bed->chromStart, bed->chromEnd);
	if (psl != NULL)
	    {
	    foldPslIntoStats(psl, chromSeq, otherHash, stats);
	    pslFree(&psl);
	    }
	}
    slFreeList(&list);
    stats->bedCount += 1;
    stats->bedBaseCount += bed->chromEnd - bed->chromStart;
    sqlFreeResult(&sr);
    }
freeDnaSeq(&chromSeq);
pslFreeList(&pslList);
binKeeperFree(&bk);
hFreeConn(&conn);
}


void bedCons(char *database, char *refAliTrack, char *bedTrack)
/* bedCons - Look at conservation of a BED track vs. a refence 
 * (nonredundant) alignment track. */
{
struct slName *chromList, *chrom;
struct stats *stats = NULL;
struct hash *otherHash = makeOtherHash(database, "mouseChrom");

if (optionExists("chrom"))
    chromList = newSlName(optionVal("chrom", NULL));
else
    chromList = hAllChromNames(database);
AllocVar(stats);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    uglyf("%s\n", chrom->name);
    oneChrom(database, chrom->name, refAliTrack, bedTrack, otherHash, stats);
    }

printStats(stats);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
bedCons(argv[1], argv[2], argv[3]);
return 0;
}
