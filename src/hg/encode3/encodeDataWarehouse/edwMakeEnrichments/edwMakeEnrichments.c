/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and 
 * store in database.  Note to compare with featureBits enrichments use -countGaps 
 * in featureBits */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "basicBed.h"
#include "options.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "bits.h"
#include "portable.h"
#include "localmem.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "bigWig.h"
#include "genomeRangeTree.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and \n"
  "store in database. Enrichments similar to 'featureBits -countGaps' enrichments.\n"
  "usage:\n"
  "   edwMakeEnrichments startFileId endFileId\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct target
/* Information about a target */
    {
    struct target *next;
    struct edwQaEnrichTarget *target;  /* The database target structure */
    struct genomeRangeTree *grt;       /* Random access intersection structure for target */
    double overlapBases;	       /* Sum of all overlaps with target. */
    long long uniqOverlapBases;	       /* Sum of unique overlaps with target. */
    boolean skip;		       /* If true skip calcs on this one. */
    };

struct target *targetNew(struct edwQaEnrichTarget *et, struct genomeRangeTree *grt)
/* Make a little local target object */
{
struct target *target;
AllocVar(target);
target->target = et;
target->grt = grt;
return target;
}

struct edwQaEnrich *enrichFromOverlaps(struct edwFile *ef, struct edwValidFile *vf,
	struct edwAssembly *assembly, struct target *target, 
	long long overlapBases, long long uniqOverlapBases)
/* Build up enrichment structure based on overlaps between file and target */
{
struct edwQaEnrich *enrich;
AllocVar(enrich);
long long targetSize = target->target->targetSize;
enrich->fileId = ef->id;
enrich->qaEnrichTargetId = target->target->id;
enrich->targetBaseHits = overlapBases;
enrich->targetUniqHits = uniqOverlapBases;
enrich->coverage = (double)uniqOverlapBases/targetSize;
if (vf->sampleCount > 0)
    {
    double sampleSizeFactor = (double)vf->itemCount /vf->sampleCount;
    double sampleTargetDepth = (double)overlapBases/targetSize;
    if (vf->depth > 0)
	{
	enrich->enrichment = sampleSizeFactor * sampleTargetDepth / vf->depth;
	if (vf->sampleCoverage > 0)
	    enrich->uniqEnrich = enrich->coverage / vf->sampleCoverage;
	}
    verbose(2, "%s size %lld (%0.3f%%), %s (%0.3f%%), overlap %lld (%0.3f%%)\n", 
	target->target->name, targetSize, 100.0*targetSize/assembly->baseCount, 
	ef->edwFileName, 100.0*vf->sampleCoverage, 
	uniqOverlapBases, 100.0*uniqOverlapBases/assembly->baseCount);
    }
return enrich;
}

boolean enrichmentExists(struct sqlConnection *conn, struct edwFile *ef, 
    struct edwQaEnrichTarget *target)
/* Return TRUE if already is a an enrichment in database for this combination. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwQaEnrich"
    " where fileId=%lld and qaEnrichTargetId=%d", (long long)ef->id, target->id);
return sqlQuickNum(conn, query) != 0;
}

void doEnrichmentsFromBed3Sample(struct bed3 *sampleList,
    struct sqlConnection *conn,
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Given a bed3 list,  calculate enrichments for targets */
{
struct genomeRangeTree *sampleGrt = edwMakeGrtFromBed3List(sampleList);
struct hashEl *chrom, *chromList = hashElListHash(sampleGrt->hash);

/* Iterate through each target - and in lockstep each associated grt to calculate unique overlap */
struct target *target;
for (target = targetList; target != NULL; target = target->next)
    {
    if (target->skip)
        continue;
    struct genomeRangeTree *grt = target->grt;
    long long uniqOverlapBases = 0;
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	struct rbTree *sampleTree = chrom->val;
	struct rbTree *targetTree = genomeRangeTreeFindRangeTree(grt, chrom->name);
	if (targetTree != NULL)
	    {
	    struct range *range, *rangeList = rangeTreeList(sampleTree);
	    for (range = rangeList; range != NULL; range = range->next)
		{
		/* Do unique base overlap counts (since using range trees both sides) */
		int overlap = rangeTreeOverlapSize(targetTree, range->start, range->end);
		uniqOverlapBases += overlap;
		}
	    }
	}

    /* Figure out how much we overlap allowing same bases in genome
     * to part of more than one overlap. */ 
    long long overlapBases = 0;
    struct bed3 *sample;
    for (sample = sampleList; sample != NULL; sample = sample->next)
        {
	int overlap = genomeRangeTreeOverlapSize(grt, 
	    sample->chrom, sample->chromStart, sample->chromEnd);
	overlapBases += overlap;
	}

    /* Save to database. */
    struct edwQaEnrich *enrich = enrichFromOverlaps(ef, vf, assembly,
	target, overlapBases, uniqOverlapBases);
    edwQaEnrichSaveToDb(conn, enrich, "edwQaEnrich", 128);
    edwQaEnrichFree(&enrich);
    }
