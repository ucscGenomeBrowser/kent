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

struct stat
/* One type of stat. */
    {
    int features;	/* Number of this type of feature. */
    int hits;		/* Number of hits to this type of features. */
    int basesTotal;	/* Total bases in all features. */
    int basesPainted;	/* Total bases hit. */
    double cumIdRatio;	/* Identity ratio times basesTotal. */
    };

struct blatStats 
/* Blat summary statistics. */
    {
    struct blatStats	*next;
    struct stat upstream100;	/* 100 bases upstream of txn start. */
    struct stat upstream200;	/* 200 bases upstream of txn start. */
    struct stat upstream400;	/* 400 bases upstream of txn start. */
    struct stat upstream800;	/* 800 bases upstream of txn start. */
    struct stat mrnaTotal;	/* CDS + UTR's. */
    struct stat utr5;		/* 5' UTR. */
    struct stat firstCds;	/* Coding part of first coding exon. */
    struct stat firstIntron;	/* First intron. */
    struct stat middleCds;	/* Middle coding exons. */
    struct stat onlyCds;	/* Case where only one CDS exon. */
    struct stat middleIntron;	/* Middle introns. */
    struct stat onlyIntron;	/* Case where only single intron. */
    struct stat endCds;		/* Coding part of last coding exon. */
    struct stat endIntron;	/* Last intron. */
    struct stat splice5;	/* First 10 bases of intron. */
    struct stat splice3;	/* Last 10 bases of intron. */
    struct stat utr3;		/* 3' UTR. */
    struct stat downstream200;	/* 200 bases past end of UTR. */
    };

float divAsPercent(double a, double b)
/* Return a/b * 100. */
{
if (b == 0)
   return 0.0;
return 100.0 * a / b;
}

void reportStat(FILE *f, char *name, struct stat *stat)
/* Print out one set of stats. */
{
char buf[64];
fprintf(f, "%-15s ", name);
sprintf(buf, "%4.1f%% (%d/%d)", 
	divAsPercent(stat->basesPainted, stat->basesTotal),
	stat->basesPainted, stat->basesTotal);
fprintf(f, "%-25s ", buf);
sprintf(buf, "%4.1f%% (%d/%d)", 
	divAsPercent(stat->hits, stat->features),
	stat->hits, stat->features);
fprintf(f, "%-20s ", buf);
if (reportPercentId)
    fprintf(f, "%4.1f%%", divAsPercent(stat->cumIdRatio, stat->basesPainted));
fprintf(f, "\n");
}

void reportStats(FILE *f, struct blatStats *stats, char *name)
/* Print out stats. */
{
fprintf(f, "%s stats:\n", name);
fprintf(f, "region         bases hit           features hit ");
if (reportPercentId)
    fprintf(f, "percent identity");
fprintf(f, "\n");
fprintf(f, "------------------------------------------------");
if (reportPercentId)
    fprintf(f, "---------------------");
fprintf(f, "\n");
reportStat(f, "upstream 100", &stats->upstream100);
reportStat(f, "upstream 200", &stats->upstream200);
reportStat(f, "upstream 400", &stats->upstream400);
reportStat(f, "upstream 800", &stats->upstream800);
reportStat(f, "downstream 200", &stats->downstream200);
reportStat(f, "mrna total", &stats->mrnaTotal);
reportStat(f, "5' UTR", &stats->utr5);
reportStat(f, "3' UTR", &stats->utr3);
reportStat(f, "first CDS", &stats->firstCds);
reportStat(f, "middle CDS", &stats->middleCds);
reportStat(f, "end CDS", &stats->endCds);
reportStat(f, "only CDS", &stats->onlyCds);
reportStat(f, "5' splice", &stats->splice5);
reportStat(f, "3' splice", &stats->splice3);
reportStat(f, "first intron", &stats->firstIntron);
reportStat(f, "middle intron", &stats->middleIntron);
reportStat(f, "end intron", &stats->endIntron);
reportStat(f, "only intron", &stats->onlyIntron);
fprintf(f, "\n");
}

