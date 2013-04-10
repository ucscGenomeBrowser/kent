/* edwMakeValidFile - Add range of ids to valid file table. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "twoBit.h"
#include "genomeRangeTree.h"
#include "bigWig.h"
#include "bigBed.h"
#include "bamFile.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "encode3/encode3Valid.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeValidFile - Add range of ids to valid file table.\n"
  "usage:\n"
  "   edwMakeValidFile startId endId\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean maybeCopyFastqRecord(struct lineFile *lf, FILE *f, boolean copy, int *retSeqSize)
/* Read next fastq record from LF, and optionally copy it to f.  Return FALSE at end of file 
 * Do a _little_ error checking on record while we're at it.  The format has already been
 * validated on the client side fairly thoroughly. */
{
char *line;
int lineSize;

/* Deal with initial line starting with '@' */
if (!lineFileNext(lf, &line, &lineSize))
    return FALSE;
if (line[0] != '@')
    errAbort("Expecting line starting with '@' got '%c' line %d of %s", line[0],
	lf->lineIx, lf->fileName);
if (copy)
    mustWrite(f, line, lineSize);

/* Deal with line containing sequence. */
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s truncated in middle of record", lf->fileName);
if (copy)
    mustWrite(f, line, lineSize);
int seqSize = lineSize-1;

/* Deal with line containing just '+' that separates sequence from quality. */
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s truncated in middle of record", lf->fileName);
if (line[0] != '+')
    errAbort("Expecting line starting with '+' got '%c' line %d of %s", line[0],
	lf->lineIx, lf->fileName);
if (copy)
    mustWrite(f, line, lineSize);

/* Deal with quality score line. */
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s truncated in middle of record", lf->fileName);
if (copy)
    mustWrite(f, line, lineSize);
int qualSize = lineSize-1;

if (seqSize != qualSize)
    errAbort("Sequence and quality size differ line %d and %d of %s", 
	lf->lineIx-2, lf->lineIx, lf->fileName);

*retSeqSize = seqSize;
return TRUE;
}

void makeSampleOfFastq(char *source, FILE *f, int downStep, struct edwValidFile *vf)
/* Sample every downStep items in source and write to dest. Count how many fastq
 * records and update vf fields with this. */
{
struct lineFile *lf = lineFileOpen(source, FALSE);
boolean done = FALSE;
while (!done)
    {
    int hotPosInCycle = rand()%downStep;
    int cycle;
    for (cycle=0; cycle<downStep; ++cycle)
        {
	boolean hotPos = (cycle == hotPosInCycle);
	int seqSize;
	if (!maybeCopyFastqRecord(lf, f, hotPos, &seqSize))
	    {
	    done = TRUE;
	    break;
	    }
	vf->itemCount += 1;
	vf->basesInItems += seqSize;
	if (hotPos)
	   {
	   vf->sampleCount += 1;
	   vf->basesInSample += seqSize;
	   }
	}
    }
lineFileClose(&lf);
}

void systemWithCheck(char *command)
/* Do a system call and abort with error if there's a problem. */
{
int err = system(command);
if (err != 0)
    errAbort("error executing: %s", command);
}

void scanSam(char *samIn, FILE *f, struct genomeRangeTree *grt, long long *retHit, 
    long long *retMiss,  long long *retTotalBasesInHits)
/* Scan through sam file doing several things:counting how many reads hit and how many 
 * miss target during mapping phase, copying those that hit to a little bed file, and 
 * also defining regions covered in a genomeRangeTree. */
{
samfile_t *sf = samopen(samIn, "r", NULL);
bam_header_t *bamHeader = sf->header;
bam1_t one;
ZeroVar(&one);
int err;
long long hit = 0, miss = 0, totalBasesInHits = 0;
while ((err = samread(sf, &one)) >= 0)
    {
    int32_t tid = one.core.tid;
    if (tid < 0)
	{
	++miss;
        continue;
	}
    ++hit;
    char *chrom = bamHeader->target_name[tid];
    // Approximate here... can do better if parse cigar.
    int start = one.core.pos;
    int size = one.core.l_qseq;
    int end = start + size;	
    totalBasesInHits += size;
    boolean isRc = (one.core.flag & BAM_FREVERSE);
    char strand = '+';
    if (isRc)
	{
	strand = '-';
	reverseIntRange(&start, &end, bamHeader->target_len[tid]);
	}
    if (start < 0) start=0;
    fprintf(f, "%s\t%d\t%d\t.\t0\t%c\n", chrom, start, end, strand);
    genomeRangeTreeAdd(grt, chrom, start, end);
    }
if (err < 0 && err != -1)
    errnoAbort("samread err %d", err);
samclose(sf);
*retHit = hit;
*retMiss = miss;
*retTotalBasesInHits = totalBasesInHits;
}

static void rangeSummer(void *item, void *context)
/* This is a callback for rbTreeTraverse with context.  It just adds up
 * end-start */
{
struct range *range = item;
long long *pSum = context;
*pSum += range->end - range->start;
}

long long rangeTreeSumRanges(struct rbTree *tree)
/* Return sum of end-start of all items. */
{
long long sum = 0;
rbTreeTraverseWithContext(tree, rangeSummer, &sum);
return sum;
}

long long genomeRangeTreeSumRanges(struct genomeRangeTree *grt)
/* Sum up all ranges in tree. */
{
long long sum = 0;
struct hashEl *chrom, *chromList = hashElListHash(grt->hash);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    rbTreeTraverseWithContext(chrom->val, rangeSummer, &sum);
hashElFreeList(&chromList);
return sum;
}

