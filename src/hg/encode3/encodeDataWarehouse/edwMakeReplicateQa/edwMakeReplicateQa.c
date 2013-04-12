/* edwMakeReplicateQa - Do qa level comparisons of replicates.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "jksql.h"
#include "basicBed.h"
#include "genomeRangeTree.h"
#include "correlate.h"
#include "bigWig.h"
#include "bigBed.h"
// #include "bamFile.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMakeReplicateQa - Do qa level comparisons of replicates.\n"
  "usage:\n"
  "   edwMakeReplicateQa startId endId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct edwValidFile *findElderReplicates(struct sqlConnection *conn, struct edwValidFile *vf)
/* Find all replicates of same output and format type for experiment that are elder
 * (fileId less than your file Id).  Younger replicates are responsible for taking care 
 * of correlations with older ones.  Sorry younguns, it's like social security. */
{
char query[256];
safef(query, sizeof(query), 
    "select * from edwValidFile where fileId<%d and experiment='%s' and format='%s'"
    " and outputType='%s'"
    , vf->fileId, vf->experiment, vf->format, vf->outputType);
return edwValidFileLoadByQuery(conn, query);
}

struct bed3 *edwLoadSampleBed3(struct sqlConnection *conn, struct edwValidFile *vf)
/* Load up sample bed3 attached to vf */
{
char *sampleBed = vf->sampleBed;
if (isEmpty(sampleBed))
    errAbort("No sample bed for %s", vf->licensePlate);
return bed3LoadAll(sampleBed);
}

