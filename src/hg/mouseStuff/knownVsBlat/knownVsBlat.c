/* knownVsBlat - Categorize BLAT mouse hits to known genes. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "psl.h"
#include "genePred.h"
#include "hdb.h"
#include "refLink.h"
#include "nib.h"
#include "bed.h"
#include "blatStats.h"

static char const rcsid[] = "$Id: knownVsBlat.c,v 1.14.106.1 2008/07/31 02:24:41 markd Exp $";

/* Variables that can be set from command line. */
int dotEvery = 0;	/* How often to print I'm alive dots. */
char *clChrom = "all";	/* Which chromosome. */
boolean reportPercentId = FALSE;  /* Report percent id. */
char *format = "psl";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "knownVsBlat - Categorize BLAT mouse hits to known genes\n"
  "usage:\n"
  "   knownVsBlat database table output.stats\n"
  "options:\n"
  "   -dots=N - Output a dot every N known genes\n"
  "   -chrom=chrN - Restrict to a single chromosome\n"
  "   -percentId - calculate percent identity. Only works for psl tables. Slow\n"
  "   -format=type.  Type = 'bed' or 'psl'\n"
  );
}

void dotOut()
/* Put out a dot every now and then if user wants to. */
{
static int mod = 1;
if (dotEvery > 0)
    {
    if (--mod <= 0)
	{
	fputc('.', stdout);
	fflush(stdout);
	mod = dotEvery;
	}
    }
}
void addPslBestMilli(struct psl *psl, int *milliMatches, 
	struct dnaSeq *geno, int genoStart, int genoEnd,
	struct dnaSeq *query)
/* Evaluated psl block-by-block for identity in parts per thousand.
 * Update milliMatches for any bases where identity is greater
 * than that already recorded. */
{
DNA *q, *t;
int  i, j, blockCount = psl->blockCount, blockSize, tStart, qStart, clipOut;
int same, milli, *milliPt;
int tOffset;
int regionSize = geno->size;
boolean tIsRc = (psl->strand[1] == '-');

/* Reverse complement coordinates and sequence if necessary. */
if (tIsRc)
    {
    int gs = genoStart, ge = genoEnd, sz = psl->tSize;
    genoStart = sz-ge;
    genoEnd = sz-gs;
    reverseComplement(geno->dna, regionSize);
    }
if (psl->strand[0] == '-')
    reverseComplement(query->dna, query->size);

/* Loop through each block.... */
for (i=0; i<blockCount; ++i)
    {
    /* Get coordinates of block. */
    blockSize = psl->blockSizes[i];
    tStart = psl->tStarts[i];
    qStart = psl->qStarts[i];

    /* Clip to fit in region. */
    clipOut = genoStart - tStart;
    if (clipOut > 0)
	{
	tStart += clipOut;
	qStart += clipOut;
	blockSize -= clipOut;
	}
    clipOut = (tStart + blockSize) - genoEnd;
    if (clipOut > 0)
	{
	blockSize -= clipOut;
	}

    /* Calc identity in parts per thousand and
     * update milliMatches. */
    if (blockSize > 0)
	{
	tOffset = tStart - genoStart;
	assert(qStart >= 0 && tOffset >= 0);
	assert(qStart + blockSize <= query->size);
	assert(tOffset + blockSize <= geno->size);
	q = query->dna + qStart;
	t = geno->dna + tOffset;
	same = 0;
	for (j=0; j<blockSize; ++j)
	    {
	    if (q[j] == t[j])
		++same;
	    }
	milli = roundingScale(1000, same, blockSize);
	if (tIsRc)
	    milliPt = milliMatches + regionSize - tOffset - blockSize;
	else
	    milliPt = milliMatches + tOffset;
	assert(milliPt >= milliMatches);
	assert(milliPt + blockSize <= milliMatches + geno->size);
	for (j=0; j<blockSize; ++j)
	    if (milli > milliPt[j]) milliPt[j] = milli;
	}
}
if (psl->strand[0] == '-')
    reverseComplement(query->dna, query->size);
if (tIsRc)
    reverseComplement(geno->dna, regionSize);
}