void alignFastqMakeBed(struct edwFile *ef, struct edwAssembly *assembly,
    char *fastqPath, struct edwValidFile *vf, FILE *bedF)
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. */
{
/* Hmm, tried doing this with Mark's pipeline code, but somehow it would be flaky the
 * second time it was run in same app.  Resorting therefore to temp files. */
char genoFile[PATH_LEN];
safef(genoFile, sizeof(genoFile), "%s%s/bwaData/%s.fa", 
    edwValDataDir, vf->ucscDb, vf->ucscDb);

char cmd[3*PATH_LEN];
char *saiName = cloneString(rTempName(edwTempDir(), "edwSample1", ".sai"));
safef(cmd, sizeof(cmd), "bwa aln %s %s > %s", genoFile, fastqPath, saiName);
systemWithCheck(cmd);

char *samName = cloneString(rTempName(edwTempDir(), "ewdSample1", ".sam"));
safef(cmd, sizeof(cmd), "bwa samse %s %s %s > %s", genoFile, saiName, fastqPath, samName);
systemWithCheck(cmd);
remove(saiName);

/* Scan sam file to calculate vf->mapRatio, vf->sampleCoverage and vf->depth. 
 * and also to produce little bed file for enrichment step. */
struct genomeRangeTree *grt = genomeRangeTreeNew();
long long hitCount=0, missCount=0, totalBasesInHits=0;
scanSam(samName, bedF, grt, &hitCount, &missCount, &totalBasesInHits);
uglyf("hitCount=%lld, missCount=%lld, totalBasesInHits=%lld, grt=%p\n", hitCount, missCount, totalBasesInHits, grt);
vf->mapRatio = (double)hitCount/(hitCount+missCount);
vf->depth = (double)totalBasesInHits/assembly->baseCount * (double)vf->itemCount/vf->sampleCount;
long long basesHitBySample = genomeRangeTreeSumRanges(grt);
vf->sampleCoverage = (double)basesHitBySample/assembly->baseCount;
genomeRangeTreeFree(&grt);
remove(samName);
}


#define edwSampleReduction 50

void makeValidFastq( struct sqlConnection *conn, char *path, struct edwFile *ef, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf.  Create sample subset. */
{
/* Make sample of fastq */
char smallFastqName[PATH_LEN];
safef(smallFastqName, PATH_LEN, "%sedwSampleFastqXXXXXX", edwTempDir());
int smallFd = mkstemp(smallFastqName);
FILE *smallF = fdopen(smallFd, "w");
makeSampleOfFastq(path, smallF, edwSampleReduction, vf);
carefulClose(&smallF);
verbose(1, "Made sample fastq with %lld reads\n", vf->sampleCount);

/* Align fastq and turn results into bed. */
char sampleBedName[PATH_LEN];
safef(sampleBedName, PATH_LEN, "%sedwSampleBedXXXXXX", edwTempDir());
int bedFd = mkstemp(sampleBedName);
FILE *bedF = fdopen(bedFd, "w");
alignFastqMakeBed(ef, assembly, smallFastqName, vf, bedF);
carefulClose(&bedF);

remove(smallFastqName);

vf->sampleBed = cloneString(sampleBedName);
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

void makeValidBam( struct sqlConnection *conn, char *path, struct edwFile *ef, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf based on bam.  Create sample subset as a little bed file. */
{
char sampleFileName[PATH_LEN];
safef(sampleFileName, PATH_LEN, "%sedwBamSampleToBedXXXXXX", edwTempDir());
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

void makeValidFile(struct sqlConnection *conn, struct edwFile *ef, struct cgiParsedVars *tags)
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
    if (!isEmpty(vf->ucscDb))
	{
	char *ucscDb = vf->ucscDb;
	char query[256];
	safef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
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
    else if (sameString(format, "broadPeak") || sameString(format, "narrowPeak") || 
	     sameString(format, "bigBed"))
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
    else if (sameString(format, "unknown"))
        {
	}
    else
        {
	errAbort("Unrecognized format %s for %s\n", format, ef->edwFileName);
	}

    /* Save record except for license plate to DB. */
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
	uglyf("Seriously considering renaming %s to %s\n", fileName, newName->string);

	/* Now build full path names and attempt rename in file system. */
	char oldPath[PATH_LEN], newPath[PATH_LEN];
	safef(oldPath, sizeof(oldPath), "%s%s", edwRootDir, fileName);
	safef(newPath, sizeof(newPath), "%s%s", edwRootDir, newName->string);
	mustRename(oldPath, newPath);
	uglyf("Actually done renaming %s to %s\n", oldPath, newPath);

	/* Update database with new name - small window of vulnerability here sadly 
	 * two makeValidates running at same time stepping on each other. */
	char query[PATH_LEN+256];
	safef(query, sizeof(query), "update edwFile set edwFileName='%s' where id=%lld",
	    newName->string, (long long)ef->id);
	sqlUpdate(conn, query);

	dyStringFree(&newName);
	}



    /* Update validFile record with license plate. */
    char query[256];
    safef(query, sizeof(query), "update edwValidFile set licensePlate='%s' where id=%lld", 
	vf->licensePlate, (long long)vf->id);
    sqlUpdate(conn, query);
    }
freez(&vf);
}

void edwMakeValidFile(int startId, int endId)
/* edwMakeValidFile - Add range of ids to valid file table.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and endUploadTime != 0 "
    "and updateTime != 0 and deprecated = ''", 
    startId, endId);
struct edwFile *ef, *efList = edwFileLoadByQuery(conn, query);

for (ef = efList; ef != NULL; ef = ef->next)
    {
    verbose(1, "processing %s %s\n", ef->edwFileName, ef->submitFileName);
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);

    if (ef->tags) // All ones we care about have tags
        {
	struct cgiParsedVars *tags = cgiParsedVarsNew(ef->tags);
	makeValidFile(conn, ef, tags);
	cgiParsedVarsFree(&tags);
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
edwMakeValidFile(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
