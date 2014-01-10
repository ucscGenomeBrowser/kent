/* edwScheduleAnalysis - Schedule analysis runs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "bamFile.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "eapLib.h"

boolean again = FALSE;
boolean retry = FALSE;
boolean updateSoftwareMd5 = FALSE;
boolean noJob = FALSE;
boolean ignoreQa = FALSE;
boolean justLink = FALSE;
boolean dry = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwScheduleAnalysis - Schedule analysis runs.\n"
  "usage:\n"
  "   edwScheduleAnalysis startFileId endFileId\n"
  "options:\n"
  "   -retry - if job has run and failed retry it\n"
  "   -again - if set schedule it even if it's been run once\n"
  "   -up - update on software MD5s rather than aborting on them\n"
  "   -noJob - if set then don't put job on edwAnalysisJob table\n"
  "   -ignoreQa - if set then ignore QA results when scheduling\n"
  "   -justLink - just symbolically link rather than copy EDW files to cache\n"
  "   -dry - just print out the commands that would result\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"retry", OPTION_BOOLEAN},
   {"again", OPTION_BOOLEAN},
   {"up", OPTION_BOOLEAN},
   {"noJob", OPTION_BOOLEAN},
   {"ignoreQa", OPTION_BOOLEAN},
   {"justLink", OPTION_BOOLEAN},
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

struct edwAssembly *targetAssemblyForDbAndSex(struct sqlConnection *conn, char *ucscDb, char *sex)
/* Figure out best target assembly for something that is associated with a given ucscDB.  
 * In general this will be the most recent for the organism. */
{
char *newDb = ucscDb;
if (sameString("hg19", ucscDb))
    newDb = "hg38";
char sexedName[128];
safef(sexedName, sizeof(sexedName), "%s.%s", sex, newDb);
char query[256];
sqlSafef(query, sizeof(query), "select * from edwAssembly where name='%s'", sexedName);
return edwAssemblyLoadByQuery(conn, query);
}

struct edwBiosample *edwBiosampleFromTerm(struct sqlConnection *conn, char *term)
/* Return biosamples associated with term. */
{
char query[512];
sqlSafef(query, sizeof(query), "select * from edwBiosample where term = '%s'", term);
return edwBiosampleLoadByQuery(conn, query);
}

char *sexFromExp(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf)
/* Return "male" or "female" */
{
char *sex = "male";

struct edwExperiment *exp = edwExperimentFromAccession(conn, vf->experiment);
if (exp != NULL)
    {
    struct edwBiosample *bio = edwBiosampleFromTerm(conn, exp->biosample);
    if (bio != NULL)
        {
	if (sameWord(bio->sex, "F"))
	    sex = "female";
	}
    }
if (sex == NULL)
    {
    // Warn not fatal since mapping to male genome not a big deal, and also there
    // are a few cases without biosample
    warn("Can't find sex for fileId %u", ef->id);
    }
return sex;
}

struct edwAssembly *chooseTarget(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf)
/* Pick mapping target - according to taxon and sex. */
{
char *sex = sexFromExp(conn, ef, vf);
return targetAssemblyForDbAndSex(conn, vf->ucscDb, sex);
}

static int countAlreadyScheduled(struct sqlConnection *conn, char *analysisStep, 
    long long firstInput)
/* Return how many of the existing job are already scheduled */
{
if (again)
    return 0;
char query[512];
if (retry)
    sqlSafef(query, sizeof(query),
	"select count(*) from edwAnalysisRun"
	" where firstInputId=%lld and analysisStep='%s' and createStatus >= 0",
	firstInput, analysisStep);
else
    sqlSafef(query, sizeof(query),
	"select count(*) from edwAnalysisRun where firstInputId=%lld and analysisStep='%s'",
	firstInput, analysisStep);
return sqlQuickNum(conn, query);
}

static char *newTempDir(char *name)
/* Return a freshly created temp dir where name occurs somewhere in it's name. */
{
char *tempDir = rTempName(eapTempDir, name, "");
makeDir(tempDir);
int withLastSize = strlen(tempDir) + 2;
char withLast[withLastSize];
safef(withLast, sizeof(withLast), "%s/", tempDir);
return cloneString(withLast);
}


char *scheduleStep(struct sqlConnection *conn, char *tempDir,
    char *analysisStep, char *commandLine, char *experiment, struct edwAssembly *assembly,
    int inCount, unsigned inputIds[], char *inputTypes[],
    int outCount, char *outputNames[], char *outputTypes[], char *outputFormats[])