int pslMilliId(struct psl *psl)
/* Return percent ID more or less. */
{
return 1000 - pslCalcMilliBad(psl, FALSE);
}

void simpleAddMilli(int *milliMatches, int score, int start, int end, int genoStart, int genoEnd)
/* Add bits of block that intersect region from genoStart to genoEnd to milliMatches */
{
/* Clip to window and if anything left update score array. */
if (start < genoStart) start = genoStart;
if (end > genoEnd) end = genoEnd;
if (start < end)
    {
    int i;
    start -= genoStart;
    end -= genoStart;
    for (i=start; i<end; ++i)
	if (score > milliMatches[i]) milliMatches[i] = score;
    }
}

void addPslFakeMilli(struct psl *psl, int *milliMatches, 
	int genoStart, int genoEnd)
/* Similar to addBestMilli above.  However this only makes as much
 * of the stats as it can without having the sequence loaded. */
{
int  i, blockCount = psl->blockCount, blockStart, blockEnd;
int score = pslMilliId(psl);
boolean tIsRc = (psl->strand[1] == '-');
int start, end;

/* Loop through each block.... */
for (i=0; i<blockCount; ++i)
    {
    /* Get coordinates of block, coping with minus strand adjustment
     * if necessary.  Then update block in milliMatches array. */
    blockStart = psl->tStarts[i];
    blockEnd = blockStart + psl->blockSizes[i];
    if (tIsRc)
        {
	start = psl->tSize - blockEnd;
	end = psl->tSize - blockStart;
	}
    else
        {
	start = blockStart;
	end = blockEnd;
	}
    simpleAddMilli(milliMatches, score, start, end, genoStart, genoEnd);
    }
}

int halfAddToStats(struct oneStat *stat, int hitStart, int hitEnd, 
    int genoStart, int genoEnd, int *milliMatches)
/* Fold in info from milliMatches between hitStart and hitEnd to stat.
 * Update base info but not feature info. */
{
int count, i, mm;
int painted = 0;

if (rangeIntersection(hitStart, hitEnd, genoStart, genoEnd) <= 0)
    {
    return 0;
    }
if (hitStart < genoStart) hitStart = genoStart;
if (hitEnd > genoEnd) hitEnd = genoEnd;
count = hitEnd - hitStart;
milliMatches += (hitStart - genoStart);
for (i=0; i<count; ++i)
    {
    if ((mm = milliMatches[i]) != 0)
        {
	++painted;
	stat->cumIdRatio += 0.001*mm;
	}
    }
stat->basesPainted += painted;
stat->basesTotal += count;
return painted;
}

int addToStats(struct oneStat *stat, int hitStart, int hitEnd, 
    int genoStart, int genoEnd, int *milliMatches)
/* Fold in info from milliMatches between hitStart and hitEnd to stat. */
{
int painted = 0;

painted = halfAddToStats(stat, hitStart, hitEnd, 
	genoStart, genoEnd, milliMatches);
if (painted > 0)
     stat->hits += 1;
stat->features += 1;
return painted;
}

void pslMakeMilli(char *database, struct dnaSeq *geno, struct psl *pslList, int *milliMatches, 
	char *chrom, int genoStart, int genoEnd, 
	struct hash *traceHash, struct dnaSeq **pTraceList)
/* Fill in milliMatches array with scores from pslList. */
{
struct psl *psl;

/* Loop through alignments that might intersect window. */
for (psl = pslList; psl != NULL && psl->tStart < genoEnd; psl = psl->next)
    {
    /* If an alignment actually intersects window load trace (query)
     * dna, caching it temporarily in hash table.  Keep track
     * of percentage identity of best alignment for each base in 
     * milliMatches. */
    if (psl->tStart < genoEnd && psl->tEnd > genoStart)
	{
	if (geno != NULL)
	    {
	    char *traceName = psl->qName;
	    struct dnaSeq *trace;
	    if ((trace = hashFindVal(traceHash, traceName)) == NULL)
		{
		trace = hExtSeq(database, traceName);
		slAddHead(pTraceList, trace);
		hashAdd(traceHash, traceName, trace);
		}
	    addPslBestMilli(psl, milliMatches, geno, genoStart, genoEnd, trace);
	    }
	else
	    {
	    addPslFakeMilli(psl, milliMatches, genoStart, genoEnd);
	    }
	}
    }
}