void doSampleReplicate(struct sqlConnection *conn, char *format, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do correlation analysis between elder and younger and save result to
 * a new edwQaPairCorrelate record. Do this for a format where we have a bed3 sample file. */
{
struct edwQaPairCorrelate *cor;
AllocVar(cor);
cor->elderFileId = elderVf->fileId;
cor->youngerFileId = youngerVf->fileId;
cor->elderSampleBases = elderVf->basesInSample;
cor->youngerSampleBases = youngerVf->basesInSample;

/* Load up elder into genome range tree. */
struct bed3 *elderBedList = edwLoadSampleBed3(conn, elderVf);
struct genomeRangeTree *elderGrt = edwMakeGrtFromBed3List(elderBedList);

/* Load up younger as bed, and loop through to get overlap */
long long totalOverlap = 0;
struct bed3 *bed, *youngerBedList = edwLoadSampleBed3(conn, youngerVf);
for (bed = youngerBedList; bed != NULL; bed = bed->next)
    {
    int overlap = genomeRangeTreeOverlapSize(elderGrt, 
	bed->chrom, bed->chromStart, bed->chromEnd);
    totalOverlap += overlap;
    }
cor->sampleOverlapBases = totalOverlap;

/* Figure out sample/sample enrichment. */
double overlap = totalOverlap;
double covElder = overlap/cor->elderSampleBases;
double covYounger = overlap/cor->youngerSampleBases;
if (sameString(format, "fastq"))
    {
    /* Adjust for not all bases in fastq sample mapping. */
    covElder /= elderVf->mapRatio;
    covYounger /= youngerVf->mapRatio;
    }
double enrichment1 = covElder/((double)cor->youngerSampleBases/assembly->realBaseCount);
double enrichment2 = covYounger/((double)cor->elderSampleBases/assembly->realBaseCount);
cor->sampleSampleEnrichment = 0.5 * (enrichment1+enrichment2);

/* Save to database, clean up, go home. */
edwQaPairCorrelateSaveToDb(conn, cor, "edwQaPairCorrelate", 128);
genomeRangeTreeFree(&elderGrt);
bed3FreeList(&elderBedList);
bed3FreeList(&youngerBedList);
}

boolean getDoubleValAt(char *s, int tabsBefore)
/* Skip # of tabs before in s, and the return field there as a double */
{
while (--tabsBefore >= 0)
    {
    s = strchr(s, '\t');
    if (s == NULL)
        errAbort("Not enough tabs in getDoubleValAt");
    ++s;    // skip over tab itself
    }
return atof(s);    // Use atof so don't need to worry about trailing chars 
}

static void addBbTargetedCorrelations(struct bbiChromInfo *chrom, struct rbTree *targetRanges,
    struct bbiFile *aBbi, struct bbiFile *bBbi, int numColIx, struct correlate *c)
/* Find bits of a and b that overlap and also overlap with targetRanges.  Try to extract
 * some number from the bed (which number depends on format) */
{
struct lm *lm = lmInit(0);
struct bigBedInterval *a, *aList = bigBedIntervalQuery(aBbi, chrom->name, 0, chrom->size, 0, lm);
struct bigBedInterval *b, *bList = bigBedIntervalQuery(bBbi, chrom->name, 0, chrom->size, 0, lm);

/* This is a slightly complex but useful loop for two sorted lists that will get overlaps between 
 * the two in linear time. */
a = aList;
b = bList;
for (;;)
    {
    if (a == NULL || b == NULL)
        break;
    int s = max(a->start,b->start);
    int e = min(a->end,b->end);
    if (s < e)
        {
	/* Got intersection of a and b - is it also in targetRange? */
	int targetOverlap = rangeTreeOverlapSize(targetRanges, s, e);
	if (targetOverlap > 0)
	    {
	    double aVal = getDoubleValAt(a->rest, numColIx);
	    double bVal = getDoubleValAt(b->rest, numColIx);
	    correlateNextMulti(c, aVal, bVal, targetOverlap);
	    }
	}
    if (a->end < b->end)
       a = a->next;
    else 
       b = b->next;
    }
lmCleanup(&lm);
}


static struct genomeRangeTree *genomeRangeTreeForTarget(struct sqlConnection *conn,
    struct edwAssembly *assembly, char *enrichedIn)
/* Return genome range tree filled with enrichment target for assembly */
{
char query[256];
safef(query, sizeof(query), "select * from edwQaEnrichTarget where assemblyId=%d and name='%s'", 
    assembly->id, enrichedIn);
struct edwQaEnrichTarget *target = edwQaEnrichTargetLoadByQuery(conn, query);
if (target == NULL)
   errAbort("Can't find %s enrichment target for assembly %s", enrichedIn, assembly->name);
char *targetPath = edwPathForFileId(conn, target->fileId);
struct genomeRangeTree *targetGrt = edwGrtFromBigBed(targetPath);
edwQaEnrichTargetFree(&target);
freez(&targetPath);
return targetGrt;
}
    
void doBigBedReplicate(struct sqlConnection *conn, char *format, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do correlation analysis between elder and younger and save result to
 * a new edwQaPairCorrelate record. Do this for a format where we have a bigBed file. */
{
uglyf("doBigBedReplicate %s vs %s\n", elderEf->edwFileName, youngerEf->edwFileName);
int numColIx = 0;
if (sameString(format, "narrowPeak") || sameString(format, "broadPeak"))
    numColIx = 6;	// signalVal
else
    numColIx = 4;	// score
numColIx -= 3;		// Subtract off chrom/start/end
char *enrichedIn = elderVf->enrichedIn;
if (!isEmpty(enrichedIn) && !sameString(enrichedIn, "unknown"))
    {
    struct genomeRangeTree *targetGrt = genomeRangeTreeForTarget(conn, assembly, enrichedIn);

    /* Get open big bed files for both younger and older. */
    char *elderPath = edwPathForFileId(conn, elderEf->id);
    char *youngerPath = edwPathForFileId(conn, youngerEf->id);
    struct bbiFile *elderBbi = bigBedFileOpen(elderPath);
    struct bbiFile *youngerBbi = bigBedFileOpen(youngerPath);

    /* Loop through a chromosome at a time adding to correlation, and at the end save result in r.*/
    struct correlate *c = correlateNew();
    struct bbiChromInfo *chrom, *chromList = bbiChromList(elderBbi);
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	struct rbTree *targetRanges = genomeRangeTreeFindRangeTree(targetGrt, chrom->name);
	if (targetRanges != NULL)
	    addBbTargetedCorrelations(chrom, targetRanges, elderBbi, youngerBbi, numColIx, c);
	}

    /* Make up correlation structure . */
    struct edwQaPairCorrelate *cor;
    AllocVar(cor);
    cor->elderFileId = elderVf->fileId;
    cor->youngerFileId = youngerVf->fileId;
    cor->pearsonInEnriched = correlateResult(c);
    cor->gotPearsonInEnriched = TRUE;
    edwQaPairCorrelateSaveToDb(conn, cor, "edwQaPairCorrelate", 128);


    genomeRangeTreeFree(&targetGrt);
    freez(&cor);
    correlateFree(&c);
    bigBedFileClose(&youngerBbi);
    bigBedFileClose(&elderBbi);
    freez(&youngerPath);
    freez(&elderPath);
    }
}

static void addBwTargetedCorrelations(struct bbiChromInfo *chrom, struct rbTree *targetRanges,
    struct bigWigValsOnChrom *aVals, struct bigWigValsOnChrom *bVals,
    struct bbiFile *aBbi, struct bbiFile *bBbi, struct correlate *c)
/* Find bits of a and b that overlap and also overlap with targetRanges.  Do correlations there */
{
if (bigWigValsOnChromFetchData(aVals, chrom->name, aBbi) &&
    bigWigValsOnChromFetchData(bVals, chrom->name, bBbi) )
    {
    double *a = aVals->valBuf, *b = bVals->valBuf;
    struct range *range, *rangeList = rangeTreeList(targetRanges);
    for (range = rangeList; range != NULL; range = range->next)
        {
	int start = range->start, end = range->end, i;
	for (i=start; i<end; ++i)
	    correlateNext(c, a[i], b[i]);
	}
    }
}

void doBigWigReplicate(struct sqlConnection *conn, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do correlation analysis between elder and younger and save result to
 * a new edwQaPairCorrelate record. Do this for a format where we have a bigWig file. */
{
uglyf("doBigWigReplicate %s vs %s\n", elderEf->edwFileName, youngerEf->edwFileName);
char *enrichedIn = elderVf->enrichedIn;
if (!isEmpty(enrichedIn) && !sameString(enrichedIn, "unknown"))
    {
    struct genomeRangeTree *targetGrt = genomeRangeTreeForTarget(conn, assembly, enrichedIn);

    /* Get open big wig files for both younger and older. */
    char *elderPath = edwPathForFileId(conn, elderEf->id);
    char *youngerPath = edwPathForFileId(conn, youngerEf->id);
    struct bbiFile *elderBbi = bigWigFileOpen(elderPath);
    struct bbiFile *youngerBbi = bigWigFileOpen(youngerPath);

    /* Loop through a chromosome at a time adding to correlation, and at the end save result in r.*/
    struct correlate *c = correlateNew();
    struct bbiChromInfo *chrom, *chromList = bbiChromList(elderBbi);
    struct bigWigValsOnChrom *aVals = bigWigValsOnChromNew();
    struct bigWigValsOnChrom *bVals = bigWigValsOnChromNew();
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	struct rbTree *targetRanges = genomeRangeTreeFindRangeTree(targetGrt, chrom->name);
	if (targetRanges != NULL)
	    addBwTargetedCorrelations(chrom, targetRanges, aVals, bVals, elderBbi, youngerBbi, c);
	}

    /* Make up correlation structure . */
    struct edwQaPairCorrelate *cor;
    AllocVar(cor);
    cor->elderFileId = elderVf->fileId;
    cor->youngerFileId = youngerVf->fileId;
    cor->gotPearsonInEnriched = TRUE;
    edwQaPairCorrelateSaveToDb(conn, cor, "edwQaPairCorrelate", 128);


    bigWigValsOnChromFree(&bVals);
    bigWigValsOnChromFree(&aVals);
    genomeRangeTreeFree(&targetGrt);
    freez(&cor);
    correlateFree(&c);
    bigWigFileClose(&youngerBbi);
    bigWigFileClose(&elderBbi);
    freez(&youngerPath);
    freez(&elderPath);
    }
}

void doReplicatePair(struct sqlConnection *conn, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do processing on a pair of replicates. */
{
uglyf("And now for the show:\n\t%s\t%s\n", elderEf->submitFileName, youngerEf->submitFileName);
char *format = elderVf->format;
if (sameString(format, "fastq") || sameString(format, "bam") || sameString(format, "gff")
    || sameString(format, "gtf"))
    {
    doSampleReplicate(conn, format, assembly, elderEf, elderVf, youngerEf, youngerVf);
    }
else if (edwIsSupportedBigBedFormat(format))
    {
    doBigBedReplicate(conn, format, assembly, elderEf, elderVf, youngerEf, youngerVf);
    }
else if (sameString(format, "bigWig"))
    {
    doBigWigReplicate(conn, assembly, elderEf, elderVf, youngerEf, youngerVf);
    }
else if (sameString(format, "unknown"))
    {
    // Nothing we can do
    }
else
    errAbort("Unrecognized format %s in doReplicatePair", format);
}

void doReplicateQa(struct sqlConnection *conn, struct edwFile *ef)
/* Try and do replicate level QA - find matching file and do correlation-like
 * things. */
{
/* Get validated file info.  If not validated we don't bother. */
struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
if (vf == NULL)
    return;

char *replicate = vf->replicate;
if (!isEmpty(replicate) && !sameString(replicate, "both"))
    {
    /* Try to find other replicates of same experiment, format, and output type. */
    struct edwValidFile *elder, *elderList = findElderReplicates(conn, vf);
    if (elderList != NULL)
	{
	struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
	for (elder = elderList; elder != NULL; elder = elder->next)
	    {
	    doReplicatePair(conn, assembly, edwFileFromIdOrDie(conn, elder->fileId), elder, ef, vf);
	    }
	edwAssemblyFree(&assembly);
	}
    }
edwValidFileFree(&vf);
}

void edwMakeReplicateQa(int startId, int endId)
/* edwMakeReplicateQa - Do qa level comparisons of replicates.. */
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
    doReplicateQa(conn, ef);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwMakeReplicateQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
