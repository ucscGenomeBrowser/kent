/* edwQaEvaluate - Consider available evidence and set edwValidFile.*QaStatus. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errAbort.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

/* Globals */
int version = 5;

/* Version history 
 *  1 - initial release
 *  2 - Relaxed most thresholds on RAMPAGE data type, which is not expected to map well now.
 *      Relaxed enrichment threshold in ChIP-seq and DNase-seq down to 2 since 'open is not 
 *      very specific.
 *      Split RNA-seq into:
 *      'Long RNA-seq' - reads 200 bases or longer or polyadenylated mRNA
 *      'RNA-seq' - unspecified, treated same as RNA-seq
 *      'Short RNA-seq' - reads 200 bases or shorter
 *      'miRNA-seq' - micro RNA sequencing
 *  3 - Relaxed DNAse threshold for enrichment in 'open chromatin' to 1.6.
 *  4 - Fixed bug that was putting reverse strand reads in BAM and FASTQ files in the wrong
 *      place.  Did not change thresholds, but getting much better (almost 2x better) 
 *      enrichments and maybe 50% better cross-enrichments as a result.  Also made it so
 *      that files had to be from two different replicates (not technical replicates) to
 *      get credit for a good replicateQa.
 *  5 - Relaxing thresholds for ENCODE2 experiments to be half of those for ENCODE3. 
 *      Relaxing threshold for ChIP-seq to cross-enrichment of 3 (for broad peaks)
 *      Relaxing threshold for ChIP-seq mapping from 80% to 65%
 */

boolean solexaFastqOk = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwQaEvaluate - Consider available evidence and set edwValidFile.*QaStatus\n"
  "version: %d\n"
  "usage:\n"
  "   edwQaEvaluate startId endId\n"
  "options:\n"
  "   -solexaFastqOk - if set will pass in spite of fastq being in solexa format\n"
  , version
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"solexaFastqOk", OPTION_BOOLEAN},
   {NULL, 0},
};

struct qaThresholds
/* Qa thresholds for a data type */
    {
    double fastqMapRatio;	// Minimum mapping ratio for fastq
    double bamMapRatio;		// Minimum mapping ratio for bam
    double fastqQual;	        // Minimum average quality for fastq
    double fastqPairConcordance;  // Minimum concordance between read pairs
    double repeatContent;       // Maximum total repeat content
    double ribosomeContent;	// Maximum ribosomal RNA content
    double closeContamination;	// Maximum contamination from close species (mammal vs mammal)
    double farContamination;	// Maximum contamintation from distant species
    double enrichment;		// Minimum enrichment
    double crossEnrichment;     // Minimum cross-enrichment
    double pearsonClipped;	// Minimum pearson clipped correlation
    };

struct qaThresholds dnaseThresholds = 
/* Thresholds for DNAse - pretty high since no introns like RNA-seq or base-changing
 * as in methy-seq. */
    {
    .fastqMapRatio = 0.50,
    .bamMapRatio = 0.50,
    .fastqQual = 20,
    .fastqPairConcordance = 0.8,
    .repeatContent = 0.1,
    .ribosomeContent = 0.05,
    .closeContamination = 0.06,
    .farContamination = 0.02,
    .enrichment = 1.6,
    .crossEnrichment = 15,
    .pearsonClipped = 0.10,
    };

struct qaThresholds longRnaSeqThresholds = 
/* Thresholds for RNA-seq - lower from introns and other issues. */
    {
    .fastqMapRatio = 0.20,
    .bamMapRatio = 0.50,
    .fastqQual = 20,
    .fastqPairConcordance = 0.6,  // introns
    .repeatContent = 0.5,
    .ribosomeContent = 0.15,
    .closeContamination = 0.5,
    .farContamination = 0.03,
    .enrichment = 4,
    .crossEnrichment = 5,
    .pearsonClipped = 0.05,
    };

struct qaThresholds shortRnaSeqThresholds = 
/* Thresholds for RNA-seq - lower from introns and other issues. */
    {
    .fastqMapRatio = 0.20,
    .bamMapRatio = 0.50,
    .fastqQual = 20,
    .fastqPairConcordance = 0.7,
    .repeatContent = 0.5,
    .ribosomeContent = 0.15,
    .closeContamination = 0.5,
    .farContamination = 0.03,
    .enrichment = 1.5,
    .crossEnrichment = 5,
    .pearsonClipped = 0.05,
    };