/* Schedule a step.  Returns temp dir it is to be run in. */
{
struct edwAnalysisStep *step = edwAnalysisStepFromNameOrDie(conn, analysisStep);

if (updateSoftwareMd5)
    edwAnalysisSoftwareUpdateMd5ForStep(conn, analysisStep);
edwAnalysisCheckVersions(conn, analysisStep);

if (dry)
    printf("%s\n", commandLine);
else
    {
    verbose(1, "scheduling %s on %u in %s\n", analysisStep, inputIds[0], tempDir);

    /* Wrap command line in cd to temp dir */
    char fullCommandLine[strlen(commandLine)+128];
    safef(fullCommandLine, sizeof(fullCommandLine), 
	"edwCdJob %s", commandLine);

    /* Make up edwAnalysisRun record */
    struct edwAnalysisRun *run;
    AllocVar(run);
    if (!noJob)
	run->jobId = edwAnalysisJobAdd(conn, fullCommandLine, step->cpusRequested);
    safef(run->experiment, sizeof(run->experiment), "%s",  experiment);
    run->analysisStep = analysisStep;
    run->configuration = "";
    run->tempDir = tempDir;
    run->firstInputId = inputIds[0];
    run->inputFileCount = inCount;
    run->inputFileIds = inputIds;
    run->inputTypes = inputTypes;
    run->outputFileCount = outCount;
    run->outputNamesInTempDir = outputNames;
    run->outputFormats = outputFormats;
    run->outputTypes = outputTypes;
    if (assembly != NULL) run->assemblyId = assembly->id;
    run->jsonResult = "";
    edwAnalysisRunSaveToDb(conn, run, "edwAnalysisRun", 0);
    freez(&run);
    }
return tempDir;
}

static void makeBwaIndexPath(struct edwAssembly *assembly, char indexPath[PATH_LEN])
/* Fill in path to BWA index. */
{
safef(indexPath, PATH_LEN, "%s%s/bwaData/%s.fa", 
    "/scratch/data/encValDir/", assembly->name, assembly->name);
}

struct edwAssembly *getBwaIndex(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf, 
    char indexPath[PATH_LEN])
/* Target for now is same as UCSC DB.  Returns assembly ID */
{
struct edwAssembly *assembly = chooseTarget(conn, ef, vf);
makeBwaIndexPath(assembly, indexPath);
return assembly;
}

struct cache
/* Keep track of files to keep local copies of. */
    {
    struct slRef *list;  // Keyed by licensePlate, value is file name w/in cache
    };

void eapCacheName(struct edwFile *ef, char fileName[PATH_LEN])
/* Fill in fileName with name of ef when cached locally */
{
/* Figure out file name within cache. */
char *noDir = strrchr(ef->edwFileName, '/');
assert(noDir != NULL);
noDir += 1;
safef(fileName, PATH_LEN, "%s%s", eapEdwCacheDir, noDir);
}

char *cacheMore(struct cache *cache, struct edwFile *ef, struct edwValidFile *vf)
/* Add file to hash and return translated name. */
{
refAdd(&cache->list, ef);
char fileName[PATH_LEN];
eapCacheName(ef, fileName);
return cloneString(fileName);
}

struct cache *cacheNew()
/* Return new empty cache */
{
struct cache *cache;
return AllocVar(cache);
}

void preloadCache(struct sqlConnection *conn, struct cache *cache)
/* If there's anything that needs precashing, return a command line
 * with a semicolon at the end to precache.  Otherwise return blank. */
{
if (dry)
   return;
struct slRef *el;
for (el = cache->list; el != NULL; el = el->next)
    {
    /* Figure out file name within cache. */
    struct edwFile *ef = el->val;
    char fileName[PATH_LEN];
    eapCacheName(ef, fileName);

    /* Copy file into cache if it's not already there */
    if (!fileExists(fileName))
	{
	char *source = edwPathForFileId(conn, ef->id);
	if (justLink)
	    {
	    verbose(1, "Symbolic link of %s to %s (%6.1g gig)\n", 
		source, fileName, fileSize(source)/1e9);
	    if (symlink(source, fileName) < 0)
	        errnoAbort("Couldn't symlink %s to %s", source, fileName);
	    }
	else
	    {
	    verbose(1, "Copy %s to %s (%6.1g gig)\n", source, fileName, fileSize(source)/1e9);
	    copyFile(source, fileName);
	    }
	}
    }
}

