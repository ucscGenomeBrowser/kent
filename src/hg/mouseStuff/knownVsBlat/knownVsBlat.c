/* knownVsBlat - Categorize BLAT mouse hits to known genes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "psl.h"
#include "genePred.h"
#include "hdb.h"
#include "refLink.h"
#include "nib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "knownVsBlat - Categorize BLAT mouse hits to known genes\n"
  "usage:\n"
  "   knownVsBlat database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
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
    struct stat utr5;		/* 5' UTR. */
    struct stat firstCds;	/* Coding part of first coding exon. */
    struct stat firstIntron;	/* First intron. */
    struct stat middleCds;	/* Middle coding exons. */
    struct stat middleIntron;	/* Middle introns. */
    struct stat endCds;		/* Coding part of last coding exon. */
    struct stat endIntron;	/* Last intron. */
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
fprintf(f, "%s\t%d/%d (%4.2f%%)\t%d/%d (%4.2f%%)\t%4.2f\n",
	name,  stat->hits, stat->features, divAsPercent(stat->hits, stat->features),
	stat->basesPainted, stat->basesTotal, 
	divAsPercent(stat->basesPainted, stat->basesTotal),
	divAsPercent(stat->cumIdRatio, stat->basesTotal));
}

void reportStats(FILE *f, struct blatStats *stats)
/* Print out stats. */
{
reportStat(f, "upstream100", &stats->upstream100);
reportStat(f, "upstream200", &stats->upstream200);
reportStat(f, "upstream400", &stats->upstream400);
reportStat(f, "upstream800", &stats->upstream800);
reportStat(f, "utr5", &stats->utr5);
reportStat(f, "firstCds", &stats->firstCds);
reportStat(f, "firstIntron", &stats->firstIntron);
reportStat(f, "middleCds", &stats->middleCds);
reportStat(f, "middleIntron", &stats->middleIntron);
reportStat(f, "endCds", &stats->endCds);
reportStat(f, "endIntron", &stats->endIntron);
reportStat(f, "utr3", &stats->utr3);
reportStat(f, "downstream200", &stats->downstream200);
}

void addStat(struct stat *a, struct stat *acc)
/* Add stats from a into acc. */
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
addStat(&a->utr5, &acc->utr5);
addStat(&a->firstCds, &acc->firstCds);
addStat(&a->firstIntron, &acc->firstIntron);
addStat(&a->middleCds, &acc->middleCds);
addStat(&a->middleIntron, &acc->middleIntron);
addStat(&a->endCds, &acc->endCds);
addStat(&a->endIntron, &acc->endIntron);
addStat(&a->utr3, &acc->utr3);
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

struct blatStats *geneStats(char *chrom, 
	char *nibName, FILE *nibFile, int chromSize,
	struct genePred *gp)
/* Figure out how BLAT hits gene and return resulting stats. */
{
int extraBefore = 800;
int extraAfter = 200;
int startRegion, endRegion;
int sizeRegion;
struct dnaSeq *geno;
struct blatStats *stats;

AllocVar(stats);

/* Expand region around gene a little and load corresponding sequence. */
startRegion = gp->txStart - extraBefore;
if (startRegion < 0) startRegion = 0;
endRegion = gp->txEnd + extraAfter;
if (endRegion > chromSize)
    endRegion = chromSize;
sizeRegion = endRegion - startRegion;
geno = nibLdPart(nibName, nibFile, chromSize, startRegion, sizeRegion);

freeDnaSeq(&geno);
return stats;
}

struct geneIsoforms
/* A list of isoforms for each gene. */
    {
    struct geneIsoforms *next;
    char *name;			/* Name of gene. Not allocated here. */
    struct genePred *gpList;	/* List of isoforms. */
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
    if ((gi = hashFindVal(geneHash, geneName)) == NULL)
        {
	AllocVar(gi);
	slAddHead(&giList, gi);
	gi->name = geneName;
	hashAdd(geneHash, geneName, gi);
	}
    slAddTail(&gi->gpList, gp);
    }

freeHash(&geneHash);
hFreeConn(&conn);
    {
    int maxIsoforms = 0;
    char *maxIsoGene = NULL;
    for (gi = giList; gi != NULL; gi = gi->next)
	{
	int count = slCount(gi->gpList);
	if (count > maxIsoforms) 
	    {
	    maxIsoforms = count;
	    maxIsoGene = gi->name;
	    }
	}
    uglyf("Maximum isoforms on %s is %d on %s\n", chrom, maxIsoforms, maxIsoGene);
    }
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

struct blatStats *chromStats(char *chrom, struct hash *nmToGeneHash)
/* Produce stats for one chromosome. Just consider longest isoform
 * of each gene. */
{
struct blatStats *stats, *gStats;
struct geneIsoforms *giList = NULL, *gi;
struct genePred *gp;
char nibName[512];
FILE *nibFile;
int chromSize;

hNibForChrom(chrom, nibName);
nibOpenVerify(nibName, &nibFile, &chromSize);
AllocVar(stats);
giList = getChromGenes(chrom, nmToGeneHash);
for (gi = giList; gi != NULL; gi = gi->next)
    {
    gp = longestIsoform(gi);
    gStats = geneStats(chrom, nibName, nibFile, chromSize, gp);
    addStats(gStats, stats);
    freez(&gStats);
    }
geneIsoformsFreeList(&giList);
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

void knownVsBlat(char *database)
/* knownVsBlat - Categorize BLAT mouse hits to known genes. */
{
struct slName *allChroms, *chrom;
struct blatStats *statsList = NULL, *stats, *sumStats;
struct hash *nmToGeneHash;

hSetDb(database);
nmToGeneHash = makeNmToGeneHash();
allChroms = hAllChromNames();
for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
    {
    stats = chromStats(chrom->name, nmToGeneHash);
    slAddHead(&statsList, stats);
    break;	/* uglyf */
    }
slReverse(statsList);
sumStats = sumStatsList(statsList);
reportStats(stdout, sumStats);
freeHashAndVals(&nmToGeneHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
knownVsBlat(argv[1]);
return 0;
}