struct qaThresholds miRnaSeqThresholds = 
/* Thresholds for miRNA-seq - lower from introns and other issues. */
    {
    .fastqMapRatio = 0.0005,	// Low expectations for this to align with BWA - 22bp reads
    .bamMapRatio = 0.05,
    .fastqQual = 20,
    .fastqPairConcordance = 0.7,
    .repeatContent = 0.5,
    .ribosomeContent = 0.15,
    .closeContamination = 0.5,
    .farContamination = 0.03,
    .enrichment = 4,
    .crossEnrichment = 5,
    .pearsonClipped = 0.05,
    };

struct qaThresholds chipSeqThresholds = 
/* Chip-seq thresholds - pretty high since is all straight DNA */
    {
    .fastqMapRatio = 0.65,
    .bamMapRatio = 0.65,
    .fastqQual = 20,
    .fastqPairConcordance = 0.8,
    .repeatContent = 0.1,
    .ribosomeContent = 0.05,
    .closeContamination = 0.06,
    .farContamination = 0.02,
    .enrichment = 2,
    .crossEnrichment = 3,
    .pearsonClipped = 0.03,
    };

struct qaThresholds shotgunBisulfiteSeqThresholds = 
/* Chip-seq thresholds - pretty high since is all straight DNA */
    {
    .fastqMapRatio = 0.04,  // Don't expect much mapping with BWA
    .bamMapRatio = 0.3,
    .fastqQual = 20,
    .fastqPairConcordance = 0.7,
    .repeatContent = 0.1,
    .ribosomeContent = 0.05,
    .closeContamination = 0.06,
    .farContamination = 0.02,
    .enrichment = 2.5,
    .crossEnrichment = 5,
    .pearsonClipped = 0.05,
    };

struct qaThresholds rampageThresholds = 
/* Rampage is a way of sequencing selectively near the transcription start site.
 * It requires a special aligner, so don't expect too much from generic BWA here. */
    {
    .fastqMapRatio = 0.001,
    .bamMapRatio = 0.10,
    .fastqQual = 20,
    .fastqPairConcordance = 0.3,
    .repeatContent = 0.1,
    .ribosomeContent = 0.05,
    .closeContamination = 0.06,
    .farContamination = 0.02,
    .enrichment = 2.0,
    .crossEnrichment = 3,
    .pearsonClipped = 0.05,
    };

struct qaThresholds chiaPetThresholds = 
/* Chia pet links together DNA brought together from a long distance using an antibody. */
    {
    .fastqMapRatio = 0.001,
    .bamMapRatio = 0.40,
    .fastqQual = 25,
    .fastqPairConcordance = 0.001,
    .repeatContent = 0.1,
    .ribosomeContent = 0.05,
    .closeContamination = 0.06,
    .farContamination = 0.02,
    .enrichment = 1.5,
    .crossEnrichment = 1.1,
    .pearsonClipped = 0.0001,
    };

int failQa(struct sqlConnection *conn, struct edwFile *ef, char *whyFormat, ...)
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
   ;
   /* Explain why this failed QA */

int failQa(struct sqlConnection *conn, struct edwFile *ef, char *whyFormat, ...)
/* Explain why this failed QA  - always returns -1*/
{
/* First just do the warn */
warn("Failing QA on fileId %u", ef->id);
va_list args;
va_start(args, whyFormat);
char reason[512];
vasafef(reason, sizeof(reason), whyFormat, args);
warn("%s", reason);

/* See if already have a failure with this file and version. */
char query[512];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwQaFail where fileId=%u and qaVersion=%d", ef->id, version);
int prevFailCount = sqlQuickNum(conn, query);

/* If we are the first need to save it to database. */
if (prevFailCount == 0)
    {
    struct edwQaFail fail = {.id=0, .fileId=ef->id, .qaVersion=version, .reason=reason};
    edwQaFailSaveToDb(conn, &fail, "edwQaFail", 0);
    }
return -1;
}

struct edwQaRepeat *edwQaRepeatMatching(struct sqlConnection *conn, 
	long long fileId, char *repClass)
/* Return record associated with repeat of given class and fileId */
{
char query[256];
sqlSafef(query, sizeof(query),
    "select * from edwQaRepeat where fileId=%lld and repeatClass='%s'"
    , fileId, repClass);
return edwQaRepeatLoadByQuery(conn, query);
}