void bedMakeMilli(struct bed *bedList, int *milliMatches, char *chrom, 
	int genoStart, int genoEnd)
/* Fill in milliMatches array from bedList. */
{
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart < genoEnd && bed->chromEnd > genoStart)
        simpleAddMilli(milliMatches, 500, bed->chromStart, bed->chromEnd, genoStart, genoEnd);
    }
}

struct blatStats *calcGeneStats(char *chrom, 
	int genoStart, int genoEnd,
	int *milliMatches,
	struct genePred *gp)
/* Figure out how psl hits gene and return resulting stats. 
 * This will take a short-cut and be much faster if geno is NULL.
 * (But the percent ID stuff won't be calculated. */
{
struct blatStats *stats;
int exonCount = gp->exonCount, exonIx, exonStart, exonEnd;
boolean anyMrna = FALSE;
int mrnaSize, cdsSize, utrSize;

AllocVar(stats);

/* Gather stats on various regions starting with upstream and downstream
 * regions. */
if (gp->strand[0] == '+')
    {
    if (gp->txStart != gp->cdsStart)
	{
	addToStats(&stats->upstream100, gp->txStart - 100, gp->txStart, 
	    genoStart, genoEnd, milliMatches);
	addToStats(&stats->upstream200, gp->txStart - 200, gp->txStart, 
	    genoStart, genoEnd, milliMatches);
	addToStats(&stats->upstream400, gp->txStart - 400, gp->txStart, 
	    genoStart, genoEnd, milliMatches);
	addToStats(&stats->upstream800, gp->txStart - 800, gp->txStart, 
	    genoStart, genoEnd, milliMatches);
	}
    if (gp->txEnd != gp->cdsEnd)
	{
	addToStats(&stats->downstream200, gp->txEnd, gp->txEnd + 200,
	    genoStart, genoEnd, milliMatches);
	}
    }
else
    {
    if (gp->txEnd != gp->cdsEnd)
	{
	addToStats(&stats->upstream100, gp->txEnd, gp->txEnd + 100, 
	    genoStart, genoEnd, milliMatches);
	addToStats(&stats->upstream200, gp->txEnd, gp->txEnd + 200, 
	    genoStart, genoEnd, milliMatches);
	addToStats(&stats->upstream400, gp->txEnd, gp->txEnd + 400, 
	    genoStart, genoEnd, milliMatches);
	addToStats(&stats->upstream800, gp->txEnd, gp->txEnd + 800, 
	    genoStart, genoEnd, milliMatches);
	}
    if (gp->txStart != gp->cdsStart)
	{
	addToStats(&stats->downstream200, gp->txStart-200, gp->txStart,
	    genoStart, genoEnd, milliMatches);
	}
    }

/* Gather stats on exons, tracking UTR and CDS part of
 * exons separately. */
mrnaSize = utrSize = cdsSize = 0;
for (exonIx = 0; exonIx < exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonEnd = gp->exonEnds[exonIx];
    mrnaSize += exonEnd - exonStart;
    if (halfAddToStats(&stats->mrnaTotal, exonStart, exonEnd, genoStart, genoEnd, milliMatches) > 0)
	{
	anyMrna = TRUE;
	}
    if (exonStart < gp->cdsStart)	/* UTR */
        {
	struct oneStat *stat = (gp->strand[0] == '+' ? &stats->utr5 : &stats->utr3);
	int end = min(exonEnd, gp->cdsStart);
	addToStats(stat, exonStart, end, genoStart, genoEnd, milliMatches);
	utrSize += end - exonStart;
	}
    if (exonEnd > gp->cdsEnd)		/* Other UTR */
        {
	struct oneStat *stat = (gp->strand[0] == '-' ? &stats->utr5 : &stats->utr3);
	int start = max(exonStart, gp->cdsEnd);
	addToStats(stat, start, exonEnd, genoStart, genoEnd, milliMatches);
	utrSize += exonEnd - start;
	}
    if (exonStart <= gp->cdsStart && exonEnd >= gp->cdsEnd)  /* Single exon CDS */
        {
	addToStats(&stats->onlyCds, gp->cdsStart, gp->cdsEnd, 
		genoStart, genoEnd, milliMatches);
	cdsSize += gp->cdsEnd - gp->cdsStart;
	}
    else if (exonStart <= gp->cdsStart && exonEnd > gp->cdsStart)
        {
	struct oneStat *stat = (gp->strand[0] == '+' ? &stats->firstCds : &stats->endCds);
	addToStats(stat, gp->cdsStart, exonEnd, genoStart, genoEnd, milliMatches);
	cdsSize += exonEnd - gp->cdsStart;
	}
    else if (exonStart < gp->cdsEnd && exonEnd >= gp->cdsEnd)
        {
	struct oneStat *stat = (gp->strand[0] == '-' ? &stats->firstCds : &stats->endCds);
	addToStats(stat, exonStart, gp->cdsEnd, genoStart, genoEnd, milliMatches);
	cdsSize += gp->cdsEnd - exonStart;
	}
    else if (exonStart >= gp->cdsStart && exonEnd <= gp->cdsEnd)
        {
	addToStats(&stats->middleCds, exonStart, exonEnd, genoStart, genoEnd, milliMatches);
	cdsSize += exonEnd - exonStart;
	}
    }
if (mrnaSize != cdsSize + utrSize)
   uglyf("%s cds %d  utr %d  mrna %d\n", gp->name, cdsSize, utrSize, mrnaSize);

if (anyMrna)
    stats->mrnaTotal.hits += 1;
stats->mrnaTotal.features += 1;

/* Gather stats on introns and splice sites. */
for (exonIx = 1; exonIx < exonCount; ++exonIx)
    {
    int intronStart = gp->exonEnds[exonIx-1];
    int intronEnd = gp->exonStarts[exonIx];
    int spliceSize = 10;
    struct oneStat *stat;
    if (intronEnd - intronStart > 2*spliceSize)
        {
	if (gp->strand[0] == '+')
	    {
	    addToStats(&stats->splice5, intronStart, intronStart+spliceSize, 
	    	genoStart, genoEnd, milliMatches);
	    addToStats(&stats->splice3, intronEnd-spliceSize, intronEnd, 
	    	genoStart, genoEnd, milliMatches);
	    }
	else
	    {
	    addToStats(&stats->splice3, intronStart, intronStart+spliceSize, 
	    	genoStart, genoEnd, milliMatches);
	    addToStats(&stats->splice5, intronEnd-spliceSize, intronEnd, 
	    	genoStart, genoEnd, milliMatches);
	    }
	if (exonCount == 2)
	    stat = &stats->onlyIntron;
	else if (exonIx == 1)
	    stat = (gp->strand[0] == '+' ? &stats->firstIntron : &stats->endIntron);
	else if (exonIx == exonCount-1)
	    stat = (gp->strand[0] == '-' ? &stats->firstIntron : &stats->endIntron);
	else
	    stat = &stats->middleIntron;
	addToStats(stat, intronStart+spliceSize, intronEnd-spliceSize, 
	    genoStart, genoEnd, milliMatches);
	}
    }

/* Clean up and return. */
return stats;
}


