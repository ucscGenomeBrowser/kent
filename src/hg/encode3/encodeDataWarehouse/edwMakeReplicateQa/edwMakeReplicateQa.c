/* edwMakeReplicateQa - Do qa level comparisons of replicates.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "hmmstats.h"
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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean pairExists(struct sqlConnection *conn, long long elderId, long long youngerId, char *table)
/* Return true if elder/younger pair already in table. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from %s where elderFileId=%lld and youngerFileId=%lld",
    table, elderId, youngerId);
return sqlQuickNum(conn, query) != 0;
}

struct bed3 *edwLoadSampleBed3(struct sqlConnection *conn, struct edwValidFile *vf)
/* Load up sample bed3 attached to vf */
{
char *sampleBed = vf->sampleBed;
if (isEmpty(sampleBed))
    errAbort("No sample bed for %s", vf->licensePlate);
return bed3LoadAll(sampleBed);
}

void setSampleSampleEnrichment(struct edwQaPairSampleOverlap *sam, char *format, 
    struct edwAssembly *assembly, struct edwValidFile *elderVf, struct edwValidFile *youngerVf)
/* Figure out sample/sample enrichment. */
{
double overlap = sam->sampleOverlapBases;
double covElder = overlap/sam->elderSampleBases;
double covYounger = overlap/sam->youngerSampleBases;
if (sameString(format, "fastq"))
    {
    /* Adjust for not all bases in fastq sample mapping. */
    if (elderVf->mapRatio != 0)
	covElder /= elderVf->mapRatio;
    if (youngerVf->mapRatio != 0)
	covYounger /= youngerVf->mapRatio;
    }
double enrichment1 = covElder/((double)sam->youngerSampleBases/assembly->realBaseCount);
double enrichment2 = covYounger/((double)sam->elderSampleBases/assembly->realBaseCount);
sam->sampleSampleEnrichment = 0.5 * (enrichment1+enrichment2);
}

void doBed3Replicate(struct sqlConnection *conn, char *format, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf, struct bed3 *elderBedList,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf, struct bed3 *youngerBedList)
/* Do correlation analysis between elder and younger bedLists and save result to
 * a new edwQaPairSampleOverlap record. Do this for a format where we have a bed3 sample file. */
{
struct edwQaPairSampleOverlap *sam;
AllocVar(sam);
sam->elderFileId = elderVf->fileId;
sam->youngerFileId = youngerVf->fileId;
sam->elderSampleBases = elderVf->basesInSample;
sam->youngerSampleBases = youngerVf->basesInSample;

/* Load up elder into genome range tree. */
struct genomeRangeTree *elderGrt = edwMakeGrtFromBed3List(elderBedList);

/* Load up younger as bed, and loop through to get overlap */
long long totalOverlap = 0;
struct bed3 *bed;
for (bed = youngerBedList; bed != NULL; bed = bed->next)
    {
    int overlap = genomeRangeTreeOverlapSize(elderGrt, 
	bed->chrom, bed->chromStart, bed->chromEnd);
    totalOverlap += overlap;
    }
sam->sampleOverlapBases = totalOverlap;
setSampleSampleEnrichment(sam, format, assembly, elderVf, youngerVf);

/* Save to database, clean up, go home. */
edwQaPairSampleOverlapSaveToDb(conn, sam, "edwQaPairSampleOverlap", 128);
freez(&sam);
genomeRangeTreeFree(&elderGrt);
}