boolean isInArray(int *array, int size, int x)
/* Return TRUE if x is in array of given size. */
{
int i;
for (i=0; i<size; ++i)
    if (array[i] == x)
        return TRUE;
return FALSE;
}

int mammalianTaxons[] = {9606, 10090, 10116};
char *mammalianTaxonsString = "9606,10090,10116";

double maxContam(struct sqlConnection *conn, struct edwValidFile *vf, boolean isClose)
/* Get list of contamination targets that are close or far */
{
char query[512];
sqlSafef(query, sizeof(query), "select taxon from edwAssembly where ucscDb='%s'", vf->ucscDb);
int taxon = sqlQuickNum(conn, query);
assert(taxon != 0);

boolean isMammal = isInArray(mammalianTaxons, ArraySize(mammalianTaxons), taxon);
if (!isMammal)
    {
    /* Non-mammalian case is relatively easy - there are no close species, everything is distant */
    if (isClose)
        return 0.0;
    else
        {
	sqlSafef(query, sizeof(query), "select max(mapRatio) from edwQaContam where fileId=%u",
	    vf->fileId);
	}
    }
else
    {
    sqlSafef(query, sizeof(query), 
	"select max(mapRatio) from edwQaContam c,edwQaContamTarget t, edwAssembly a "
	"where c.fileId=%u and c.qaContamTargetId=t.id and t.assemblyId = a.id "
	"and a.taxon %s in (%s)", 
	vf->fileId, (isClose ? "" : "not"), mammalianTaxonsString);
    }
return sqlQuickDouble(conn, query);
}

boolean passContamination(struct sqlConnection *conn,
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp,
    struct qaThresholds *threshold)
/* Return TRUE if pass QA threshold. */
{
double closeContamination = maxContam(conn, vf, TRUE);
double farContamination = maxContam(conn, vf, FALSE);
if (closeContamination > threshold->closeContamination)
    {
    failQa(conn, ef, "closeContamination = %g, threshold for %s is %g",
	closeContamination, exp->dataType, threshold->closeContamination);
    return FALSE;
    }
else if (farContamination > threshold->farContamination)
    {
    failQa(conn, ef, "farContamination = %g, threshold for %s is %g", 
	farContamination, exp->dataType, threshold->farContamination);
    return FALSE;
    }
return TRUE;
}

boolean edwBestPairedSomething(struct sqlConnection *conn, char *query, double *retBest)
/* Finish up some sort of max query */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
boolean gotResult = row[0] != NULL;
if (gotResult)
    *retBest = sqlDouble(row[0]);
sqlFreeResult(&sr);
return gotResult;
}

boolean edwBestCrossEnrichment(struct sqlConnection *conn, long long fileId, double *retBest)
/* If there are biological replicates for same experiment and output type,  return
 * TRUE and calculate the best cross enrichment. */
{
char query[256];
sqlSafef(query, sizeof(query),
    "select max(sampleSampleEnrichment) from edwQaPairSampleOverlap p,edwValidFile e,edwValidFile y "
    "where elderFileId=e.fileId and youngerFileId=y.fileId and e.replicate != y.replicate "
    "and (elderFileId = %lld or youngerFileId = %lld) "
    , fileId, fileId);
return edwBestPairedSomething(conn, query, retBest);
}

boolean edwBestPearsonClipped(struct sqlConnection *conn, long long fileId, double *retBest)
/* If there are biological replicates for same experiment and output type,  return
 * TRUE and calculate the best cross enrichment. */
{
char query[256];
sqlSafef(query, sizeof(query),
    "select max(pearsonClipped) from edwQaPairCorrelation p,edwValidFile e,edwValidFile y "
    "where elderFileId=e.fileId and youngerFileId=y.fileId and e.replicate != y.replicate "
    "and (elderFileId = %lld or youngerFileId = %lld) "
    , fileId, fileId);
return edwBestPairedSomething(conn, query, retBest);
}

int respectManualOverride(int newVal, int oldVal)
/* If oldVal shows manual override, return it, else newVal. */
{
if (oldVal > 1 || oldVal < -1)
    return oldVal;
return newVal;
}

void checkThresholds(struct sqlConnection *conn,
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp, 
    struct qaThresholds *threshold)