char *pickPairedBwaCommand(struct sqlConnection *conn, struct edwValidFile *vf1, struct edwValidFile *vf2)
/* Pick either regular of solexa-converting bwa step. */
{
char *result = NULL;
struct edwFastqFile *fq1 = edwFastqFileFromFileId(conn, vf1->fileId);
struct edwFastqFile *fq2 = edwFastqFileFromFileId(conn, vf2->fileId);
if (!sameString(fq1->qualType, fq2->qualType))
    errAbort("Mixed quality types in %u and %u", vf1->fileId, vf2->fileId);
if (sameWord(fq1->qualType, "solexa"))
    result = "eap_run_slx_bwa_pe";
else if (sameWord(fq1->qualType, "sanger"))
    result = "eap_run_bwa_pe";
else
    internalErr();
edwFastqFileFree(&fq1);
edwFastqFileFree(&fq2);
return result;
}

char *pickSingleBwaCommand(struct sqlConnection *conn, struct edwValidFile *vf)
/* Pick either regular of solexa-converting bwa step. */
{
struct edwFastqFile *fq = edwFastqFileFromFileId(conn, vf->fileId);
char *result = NULL;
if (sameWord(fq->qualType, "solexa"))
    result = "eap_run_slx_bwa_se";
else if (sameWord(fq->qualType, "sanger"))
    result = "eap_run_bwa_se";
else
    internalErr();
edwFastqFileFree(&fq);
return result;
}

void scheduleBwaPaired(struct sqlConnection *conn,  
    struct edwValidFile *vf1, struct edwValidFile *vf2, struct edwExperiment *exp)
/* If it hasn't already been done schedule bwa analysis of the two files. */
{
/* Make sure that we don't schedule it again and again */
char *analysisStep = "bwa_paired_end";
if (countAlreadyScheduled(conn, analysisStep, vf1->fileId))
    return;

/* Get ef records */
struct edwFile *ef1 = edwFileFromIdOrDie(conn, vf1->fileId);
struct edwFile *ef2 = edwFileFromIdOrDie(conn, vf2->fileId);

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
struct edwAssembly *assembly = getBwaIndex(conn, ef1, vf1, indexPath);

/* Grab temp dir */
char *tempDir = newTempDir(analysisStep);
char *outBamName = "out.bam";

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *ef1Name = cacheMore(cache, ef1, vf1);
char *ef2Name = cacheMore(cache, ef2, vf2);
char *command = pickPairedBwaCommand(conn, vf1, vf2);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), 
    "%s %s %s %s %s%s", 
    command, indexPath, ef1Name, ef2Name, tempDir, outBamName);

/* Make up edwAnalysisRun record and schedule it*/
unsigned inFileIds[2] = {ef1->id, ef2->id};
char *inTypes[2] = {"read1", "read2"};
char *outNames[1] = {outBamName};
char *outTypes[1] = {"alignments"};
char *outFormats[1] = {"bam"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);

/* Clean up */
edwFileFree(&ef1);
edwFileFree(&ef2);
}

void scheduleBwaSingle(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule bwa analysis of the two files. */
{
/* Make sure that we don't schedule it again and again */
char *analysisStep = "bwa_single_end";
if (countAlreadyScheduled(conn, analysisStep, ef->id))
    return;

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
struct edwAssembly *assembly = getBwaIndex(conn, ef, vf, indexPath);

/* Grab temp dir */
char *tempDir = newTempDir(analysisStep);
char *outBamName = "out.bam";

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef, vf);
char *command = pickSingleBwaCommand(conn, vf);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s%s", 
    command, indexPath, efName, tempDir, outBamName);

/* Make up edwAnalysisRun record and schedule it*/
unsigned inFileIds[1] = {ef->id};
char *inTypes[1] = {"reads"};
char *outNames[1] = {"out.bam"};
char *outTypes[1] = {"alignments"};
char *outFormats[1] = {"bam"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);
}

boolean bamIsPaired(char *fileName, int maxCount)
/* Read up to maxCount records, figure out if BAM is from a paired run */
{
boolean isPaired = FALSE;
samfile_t *sf = samopen(fileName, "rb", NULL);
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
int i;
for (i=0; i<maxCount; ++i)
    {
    int err = bam_read1(sf->x.bam, &one);
    if (err < 0)
	break;
    if (one.core.flag&BAM_FPAIRED)
       {
       isPaired = TRUE;
       break;
       }
    }
samclose(sf);
return isPaired;
}

