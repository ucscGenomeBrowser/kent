/* edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and 
 * store in database.. */

#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "basicBed.h"
#include "options.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "cheapcgi.h"
#include "genomeRangeTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeEnrichments - Scan through database and make a makefile to calc. enrichments and \n"
  "store in database.\n"
  "usage:\n"
  "   edwMakeEnrichments startFileId endFileId\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

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

void doEnrichmentsFromSampleBed(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, 
    struct edwAssembly *assembly, struct edwQaEnrichTarget *targetList, 
    struct genomeRangeTree *grtList)
/* Figure out enrichments from sample bed file. */
{
char *sampleBed = vf->sampleBed;
if (isEmpty(sampleBed))
    {
    warn("No sample bed for %s", ef->edwFileName);
    return;
    }
struct bed3 *sample, *sampleList = bed3LoadAll(sampleBed);
long long sampleCount = slCount(sampleList), sampleBases = bed3TotalSize(sampleList);
verbose(1, "Got %lld items totalling %lld bases in %s\n", sampleCount, sampleBases, sampleBed);
struct genomeRangeTree *sampleGrt = grtFromBed3List(sampleList);
struct hashEl *chrom, *chromList = hashElListHash(sampleGrt->hash);
struct edwQaEnrichTarget *target;
struct genomeRangeTree *grt;
for (target = targetList, grt=grtList; target != NULL; target = target->next, grt=grt->next)
    {
    long long uniqOverlapBases = 0;
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	struct rbTree *sampleTree = chrom->val;
	struct rbTree *targetTree = genomeRangeTreeFindRangeTree(grt, chrom->name);
	if (targetTree != NULL)
	    {
	    struct range *range, *rangeList = rangeTreeList(sampleTree);
	    /* Do unique base overlap counts (since using range tree) */
	    for (range = rangeList; range != NULL; range = range->next)
		{
		int overlap = rangeTreeOverlapSize(targetTree, range->start, range->end);
		uniqOverlapBases += overlap;
		}
	    }
	}
    long long overlapBases = 0;
    for (sample = sampleList; sample != NULL; sample = sample->next)
        {
	int overlap = genomeRangeTreeOverlapSize(grt, 
	    sample->chrom, sample->chromStart, sample->chromEnd);
	overlapBases += overlap;
	}

    /* Fix up edwQaEnrich data structure */
    struct edwQaEnrich *enrich;
    AllocVar(enrich);
    enrich->fileId = ef->id;
    enrich->qaEnrichTargetId = target->id;
    enrich->targetBaseHits = overlapBases;
    enrich->targetUniqHits = uniqOverlapBases;
    enrich->coverage = (double)uniqOverlapBases/target->targetSize;
    double sampleSizeFactor = vf->itemCount /vf->sampleCount;
    double sampleTargetDepth = (double)overlapBases/target->targetSize;
    enrich->enrichment = sampleSizeFactor * sampleTargetDepth / vf->depth;
    enrich->uniqEnrich = enrich->coverage / vf->sampleCoverage;

    /* Save to database. */
    edwQaEnrichSaveToDb(conn, enrich, "edwQaEnrich", 128);
    }
genomeRangeTreeFree(&sampleGrt);
}

void doEnrichments(struct sqlConnection *conn, struct edwFile *ef, char *path,
    struct edwQaEnrichTarget *targetList, struct genomeRangeTree *grtList)
/* Calculate enrichments on for all targets file. The targetList and the
 * grtList are in the same order. */
{
char query[256];
safef(query, sizeof(query), "select * from edwValidFile where fileId=%d", ef->id);
struct edwValidFile *vf = edwValidFileLoadByQuery(conn, query);
if (vf == NULL)
    return;	/* We can only work if have validFile table entry */

char *format = vf->format;
char *ucscDb = vf->ucscDb;
safef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %s", ucscDb);
if (sameString(format, "fastq"))
    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList, grtList);
else if (sameString(format, "bigWig"))
    verbose(2,"Got bigWig\n");
else if (sameString(format, "bigBed"))
    verbose(2,"Got bigBed\n");
else if (sameString(format, "narrowPeak"))
    verbose(2,"Got narrowPeak.bigBed\n");
else if (sameString(format, "broadPeak"))
    verbose(2,"Got broadPeak.bigBed\n");
else if (sameString(format, "bam"))
    doEnrichmentsFromSampleBed(conn, ef, vf, assembly, targetList, grtList);
else if (sameString(format, "unknown"))
    verbose(2, "Unknown format in doEnrichments(%s), that's chill.", ef->edwFileName);
else
    errAbort("Unrecognized format %s in doEnrichments(%s)", format, path);

}


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
uglyf("doing %d files with id's between %d and %d\n", slCount(fileList), startFileId, endFileId);

/* Get list of all targets. */
struct edwQaEnrichTarget *target, *targetList = 
    edwQaEnrichTargetLoadByQuery(conn, "select * from edwQaEnrichTarget");
uglyf("Got %d targets.\n", slCount(targetList));

/* Load range trees for all targets. */
struct genomeRangeTree *grt, *grtList=NULL;
for (target = targetList; target != NULL; target = target->next)
    {
    char *targetBed = edwPathForFileId(conn, target->fileId);
    grt = grtFromSimpleBed(targetBed);
    slAddHead(&grtList, grt);
    freeMem(targetBed);
    }
slReverse(&grtList);

for (file = fileList; file != NULL; file = file->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, file->edwFileName);
    uglyf("processing %s aka %s\n", file->submitFileName, path);

    if (file->tags) // All ones we care about have tags
        {
	doEnrichments(conn, file, path, targetList, grtList);
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