struct geneIsoforms
/* A list of isoforms for each gene. */
    {
    struct geneIsoforms *next;
    char *name;			/* Name of gene. Not allocated here. */
    struct genePred *gpList;	/* List of isoforms. */
    int start, end;		/* Start/end in chromosome. */
    };

void geneIsoformsFree(struct geneIsoforms **pGi)
/* Free a gene isoform. */
{
struct geneIsoforms *gi = *pGi;
if (gi != NULL)
    {
    genePredFreeList(&gi->gpList);
    freez(pGi);
    }
}

void geneIsoformsFreeList(struct geneIsoforms **pList)
/* Free a list of gene isoforms. */
{
struct geneIsoforms *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    geneIsoformsFree(&el);
    }
*pList = NULL;
}


struct geneIsoforms *getChromGenes(char *database, char *chrom, struct hash *nmToGeneHash)
/* Get list of genes in this chromosome with isoforms bundled together. */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct hash *geneHash = newHash(0);
struct geneIsoforms *giList = NULL, *gi;
struct genePred *gp;
char *geneName;

sprintf(query, "select * from refGene where chrom = '%s'", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    geneName = hashMustFindVal(nmToGeneHash, gp->name);
    if ((gi = hashFindVal(geneHash, geneName)) == NULL || 
    	rangeIntersection(gi->start, gi->end, gp->txStart, gp->txEnd) <= 0)
        {
	AllocVar(gi);
	slAddHead(&giList, gi);
	gi->name = geneName;
	gi->start = gp->txStart;
	gi->end = gp->txEnd;
	hashAdd(geneHash, geneName, gi);
	}
    slAddTail(&gi->gpList, gp);
    if (gp->txStart < gi->start) gi->start = gp->txStart;
    if (gp->txEnd > gi->end) gi->end = gp->txEnd;
    }
