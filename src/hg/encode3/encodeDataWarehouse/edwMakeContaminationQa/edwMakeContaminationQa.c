/* edwMakeContaminationQa - Screen for contaminants by aligning against contaminant genomes.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "options.h"
#include "sqlNum.h"
#include "jksql.h"
#include "basicBed.h"
#include "genomeRangeTree.h"
#include "correlate.h"
#include "hmmstats.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeContaminationQa - Screen for contaminants by aligning against contaminant genomes.\n"
  "usage:\n"
  "   edwMakeContaminationQa startId endId\n"
  "where startId and endId are id's in the edwFile table\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct edwFile *edwFileLoadIdRange(struct sqlConnection *conn, long long startId, long long endId)
/* Return list of all files in given id range */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from edwFile where id>=%lld and id<=%lld and endUploadTime != 0 "
    "and updateTime != 0 and deprecated = ''", 
    startId, endId);
return edwFileLoadByQuery(conn, query);
}

void alignFastqMakeBed(struct edwFile *ef, struct edwAssembly *assembly,
    char *fastqPath, struct edwValidFile *vf, FILE *bedF)
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. 
 * Update vf->mapRatio and related fields. */
{
edwAlignFastqMakeBed(ef, assembly, fastqPath, vf, bedF, 
    &vf->mapRatio, &vf->depth, &vf->sampleCoverage);
}

int mustMkstemp(char *template)
/* Call mkstemp to make a temp file with name based on template (which is altered)
 * by the call to be the file name.   Return unix file descriptor. */
{
int fd = mkstemp(template);
if (fd == -1)
    errnoAbort("Couldn't make temp file based on %s", template);
return fd;
}

void makeTempFastqSample(char *source, int size, char dest[PATH_LEN])
/* Copy size records from source into a new temporary dest.  Fills in dest */
{
/* Make temporary file to save us a unique place in file system. */
safef(dest, PATH_LEN, "%sedwSampleFastqXXXXXX", edwTempDir());
int fd = mustMkstemp(dest);
close(fd);

char command[3*PATH_LEN];
safef(command, sizeof(command), 
    "fastqStatsAndSubsample %s /dev/null %s -sampleSize=%d", source, dest, size);
verbose(2, "command: %s\n", command);
mustSystem(command);
}

#define FASTQ_SAMPLE_SIZE 100000

int edwQaContamMade(struct sqlConnection *conn, long long fileId, int targetId)
/* Return number of times have fileId paired with targetId in edwQaContam table. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwQaContam where fileId=%lld and qaContamTargetId=%d",
    fileId, targetId);
return sqlQuickNum(conn, query);
}

struct edwQaContamTarget *getContamTargets(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf)
/* Get list of contamination targets for file - basically all targets that aren't in same
 * taxon as self. */
{
assert(vf->ucscDb != NULL);
struct edwAssembly *origAsm = edwAssemblyForUcscDb(conn, vf->ucscDb);
assert(origAsm != NULL);
char query[256];
sqlSafef(query, sizeof(query), 
    "select edwQaContamTarget.* from edwQaContamTarget,edwAssembly "
    "where edwQaContamTarget.assemblyId = edwAssembly.id "
         " and edwAssembly.taxon != %d", origAsm->taxon);
struct edwQaContamTarget *targetList  = edwQaContamTargetLoadByQuery(conn, query);
edwAssemblyFree(&origAsm);
return targetList;
}

void screenFastqForContaminants(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf)
/* The ef/vf point to same file, which is fastq format.  Set alignments up for a sample against all
 * contamination targets. */
{
/* Get target list and see if we have any work to do. */
struct edwQaContamTarget *target, *targetList;
targetList = getContamTargets(conn, ef, vf);
boolean needScreen = FALSE;
for (target = targetList; target != NULL; target = target->next)
    {
    if (edwQaContamMade(conn, ef->id, target->id) <= 0)
        {
	needScreen = TRUE;
	break;
	}
    }

if (needScreen)
    {
    verbose(1, "screenFastqForContaminants(%u(%s))\n", ef->id, ef->submitFileName);
    char sourceFastqName[PATH_LEN];
    safef(sourceFastqName, PATH_LEN, "%s%s", edwRootDir, ef->edwFileName);
    char query[512];

    /* Create downsampled fastq in temp directory. */
    char sampleFastqName[PATH_LEN];
    makeTempFastqSample(sourceFastqName, FASTQ_SAMPLE_SIZE, sampleFastqName);
    verbose(1, "downsampled %s into %s\n", vf->licensePlate, sampleFastqName);

    for (target = targetList; target != NULL; target = target->next)
	{
	/* Get assembly associated with target */
	int assemblyId = target->assemblyId;
	sqlSafef(query, sizeof(query), "select * from edwAssembly where id=%d", assemblyId);
	struct edwAssembly *newAsm = edwAssemblyLoadByQuery(conn, query);
	if (newAsm == NULL)
	    errAbort("warehouse edwQaContamTarget %d not found", assemblyId);

	/* If we don't already have a match, do work to create contam record. */
	int matchCount = edwQaContamMade(conn, ef->id, target->id);
	if (matchCount <= 0)
	    {
	    /* We run the bed-file maker, just for side effect calcs. */
	    double mapRatio = 0, depth = 0, sampleCoverage = 0;
	    edwAlignFastqMakeBed(ef, newAsm, sampleFastqName, vf, NULL,
		&mapRatio, &depth, &sampleCoverage);

	    verbose(1, "%s mapRatio %g, depth %g, sampleCoverage %g\n", 
		newAsm->name, mapRatio, depth, sampleCoverage);
	    struct edwQaContam contam = 
		    {.fileId=ef->id, .qaContamTargetId=target->id, .mapRatio = mapRatio};
	    edwQaContamSaveToDb(conn, &contam, "edwQaContam", 256);
	    }
	edwAssemblyFree(&newAsm);
	}
    edwQaContamTargetFreeList(&targetList);
    remove(sampleFastqName);
    }
}

void doContaminationQa(struct sqlConnection *conn, struct edwFile *ef)
/* Try and do contamination level QA - mostly mapping fastq files to other
 * genomes. */
{
/* Get validated file info.  If not validated we don't bother. */
struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
if (vf == NULL)
    return;

/* We only work on fastq. */
if (!sameString(vf->format, "fastq"))
    return;

screenFastqForContaminants(conn, ef, vf);
}


void edwMakeContaminationQa(int startId, int endId)
/* edwMakeContaminationQa - Screen for contaminants by aligning against contaminant genomes.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileLoadIdRange(conn, startId, endId);

for (ef = efList; ef != NULL; ef = ef->next)
    {
    doContaminationQa(conn, ef);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwMakeContaminationQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
