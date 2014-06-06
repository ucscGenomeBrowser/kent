/* edwFixUniqueMapRatio - Fill in uniqueMapRatio for files that are lacking it.. */
/* Also recalculates edwValidFile.sample bed for fastqs.  DOes the same for
 * bam file, but by the easier route of just forcing file revalidation. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "bamFile.h"
#include "portable.h"
#include "genomeRangeTree.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixUniqueMapRatio - Fill in uniqueMapRatio for files that are lacking it.\n"
  "Also recalculate edwValidFile.sampleBed which happens to be wrong on same files\n"
  "usage:\n"
  "   edwFixUniqueMapRatio startId endId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


double fastqUniqueMapRatio(struct sqlConnection *conn, struct edwValidFile *vf,
    struct edwAssembly *assembly)
/* Figure out uniquely mapping ratio the hard way - by aligning sample. */
{
struct edwFile *ef = edwFileFromId(conn, vf->fileId);
struct edwFastqFile *fqf = edwFastqFileFromFileId(conn, vf->fileId);
/* Align fastq and turn results into bed. */
char sampleBedName[PATH_LEN], temp[PATH_LEN];
safef(sampleBedName, PATH_LEN, "%sedwSampleBedXXXXXX", edwTempDirForToday(temp));
int bedFd = mkstemp(sampleBedName);
if (fchmod(bedFd, 0664) == -1)
    errnoAbort("Couldn't change permissions on temp file %s", sampleBedName);
FILE *bedF = fdopen(bedFd, "w");
edwAlignFastqMakeBed(ef, assembly, fqf->sampleFileName, vf, bedF,
    &vf->mapRatio, &vf->depth, &vf->sampleCoverage, &vf->uniqueMapRatio);

/* Update validFile record with recomputed fields. */
char query[512];
sqlSafef(query, sizeof(query), 
    "update edwValidFile set sampleCoverage=%g,sampleBed='%s' where id=%u", 
    vf->sampleCoverage, sampleBedName, vf->id);
sqlUpdate(conn, query);

/* Remove old file. */
if (!isEmpty(vf->sampleBed))
    remove(vf->sampleBed);

/* CLean up and return result. */
edwFileFree(&ef);
edwFastqFileFree(&fqf);
return vf->uniqueMapRatio;
}

double bamUniqueMapRatio(struct sqlConnection *conn, struct edwValidFile *vf,
	struct edwAssembly *assembly)
/* Fill out fields of vf based on bam.  Create sample subset as a little bed file. */
{
struct edwFile *ef = edwFileFromId(conn, vf->fileId);

/* Have edwBamStats do most of the work. */
char sampleFileName[PATH_LEN];
struct edwBamFile *ebf = edwMakeBamStatsAndSample(conn, ef->id, sampleFileName);

/* Fill in some of validFile record from bamFile record */
vf->uniqueMapRatio = (double)ebf->uniqueMappedCount/ebf->readCount;
vf->sampleCount = vf->basesInSample = 0;

/* Scan through the bed file to make up information about the sample bits */
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct lineFile *lf = lineFileOpen(sampleFileName, "TRUE");
char *row[3];
while (lineFileRow(lf, row))
    {
    char *chrom = row[0];
    unsigned start = sqlUnsigned(row[1]);
    unsigned end = sqlUnsigned(row[2]);
    vf->sampleCount += 1;
    vf->basesInSample += end - start;
    genomeRangeTreeAdd(grt, chrom, start, end);
    }
lineFileClose(&lf);

/* Fill in last bits that need summing from the genome range tree. */
long long basesHitBySample = genomeRangeTreeSumRanges(grt);
vf->sampleCoverage = (double)basesHitBySample/assembly->baseCount;
genomeRangeTreeFree(&grt);

/* Update validFile record with recomputed fields. */
char query[512];
sqlSafef(query, sizeof(query), 
    "update edwValidFile set sampleCoverage=%g,sampleBed='%s',sampleCount=%lld,basesInSample=%lld "
    "where id=%u", 
    vf->sampleCoverage, sampleFileName, vf->sampleCount, vf->basesInSample, vf->id);
uglyf("%s\n", query);
sqlUpdate(conn, query);

/* Remove old file. */
if (!isEmpty(vf->sampleBed))
    remove(vf->sampleBed);
uglyf("Removed %s, new sampleBed is %s\n", vf->sampleBed, sampleFileName);
vf->sampleBed = cloneString(sampleFileName);

edwBamFileFree(&ebf);
edwFileFree(&ef);
return vf->uniqueMapRatio;
}

void edwFixUniqueMapRatio(unsigned startId, unsigned endId)
/* edwFixUniqueMapRatio - Fill in uniqueMapRatio for files that are lacking it.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
unsigned fileId;
for (fileId=startId; fileId <= endId; ++fileId)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, fileId);
    if (vf != NULL && vf->uniqueMapRatio == 0.0)
	{
	char *format = vf->format;
	if (!(sameString(format, "bam") || sameString(format, "fastq")))
	    continue;

	/* Look up assembly. */
	struct edwAssembly *assembly = NULL;
	if (!isEmpty(vf->ucscDb) && !sameString(vf->ucscDb, "unknown"))
	    {
	    char *ucscDb = vf->ucscDb;
	    char query[256];
	    sqlSafef(query, sizeof(query), 
		"select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
	    assembly = edwAssemblyLoadByQuery(conn, query);
	    if (assembly == NULL)
		errAbort("Couldn't find assembly corresponding to %s", ucscDb);
	    }
	else
	    errAbort("No assembly for %s", vf->licensePlate);

	double ratio = 0.0;
	boolean doSomething = TRUE;
	if (sameString(format, "bam"))
	    {
	    ratio = bamUniqueMapRatio(conn, vf, assembly);
	    }
	else if (sameString(format, "fastq"))
	    {
	    ratio = fastqUniqueMapRatio(conn, vf, assembly);
	    }
	else
	    doSomething = FALSE;
	if (doSomething)
	    {
	    /* Update uniqueMapRatio field. */
	    char query[256];
	    sqlSafef(query, sizeof(query), 
		"update edwValidFile set uniqueMapRatio=%g where id=%u", ratio, vf->id);
	    sqlUpdate(conn, query);

	    /* Delete some QA table rows that need to be recomputed. */
	    sqlSafef(query, sizeof(query),
	        "delete from edwQaEnrich where fileId=%u", vf->fileId);
	    sqlUpdate(conn, query);
	    sqlSafef(query, sizeof(query),
	        "delete from edwQaPairSampleOverlap where elderFileId=%u or youngerFileId=%u",
		vf->fileId, vf->fileId);
	    sqlUpdate(conn, query);

	    /* Put commands to recompute the single file one on job queue. */
	    sqlSafef(query, sizeof(query),
	        "insert into edwJob (commandLine) values ('edwMakeEnrichments %u %u')",
		vf->fileId, vf->fileId);
	    sqlUpdate(conn, query);

#ifdef NOT_BEST_ASYNCHRONOUSLY
	    /* Need to do a big edwMakeReplicateQa afterwards when dust has settled
	     * Otherwise will be doing it on mixed bunch of bed samples. */

	    sqlSafef(query, sizeof(query),
	        "insert into edwJob (commandLine) values ('edwMakeReplicateQa %u %u')",
		vf->fileId, vf->fileId);
	    sqlUpdate(conn, query);
#endif /* NOT_BEST_ASYNCHRONOUSLY */

	    edwPokeFifo("edwQaAgent.fifo");
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixUniqueMapRatio(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