printf("%d known genes on %s\n", slCount(giList), chrom);

/* Clean up and return. */
freeHash(&geneHash);
hFreeConn(&conn);
slReverse(&giList);
return giList;
}

struct genePred *longestIsoform(struct geneIsoforms *gi)
/* Return longest isoform. */
{
int maxSize = 0;
struct genePred *gp, *maxGp = NULL;
int size;

for (gp = gi->gpList; gp != NULL; gp = gp->next)
    {
    size = gp->txEnd - gp->txStart;
    if (size > maxSize)
        {
	maxSize = size;
	maxGp = gp;
	}
    }
return maxGp;
}

struct psl *getChromPsl(char *database, char *chrom, char *splitTable)
/* Get all alignments for chromosome sorted by chromosome
 * start position. */
{
char table[64];
char query[256], **row;
struct sqlResult *sr;
struct psl *pslList = NULL, *psl;
boolean hasBin;
boolean isSplit = hFindSplitTable(database, chrom, splitTable, table, &hasBin);
if (hTableExists(database, table))
    {
    struct sqlConnection *conn = hAllocConn(database);
    if (isSplit)
	sprintf(query, "select * from %s", table);
    else
	sprintf(query, "select * from %s where qName = '%s'", table, chrom);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row+hasBin);
	slAddHead(&pslList, psl);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slReverse(&pslList);
    }
else
    warn("Table %s doesn't exist", table);
return pslList;
}

struct bed *getChromBed(char *database, char *chrom, char *splitTable)
/* Get all alignments for chromosome sorted by chromosome
 * start position. */
{
char table[64];
char query[256], **row;
struct sqlResult *sr;
struct bed *bedList = NULL, *bed;
boolean hasBin;
boolean isSplit = hFindSplitTable(database, chrom, splitTable, table, &hasBin);

uglyf("hFindSplitTable(%s, %s, %s, %d)\n", chrom, splitTable, table, hasBin);
if (hTableExists(database, table))
    {
    struct sqlConnection *conn = hAllocConn(database);
    if (isSplit)
	sprintf(query, "select chrom,chromStart,chromEnd from %s", table);
    else
	sprintf(query, "select chrom,chromStart,chromEnd from %s where chrom = '%s'", 
		table, chrom);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	AllocVar(bed);
	bed->chrom = cloneString(row[0]);
	bed->chromStart = sqlUnsigned(row[1]);
	bed->chromEnd = sqlUnsigned(row[2]);
	slAddHead(&bedList, bed);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slReverse(&bedList);
    }
else
    warn("Table %s doesn't exist", table);
uglyf("Got %d beds\n", slCount(bedList));
uglyf("bed1 = %s:%d-%d\n", bedList->chrom, bedList->chromStart, bedList->chromEnd);
return bedList;
}