void addStat(struct stat *a, struct stat *acc)
/* Add stat from a into acc. */
{
acc->features += a->features;
acc->hits += a->hits;
acc->basesTotal += a->basesTotal;
acc->basesPainted += a->basesPainted;
acc->cumIdRatio += a->cumIdRatio;
}

struct blatStats *addStats(struct blatStats *a, struct blatStats *acc)
/* Add stats in a to acc. */
{
addStat(&a->upstream100, &acc->upstream100);
addStat(&a->upstream200, &acc->upstream200);
addStat(&a->upstream400, &acc->upstream400);
addStat(&a->upstream800, &acc->upstream800);
addStat(&a->mrnaTotal, &acc->mrnaTotal);
addStat(&a->utr5, &acc->utr5);
addStat(&a->firstCds, &acc->firstCds);
addStat(&a->firstIntron, &acc->firstIntron);
addStat(&a->middleCds, &acc->middleCds);
addStat(&a->onlyCds, &acc->onlyCds);
addStat(&a->middleIntron, &acc->middleIntron);
addStat(&a->onlyIntron, &acc->onlyIntron);
addStat(&a->endCds, &acc->endCds);
addStat(&a->endIntron, &acc->endIntron);
addStat(&a->utr3, &acc->utr3);
addStat(&a->splice5, &acc->splice5);
addStat(&a->splice3, &acc->splice3);
addStat(&a->downstream200, &acc->downstream200);
}