void scheduleMacsDnase(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule macs analysis of the file. */
{
// Figure out input bam name
char bamName[PATH_LEN];
safef(bamName, sizeof(bamName), "%s%s", edwRootDir, ef->edwFileName);

// Figure out analysis step and script based on paired end status of input bam
boolean isPaired = bamIsPaired(bamName, 1000);
char *analysisStep, *scriptName;
if (isPaired)
    {
    analysisStep = "macs2_dnase_pe";
    scriptName = "eap_run_macs2_dnase_pe";
    }
else
    {
    analysisStep = "macs2_dnase_se";
    scriptName = "eap_run_macs2_dnase_se";
    }

verbose(2, "schedulingMacsDnase on %s step %s, script %s\n", ef->edwFileName, 
    analysisStep, scriptName);
/* Make sure that we don't schedule it again and again */
if (countAlreadyScheduled(conn, analysisStep, ef->id))
    return;

struct edwAssembly *assembly = chooseTarget(conn, ef, vf);
char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef, vf);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s%s %s%s", 
    scriptName, assembly->ucscDb, efName, 
    tempDir, "out.narrowPeak.bigBed", tempDir, "out.bigWig");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"alignments"};
char *outFormats[] = {"narrowPeak", "bigWig"};
char *outNames[] = {"out.narrowPeak.bigBed", "out.bigWig"};
char *outTypes[] = {"macs2_dnase_peaks", "macs2_dnase_signal"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);
}

int mustGetReadLengthForFastq(struct sqlConnection *conn, unsigned fileId)
/* Verify that associated file is in edwFastqFile table and return average read length */
{
char query[256];
sqlSafef(query, sizeof(query), "select readSizeMean from edwFastqFile where fileId=%u", fileId);
double result = sqlQuickDouble(conn, query);
if (result < 1)
    errAbort("No edwFastqFile record for %u", fileId);
return round(result);
}

boolean edwAnalysisRunHasOutput(struct edwAnalysisRun *run, unsigned outId)
/* Return TRUE if any of the outputs of run match outId */
{
int i;
for (i=0; i<run->createCount; ++i)
    if (run->createFileIds[i] == outId)
        return TRUE;
return FALSE;
}


unsigned getFastqForBam(struct sqlConnection *conn, unsigned bamFileId)
/* Return fastq id associated with bam file or die trying. */
{
unsigned fastqId = 0;   

/* Silly this - we have to scan all analysisRun records. */
char query[512];
sqlSafef(query, sizeof(query), 
    "select * from edwAnalysisRun where createStatus > 0  and createCount > 0");
struct edwAnalysisRun *run, *runList = edwAnalysisRunLoadByQuery(conn, query);

for (run = runList; run != NULL; run = run->next)
    {
    if (edwAnalysisRunHasOutput(run, bamFileId))
       {
       struct edwValidFile *vf = edwValidFileFromFileId(conn, run->firstInputId);
       if (sameString(vf->format, "fastq"))
	   {
	   fastqId = vf->fileId;
	   break;
	   }
       else
           {
	   fastqId = getFastqForBam(conn, vf->fileId);
	   if (fastqId != 0)
	       break;
	   }
       edwValidFileFree(&vf);
       }
    }

edwAnalysisRunFreeList(&runList);
return fastqId;
}

int bamReadLength(char *fileName, int maxCount)
/* Open up bam, read up to maxCount reads, and return size of read, enforcing
 * all are same size. */
{
int readLength = 0;
samfile_t *sf = samopen(fileName, "rb", NULL);
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
int i;
for (i=0; i<maxCount; ++i)
    {
    int err = bam_read1(sf->x.bam, &one);
    if (err < 0)
	break;
    int length = one.core.l_qseq;
    if (readLength == 0)
        readLength = length;
    else if (readLength != length)
        errAbort("Varying size reads in %s, %d and %d", fileName, readLength, length);
    }
samclose(sf);
assert(readLength != 0);
return readLength;
}

void scheduleHotspot(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule hotspot analysis of the file. */
{
/* Make sure that we don't schedule it again and again */
char *analysisStep = "hotspot";
if (countAlreadyScheduled(conn, analysisStep, ef->id))
    return;

verbose(2, "schedulingHotspot on %s step %s\n", ef->edwFileName, analysisStep);
/* Make command line */
struct edwAssembly *assembly = chooseTarget(conn, ef, vf);
int fastqId = getFastqForBam(conn, ef->id);
int readLength = 0;
if (fastqId == 0)
    readLength = bamReadLength(edwPathForFileId(conn, ef->id), 1000);
else
    readLength = mustGetReadLengthForFastq(conn, fastqId);

char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef, vf);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "eap_run_hotspot %s %s %d %s%s %s%s %s%s",
    assembly->ucscDb, efName,
    readLength, tempDir, "out.narrowPeak.bigBed", 
    tempDir, "out.broadPeak.bigBed", tempDir, "out.bigWig");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"alignments"};