genomeRangeTreeFree(&sampleGrt);
hashElFreeList(&chromList);
}

void doEnrichmentsFromSampleBed(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Figure out enrichments from sample bed file. */
{
char *sampleBed = vf->sampleBed;
if (isEmpty(sampleBed))
    {
    warn("No sample bed for %s", ef->edwFileName);
    return;
    }
/* Load sample bed, make a range tree to track unique coverage, and get list of all chroms .*/
struct bed3 *sampleList = bed3LoadAll(sampleBed);
if (sampleList == NULL)
    {
    warn("Sample bed is empty for %s", ef->edwFileName);
    return;
    }
doEnrichmentsFromBed3Sample(sampleList, conn, ef, vf, assembly, targetList);
bed3FreeList(&sampleList);
}

void doEnrichmentsFromBed(struct sqlConnection *conn,
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Figure out enrichments from a bed file. */
{
char *bedPath = edwPathForFileId(conn, ef->id);
struct bed3 *sampleList = bed3LoadAll(bedPath);
doEnrichmentsFromBed3Sample(sampleList, conn, ef, vf, assembly, targetList);
bed3FreeList(&sampleList);
freez(&bedPath);
}

void doEnrichmentsFromBigBed(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Figure out enrichments from a bigBed file. */
{
/* Get path to bigBed, open it, and read all chromosomes. */
char *bigBedPath = edwPathForFileId(conn, ef->id);
struct bbiFile *bbi = bigBedFileOpen(bigBedPath);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);

/* Do a pretty complex loop that just aims to set target->overlapBases and ->uniqOverlapBases
 * for all targets.  This is complicated by just wanting to keep one chromosome worth of
 * bigBed data in memory. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    /* Get list of intervals in bigBed for this chromosome, and feed it to a rangeTree. */
    struct lm *lm = lmInit(0);
    struct bigBedInterval *ivList = bigBedIntervalQuery(bbi, chrom->name, 0, chrom->size, 0, lm);
    struct bigBedInterval *iv;
    struct rbTree *bbTree = rangeTreeNew();
    for (iv = ivList; iv != NULL; iv = iv->next)
	 rangeTreeAdd(bbTree, iv->start, iv->end);
    struct range *bbRange, *bbRangeList = rangeTreeList(bbTree);

    /* Loop through all targets adding overlaps from ivList and unique overlaps from bbRangeList */
    struct target *target;
    for (target = targetList; target != NULL; target = target->next)
        {
	if (target->skip)
	    continue;
	struct genomeRangeTree *grt = target->grt;
	struct rbTree *targetTree = genomeRangeTreeFindRangeTree(grt, chrom->name);
	if (targetTree != NULL)
	    {
	    struct bigBedInterval *iv;
	    for (iv = ivList; iv != NULL; iv = iv->next)
		{
		int overlap = rangeTreeOverlapSize(targetTree, iv->start, iv->end);
		target->overlapBases += overlap;
		}
	    for (bbRange = bbRangeList; bbRange != NULL; bbRange = bbRange->next)
		{
		int overlap = rangeTreeOverlapSize(targetTree, bbRange->start, bbRange->end);
		target->uniqOverlapBases += overlap;
		}
	    }
	}
    rangeTreeFree(&bbTree);
    lmCleanup(&lm);
    }

/* Now loop through targets and save enrichment info to database */
struct target *target;
for (target = targetList; target != NULL; target = target->next)
    {
    if (target->skip)
	continue;
    struct edwQaEnrich *enrich = enrichFromOverlaps(ef, vf, assembly, target, 
	target->overlapBases, target->uniqOverlapBases);
    edwQaEnrichSaveToDb(conn, enrich, "edwQaEnrich", 128);
    edwQaEnrichFree(&enrich);
    }

bbiChromInfoFreeList(&chromList);
bigBedFileClose(&bbi);
freez(&bigBedPath);
}

#ifdef OLDWAY
/* This old way is ~3 times as slow */
void doEnrichmentsFromBigWig(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Figure out enrichments from a bigBed file. */
{
/* Get path to bigBed, open it, and read all chromosomes. */
char *bigWigPath = edwPathForFileId(conn, ef->id);
struct bbiFile *bbi = bigWigFileOpen(bigWigPath);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);

/* This takes a while, so let's figure out what parts take the time. */
long totalBigQueryTime = 0;
long totalOverlapTime = 0;

/* Do a pretty complex loop that just aims to set target->overlapBases and ->uniqOverlapBases
 * for all targets.  This is complicated by just wanting to keep one chromosome worth of
 * bigWig data in memory. Also just for performance we do a lookup of target range tree to
 * get chromosome specific one to use, which avoids a hash lookup in the inner loop. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    /* Get list of intervals in bigWig for this chromosome, and feed it to a rangeTree. */
    struct lm *lm = lmInit(0);
    long startBigQueryTime = clock1000();
    struct bbiInterval *ivList = bigWigIntervalQuery(bbi, chrom->name, 0, chrom->size, lm);
    long endBigQueryTime = clock1000();
    totalBigQueryTime += endBigQueryTime - startBigQueryTime;
    struct bbiInterval *iv;

    /* Loop through all targets adding overlaps from ivList */
    long startOverlapTime = clock1000();
    struct target *target;
    for (target = targetList; target != NULL; target = target->next)
        {
	struct genomeRangeTree *grt = target->grt;
	struct rbTree *targetTree = genomeRangeTreeFindRangeTree(grt, chrom->name);
	if (targetTree != NULL)
	    {
	    for (iv = ivList; iv != NULL; iv = iv->next)
		{
		int overlap = rangeTreeOverlapSize(targetTree, iv->start, iv->end);
		target->uniqOverlapBases += overlap;
		target->overlapBases += overlap * iv->val;
		}
	    }
	}
    long endOverlapTime = clock1000();
    totalOverlapTime += endOverlapTime - startOverlapTime;
    lmCleanup(&lm);
    }

verbose(1, "totalBig %0.3f, totalOverlap %0.3f\n", 0.001*totalBigQueryTime, 0.001*totalOverlapTime);

/* Now loop through targets and save enrichment info to database */
struct target *target;
for (target = targetList; target != NULL; target = target->next)
    {
    struct edwQaEnrich *enrich = enrichFromOverlaps(ef, vf, assembly, target, 
	target->overlapBases, target->uniqOverlapBases);
    edwQaEnrichSaveToDb(conn, enrich, "edwQaEnrich", 128);
    edwQaEnrichFree(&enrich);
    }

bbiChromInfoFreeList(&chromList);
bigWigFileClose(&bbi);
freez(&bigWigPath);
}
#endif /* OLDWAY */