/* Check that files meet thresholds if possible. */
{
char query[512];
char *dataType = exp->dataType;
boolean isEncode2 = sameWord(exp->rfa, "ENCODE2");

/* Make local copy of threshold with oldFactor weighed in if need be. */
struct qaThresholds lt = *threshold;
threshold = &lt;
if (isEncode2)
    {
    double oldFactor = 0.5;
    lt.fastqMapRatio *= oldFactor;
    lt.bamMapRatio *= oldFactor;
    lt.fastqQual *= oldFactor;
    lt.fastqPairConcordance *= oldFactor;
    lt.repeatContent /= oldFactor;
    lt.ribosomeContent /= oldFactor;
    lt.closeContamination /= oldFactor;
    lt.farContamination /= oldFactor;
    lt.enrichment *= oldFactor;
    lt.crossEnrichment *= oldFactor;
    lt.pearsonClipped *= oldFactor;
    }

/* First check individual file thresholds. */
int passStat = 1;
int pairPassStat = 0;
if (sameString(vf->format, "fastq"))
    {
    /* Check a bunch of things either in edwValidFile record or edwFastqFile record. */
    struct edwFastqFile *fq = edwFastqFileFromFileId(conn, ef->id);
    if (fq == NULL)
       errAbort("No edwFastqFile for file ID %u", ef->id);
    if (vf->mapRatio < threshold->fastqMapRatio)
        {
	passStat = failQa(conn, ef, "Map ratio is %g, threshold for %s is %g", 
	    vf->mapRatio, dataType, threshold->fastqMapRatio);
	}
    else if (!sameWord(fq->qualType, "sanger") && !solexaFastqOk && !isEncode2)
        {
	passStat = failQa(conn, ef, "Quality scores type %s not sanger", fq->qualType);
	}
    else if (fq->qualMean < threshold->fastqQual)
        {
	passStat = failQa(conn, ef, "Mean quality score is %g, threshold for %s is %g",
	    fq->qualMean, dataType, threshold->fastqQual);
	}
    else if (!isEmpty(vf->pairedEnd))
        {
	struct edwValidFile *otherVf = edwOppositePairedEnd(conn, vf);
	if (otherVf == NULL)
	    passStat = 0;   // Need to wait for other end.
	else
	    {
	    struct edwQaPairedEndFastq *pair = edwQaPairedEndFastqFromVfs(conn, vf, otherVf,
			    NULL, NULL);
	    if (pair == NULL)
		errAbort("edwQaPairedEndFastq not found for %u %u", vf->fileId, otherVf->fileId);
	    if (pair->concordance < threshold->fastqPairConcordance)
	       failQa(conn, ef, "Pair concordance is %g, threshold for %s is %g",
			pair->concordance, dataType, threshold->fastqPairConcordance);
	    edwQaPairedEndFastqFree(&pair);
	    }
	}

    /* If still passing check repeatContent */
    if (passStat == 1)
        {
	struct edwQaRepeat *rep = edwQaRepeatMatching(conn, ef->id, "total");
	if (rep == NULL)
	    errAbort("No total repeat record for file id %u", ef->id);
	if (rep->mapRatio > threshold->repeatContent)
	    passStat = failQa(conn, ef, "Repeat content is %g, threshold for %s is %g",
		rep->mapRatio, dataType, threshold->repeatContent);
	edwQaRepeatFree(&rep);
	}

    /* If still passing check ribosomeContent */
    if (passStat == 1)
        {
	struct edwQaRepeat *rep = edwQaRepeatMatching(conn, ef->id, "rRNA");
	if (rep != NULL && rep->mapRatio > threshold->ribosomeContent)
	    {
	    passStat = failQa(conn, ef, "Ribosomal content is %g, threshold for %s is %g",
	       rep->mapRatio, dataType, threshold->ribosomeContent);
	    }
	edwQaRepeatFree(&rep);
	}

    /* If still passing check for contamination with other organisms. */
    if (passStat == 1)
	if (!passContamination(conn, ef, vf, exp, threshold))
	    passStat = -1;

    edwFastqFileFree(&fq);
    }
else if (sameString(vf->format, "bam"))
    {
    if (vf->mapRatio < threshold->bamMapRatio)
        {
	passStat = failQa(conn, ef, "Map ratio is %g, threshold for %s is %g", 
	    vf->mapRatio, dataType, threshold->fastqMapRatio);
	}
    }
if (sameString(vf->format, "bam") || sameString(vf->format, "fastq") 
   || sameString(vf->format, "bigBed") || sameString(vf->format, "broadPeak")
   || sameString(vf->format, "narrowPeak") || sameString(vf->format, "gtf")
   || sameString(vf->format, "bigWig"))
    {
    /* If still passing check for enrichment */
    char *enrichedIn = vf->enrichedIn;
    if (passStat == 1 && !isEmpty(enrichedIn) && !sameWord(enrichedIn, "unknown"))
	{
	sqlSafef(query, sizeof(query),
	   "select e.enrichment from edwQaEnrich e,edwQaEnrichTarget t "
	   "where e.qaEnrichTargetId = t.id "
	   "and e.fileId = %u "
	   "and t.name = '%s' "
	   , ef->id, enrichedIn);
	double enrichment = sqlQuickDouble(conn, query);
	if (enrichment < threshold->enrichment)
	    {
	    passStat = failQa(conn, ef, "Enrichment in %s is %g, threshold for %s is %g",
		enrichedIn, enrichment, dataType, threshold->enrichment);
	    }
	}

    /* Finally check cross enrichment */
    if (passStat == 1)
	{
	if (sameString(vf->format, "bigWig"))
	    {
	    double r = 0;
	    if (edwBestPearsonClipped(conn, ef->id, &r))
		{
		if (r < threshold->pearsonClipped)
		    pairPassStat = failQa(conn, ef, "ClippedR is %g, threshold for %s is %g",
			r, dataType, threshold->pearsonClipped);
		else
		    pairPassStat = 1;
		}
	    }
	else
	    {
	    double crossEnrichment = 0;
	    if (edwBestCrossEnrichment(conn, ef->id, &crossEnrichment))
		{
		if (crossEnrichment < threshold->crossEnrichment)
		    pairPassStat = failQa(conn, ef, "CrossEnrichment is %g, threshold for %s is %g",
			crossEnrichment, dataType, threshold->crossEnrichment);
		else
		    pairPassStat = 1;
		}
	    }
	}
    }
else
    passStat = 0;
passStat = respectManualOverride(passStat, vf->singleQaStatus);
pairPassStat = respectManualOverride(pairPassStat, vf->replicateQaStatus);
sqlSafef(query, sizeof(query),
    "update edwValidFile set singleQaStatus=%d, replicateQaStatus=%d, qaVersion=%d "
    "where id=%u",  passStat, pairPassStat, version, vf->id);
sqlUpdate(conn, query);
}

