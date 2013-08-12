/* edwMakeValidFile - Add range of ids to valid file table. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "errCatch.h"
#include "errabort.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "jksql.h"
#include "twoBit.h"
#include "genomeRangeTree.h"
#include "bigWig.h"
#include "bigBed.h"
#include "bamFile.h"
#include "portable.h"
#include "gff.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "fa.h"
#include "encode3/encode3Valid.h"

int maxErrCount = 1;	/* Set from command line. */
int errCount;		/* Set as we run. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeValidFile - Add range of ids to valid file table.\n"
  "usage:\n"
  "   edwMakeValidFile startId endId\n"
  "options:\n"
  "   maxErrCount=N - maximum errors allowed before it stops, default %d\n"
  , maxErrCount);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxErrCount", OPTION_INT},
   {NULL, 0},
};

void alignFastqMakeBed(struct edwFile *ef, struct edwAssembly *assembly,
    char *fastqPath, struct edwValidFile *vf, FILE *bedF)
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. 
 * Update vf->mapRatio and related fields. */
{
edwAlignFastqMakeBed(ef, assembly, fastqPath, vf, bedF, 
    &vf->mapRatio, &vf->depth, &vf->sampleCoverage);
}


void makeValidFastq( struct sqlConnection *conn, char *path, struct edwFile *ef, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf.  Create sample subset. */
{
/* Make edwFastqFile record. */
long long fileId = ef->id;
edwMakeFastqStatsAndSample(conn, fileId);
struct edwFastqFile *fqf = edwFastqFileFromFileId(conn, fileId);
verbose(1, "Made sample fastq with %lld reads\n", fqf->sampleCount);

/* Save some key pieces in vf. */
vf->itemCount = fqf->readCount;
vf->basesInItems = fqf->baseCount;
vf->sampleCount = fqf->sampleCount;
vf->basesInSample = fqf->basesInSample;

/* Align fastq and turn results into bed. */
char sampleBedName[PATH_LEN], temp[PATH_LEN];
safef(sampleBedName, PATH_LEN, "%sedwSampleBedXXXXXX", edwTempDirForToday(temp));
int bedFd = mkstemp(sampleBedName);
FILE *bedF = fdopen(bedFd, "w");
alignFastqMakeBed(ef, assembly, fqf->sampleFileName, vf, bedF);
carefulClose(&bedF);
vf->sampleBed = cloneString(sampleBedName);

edwFastqFileFree(&fqf);
}

#define TYPE_BAM  1
#define TYPE_READ 2

void edwMakeSampleOfBam(char *inBamName, FILE *outBed, int downStep, 
    struct edwAssembly *assembly, struct genomeRangeTree *grt, struct edwValidFile *vf)
/* Sample every downStep items in inBam and write in simplified bed 5 fashion to outBed. */
{
samfile_t *sf = samopen(inBamName, "rb", NULL);
bam_header_t *bamHeader = sf->header;

bam1_t one;
ZeroVar(&one);	// This seems to be necessary!

long long mappedCount = 0;
boolean done = FALSE;
while (!done)
    {
    int hotPosInCycle = rand()%downStep;
    int cycle;
    for (cycle=0; cycle<downStep; ++cycle)
        {
	boolean hotPos = (cycle == hotPosInCycle);
	int err = bam_read1(sf->x.bam, &one);
	if (err < 0)
	    {
	    done = TRUE;
	    break;
	    }
	int32_t tid = one.core.tid;
	int l_qseq = one.core.l_qseq;
	if (tid > 0)
	    ++mappedCount;
	vf->itemCount += 1;
	vf->basesInItems += l_qseq;
	if (hotPos)
	   {
	   vf->sampleCount += 1;
	   vf->basesInSample += l_qseq;
	   if (tid > 0)
	       {
	       char *chrom = bamHeader->target_name[tid];
	       int start = one.core.pos;
	       // Approximate here... can do better if parse cigar.
	       int end = start + l_qseq;	
	       boolean isRc = (one.core.flag & BAM_FREVERSE);
	       char strand = '+';
	       if (isRc)
	           {
		   strand = '-';
		   reverseIntRange(&start, &end, bamHeader->target_len[tid]);
		   }
	       if (start < 0) start=0;
	       fprintf(outBed, "%s\t%d\t%d\t.\t0\t%c\n", chrom, start, end, strand);
	       genomeRangeTreeAdd(grt, chrom, start, end);
	       }
	   }
	}
    }
vf->mapRatio = (double)mappedCount/vf->itemCount;
vf->depth = vf->basesInItems*vf->mapRatio/assembly->baseCount;
samclose(sf);
}

void makeValidBigBed( struct sqlConnection *conn, char *path, struct edwFile *ef, 
	struct edwAssembly *assembly, char *format, struct edwValidFile *vf)
/* Fill in fields of vf based on bigBed. */
{
struct bbiFile *bbi = bigBedFileOpen(path);
vf->sampleCount = vf->itemCount = bigBedItemCount(bbi);
struct bbiSummaryElement sum = bbiTotalSummary(bbi);
vf->basesInSample = vf->basesInItems = sum.sumData;
vf->sampleCoverage = (double)sum.validCount/assembly->baseCount;
vf->depth = (double)sum.sumData/assembly->baseCount;
vf->mapRatio = 1.0;
bigBedFileClose(&bbi);
}

void makeValidBigWig(struct sqlConnection *conn, char *path, struct edwFile *ef, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill in fields of vf based on bigWig. */
{
struct bbiFile *bbi = bigWigFileOpen(path);
struct bbiSummaryElement sum = bbiTotalSummary(bbi);
vf->sampleCount = vf->itemCount = vf->basesInSample = vf->basesInItems = sum.validCount;
vf->sampleCoverage = (double)sum.validCount/assembly->baseCount;
vf->depth = (double)sum.sumData/assembly->baseCount;
vf->mapRatio = 1.0;
bigWigFileClose(&bbi);
}

void edwValidFileDump(struct edwValidFile *vf, FILE *f)
/* Write out info about vf, just for debugging really */
{
fprintf(f, "vf->id = %d\n", vf->id);
fprintf(f, "vf->licensePlate = %s\n", vf->licensePlate);
fprintf(f, "vf->fileId = %d\n", vf->fileId);
fprintf(f, "vf->format = %s\n", vf->format);
fprintf(f, "vf->outputType = %s\n", vf->outputType);
fprintf(f, "vf->experiment = %s\n", vf->experiment);
fprintf(f, "vf->replicate = %s\n", vf->replicate);
fprintf(f, "vf->validKey = %s\n", vf->validKey);
fprintf(f, "vf->enrichedIn = %s\n", vf->enrichedIn);
fprintf(f, "vf->ucscDb = %s\n", vf->ucscDb);
fprintf(f, "vf->itemCount = %lld\n", vf->itemCount);
fprintf(f, "vf->basesInItems = %lld\n", vf->basesInItems);
fprintf(f, "vf->sampleBed = %s\n", vf->sampleBed);
fprintf(f, "vf->sampleCount = %lld\n", vf->sampleCount);
fprintf(f, "vf->basesInSample = %lld\n", vf->basesInSample);
fprintf(f, "vf->sampleCoverage = %g\n", vf->sampleCoverage);
fprintf(f, "vf->sampleCount = %g\n", vf->depth);
}

#define edwSampleReduction 40	    /* Initially sample ever Nth read where this defines N */

void makeValidBam( struct sqlConnection *conn, char *path, struct edwFile *ef, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf based on bam.  Create sample subset as a little bed file. */
{
char sampleFileName[PATH_LEN], temp[PATH_LEN];
safef(sampleFileName, PATH_LEN, "%sedwBamSampleToBedXXXXXX", edwTempDirForToday(temp));
int sampleFd = mkstemp(sampleFileName);
FILE *f = fdopen(sampleFd, "w");
struct genomeRangeTree *grt = genomeRangeTreeNew();
edwMakeSampleOfBam(path, f, edwSampleReduction, assembly, grt, vf);
carefulClose(&f);
vf->sampleBed = cloneString(sampleFileName);
long long basesHitBySample = genomeRangeTreeSumRanges(grt);
genomeRangeTreeFree(&grt);
vf->sampleCoverage = (double)basesHitBySample/assembly->baseCount;
}

void makeValid2Bit(struct sqlConnection *conn, char *path, struct edwFile *ef, 
    struct edwValidFile *vf)
/* Fill in info about assembly */
{
struct twoBitFile *tbf = twoBitOpen(path);
vf->basesInItems = vf->basesInSample = twoBitTotalSize(tbf);
vf->itemCount = vf->sampleCount = tbf->seqCount;
vf->mapRatio = 1.0;
vf->sampleCoverage = 1.0;
vf->depth = 1.0;
twoBitClose(&tbf);
}

void makeValidFasta(struct sqlConnection *conn, char *path, struct edwFile *ef, 
    struct edwValidFile *vf)
/* Fill in info about fasta file */
{
struct lineFile *lf = lineFileOpen(path, FALSE);
DNA *dna;
int size;
char *name;
while (faSpeedReadNext(lf, &dna, &size, &name))
    {
    vf->basesInItems += size;
    vf->itemCount += 1;
    }
lineFileClose(&lf);
}

void makeValidGtf(struct sqlConnection *conn, char *path, struct edwFile *ef, 
    struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill in info about a gtf file. */
{
/* Open and read file with generic GFF reader and check it is GTF */
struct gffFile *gff = gffRead(path);
if (!gff->isGtf)
    errAbort("file id %lld (%s) is not in GTF format - check it has gene_id and transcript_id",
	(long long)ef->id, ef->submitFileName);

/* Convert it to a somewhat smaller less informative bed file for sampling purposes. */
char sampleFileName[PATH_LEN], temp[PATH_LEN];
safef(sampleFileName, PATH_LEN, "%sedwGffBedXXXXXX", edwTempDirForToday(temp));
int sampleFd = mkstemp(sampleFileName);
FILE *f = fdopen(sampleFd, "w");
struct genomeRangeTree *grt = genomeRangeTreeNew();

/* Loop through lines writing out simple bed and adding to genome range tree. */
struct gffLine *gffLine;
long long itemCount = 0;
long long totalSize = 0;
for (gffLine = gff->lineList; gffLine != NULL; gffLine = gffLine->next)
    {
    totalSize += gffLine->end - gffLine->start;
    fprintf(f, "%s\t%d\t%d\n", gffLine->seq, gffLine->start, gffLine->end);
    genomeRangeTreeAdd(grt, gffLine->seq, gffLine->start, gffLine->end);
    ++itemCount;
    }
carefulClose(&f);

/* Fill out what we can of vf with info we've gathered. */
vf->itemCount = vf->sampleCount = itemCount;
vf->basesInItems = vf->basesInSample = totalSize;
vf->sampleBed = cloneString(sampleFileName);
long long basesHitBySample = genomeRangeTreeSumRanges(grt);
genomeRangeTreeFree(&grt);
vf->sampleCoverage = (double)basesHitBySample/assembly->baseCount;
vf->mapRatio = 1.0;
vf->depth = (double)totalSize/assembly->baseCount;
gffFileFree(&gff);
}

void makeValidRcc(struct sqlConnection *conn, char *path, struct edwFile *ef, struct edwValidFile *vf)
/* Fill in info about a nanostring RCC file. */
{
encode3ValidateRcc(path);
}

void makeValidIdat(struct sqlConnection *conn, char *path, struct edwFile *ef, struct edwValidFile *vf)
/* Fill in info about a illumina idac file. */
{
encode3ValidateIdat(path);
}


static void needAssembly(struct edwFile *ef, char *format, struct edwAssembly *assembly)
/* Require assembly tag be present. */
{
if (assembly == NULL)
    errAbort("file id %lld (%s) is %s format and needs an assembly tag to validate", 
	(long long)ef->id, ef->submitFileName, format);
}

static char *findTagOrEmpty(struct cgiParsedVars *tags, char *key)
/* Find key in tags.  If it is not there, or empty, or 'n/a' valued return empty string
 * otherwise return val */
{
char *val = hashFindVal(tags->hash, key);
if (val == NULL || sameString(val, "n/a"))
   return "";
else
   return val;
}

void validFileUpdateDb(struct sqlConnection *conn, struct edwValidFile *el, long long id)
/* Save edwValidFile as a row to the table specified by tableName, replacing existing record at 
 * id. */
{
struct dyString *dy = newDyString(512);
sqlDyStringAppend(dy, "update edwValidFile set ");
// omit id and licensePlate fields - one autoupdates and the other depends on this
// also omit fileId which also really can't change.
dyStringPrintf(dy, " format='%s',", el->format);
dyStringPrintf(dy, " outputType='%s',", el->outputType);
dyStringPrintf(dy, " experiment='%s',", el->experiment);
dyStringPrintf(dy, " replicate='%s',", el->replicate);
dyStringPrintf(dy, " validKey='%s',", el->validKey);
dyStringPrintf(dy, " enrichedIn='%s',", el->enrichedIn);
dyStringPrintf(dy, " ucscDb='%s',", el->ucscDb);
dyStringPrintf(dy, " itemCount=%lld,", (long long)el->itemCount);
dyStringPrintf(dy, " basesInItems=%lld,", (long long)el->basesInItems);
dyStringPrintf(dy, " sampleCount=%lld,", (long long)el->sampleCount);
dyStringPrintf(dy, " basesInSample=%lld,", (long long)el->basesInSample);
dyStringPrintf(dy, " sampleBed='%s',", el->sampleBed);
dyStringPrintf(dy, " mapRatio=%g,", el->mapRatio);
dyStringPrintf(dy, " sampleCoverage=%g,", el->sampleCoverage);
dyStringPrintf(dy, " depth=%g", el->depth);
dyStringPrintf(dy, " where id=%lld\n", (long long)id);
sqlUpdate(conn, dy->string);
freeDyString(&dy);
}

void mustMakeValidFile(struct sqlConnection *conn, struct edwFile *ef, struct cgiParsedVars *tags,
    long long oldValidId)
/* If possible make a edwValidFile record for this.  Makes sure all the right tags are there,
 * and then parses file enough to determine itemCount and the like.  For some files, like fastqs,
 * it will take a subset of the file as a sample so can do QA without processing the whole thing. */
{
/* Make up validFile from tags and id */
struct edwValidFile *vf;
AllocVar(vf);
vf->fileId = ef->id;
vf->format = hashFindVal(tags->hash, "format");
vf->outputType = findTagOrEmpty(tags, "output_type");
vf->experiment = findTagOrEmpty(tags, "experiment");
vf->replicate = findTagOrEmpty(tags, "replicate");
vf->validKey = hashFindVal(tags->hash, "valid_key");
vf->enrichedIn = findTagOrEmpty(tags, "enriched_in");
vf->ucscDb = findTagOrEmpty(tags, "ucsc_db");
vf->sampleBed = "";

if (vf->format && vf->validKey)	// We only can validate if we have something for format 
    {
    /* Check validation key */
    char *validKey = encode3CalcValidationKey(ef->md5, ef->size);
    if (!sameString(validKey, vf->validKey))
        errAbort("valid_key does not check.  Make sure to use validateManifest.");

    /* Look up assembly. */
    struct edwAssembly *assembly = NULL;
    if (!isEmpty(vf->ucscDb) && !sameString(vf->ucscDb, "unknown"))
	{
	char *ucscDb = vf->ucscDb;
	char query[256];
	sqlSafef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
	assembly = edwAssemblyLoadByQuery(conn, query);
	if (assembly == NULL)
	    errAbort("Couldn't find assembly corresponding to %s", ucscDb);
	}

    /* Make path to file */
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);

    /* And dispatch according to format. */
    char *format = vf->format;
    if (sameString(format, "fastq"))
	{
	needAssembly(ef, format, assembly);
	makeValidFastq(conn, path, ef, assembly, vf);
	}
    else if (edwIsSupportedBigBedFormat(format))
	{
	needAssembly(ef, format, assembly);
	makeValidBigBed(conn, path, ef, assembly, format, vf);
	}
    else if (sameString(format, "bigWig"))
        {
	needAssembly(ef, format, assembly);
	makeValidBigWig(conn, path, ef, assembly, vf);
	}
    else if (sameString(format, "bam"))
        {
	needAssembly(ef, format, assembly);
	makeValidBam(conn, path, ef, assembly, vf);
	}
    else if (sameString(format, "2bit"))
        {
	makeValid2Bit(conn, path, ef, vf);
	}
    else if (sameString(format, "fasta"))
        {
	makeValidFasta(conn, path, ef, vf);
	}
    else if (sameString(format, "gtf"))
        {
	needAssembly(ef, format, assembly);
	makeValidGtf(conn, path, ef, assembly, vf);
	}
    else if (sameString(format, "rcc"))
        {
	makeValidRcc(conn, path, ef, vf);
	}
    else if (sameString(format, "idat"))
        {
	makeValidIdat(conn, path, ef, vf);
	}
    else if (sameString(format, "unknown"))
        {
	/* No specific validation needed for unknown format. */
	}
    else
        {
	errAbort("Unrecognized format %s for %s\n", format, ef->edwFileName);
	}

    /* Save record except for license plate to DB. */
    if (oldValidId == 0)
	{
	edwValidFileSaveToDb(conn, vf, "edwValidFile", 512);
	vf->id = sqlLastAutoId(conn);

	/* Create license plate around our ID.  File in warehouse to use license plate
	 * instead of baby-babble IDs. */
	    {
	    edwMakeLicensePlate(edwLicensePlatePrefix, vf->id, vf->licensePlate, edwMaxPlateSize);

	    /* Create swapped out version of edwFileName in newName. */
	    struct dyString *newName = dyStringNew(0);
	    char *fileName = ef->edwFileName;
	    char *dirEnd = strrchr(fileName, '/');
	    if (dirEnd == NULL)
		dirEnd = fileName;
	    else
		dirEnd += 1;
	    char *suffix = edwFindDoubleFileSuffix(fileName);
	    dyStringAppendN(newName, fileName, dirEnd - fileName);
	    dyStringAppend(newName, vf->licensePlate);
	    dyStringAppend(newName, suffix);

	    /* Now build full path names and attempt rename in file system. */
	    char oldPath[PATH_LEN], newPath[PATH_LEN];
	    safef(oldPath, sizeof(oldPath), "%s%s", edwRootDir, fileName);
	    safef(newPath, sizeof(newPath), "%s%s", edwRootDir, newName->string);
	    mustRename(oldPath, newPath);
	    verbose(2, "Renamed %s to %s\n", oldPath, newPath);

	    /* Update database with new name - small window of vulnerability here sadly 
	     * two makeValidates running at same time stepping on each other. */
	    char query[PATH_LEN+256];
	    sqlSafef(query, sizeof(query), "update edwFile set edwFileName='%s' where id=%lld",
		newName->string, (long long)ef->id);
	    sqlUpdate(conn, query);

	    dyStringFree(&newName);
	    }

	/* Update validFile record with license plate. */
	char query[256];
	sqlSafef(query, sizeof(query), "update edwValidFile set licensePlate='%s' where id=%lld", 
	    vf->licensePlate, (long long)vf->id);
	sqlUpdate(conn, query);
	}
    else
        {
	validFileUpdateDb(conn, vf, oldValidId);
	}

    }
freez(&vf);
}

boolean makeValidFile(struct sqlConnection *conn, struct edwFile *ef, struct cgiParsedVars *tags,
    long long oldValidId)
/* Attempt to make validation.  If it fails catch error and attach it to ef->errorMessage as well
 * as sending it to stderr, and return FALSE.  Otherwise return TRUE. */
{
struct errCatch *errCatch = errCatchNew();
boolean success = TRUE;
if (errCatchStart(errCatch))
    {
    mustMakeValidFile(conn, ef, tags, oldValidId);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    edwWriteErrToStderrAndTable(conn, "edwFile", ef->id, errCatch->message->string);
    warn("This is from submitted file %s", ef->submitFileName);
    success = FALSE;
    }
else
    {
    char query[256];
    sqlSafef(query, sizeof(query), "update edwFile set errorMessage='' where id=%lld",
	(long long)ef->id);
    sqlUpdate(conn, query);
    }
errCatchFree(&errCatch);
return success;
}

void edwClearFileError(struct sqlConnection *conn, long long fileId)
/* Clear file error message */
{
char query[256];
sqlSafef(query, sizeof(query), "update edwFile set errorMessage='' where id=%lld", fileId);
sqlUpdate(conn, query);
}

void edwMakeValidFile(int startId, int endId)
/* edwMakeValidFile - Add range of ids to valid file table.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startId, endId);

for (ef = efList; ef != NULL; ef = ef->next)
    {
    char query[256];
    sqlSafef(query, sizeof(query), "select id from edwValidFile where fileId=%lld", (long long)ef->id);
    long long vfId = sqlQuickLongLong(conn, query);
    if (vfId != 0 && isEmpty(ef->errorMessage))
	{
        verbose(2, "already validated %s %s\n", ef->edwFileName, ef->submitFileName);
	}
    else
	{
	verbose(1, "processing %lld %s %s\n", (long long)ef->id, ef->edwFileName, ef->submitFileName);
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);
	if (!isEmpty(ef->tags)) // All ones we care about have tags
	    {
	    if (vfId != 0)
	        edwClearFileError(conn, ef->id);
	    struct cgiParsedVars *tags = cgiParsedVarsNew(ef->tags);
	    if (!makeValidFile(conn, ef, tags, vfId))
	        {
		if (++errCount >= maxErrCount)
		    errAbort("Aborting after %d errors", errCount);
		}
	    cgiParsedVarsFree(&tags);
	    }
	else
	    {
	    verbose(2, "no tags to validate on %s %s\n", ef->edwFileName, ef->submitFileName);
	    }
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
maxErrCount = optionInt("maxErrCount", maxErrCount);
edwMakeValidFile(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
