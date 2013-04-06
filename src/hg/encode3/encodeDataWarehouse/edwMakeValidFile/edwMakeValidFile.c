/* edwMakeValidFile - Add range of ids to valid file table. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "bigWig.h"
#include "bigBed.h"
#include "bamFile.h"

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
/* Sample every downStep items in source and write to dest. */
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

#define TYPE_BAM  1
#define TYPE_READ 2

void makeSampleOfBam(char *inBamName, FILE *outBed, int downStep, struct edwValidFile *vf)
/* Sample every downStep items in inBam and write in simplified bed 5 fashion to outBed. */
{
/* Open and close with our C library just to fail quickly with decent error message if not BAM. */
uglyf("makeSampleOfBam(inBamName=%s)\n", inBamName);
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
	       fprintf(outBed, "%s\t%d\t%d\t.\t0\t%c\n", chrom, start, end, strand);
	       }
	   }
	}
    }
vf->mapRatio = (double)mappedCount/vf->itemCount;
samclose(sf);
}

#define edwSampleReduction 50

void makeValidFastq( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf.  Create sample subset. */
{
char sampleFileName[PATH_LEN];
safef(sampleFileName, PATH_LEN, "%sedwSampleXXXXXX", edwTempDir());
int sampleFd = mkstemp(sampleFileName);
FILE *f = fdopen(sampleFd, "w");
makeSampleOfFastq(path, f, edwSampleReduction, vf);
carefulClose(&f);
vf->samplePath = cloneString(sampleFileName);
}

void makeValidBigBed( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
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

void makeValidBigWig( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
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
fprintf(f, "vf->samplePath = %s\n", vf->samplePath);
fprintf(f, "vf->sampleCount = %lld\n", vf->sampleCount);
fprintf(f, "vf->basesInSample = %lld\n", vf->basesInSample);
fprintf(f, "vf->sampleCoverage = %g\n", vf->sampleCoverage);
fprintf(f, "vf->sampleCount = %g\n", vf->depth);
}

void makeValidBam( struct sqlConnection *conn, char *path, struct edwFile *eFile, 
	struct edwAssembly *assembly, struct edwValidFile *vf)
/* Fill out fields of vf based on bam.  Create sample subset as a little bed file. */
{
char sampleFileName[PATH_LEN];
safef(sampleFileName, PATH_LEN, "%sedwBamSampleToBedXXXXXX", edwTempDir());
int sampleFd = mkstemp(sampleFileName);
FILE *f = fdopen(sampleFd, "w");
makeSampleOfBam(path, f, edwSampleReduction, vf);
uglyf("After makeSampleOfBam - sampleFileName %s\n", sampleFileName);
carefulClose(&f);
vf->depth = vf->basesInItems*vf->mapRatio/assembly->baseCount;
vf->samplePath = cloneString(sampleFileName);
}

void makeValidFile(struct sqlConnection *conn, struct edwFile *eFile, struct cgiParsedVars *tags)
/* If possible make a edwValidFile record for this.  Makes sure all the right tags are there,
 * and then parses file enough to determine itemCount and the like.  For some files, like fastqs,
 * it will take a subset of the file as a sample so can do QA without processing the whole thing. */
{
struct edwValidFile *vf;
AllocVar(vf);
strcpy(vf->licensePlate, eFile->licensePlate);
vf->fileId = eFile->id;
vf->format = hashFindVal(tags->hash, "format");
vf->outputType = hashFindVal(tags->hash, "output_type");
vf->experiment = hashFindVal(tags->hash, "experiment");
vf->replicate = hashFindVal(tags->hash, "replicate");
vf->validKey = hashFindVal(tags->hash, "valid_key");
vf->enrichedIn = hashFindVal(tags->hash, "enriched_in");
vf->ucscDb = hashFindVal(tags->hash, "ucsc_db");
vf->samplePath = "";
if (vf->format && vf->outputType && vf->experiment && vf->replicate && 
	vf->validKey && vf->enrichedIn && vf->ucscDb)
    {
    /* Look up assembly. */
    char *ucscDb = vf->ucscDb;
    char query[256];
    safef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", vf->ucscDb);
    struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
    if (assembly == NULL)
        errAbort("Couldn't find assembly corresponding to %s", ucscDb);

    /* Make path to file */
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, eFile->edwFileName);

    /* And dispatch according to format. */
    char *format = vf->format;
    if (sameString(format, "fastq"))
	makeValidFastq(conn, path, eFile, assembly, vf);
    else if (sameString(format, "broadPeak") || sameString(format, "narrowPeak") || 
	     sameString(format, "bigBed"))
	{
	makeValidBigBed(conn, path, eFile, assembly, format, vf);
	}
    else if (sameString(format, "bigWig"))
        {
	makeValidBigWig(conn, path, eFile, assembly, vf);
	}
    else if (sameString(format, "bam"))
        {
	makeValidBam(conn, path, eFile, assembly, vf);
	}
    else if (sameString(format, "unknown"))
        {
	}
    else
        {
	errAbort("Unrecognized format %s for %s\n", format, eFile->edwFileName);
	}

    /* Create edwValidRecord with our result */
    edwValidFileSaveToDb(conn, vf, "edwValidFile", 512);
    }
}

void edwMakeValidFile(int startId, int endId)
/* edwMakeValidFile - Add range of ids to valid file table.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and md5 != '' and deprecated = ''", 
    startId, endId);
struct edwFile *eFile, *eFileList = edwFileLoadByQuery(conn, query);
uglyf("doing %d files with id's between %d and %d\n", slCount(eFileList), startId, endId);

for (eFile = eFileList; eFile != NULL; eFile = eFile->next)
    {
    uglyf("processing %s %s\n", eFile->licensePlate, eFile->submitFileName);
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", edwRootDir, eFile->edwFileName);

    if (eFile->tags) // All ones we care about have tags
        {
	struct cgiParsedVars *tags = cgiParsedVarsNew(eFile->tags);
	makeValidFile(conn, eFile, tags);
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