void edwQaEvaluate(int startFileId, int endFileId)
/* edwQaEvaluate - Consider available evidence and set edwValidFile.*QaStatus. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startFileId, endFileId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    verbose(2, "Looking at %u\n", ef->id);
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	verbose(2, " aka %s\n", vf->licensePlate);
	if (!isEmpty(vf->experiment))
	    {
	    struct edwExperiment *exp = edwExperimentFromAccession(conn, vf->experiment);
	    if (exp != NULL)
		{
		char *dataType = exp->dataType;
		struct qaThresholds *thresholds = NULL;
		if (sameWord("DNase-seq", dataType))
		    thresholds = &dnaseThresholds;
		else if (sameWord("RNA-seq", dataType) || sameWord("Long RNA-seq", dataType))
		    thresholds = &longRnaSeqThresholds;   
		else if (sameWord("short RNA-seq", dataType))
		    thresholds = &shortRnaSeqThresholds;   
		else if (sameWord("miRNA-seq", dataType))
		    thresholds = &miRnaSeqThresholds;   
		else if (sameWord("ChIP-seq", dataType))
		    thresholds = &chipSeqThresholds;
		else if (sameWord("Shotgun Bisulfite-seq", dataType))
		    thresholds = &shotgunBisulfiteSeqThresholds;
		else if (sameWord("RAMPAGE", dataType))
		    thresholds = &rampageThresholds;
		else if (sameWord("ChiaPet", dataType))
		    thresholds = &chiaPetThresholds;
		else if (sameWord("", dataType))
		    ;
		else
		    verbose(2, "No thresholds for data type %s", dataType);
		verbose(2, "  dataType %s, thresholds %p\n", dataType, thresholds);
		if (thresholds != NULL)
		    checkThresholds(conn, ef, vf, exp, thresholds);
		}
	    else
		warn("Can't find experiment for '%s'", vf->experiment);
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
solexaFastqOk = optionExists("solexaFastqOk");
edwQaEvaluate(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