void doEnrichmentsFromBigWig(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Figure out enrichments from a bigBed file. */
{
/* Get path to bigBed, open it, and read all chromosomes. */
char *bigWigPath = edwPathForFileId(conn, ef->id);
struct bbiFile *bbi = bigWigFileOpen(bigWigPath);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
struct bigWigValsOnChrom *valsOnChrom = bigWigValsOnChromNew();

/* This takes a while, so let's figure out what parts take the time. */
long totalBigQueryTime = 0;
long totalOverlapTime = 0;

/* Do a pretty complex loop that just aims to set target->overlapBases and ->uniqOverlapBases
 * for all targets.  This is complicated by just wanting to keep one chromosome worth of
 * bigWig data in memory. Also just for performance we do a lookup of target range tree to
 * get chromosome specific one to use, which avoids a hash lookup in the inner loop. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    long startBigQueryTime = clock1000();
    boolean gotData = bigWigValsOnChromFetchData(valsOnChrom, chrom->name, bbi);
    long endBigQueryTime = clock1000();
    totalBigQueryTime += endBigQueryTime - startBigQueryTime;
    if (gotData)
	{
	double *valBuf = valsOnChrom->valBuf;
	Bits *covBuf = valsOnChrom->covBuf;

	/* Loop through all targets adding overlaps from ivList */
	long startOverlapTime = clock1000();
	struct target *target;
	for (target = targetList; target != NULL; target = target->next)
	    {
	    if (target->skip)
		continue;
	    struct genomeRangeTree *grt = target->grt;
	    struct rbTree *targetTree = genomeRangeTreeFindRangeTree(grt, chrom->name);
	    if (targetTree != NULL)
		{
		struct range *range, *rangeList = rangeTreeList(targetTree);
		for (range = rangeList; range != NULL; range = range->next)
		    {
		    int s = range->start, e = range->end, i;
		    for (i=s; i<=e; ++i)
		        {
			if (bitReadOne(covBuf, i))
			    {
			    double x = valBuf[i];
			    target->uniqOverlapBases += 1;
			    target->overlapBases += x;
			    }
			}
		    }
		}
	    }
	long endOverlapTime = clock1000();
	totalOverlapTime += endOverlapTime - startOverlapTime;
	}
    }

verbose(1, "totalBig %0.3f, totalOverlap %0.3f\n", 0.001*totalBigQueryTime, 0.001*totalOverlapTime);