struct blatStats *chromStats(char *database, char *chrom, struct hash *nmToGeneHash, char *table)
/* Produce stats for one chromosome. Just consider longest isoform
 * of each gene. */
{
struct blatStats *stats, *gStats;
struct geneIsoforms *giList = NULL, *gi;
struct genePred *gp;
char nibName[512];
FILE *nibFile;
int chromSize;
struct psl *pslList = NULL;
struct bed *bedList = NULL;
void *itemList = NULL;
struct dnaSeq *geno = NULL;
int extraBefore = 800;
int extraAfter = 200;
int startRegion, endRegion;
int sizeRegion;
boolean isPsl = sameWord(format, "psl");
struct hash *traceHash = newHash(0);
struct dnaSeq *traceList = NULL;

hNibForChrom(database, chrom, nibName);
nibOpenVerify(nibName, &nibFile, &chromSize);
if (isPsl)
    itemList = pslList = getChromPsl(database, chrom, table);
else
    itemList = bedList = getChromBed(database, chrom, table);

AllocVar(stats);
giList = getChromGenes(database, chrom, nmToGeneHash);
for (gi = giList; gi != NULL; gi = gi->next)
    {
    int *milliMatches = NULL;
    /* Expand region around gene a little and load corresponding sequence. */
    gp = longestIsoform(gi);
    if (gp->strand[0] == '+')
	{
	startRegion = gp->txStart - extraBefore;
	endRegion = gp->txEnd + extraAfter;
	}
    else
	{
	startRegion = gp->txStart - extraAfter;
	endRegion = gp->txEnd + extraBefore;
	}
    if (startRegion < 0) startRegion = 0;
    if (endRegion > chromSize)
	endRegion = chromSize;
    sizeRegion = endRegion - startRegion;
    if (reportPercentId)
	geno = nibLdPart(nibName, nibFile, chromSize, startRegion, sizeRegion);

    AllocArray(milliMatches, sizeRegion);
    if (isPsl)
	pslMakeMilli(database, geno, itemList, milliMatches, chrom, startRegion, endRegion, traceHash, &traceList);
    else
        bedMakeMilli(itemList, milliMatches, chrom, startRegion, endRegion);
    gStats = calcGeneStats(chrom, startRegion, endRegion, milliMatches, gp);
    addStats(gStats, stats);
    freez(&gStats);
    freeDnaSeq(&geno);
    freez(&milliMatches);
    dotOut();
    }
if (dotEvery > 0)
   printf("\n");
carefulClose(&nibFile);
geneIsoformsFreeList(&giList);
pslFreeList(&pslList);
bedFreeList(&bedList);
freeHash(&traceHash);
freeDnaSeqList(&traceList);
return stats;
}

struct hash *makeNmToGeneHash(char *database)
/* Make a hash that maps refSeq genes to their common names. */
{
struct sqlConnection *conn = hAllocConn(database);
struct hash *hash = newHash(0);
struct sqlResult *sr;
struct refLink rl;
char **row;

sr = sqlGetResult(conn, "select * from refLink");
while ((row = sqlNextRow(sr)) != NULL)
    {
    refLinkStaticLoad(row, &rl);
    hashAddUnique(hash, rl.mrnaAcc, cloneString(rl.name));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return hash;
}

void knownVsBlat(char *database, char *table, char *output)
/* knownVsBlat - Categorize BLAT mouse hits to known genes. */
{
struct slName *allChroms, *chrom;
struct blatStats *statsList = NULL, *stats, *sumStats;
struct hash *nmToGeneHash;
FILE *f = mustOpen(output, "w");

nmToGeneHash = makeNmToGeneHash(database);
if (sameWord(clChrom, "all"))
    allChroms = hAllChromNamesDb(database);
else
    allChroms = newSlName(clChrom);
slReverse(&allChroms);
for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
    {
    stats = chromStats(database, chrom->name, nmToGeneHash, table);
    reportStats(f, stats, chrom->name, reportPercentId);
    fprintf(f, "\n");
    slAddHead(&statsList, stats);
    }
slReverse(statsList);
sumStats = sumStatsList(statsList);
if (sameWord(clChrom, "all"))
    {
    reportStats(f, sumStats, "total", reportPercentId);
    }
freeHashAndVals(&nmToGeneHash);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
dotEvery = cgiUsualInt("dots", dotEvery);
clChrom = cgiUsualString("chrom", clChrom);
reportPercentId = cgiBoolean("percentId");
format = cgiUsualString("format", format);
if (argc != 4)
    usage();
knownVsBlat(argv[1], argv[2], argv[3]);
return 0;
}
