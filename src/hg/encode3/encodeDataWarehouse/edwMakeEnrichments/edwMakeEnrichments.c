/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and 
 * store in database.  Note to compare with featureBits enrichments use -countGaps 
 * in featureBits */

#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "basicBed.h"
#include "options.h"
#include "jksql.h"
#include "cheapcgi.h"
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

char *edwPathForFileId(struct sqlConnection *conn, long long fileId)
/* Return full path (which eventually should be freeMem'd) for fileId */
{
char query[256];
char fileName[PATH_LEN];
safef(query, sizeof(query), "select edwFileName from edwFile where id=%lld", fileId);
sqlNeedQuickQuery(conn, query, fileName, sizeof(fileName));
char path[512];
safef(path, sizeof(path), "%s%s", edwRootDir, fileName);
return cloneString(path);
}

struct genomeRangeTree *grtFromBed3List(struct bed3 *bedList)
/* Make up a genomeRangeTree around bed file. */
{
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct bed3 *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    genomeRangeTreeAdd(grt, bed->chrom, bed->chromStart, bed->chromEnd);
return grt;
}

struct genomeRangeTree *grtFromSimpleBed(char *fileName)
/* Return genome range tree for simple (unblocked) bed */
{
struct bed3 *bedList = bed3LoadAll(fileName);
struct genomeRangeTree *grt = grtFromBed3List(bedList);
bed3FreeList(&bedList);
return grt;
}

struct target
/* Information about a target */
    {
    struct target *next;
    struct edwQaEnrichTarget *target;
    struct genomeRangeTree *grt;
    long long overlapBases;
    long long uniqOverlapBases;
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
double sampleSizeFactor = (double)vf->itemCount /vf->sampleCount;
double sampleTargetDepth = (double)overlapBases/targetSize;
enrich->enrichment = sampleSizeFactor * sampleTargetDepth / vf->depth;
enrich->uniqEnrich = enrich->coverage / vf->sampleCoverage;
verbose(2, "%s size %lld (%0.3f%%), %s (%0.3f%%), overlap %lld (%0.3f%%)\n", 
    target->target->name, targetSize, 100.0*targetSize/assembly->baseCount, 
    ef->edwFileName, 100.0*vf->sampleCoverage, 
    uniqOverlapBases, 100.0*uniqOverlapBases/assembly->baseCount);
return enrich;
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
struct bed3 *sample, *sampleList = bed3LoadAll(sampleBed);
struct genomeRangeTree *sampleGrt = grtFromBed3List(sampleList);
struct hashEl *chrom, *chromList = hashElListHash(sampleGrt->hash);

/* Iterate through each target - and in lockstep each associated grt to calculate unique overlap */
struct target *target;
for (target = targetList; target != NULL; target = target->next)
    {
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
    struct edwQaEnrich *enrich = enrichFromOverlaps(ef, vf, assembly, target, 
	target->overlapBases, target->uniqOverlapBases);
    edwQaEnrichSaveToDb(conn, enrich, "edwQaEnrich", 128);
    edwQaEnrichFree(&enrich);
    }

bbiChromInfoFreeList(&chromList);
bigBedFileClose(&bbi);
freez(&bigBedPath);
}

void doEnrichmentsFromBigWig(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct target *targetList)
/* Figure out enrichments from a bigBed file. */
{
/* Get path to bigBed, open it, and read all chromosomes. */
char *bigWigPath = edwPathForFileId(conn, ef->id);
struct bbiFile *bbi = bigWigFileOpen(bigWigPath);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);

/* Do a pretty complex loop that just aims to set target->overlapBases and ->uniqOverlapBases
 * for all targets.  This is complicated by just wanting to keep one chromosome worth of
 * bigWig data in memory. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    /* Get list of intervals in bigWig for this chromosome, and feed it to a rangeTree. */
    struct lm *lm = lmInit(0);
    struct bbiInterval *ivList = bigWigIntervalQuery(bbi, chrom->name, 0, chrom->size, lm);
    struct bbiInterval *iv;

    /* Loop through all targets adding overlaps from ivList */
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
    lmCleanup(&lm);
    }

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

struct target *targetsForAssembly(struct sqlConnection *conn, struct edwAssembly *assembly)
/* Get list of enrichment targets for given assembly */
{
char query[128];
safef(query, sizeof(query), "select * from edwQaEnrichTarget where assemblyId=%d", assembly->id);
struct edwQaEnrichTarget *et, *etList = edwQaEnrichTargetLoadByQuery(conn, query);

/* Wrap a new structure around the enrichment targets where we'll store summary info. */
struct target *target, *targetList = NULL, **targetTail = &targetList;
for (et = etList; et != NULL; et = et->next)
    {
    char *targetBed = edwPathForFileId(conn, et->fileId);
    struct genomeRangeTree *grt = grtFromSimpleBed(targetBed);
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
char query[256];
safef(query, sizeof(query), "select * from edwValidFile where fileId=%d", ef->id);
struct edwValidFile *vf = edwValidFileLoadByQuery(conn, query);
if (vf == NULL)
    return;	/* We can only work if have validFile table entry */

/* Get our assembly */
char *format = vf->format;
char *ucscDb = vf->ucscDb;
safef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %s", ucscDb);

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
for (target = targetList; target != NULL; target = target->next)
    target->overlapBases = target->uniqOverlapBases = 0;

/* Do a big dispatch based on format. */
if (sameString(format, "fastq"))
    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList);
else if (sameString(format, "bigWig"))
    doEnrichmentsFromBigWig(conn, ef, vf, assembly, targetList);
else if (sameString(format, "bigBed"))
    doEnrichmentsFromBigBed(conn, ef, vf, assembly, targetList);
else if (sameString(format, "narrowPeak"))
    doEnrichmentsFromBigBed(conn, ef, vf, assembly, targetList);
else if (sameString(format, "broadPeak"))
    doEnrichmentsFromBigBed(conn, ef, vf, assembly, targetList);
else if (sameString(format, "bam"))
    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList);
else if (sameString(format, "unknown"))
    verbose(2, "Unknown format in doEnrichments(%s), that's chill.", ef->edwFileName);
else
    errAbort("Unrecognized format %s in doEnrichments(%s)", format, path);

/* Clean up and go home. */
edwAssemblyFree(&assembly);
edwValidFileFree(&vf);
}


void edwMakeEnrichments(int startFileId, int endFileId)
/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and store 
 * in database. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and md5 != '' and deprecated = ''", 
    startFileId, endFileId);
struct edwFile *file, *fileList = edwFileLoadByQuery(conn, query);

/* Make up a hash for targets keyed by assembly name. */

struct hash *assemblyToTarget = hashNew(0);

for (file = fileList; file != NULL; file = file->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, file->edwFileName);
    verbose(1, "processing %s aka %s\n", file->submitFileName, path);

    if (file->tags) // All ones we care about have tags
        {
	doEnrichments(conn, file, path, assemblyToTarget);
	}
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