/* Now loop through targets and save enrichment info to database */
struct target *target;
for (target = targetList; target != NULL; target = target->next)
    {
    if (target->skip)
	continue;
    struct edwQaEnrich *enrich = enrichFromOverlaps(ef, vf, assembly, target, 
	target->overlapBases, target->uniqOverlapBases);
    edwQaEnrichSaveToDb(conn, enrich, "edwQaEnrich", 128);
    edwQaEnrichFree(&enrich);
    }

bigWigValsOnChromFree(&valsOnChrom);
bbiChromInfoFreeList(&chromList);
bigWigFileClose(&bbi);
freez(&bigWigPath);
}

struct target *targetsForAssembly(struct sqlConnection *conn, struct edwAssembly *assembly)
/* Get list of enrichment targets for given assembly */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from edwQaEnrichTarget where assemblyId=%d", assembly->id);
struct edwQaEnrichTarget *et, *etList = edwQaEnrichTargetLoadByQuery(conn, query);

/* Wrap a new structure around the enrichment targets where we'll store summary info. */
struct target *target, *targetList = NULL, **targetTail = &targetList;
for (et = etList; et != NULL; et = et->next)
    {
    char *targetBed = edwPathForFileId(conn, et->fileId);
    struct genomeRangeTree *grt = edwGrtFromBigBed(targetBed);
    target = targetNew(et, grt);
    *targetTail = target;
    targetTail = &target->next;
    freez(&targetBed);
    }
return targetList;
}

void doEnrichments(struct sqlConnection *conn, struct edwFile *ef, char *path, 
    struct hash *assemblyToTarget)
/* Calculate enrichments on for all targets file. The targetList and the
 * grtList are in the same order. */
{
/* Get validFile from database. */
struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
if (vf == NULL)
    return;	/* We can only work if have validFile table entry */

if (!isEmpty(vf->enrichedIn) && !sameWord(vf->ucscDb, "unknown") 
    && !sameWord(vf->format, "unknown"))
    {
    /* Get our assembly */
    char *format = vf->format;
    char *ucscDb = vf->ucscDb;
    char *targetName = edwSimpleAssemblyName(ucscDb);
    struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, targetName);

    struct target *targetList = hashFindVal(assemblyToTarget, assembly->name);
    if (targetList == NULL)
	{
	targetList = targetsForAssembly(conn, assembly);
	if (targetList == NULL)
	    errAbort("No targets for assembly %s", assembly->name);
	hashAdd(assemblyToTarget, assembly->name, targetList);
	}

    /* Loop through targetList zeroing out existing ovelaps. */
    struct target *target;
    boolean allSkip = TRUE;
    for (target = targetList; target != NULL; target = target->next)
	{
	target->overlapBases = target->uniqOverlapBases = 0;
	target->skip = enrichmentExists(conn, ef, target->target);
	if (!target->skip)
	    allSkip = FALSE;
	}

    /* Do a big dispatch based on format. */
    if (!allSkip)
	{
	if (sameString(format, "fastq"))
	    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList);
	else if (sameString(format, "bigWig"))
	    doEnrichmentsFromBigWig(conn, ef, vf, assembly, targetList);
	else if (startsWith("bed_", format))
	    doEnrichmentsFromBed(conn, ef, vf, assembly, targetList);
	else if (edwIsSupportedBigBedFormat(format) || sameString(format, "bigBed"))
	    doEnrichmentsFromBigBed(conn, ef, vf, assembly, targetList);
	else if (sameString(format, "gtf"))
	    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList);
	else if (sameString(format, "gff"))
	    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList);
	else if (sameString(format, "bam"))
	    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList);
	else if (sameString(format, "idat"))
	    verbose(2, "Ignoring idat %s, in doEnrichments.", ef->edwFileName);
	else if (sameString(format, "customTrack"))
	    verbose(2, "Ignoring customTrack %s, in doEnrichments.", ef->edwFileName);
	else if (sameString(format, "rcc"))
	    verbose(2, "Ignoring rcc %s, in doEnrichments.", ef->edwFileName);
	else if (sameString(format, "unknown"))
	    verbose(2, "Unknown format in doEnrichments(%s), that's chill.", ef->edwFileName);
	else
	    errAbort("Unrecognized format %s in doEnrichments(%s)", format, path);
	}

    /* Clean up and go home. */
    edwAssemblyFree(&assembly);
    }
edwValidFileFree(&vf);
}


void edwMakeEnrichments(int startFileId, int endFileId)
/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and store 
 * in database. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startFileId, endFileId);

/* Make up a hash for targets keyed by assembly name. */
struct hash *assemblyToTarget = hashNew(0);

for (ef = efList; ef != NULL; ef = ef->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);
    verbose(1, "%lld processing %s aka %s\n", (long long)ef->id, ef->submitFileName, path);

    if (ef->tags) // All ones we care about have tags
	doEnrichments(conn, ef, path, assemblyToTarget);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwMakeEnrichments(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