char *outFormats[] = {"broadPeak", "narrowPeak", "bigWig"};
char *outNames[] = {"out.broadPeak.bigBed", "out.narrowPeak.bigBed", "out.bigWig"};
char *outTypes[] = {"hotspot_broad_peaks", "hotspot_narrow_peaks", "hotspot_signal"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);
}

void scheduleBamSort(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* Look to see if a bam is sorted, and if not, then produce a sorted version of it */
{
/* Make sure that we don't schedule it again and again */
char *analysisStep = "bam_sort";
if (countAlreadyScheduled(conn, analysisStep, ef->id))
    return;

char *tempDir = newTempDir(analysisStep);

/* Figure out command line */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef, vf);
preloadCache(conn, cache);
char commandLine[3*PATH_LEN];
safef(commandLine, sizeof(commandLine), "eap_bam_sort %s %s%s", 
    efName, tempDir, "out.bam");

struct edwAssembly *assembly = chooseTarget(conn, ef, vf);

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"unsorted"};
char *outFormats[] = {"bam", };
char *outNames[] = {"out.bam"};
char *outTypes[] = {"alignments",};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);
}


void runFastqAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run fastq analysis, at least on the data types where we can handle it. */
{
char *dataType = exp->dataType;
if (sameString(dataType, "DNase-seq") || sameString(dataType, "ChIP-seq") || 
    sameString(dataType, "ChiaPet") || sameString(dataType, "DnaPet") )
    {
    if (!isEmpty(vf->pairedEnd))
        {
	struct edwValidFile *vfB = edwOppositePairedEnd(conn, vf);
	if (vfB != NULL)
	    {
	    struct edwValidFile *vf1, *vf2;
	    struct edwQaPairedEndFastq *pair = edwQaPairedEndFastqFromVfs(conn, vf, vfB, 
		    &vf1, &vf2);
	    if (pair != NULL)
		{
		scheduleBwaPaired(conn, vf1, vf2, exp);
		edwQaPairedEndFastqFree(&pair);
		}
	    edwValidFileFree(&vfB);
	    }
#ifdef SOON
#endif /* SOON */
	}
    else
        {
	scheduleBwaSingle(conn, ef, vf, exp);
	}
    }
}

void runBamAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run fastq analysis, at least on the data types where we can handle it. */
{
if (sameString(exp->dataType, "DNase-seq"))
    {
    scheduleMacsDnase(conn, ef, vf, exp);
#ifdef SOON
    scheduleHotspot(conn, ef, vf, exp);
#endif /* SOON */
    }
}

void runSingleAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
verbose(2, "run single analysis format %s, dataType %s\n", vf->format, exp->dataType);
struct edwAssembly *targetAsm = chooseTarget(conn, ef, vf);
if (targetAsm == NULL || targetAsm->taxon != 9606)  // Humans only!
    return;	    // FOr the moment we're being speciest.

if (sameWord("fastq", vf->format))
    runFastqAnalysis(conn, ef, vf, exp);
#ifdef SOON
if (sameWord("bam", vf->format))
    runBamAnalysis(conn, ef, vf, exp);
#endif /* SOON */
}

void runReplicateAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
verbose(2, "run replicate analysis format %s, dataType %s\n", vf->format, exp->dataType);
#ifdef SOON
if (!inReplicatedTable(ef,vf,exp))
    {
    if (isReplicated(ef, vf, exp))
	{
	addReplicateResults(ef, vf, exp);
	}
    }
#endif
}

void edwScheduleAnalysis(int startFileId, int endFileId)
/* edwScheduleAnalysis - Schedule analysis runs.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startFileId, endFileId);
verbose(2, "Got %d intact files between %d and %d\n", slCount(efList), startFileId, endFileId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    verbose(2, "processing edwFile id %u\n", ef->id);
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	verbose(2, "aka %s\n", vf->licensePlate);
	struct edwExperiment *exp = edwExperimentFromAccession(conn, vf->experiment); 
	if (exp != NULL)
	    {
	    verbose(2, "experiment %s, singleQaStatus %d, replicateQaStatus %d\n", 
		exp->accession, vf->singleQaStatus, vf->replicateQaStatus);
	    if (vf->singleQaStatus > 0 || ignoreQa)
		runSingleAnalysis(conn, ef, vf, exp);
	    if (vf->replicateQaStatus > 0 || ignoreQa)
		runReplicateAnalysis(conn, ef, vf, exp);
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
retry = optionExists("retry");
again = optionExists("again");
updateSoftwareMd5 = optionExists("up");
noJob = optionExists("noJob");
ignoreQa = optionExists("ignoreQa");
justLink = optionExists("justLink");
dry = optionExists("dry");
edwScheduleAnalysis(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