struct blatStats *sumStatsList(struct blatStats *list)
/* Return sum of all stats. */
{
struct blatStats *el, *stats;
AllocVar(stats);
for (el = list; el != NULL; el = el->next)
    addStats(el, stats);
return stats;
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
1000 - pslCalcMilliBad(psl, FALSE);
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
int  i, j, blockCount = psl->blockCount, blockStart, blockEnd;
int same, score = pslMilliId(psl), *milliPt;
boolean tIsRc = (psl->strand[1] == '-');
int start, end;
int intersection;

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

int halfAddToStats(struct stat *stat, int hitStart, int hitEnd, 
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

int addToStats(struct stat *stat, int hitStart, int hitEnd, 
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

void pslMakeMilli(struct dnaSeq *geno, struct psl *pslList, int *milliMatches, 
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
		trace = hExtSeq(traceName);
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
struct psl *psl;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char query[256], **row;
int exonCount = gp->exonCount, exonIx, exonStart, exonEnd;
boolean anyMrna = FALSE;

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
for (exonIx = 0; exonIx < exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonEnd = gp->exonEnds[exonIx];
    if (halfAddToStats(&stats->mrnaTotal, exonStart, exonEnd, genoStart, genoEnd, milliMatches) > 0)
	{
	anyMrna = TRUE;
	}
    if (exonStart < gp->cdsStart)	/* UTR */
        {
	struct stat *stat = (gp->strand[0] == '+' ? &stats->utr5 : &stats->utr3);
	int end = min(exonEnd, gp->cdsStart);
	addToStats(stat, exonStart, end, genoStart, genoEnd, milliMatches);
	}
    if (exonEnd > gp->cdsEnd)		/* Other UTR */
        {
	struct stat *stat = (gp->strand[0] == '-' ? &stats->utr5 : &stats->utr3);
	int start = max(exonStart, gp->cdsEnd);
	addToStats(stat, start, exonEnd, genoStart, genoEnd, milliMatches);
	}
    if (exonStart <= gp->cdsStart && exonEnd >= gp->cdsEnd)  /* Single exon CDS */
        {
	addToStats(&stats->onlyCds, gp->cdsStart, gp->cdsEnd, 
		genoStart, genoEnd, milliMatches);
	}
    else if (exonStart <= gp->cdsStart && exonEnd > gp->cdsStart)
        {
	struct stat *stat = (gp->strand[0] == '+' ? &stats->firstCds : &stats->endCds);
	addToStats(stat, gp->cdsStart, exonEnd, genoStart, genoEnd, milliMatches);
	}
    else if (exonStart < gp->cdsEnd && exonEnd >= gp->cdsEnd)
        {
	struct stat *stat = (gp->strand[0] == '-' ? &stats->firstCds : &stats->endCds);
	addToStats(stat, exonStart, gp->cdsEnd, genoStart, genoEnd, milliMatches);
	}
    else
        {
	addToStats(&stats->middleCds, exonStart, exonEnd, genoStart, genoEnd, milliMatches);
	}
    }
if (anyMrna)
    stats->mrnaTotal.hits += 1;
stats->mrnaTotal.features += 1;

/* Gather stats on introns and splice sites. */
for (exonIx = 1; exonIx < exonCount; ++exonIx)
    {
    int intronStart = gp->exonEnds[exonIx-1];
    int intronEnd = gp->exonStarts[exonIx];
    int spliceSize = 10;
    struct stat *stat;
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
hFreeConn(&conn);
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


struct geneIsoforms *getChromGenes(char *chrom, struct hash *nmToGeneHash)
/* Get list of genes in this chromosome with isoforms bundled together. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
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

boolean findSplitTable(char *chrom, char *splitTable, char *table)
/* Look to see if 'origTable' is in database.   If it's
 * not then shift to chrN_table.  Returns TRUE if table is split. */
{
if (hTableExists(splitTable))
    {
    strcpy(table, splitTable);
    return FALSE;
    }
else
    {
    sprintf(table, "%s_%s", chrom, splitTable);
    return TRUE;
    }
}

struct psl *getChromPsl(char *chrom, char *splitTable)
/* Get all alignments for chromosome sorted by chromosome
 * start position. */
{
char table[64];
char query[256], **row;
struct sqlResult *sr;
struct psl *pslList = NULL, *psl;
boolean isSplit = findSplitTable(chrom, splitTable, table);
if (hTableExists(table))
    {
    struct sqlConnection *conn = hAllocConn();
    if (isSplit)
	sprintf(query, "select * from %s", table);
    else
	sprintf(query, "select * from %s and qName = '%s'", table, chrom);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row);
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

struct bed *getChromBed(char *chrom, char *splitTable)
/* Get all alignments for chromosome sorted by chromosome
 * start position. */
{
char table[64];
char query[256], **row;
struct sqlResult *sr;
struct bed *bedList = NULL, *bed;
boolean isSplit = findSplitTable(chrom, splitTable, table);

if (hTableExists(table))
    {
    struct sqlConnection *conn = hAllocConn();
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
return bedList;
}



struct blatStats *chromStats(char *chrom, struct hash *nmToGeneHash, char *table)
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
struct dnaSeq *traceList = NULL, *trace = NULL;

hNibForChrom(chrom, nibName);
nibOpenVerify(nibName, &nibFile, &chromSize);
if (isPsl)
    itemList = pslList = getChromPsl(chrom, table);
else
    itemList = bedList = getChromBed(chrom, table);

AllocVar(stats);
giList = getChromGenes(chrom, nmToGeneHash);
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
	pslMakeMilli(geno, itemList, milliMatches, chrom, startRegion, endRegion, traceHash, &traceList);
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

struct hash *makeNmToGeneHash()
/* Make a hash that maps refSeq genes to their common names. */
{
struct sqlConnection *conn = hAllocConn();
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

hSetDb(database);
nmToGeneHash = makeNmToGeneHash();
if (sameWord(clChrom, "all"))
    allChroms = hAllChromNames();
else
    allChroms = newSlName(clChrom);
slReverse(&allChroms);
for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
    {
    stats = chromStats(chrom->name, nmToGeneHash, table);
    reportStats(f, stats, chrom->name);
    fprintf(f, "\n");
    slAddHead(&statsList, stats);
    }
slReverse(statsList);
sumStats = sumStatsList(statsList);
if (sameWord(clChrom, "all"))
    {
    reportStats(f, sumStats, "total");
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