void doSampleReplicate(struct sqlConnection *conn, char *format, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do correlation analysis between elder and younger and save result to
 * a new edwQaPairSampleOverlap record. Do this for a format where we have a bed3 sample file. */
{
if (pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairSampleOverlap"))
    return;
struct bed3 *elderBedList = edwLoadSampleBed3(conn, elderVf);
struct bed3 *youngerBedList = edwLoadSampleBed3(conn, youngerVf);
doBed3Replicate(conn, format, assembly, elderEf, elderVf, elderBedList,
		youngerEf, youngerVf, youngerBedList);
bed3FreeList(&elderBedList);
bed3FreeList(&youngerBedList);
}

void doBedReplicate(struct sqlConnection *conn, char *format, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do correlation analysis between elder and younger and save result to
 * a new edwQaPairCorrelation record. Do this for a format where we have a bigBed file. */
{
/* If got both pairs, work is done already */
if (pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairSampleOverlap"))
    return;

/* Get files for both younger and older. */
char *elderPath = edwPathForFileId(conn, elderEf->id);
char *youngerPath = edwPathForFileId(conn, youngerEf->id);

/* Do replicate calcs on bed3 lists from files. */
struct bed3 *elderBedList = bed3LoadAll(elderPath);
struct bed3 *youngerBedList = bed3LoadAll(youngerPath);
doBed3Replicate(conn, format, assembly, elderEf, elderVf, elderBedList,
		youngerEf, youngerVf, youngerBedList);

/* Clean up. */
bed3FreeList(&elderBedList);
bed3FreeList(&youngerBedList);
freez(&youngerPath);
freez(&elderPath);
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

long long bbIntervalListTotalSpan(struct bigBedInterval *ivList)
/* Return sum of end-start for whole list */
{
long long total = 0;
struct bigBedInterval *iv;
for (iv = ivList; iv != NULL; iv = iv->next)
    total += iv->end - iv->start;
return total;
}

static void addBbCorrelations(struct bbiChromInfo *chrom, struct genomeRangeTree *targetGrt,
    struct bbiFile *aBbi, struct bbiFile *bBbi, 
    int numColIx, struct correlate *c, struct correlate *cInEnriched,
    long long *aTotalSpan, long long *bTotalSpan, long long *overlapTotalSpan)
/* Find bits of a and b that overlap and also overlap with targetRanges.  Try to extract
 * some number from the bed (which number depends on format). Returns total number of
 * overlapping bases between the two big-beds. */
{
struct lm *lm = lmInit(0);
struct rbTree *targetRanges = NULL;
if (targetGrt != NULL)
    targetRanges = genomeRangeTreeFindRangeTree(targetGrt, chrom->name);
struct bigBedInterval *a, *aList = bigBedIntervalQuery(aBbi, chrom->name, 0, chrom->size, 0, lm);
struct bigBedInterval *b, *bList = bigBedIntervalQuery(bBbi, chrom->name, 0, chrom->size, 0, lm);
long long totalOverlap = 0;

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
    int overlap = e - s;
    if (overlap > 0)
        {
	totalOverlap += overlap;

	/* Do correlation over a/b overlap */
	double aVal = getDoubleValAt(a->rest, numColIx);
	double bVal = getDoubleValAt(b->rest, numColIx);
	correlateNextMulti(c, aVal, bVal, overlap);

	/* Got intersection of a and b - is it also in targetRange? */
	if (targetRanges)
	    {
	    int targetOverlap = rangeTreeOverlapSize(targetRanges, s, e);
	    if (targetOverlap > 0)
		{
		correlateNextMulti(cInEnriched, aVal, bVal, targetOverlap);
		}
	    }
	}
    if (a->end < b->end)
       a = a->next;
    else 
       b = b->next;
    }
*overlapTotalSpan += totalOverlap;
*aTotalSpan += bbIntervalListTotalSpan(aList);
*bTotalSpan += bbIntervalListTotalSpan(bList);
lmCleanup(&lm);
}


static struct genomeRangeTree *genomeRangeTreeForTarget(struct sqlConnection *conn,
    struct edwAssembly *assembly, char *enrichedIn)
/* Return genome range tree filled with enrichment target for assembly */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from edwQaEnrichTarget where assemblyId=%d and name='%s'", 
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
 * a new edwQaPairCorrelation record. Do this for a format where we have a bigBed file. */
{
/* If got both pairs, work is done already */
if (pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairSampleOverlap") 
    && pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairCorrelation"))
    return;

int numColIx = 0;
if (sameString(format, "narrowPeak") || sameString(format, "broadPeak"))
    numColIx = 6;	// signalVal
else
    numColIx = 4;	// score
numColIx -= 3;		// Subtract off chrom/start/end
char *enrichedIn = elderVf->enrichedIn;
struct genomeRangeTree *targetGrt = NULL;
if (!isEmpty(enrichedIn) && !sameString(enrichedIn, "unknown"))
    targetGrt = genomeRangeTreeForTarget(conn, assembly, enrichedIn);

/* Get open big bed files for both younger and older. */
char *elderPath = edwPathForFileId(conn, elderEf->id);
char *youngerPath = edwPathForFileId(conn, youngerEf->id);
struct bbiFile *elderBbi = bigBedFileOpen(elderPath);
struct bbiFile *youngerBbi = bigBedFileOpen(youngerPath);

/* Loop through a chromosome at a time adding to correlation, and at the end save result in r.*/
struct correlate *c = correlateNew(), *cInEnriched = correlateNew();
struct bbiChromInfo *chrom, *chromList = bbiChromList(elderBbi);
long long elderTotalSpan = 0, youngerTotalSpan = 0, overlapTotalSpan = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    addBbCorrelations(chrom, targetGrt, elderBbi, youngerBbi, numColIx, c, cInEnriched,
	&elderTotalSpan, &youngerTotalSpan, &overlapTotalSpan);
    }

/* Make up correlation structure and save. */
if (!pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairCorrelation"))
    {
    struct edwQaPairCorrelation *cor;
    AllocVar(cor);
    cor->elderFileId = elderVf->fileId;
    cor->youngerFileId = youngerVf->fileId;
    cor->pearsonOverall = correlateResult(c);
    cor->pearsonInEnriched = correlateResult(cInEnriched);
    edwQaPairCorrelationSaveToDb(conn, cor, "edwQaPairCorrelation", 128);
    freez(&cor);
    }

/* Also make up sample structure and save.  */
if (!pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairSampleOverlap"))
    {
    struct edwQaPairSampleOverlap *sam;
    AllocVar(sam);
    sam->elderFileId = elderVf->fileId;
    sam->youngerFileId = youngerVf->fileId;
    sam->elderSampleBases = elderTotalSpan;
    sam->youngerSampleBases = youngerTotalSpan;
    sam->sampleOverlapBases = overlapTotalSpan;
    setSampleSampleEnrichment(sam, format, assembly, elderVf, youngerVf);
    edwQaPairSampleOverlapSaveToDb(conn, sam, "edwQaPairSampleOverlap", 128);
    freez(&sam);
    }

genomeRangeTreeFree(&targetGrt);
correlateFree(&c);
bigBedFileClose(&youngerBbi);
bigBedFileClose(&elderBbi);
freez(&youngerPath);
freez(&elderPath);
}


static void addBwCorrelations(struct bbiChromInfo *chrom, struct genomeRangeTree *targetGrt,
    struct bigWigValsOnChrom *aVals, struct bigWigValsOnChrom *bVals,
    struct bbiFile *aBbi, struct bbiFile *bBbi, 
    double aThreshold, double bThreshold,
    struct correlate *c, struct correlate *cInEnriched, struct correlate *cClipped)
/* Find bits of a and b that overlap and also overlap with targetRanges.  Do correlations there */
{
struct rbTree *targetRanges = genomeRangeTreeFindRangeTree(targetGrt, chrom->name);
if (bigWigValsOnChromFetchData(aVals, chrom->name, aBbi) &&
    bigWigValsOnChromFetchData(bVals, chrom->name, bBbi) )
    {
    double *a = aVals->valBuf, *b = bVals->valBuf;
    int i, end = chrom->size;
    for (i=0; i<end; ++i)
	{
	double aVal = a[i], bVal = b[i];
	correlateNext(c, aVal, bVal);
	if (aVal > aThreshold) aVal = aThreshold;
	if (bVal > bThreshold) bVal = bThreshold;
	correlateNext(cClipped, aVal, bVal);
	}
    if (targetRanges != NULL)
	{
	struct range *range, *rangeList = rangeTreeList(targetRanges);
	for (range = rangeList; range != NULL; range = range->next)
	    {
	    int start = range->start, end = range->end;
	    for (i=start; i<end; ++i)
		correlateNext(cInEnriched, a[i], b[i]);
	    }
	}
    }
}

double twoStdsOverMean(struct bbiFile *bbi)
/* Figure out what is two standard deviations over mean for a bigWig file.  This is
 * an often useful threshold. */
{
struct bbiSummaryElement sum = bbiTotalSummary(bbi);
double mean = sum.sumData/sum.validCount;
double std = calcStdFromSums(sum.sumData, sum.sumSquares, sum.validCount);
return mean + 2*std;
}

void doBigWigReplicate(struct sqlConnection *conn, struct edwAssembly *assembly,
    struct edwFile *elderEf, struct edwValidFile *elderVf,
    struct edwFile *youngerEf, struct edwValidFile *youngerVf)
/* Do correlation analysis between elder and younger and save result to
 * a new edwQaPairCorrelation record. Do this for a format where we have a bigWig file. */
{
if (pairExists(conn, elderEf->id, youngerEf->id, "edwQaPairCorrelation"))
    return;
char *enrichedIn = elderVf->enrichedIn;
if (!isEmpty(enrichedIn) && !sameString(enrichedIn, "unknown"))
    {
    struct genomeRangeTree *targetGrt = genomeRangeTreeForTarget(conn, assembly, enrichedIn);

    /* Get open big wig files for both younger and older. */
    char *elderPath = edwPathForFileId(conn, elderEf->id);
    char *youngerPath = edwPathForFileId(conn, youngerEf->id);
    struct bbiFile *elderBbi = bigWigFileOpen(elderPath);
    struct bbiFile *youngerBbi = bigWigFileOpen(youngerPath);

    /* Figure out thresholds */
    double elderThreshold = twoStdsOverMean(elderBbi);
    double youngerThreshold = twoStdsOverMean(youngerBbi);

    /* Loop through a chromosome at a time adding to correlation, and at the end save result in r.*/
    struct correlate *c = correlateNew(), *cInEnriched = correlateNew(), *cClipped = correlateNew();
    struct bbiChromInfo *chrom, *chromList = bbiChromList(elderBbi);
    struct bigWigValsOnChrom *aVals = bigWigValsOnChromNew();
    struct bigWigValsOnChrom *bVals = bigWigValsOnChromNew();
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	addBwCorrelations(chrom, targetGrt, aVals, bVals, elderBbi, youngerBbi, 
	    elderThreshold, youngerThreshold, c, cInEnriched, cClipped);
	}

    /* Make up correlation structure . */
    struct edwQaPairCorrelation *cor;
    AllocVar(cor);
    cor->elderFileId = elderVf->fileId;
    cor->youngerFileId = youngerVf->fileId;
    cor->pearsonOverall = correlateResult(c);
    cor->pearsonInEnriched = correlateResult(cInEnriched);
    cor->pearsonClipped = correlateResult(cClipped);
    edwQaPairCorrelationSaveToDb(conn, cor, "edwQaPairCorrelation", 128);


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
verbose(1, "processing pair %lld %s  %lld %s\n", (long long)elderEf->id, 
    elderEf->submitFileName, (long long)youngerEf->id, youngerEf->submitFileName);
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
else if (startsWith("bed_", format) && edwIsSupportedBigBedFormat(format+4))
    {
    doBedReplicate(conn, format+4, assembly, elderEf, elderVf, youngerEf, youngerVf);
    }
else if (sameString(format, "bigWig"))
    {
    doBigWigReplicate(conn, assembly, elderEf, elderVf, youngerEf, youngerVf);
    }
else if (sameString(format, "rcc") || sameString(format, "idat") 
    || sameString(format, "customTrack"))
    {
    warn("Don't know how to compare %s files", format);
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
if (!isEmpty(replicate) && !sameString(replicate, "n/a") 
    && !sameString(replicate, "pooled")) // If expanding this, to expand bits in edwWebBrowse as well
    {
    /* Try to find other replicates of same experiment, format, and output type. */
    struct edwValidFile *elder, *elderList = edwFindElderReplicates(conn, vf);
    if (elderList != NULL)
	{
	char *targetDb = edwSimpleAssemblyName(vf->ucscDb);
	struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, targetDb);
	for (elder = elderList; elder != NULL; elder = elder->next)
	    {
	    if (sameString(targetDb, edwSimpleAssemblyName(elder->ucscDb)))
		doReplicatePair(conn, assembly, 
		    edwFileFromIdOrDie(conn, elder->fileId), elder, ef, vf);
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
sqlSafef(query, sizeof(query), 
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
