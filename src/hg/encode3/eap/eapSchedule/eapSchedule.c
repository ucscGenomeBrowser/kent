/* edwScheduleAnalysis - Schedule analysis runs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "bamFile.h"
#include "obscure.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "../../../../parasol/inc/paraMessage.h"
#include "eapDb.h"
#include "eapLib.h"

boolean again = FALSE;
boolean retry = FALSE;
boolean noJob = FALSE;
boolean ignoreQa = FALSE;
boolean justLink = FALSE;
boolean dry = FALSE;
char *clStep = "*";
char *clInFormat = "*";
int clMax = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapSchedule - Schedule analysis runs.\n"
  "usage:\n"
  "   eapSchedule startFileId endFileId\n"
  "options:\n"
  "   -step=pattern - Just run steps with names matching pattern which is * by default\n"
  "   -inFormat=pattern - Just run steps that are triggered by files of this format\n"
  "   -retry - if job has run and failed retry it\n"
  "   -again - if set schedule it even if it's been run once\n"
  "   -noJob - if set then don't put job on eapJob table\n"
  "   -ignoreQa - if set then ignore QA results when scheduling\n"
  "   -justLink - just symbolically link rather than copy EDW files to cache\n"
  "   -max=count - Maximum number of jobs to schedule\n"
  "   -dry - just print out the commands that would result\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"step", OPTION_STRING},
   {"inFormat", OPTION_STRING},
   {"retry", OPTION_BOOLEAN},
   {"again", OPTION_BOOLEAN},
   {"up", OPTION_BOOLEAN},
   {"noJob", OPTION_BOOLEAN},
   {"ignoreQa", OPTION_BOOLEAN},
   {"justLink", OPTION_BOOLEAN},
   {"max", OPTION_INT},
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

struct edwAssembly *targetAssemblyForDbAndSex(struct sqlConnection *conn, char *ucscDb, char *sex)
/* Figure out best target assembly for something that is associated with a given ucscDB.  
 * In general this will be the most recent for the organism. */
{
char *newDb = ucscDb;
if (sameString("hg38", ucscDb))
    newDb = "hg19";
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
char *sex = "male";  // Consequences of Y as a mapping target not too bad, so default is male.
struct edwExperiment *exp = edwExperimentFromAccession(conn, vf->experiment);
if (exp != NULL)
    {
    struct edwBiosample *bio = edwBiosampleFromTerm(conn, exp->biosample);
    if (bio != NULL)
        {
	if (sameWord(bio->sex, "F"))
	    sex = "female";
	// Other things, unknown, pooled, etc. get to try to map to Y at least.
	}
    }
return sex;
}

struct edwAssembly *chooseTarget(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf)
/* Pick mapping target - according to taxon and sex. */
{
char *sex = sexFromExp(conn, ef, vf);
return targetAssemblyForDbAndSex(conn, vf->ucscDb, sex);
}

static boolean alreadyTakenCareOf(struct sqlConnection *conn, 
    struct edwAssembly *assembly, char *analysisStep, 
    long long firstInput)
/* Return true if job is already scheduled or otherwise taken care of */
{
if (!wildMatch(clStep, analysisStep))
    return TRUE;
if (again)
    return FALSE;
char query[512];
if (retry)
    sqlSafef(query, sizeof(query),
	"select count(*) from eapRun,eapInput"
	" where eapRun.id = eapInput.runId"
	" and eapInput.fileId=%lld and analysisStep='%s' and createStatus >= 0"
	" and assemblyId = %u",
	firstInput, analysisStep, assembly->id);
else
    sqlSafef(query, sizeof(query),
	"select count(*) from eapRun,eapInput"
	" where eapRun.id = eapInput.runId"
	" and eapInput.fileId=%lld and analysisStep='%s'"
	" and assemblyId = %u",
	firstInput, analysisStep, assembly->id);
int count = sqlQuickNum(conn, query);
verbose(2, "query %s\ncount %d\n", query, count);
return count > 0;
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


void scheduleStepInputBundle(struct sqlConnection *conn, char *tempDir,
    char *analysisStep, char *commandLine, char *experiment, struct edwAssembly *assembly,
    int inCount, unsigned inputIds[], char *inputTypes[], boolean bundleInputs)
/* Schedule a step.  Optionally bundle together all inputs into single vector input. */
{
char *commandPrefix = "edwCdJob ";
int commandPad = strlen(commandPrefix);
pmCheckCommandSize(commandLine, strlen(commandLine) + commandPad);
struct eapStep *step = eapStepFromNameOrDie(conn, analysisStep);
unsigned stepVersionId = eapCheckVersions(conn, analysisStep);

/* Check count of steps we've made against max set by command line. */
if (clMax > 0)
    {
    static int stepCount = 0;
    if (++stepCount > clMax)
        return;
    }

if (dry)
    printf("%s\n", commandLine);
else
    {
    verbose(1, "scheduling %s on %u in %s\n", analysisStep, inputIds[0], tempDir);

    /* Wrap command line in cd to temp dir */
    char fullCommandLine[strlen(commandLine)+128];
    safef(fullCommandLine, sizeof(fullCommandLine), 
	"%s%s", commandPrefix, commandLine);

    /* Make up eapRun record and save it. */
    struct eapRun *run;
    AllocVar(run);
    if (!noJob)
	run->jobId = eapJobAdd(conn, fullCommandLine, step->cpusRequested);
    safef(run->experiment, sizeof(run->experiment), "%s",  experiment);
    run->analysisStep = analysisStep;
    run->stepVersionId = stepVersionId;
    run->tempDir = tempDir;
    if (assembly != NULL) run->assemblyId = assembly->id;
    run->jsonResult = "";
    eapRunSaveToDb(conn, run, "eapRun", 0);
    run->id = sqlLastAutoId(conn);

    /* Write input records. */
    int i;
    for (i=0; i<inCount; ++i)
	{
	struct eapInput in = {.runId=run->id};
	in.name = inputTypes[i];
	in.fileId = inputIds[i];
	in.ix = (bundleInputs ? i : 0);
	eapInputSaveToDb(conn, &in, "eapInput", 0);
	}

    /* Write output records during eapFinish.... */
    freez(&run);
    }
}

void scheduleStep(struct sqlConnection *conn, char *tempDir,
    char *analysisStep, char *commandLine, char *experiment, struct edwAssembly *assembly,
    int inCount, unsigned inputIds[], char *inputTypes[])
/* Schedule a step, or maybe just think it if the dry option is set. */
{
scheduleStepInputBundle(conn, tempDir, analysisStep, commandLine, experiment, assembly,
    inCount, inputIds, inputTypes, FALSE);
}

static void makeBwaIndexPath(char *target, char indexPath[PATH_LEN])
/* Fill in path to BWA index. */
{
safef(indexPath, PATH_LEN, "%s%s/bwaData/%s.fa", 
    "/scratch/data/encValDir/", target, target);
}

struct edwAssembly *getBwaIndex(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf, 
    char indexPath[PATH_LEN])
/* Target for now is same as UCSC DB.  Returns assembly ID */
{
struct edwAssembly *assembly = chooseTarget(conn, ef, vf);
makeBwaIndexPath(assembly->name, indexPath);
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

char *cacheMore(struct cache *cache, struct edwFile *ef)
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
/* Get ef records */
struct edwFile *ef1 = edwFileFromIdOrDie(conn, vf1->fileId);
struct edwFile *ef2 = edwFileFromIdOrDie(conn, vf2->fileId);

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
struct edwAssembly *assembly = getBwaIndex(conn, ef1, vf1, indexPath);

/* Make sure that we don't schedule it again and again */
char *analysisStep = "bwa_paired_end";
if (!alreadyTakenCareOf(conn, assembly, analysisStep, vf1->fileId))
    {
    /* Grab temp dir */
    char *tempDir = newTempDir(analysisStep);
    char *outBamName = "out.bam";

    /* Stage inputs and make command line. */
    struct cache *cache = cacheNew();
    char *ef1Name = cacheMore(cache, ef1);
    char *ef2Name = cacheMore(cache, ef2);
    char *command = pickPairedBwaCommand(conn, vf1, vf2);
    preloadCache(conn, cache);
    char commandLine[4*PATH_LEN];
    safef(commandLine, sizeof(commandLine), 
	"%s %s %s %s %s%s", 
	command, indexPath, ef1Name, ef2Name, tempDir, outBamName);

    /* Make up eapRun record and schedule it*/
    unsigned inFileIds[2] = {ef1->id, ef2->id};
    char *inTypes[2] = {"read1", "read2"};
    scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
	ArraySize(inFileIds), inFileIds, inTypes);

    }
/* Clean up */
edwFileFree(&ef1);
edwFileFree(&ef2);
}

void scheduleBwaSingle(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule bwa analysis of the two files. */
{
/* Figure out path to BWA index */
char indexPath[PATH_LEN];
struct edwAssembly *assembly = getBwaIndex(conn, ef, vf, indexPath);

/* Make sure that we don't schedule it again and again */
char *analysisStep = "bwa_single_end";
if (alreadyTakenCareOf(conn, assembly, analysisStep, ef->id))
    return;

/* Grab temp dir */
char *tempDir = newTempDir(analysisStep);
char *outBamName = "out.bam";

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef);
char *command = pickSingleBwaCommand(conn, vf);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s%s", 
    command, indexPath, efName, tempDir, outBamName);

/* Make up eapRun record and schedule it*/
unsigned inFileIds[1] = {ef->id};
char *inTypes[1] = {"reads"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);
}

boolean bamIsPaired(char *fileName, int maxCount)
/* Read up to maxCount records, figure out if BAM is from a paired run */
{
#ifdef USE_BAM
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
#else // no USE_BAM
warn(COMPILE_WITH_SAMTOOLS, "bamIsPaired");
return FALSE;
#endif//ndef USE_BAM
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
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
if (alreadyTakenCareOf(conn, assembly, analysisStep, ef->id))
    return;

char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s%s %s%s", 
    scriptName, assembly->name, efName, 
    tempDir, "out.narrowPeak.bigBed", tempDir, "out.bigWig");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"alignments"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);
}

struct edwExperiment *findChipControlExp(struct sqlConnection *conn, char *accession)
/* Given experiment accession try to find control. */
{
char query[512];
sqlSafef(query, sizeof(query), "select control from edwExperiment where accession='%s'", accession);
char *controlName = sqlQuickString(conn, query);
if (isEmpty(controlName))
    return NULL;
sqlSafef(query, sizeof(query), "select * from edwExperiment where accession='%s'", controlName);
freez(&controlName);
return edwExperimentLoadByQuery(conn, query);
}

struct edwFile *findChipControlFile(struct sqlConnection *conn, 
    struct edwValidFile *vf, struct edwExperiment *exp)
/* Find control file for this chip file */
{
struct edwExperiment *controlExp = findChipControlExp(conn, exp->accession);
if (controlExp == NULL)
    return NULL;

// Got control experiment,  now let's go for a matching bam file. 
char query[PATH_LEN*3];
sqlSafef(query, sizeof(query), 
    "select * from edwValidFile where experiment='%s' and format='%s' and  ucscDb='%s'" ,
    controlExp->accession, vf->format, vf->ucscDb);
struct edwValidFile *controlVf, *controlVfList = edwValidFileLoadByQuery(conn, query);
int controlCount = slCount(controlVfList);
if (controlCount <= 0)
    return NULL;

// Go through list and try to pick one
int bestScore = 0;
struct edwValidFile *bestControl = NULL;
for (controlVf = controlVfList; controlVf != NULL; controlVf = controlVf->next)
    {
    int score = 0;
    if (sameString(controlVf->outputType, vf->outputType))
        score += 100000000;
    if (sameString(controlVf->replicate, vf->replicate))
        score += 50000000;
    if (sameString(controlVf->technicalReplicate, vf->technicalReplicate))
        score += 25000000;
    long long sizeDiff = controlVf->itemCount - vf->itemCount;
    if (sizeDiff < 0) sizeDiff = -sizeDiff;
    if (sizeDiff > 25000000)
        sizeDiff = 25000000;
    score += 25000000 - sizeDiff;
    if (score > bestScore)
        {
	bestScore = score;
	bestControl = controlVf;
	}
    }
if (bestControl == NULL)
    return NULL;

// Figure out control bam file info
return edwFileFromId(conn, bestControl->fileId);
}

void scheduleSppChip(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule SPP peak calling for a chip-seq bam file. */
{
// Figure out input bam name
char bamName[PATH_LEN];
safef(bamName, sizeof(bamName), "%s%s", edwRootDir, ef->edwFileName);

// Figure out analysis step and script based on paired end status of input bam
boolean isPaired = bamIsPaired(bamName, 1000);
char *analysisStep, *scriptName;
if (isPaired)
    {
    /*
    analysisStep = "spp_chip_pe";
    scriptName = "eap_run_spp_chip_pe";
    */
    return;
    }
else
    {
    analysisStep = "spp_chip_se";
    scriptName = "eap_run_spp_chip_se";
    }

// Get control bam file info
struct edwFile *controlEf = findChipControlFile(conn, vf, exp);

verbose(2, "schedulingSppChip on %s with control %s,  step %s, script %s\n", ef->edwFileName, 
    controlEf->edwFileName, analysisStep, scriptName);

/* Make sure that we don't schedule it again and again */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
if (alreadyTakenCareOf(conn, assembly, analysisStep, ef->id))
    return;

char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef);
char *controlEfName = cacheMore(cache, controlEf);
preloadCache(conn, cache);

char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s %s%s", 
    scriptName, assembly->name, efName, controlEfName,
    tempDir, "out.narrowPeak.bigBed");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id, controlEf->id};
char *inTypes[] = {"chipBam", "controlBam"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);
}

void scheduleMacsChip(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule Macs peak calling for a chip-seq bam file. */
{
// Figure out input bam name
char bamName[PATH_LEN];
safef(bamName, sizeof(bamName), "%s%s", edwRootDir, ef->edwFileName);

// Figure out analysis step and script based on paired end status of input bam
boolean isPaired = bamIsPaired(bamName, 1000);
char *analysisStep, *scriptName;
if (isPaired)
    {
    analysisStep = "macs2_chip_pe";
    scriptName = "eap_run_macs2_chip_pe";
    }
else
    {
    analysisStep = "macs2_chip_se";
    scriptName = "eap_run_macs2_chip_se";
    }

// Get control bam file info
struct edwFile *controlEf = findChipControlFile(conn, vf, exp);

verbose(2, "schedulingMacsChip on %s with control %s,  step %s, script %s\n", ef->edwFileName, 
    controlEf->edwFileName, analysisStep, scriptName);

/* Make sure that we don't schedule it again and again */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
if (alreadyTakenCareOf(conn, assembly, analysisStep, ef->id))
    return;

char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef);
char *controlEfName = cacheMore(cache, controlEf);
preloadCache(conn, cache);

char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s %s%s %s%s", 
    scriptName, assembly->name, efName, controlEfName,
    tempDir, "out.narrowPeak.bigBed", tempDir, "out.bigWig");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id, controlEf->id};
char *inTypes[] = {"chipBam", "controlBam"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);
}

double mustGetReadLengthForFastq(struct sqlConnection *conn, unsigned fileId, boolean *retAllSame)
/* Verify that associated file is in edwFastqFile table and return average read length */
{
char query[256];
sqlSafef(query, sizeof(query), "select readSizeMean from edwFastqFile where fileId=%u", fileId);
double result = sqlQuickDouble(conn, query);
if (result < 1)
    errAbort("No edwFastqFile record for %u", fileId);
sqlSafef(query, sizeof(query), 
    "select readSizeMax - readSizeMin from edwFastqFile where fileId=%u", fileId);
double diff = sqlQuickDouble(conn, query);
*retAllSame = (diff == 0.0);
return result;
}

unsigned getFirstParentOfFormat(struct sqlConnection *conn, char *format, unsigned childFileId)
/* Return fastq id associated with bam file or die trying. */
{
unsigned fastqId = 0;
/* Scan backwards input/output tables until reach a fastq */
/* Look for input that led to us, getting both file ID and format */
char query[256];
sqlSafef(query, sizeof(query), 
    "select eapInput.fileId,edwValidFile.format from eapInput,eapOutput,edwValidFile "
    " where eapOutput.runId = eapInput.runId and eapInput.fileId = edwValidFile.fileId"
    " and eapOutput.fileId=%u", childFileId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

/* Hopefully fastq is an immediate parent.  We loop through all input and
 * check it for fastqs.  If it's not fastq we put it on a list in case we need
 * to look further upstream for the fastq. */
struct slUnsigned *parentList = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned fileId = sqlUnsigned(row[0]);
    if (sameString(format, row[1]))
	{
	fastqId = fileId;
	break;
	}
    else
	{
	struct slUnsigned *el = slUnsignedNew(fileId);
	slAddHead(&parentList,el);
	}
    }
sqlFreeResult(&sr);

if (fastqId == 0)  // No joy here, then go look in parents.
     {
     struct slUnsigned *parent;
     for (parent = parentList; parent != NULL; parent = parent->next)
	 {
	 unsigned result = getFirstParentOfFormat(conn, format, parent->val);
	 if (result > 0)
	     {
	     fastqId = result;
	     break;
	     }
	 }
     }
slFreeList(&parentList);
return fastqId;
}

double bamReadLength(char *fileName, int maxCount, boolean *retAllSame)
/* Open up bam, read up to maxCount reads, and return size of read, checking
 * all are same size. */
{
#ifdef USE_BAM
long long totalReadLength = 0;
int count = 0;
int readLength = 0;
boolean allSame = TRUE;
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
	{
	if (allSame)
	    warn("Varying size reads in %s, %d and %d", fileName, readLength, length);
	allSame = FALSE;
	}
    totalReadLength += length;
    ++count;
    }
samclose(sf);
assert(readLength != 0);
double averageReadLength = 0;
*retAllSame = allSame;
if (count > 0)
    averageReadLength = (double)totalReadLength/count;
return averageReadLength;
#else // no USE_BAM
warn(COMPILE_WITH_SAMTOOLS, "bamReadLength");
return 0;
#endif//ndef USE_BAM
}

static boolean hotspotReadLengthOk(int length)
/* Return TRUE if hotspot read length is ok */
{
int okReadLengths[] = {32,36,40,50,58,72,76,100};
int i;
for (i=0; i<ArraySize(okReadLengths); ++i)
    {
    if (length == okReadLengths[i])
        return TRUE;
    }
return FALSE;
}

void scheduleHotspot(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule hotspot analysis of the file. */
{
/* Not ready for hg38 yet */
if (sameString("hg38", vf->ucscDb) || endsWith(vf->ucscDb, ".hg38"))
    {
    verbose(2, "Skipping hotspot on %s %s\n", vf->ucscDb, ef->edwFileName);
    return;
    }

/* Make sure that we don't schedule it again and again */
char *analysisStep = "hotspot";
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
if (alreadyTakenCareOf(conn, assembly, analysisStep, ef->id))
    return;

verbose(2, "schedulingHotspot on %s step %s\n", ef->edwFileName, analysisStep);
/* Make command line */
unsigned fastqId = getFirstParentOfFormat(conn, "fastq", ef->id);
double readLength = 0;
boolean allSame = FALSE;
if (fastqId == 0)
    readLength = bamReadLength(edwPathForFileId(conn, ef->id), 1000, &allSame);
else
    readLength = mustGetReadLengthForFastq(conn, fastqId, &allSame);
if (!allSame)
    {
    verbose(2, "Couldn't schedule hotspot on %s, not all reads the same size.\n", ef->edwFileName);
    return;
    }
if (!hotspotReadLengthOk(readLength))
    {
    verbose(2, "Hotspot read length %g not supported on %s\n", readLength, ef->edwFileName);
    return;
    }

char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "eap_run_hotspot %s %s %d %s%s %s%s %s%s",
    edwSimpleAssemblyName(assembly->ucscDb), efName,
    (int)readLength, tempDir, "out.narrowPeak.bigBed", 
    tempDir, "out.broadPeak.bigBed", tempDir, "out.bigWig");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"alignments"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);
}

void scheduleBamSort(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* Look to see if a bam is sorted, and if not, then produce a sorted version of it */
{
/* Make sure that we don't schedule it again and again */
char *analysisStep = "bam_sort";
struct edwAssembly *assembly = chooseTarget(conn, ef, vf);
if (alreadyTakenCareOf(conn, assembly, analysisStep, ef->id))
    return;

char *tempDir = newTempDir(analysisStep);

/* Figure out command line */
struct cache *cache = cacheNew();
char *efName = cacheMore(cache, ef);
preloadCache(conn, cache);
char commandLine[3*PATH_LEN];
safef(commandLine, sizeof(commandLine), "eap_bam_sort %s %s%s", 
    efName, tempDir, "out.bam");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"unsorted"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);
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
    scheduleHotspot(conn, ef, vf, exp);
    }
else if (sameString(exp->dataType, "ChIP-seq"))
    {
    scheduleSppChip(conn, ef, vf, exp);
#ifdef SOON
    scheduleMacsChip(conn, ef, vf, exp);
#endif /* SOON */
    }
}

void runSingleAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
verbose(2, "run single analysis format %s, dataType %s\n", vf->format, exp->dataType);
if (sameWord("fastq", vf->format))
    runFastqAnalysis(conn, ef, vf, exp);
if (sameWord("bam", vf->format))
    runBamAnalysis(conn, ef, vf, exp);
}

struct edwFile *listOutputTypeForExp(struct sqlConnection *conn, char *experiment, 
    char *outputType, char *format, char *ucscDb)
/* Return all files of given output type and format associated with given experiment  */
{
char query[512];
sqlSafef(query, sizeof(query), 
    "select edwFile.* from edwFile,edwValidFile where edwValidFile.fileId=edwFile.id "
    " and experiment='%s' and outputType='%s' and format='%s' and ucscDb='%s' "
    " and deprecated='' and errorMessage=''"
    " order by fileId desc"
    , experiment, outputType, format, ucscDb);
return edwFileLoadByQuery(conn, query);
}

void scheduleWigPooler(struct sqlConnection *conn, struct edwExperiment *exp,  
    struct edwValidFile *youngestVf, struct edwFile *pool)
/* Create job to run wig pooler */
{
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, youngestVf->ucscDb);
char *analysisStep = "sum_bigWig";
if (alreadyTakenCareOf(conn, assembly, analysisStep, youngestVf->fileId))
    return;
verbose(2, "Theoretically I'd be pooling %d wigs around %s\n", slCount(pool), exp->accession);

struct eapStep *step = eapStepFromNameOrDie(conn, analysisStep);
char *tempDir = newTempDir(analysisStep);

/* Stage inputs and turn into slList of file names */
struct slName *inFileNames = NULL;
struct cache *cache = cacheNew();
struct edwFile *ef;
for (ef = pool; ef != NULL; ef = ef->next)
    {
    char *fileName = cacheMore(cache, ef);
    slNameAddTail(&inFileNames, fileName);
    }
preloadCache(conn, cache);


/* Make up command line */
struct dyString *command = dyStringNew(2048);
dyStringPrintf(command, "eap_sum_bigWig %s%s %s",
    tempDir, "out.bigWig", assembly->name);
struct slName *in;
for (in = inFileNames; in != NULL; in = in->next)
     dyStringPrintf(command, " %s", in->name);

/* Create step input arrays and schedule it. */
int inCount = slCount(pool);
unsigned inFileIds[inCount];
char *inTypes[inCount];
int i;
for (i=0, ef = pool; i<inCount; ++i, ef = ef->next)
    {
    inFileIds[i] = ef->id;
    inTypes[i] = step->inputTypes[0];
    }

/* Schedule it */
scheduleStepInputBundle(conn, tempDir, analysisStep, command->string, exp->accession, assembly,
    inCount, inFileIds, inTypes, TRUE);

/* Clean up and go home. */
dyStringFree(&command);
eapStepFree(&step);
}
    
void replicaWigAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run analysis on wigs that have been replicated.  */
{
char *outputType = vf->outputType;
if (sameString(outputType, "macs2_dnase_signal") || sameString(outputType, "macs2_chip_signal")
    || sameString(outputType, "hotspot_signal"))
    {
    struct edwFile *pool = listOutputTypeForExp(conn, 
				vf->experiment, vf->outputType, vf->format, vf->ucscDb);
    if (slCount(pool) > 1)
        {
	/* Output is sorted, so if we are first we are youngest (highest id) and responsible. 
	 * (Since scheduler is called on all in pool, don't want all to try to do this.) */
	if (pool->id == ef->id)   
	    {
	    verbose(2, "Looks like %u is the youngest in the pool!\n", pool->id);
	    scheduleWigPooler(conn, exp, vf, pool);
	    }
	}
    }
}

struct edwFile *bestPeakRep(struct sqlConnection *conn, char *notRep, struct edwFile *efList)
/* Return best looking replica on list.  How to decide?  Hmm, good question! */
/* At the moment it is using the enrichment*coverage calcs if available, otherwise covereage
 * depth.  It looks like it is not preferring peaks that have been processed all the way
 * from scratch from the fastq files by pipeline though. */
{
verbose(2, "bestPeakRep for list of %d\n", slCount(efList));
/* If we are only one in list the choice is easy! */
if (efList->next == NULL)
    return efList;

/* We multiply together two factors to make a score - the enrichment in our target area
 * times the coverage.  Find the best scoring by this criteria and use it.  */
double bestScore = -1;
struct edwFile *bestEf = NULL, *ef;
struct edwQaEnrichTarget *target = NULL;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    if (differentString(vf->replicate, notRep) )
	{
	double score = 0;
	if (!isEmpty(vf->enrichedIn) && !sameString(vf->enrichedIn, "unknown")) 
	    {
	    char query[256];
	    if (target == NULL)
		{
		char *assemblyName = edwSimpleAssemblyName(vf->ucscDb);
		struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, assemblyName);
		sqlSafef(query, sizeof(query), 
		    "select * from edwQaEnrichTarget where assemblyId=%u and name='%s'"
		    , assembly->id, vf->enrichedIn);
		target = edwQaEnrichTargetLoadByQuery(conn, query);
		if (target == NULL)
		    errAbort("Unexpected empty result from %s", query);
		edwAssemblyFree(&assembly);
		}
	    sqlSafef(query, sizeof(query), 
		"select coverage*enrichment from edwQaEnrich where fileId=%u and qaEnrichTargetId=%u"
		, vf->fileId, target->id);
	    score = sqlQuickDouble(conn, query);
	    }
	else
	    {
	    score = vf->depth;
	    }
	if (score > bestScore)
	    {
	    bestScore = score;
	    bestEf = ef;
	    }
	}
    edwValidFileFree(&vf);
    }
edwQaEnrichTargetFree(&target);
return bestEf;
}

void scheduleReplicatedPeaks(struct sqlConnection *conn, char *peakType, 
    struct edwExperiment *exp,  struct edwValidFile *youngestVf, struct edwFile *pool)
/* Schedule job that produces replicated peaks out of individual peaks from two replicates. */
{
/* Figure out which step running based on peakType */
char *analysisStep = NULL;
char *outFileName = NULL;
if (sameString(peakType, "narrowPeak"))
    {
    analysisStep = "replicated_narrow_peaks";
    outFileName= "out.narrowPeak.bigBed";
    }
else
    {
    analysisStep = "replicated_broad_peaks";
    outFileName= "out.broadPeak.bigBed";
    }


/* Get assembly and figure out if we are already done */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, youngestVf->ucscDb);
if (alreadyTakenCareOf(conn, assembly, analysisStep, youngestVf->fileId))
    return;
verbose(2, "Theoretically I'd be making replicated peaks in %s on file %u=%u\n", 
    exp->accession, pool->id, youngestVf->fileId);

/* We always take the most recent replicate. Choose best remaining rep by some measure for second */
struct edwFile *ef1 = pool;
struct edwFile *ef2 = bestPeakRep(conn, youngestVf->replicate, pool->next);
if (ef2 == NULL)
    return;

/* Grab temp dir */
char *tempDir = newTempDir(analysisStep);

/* Stage inputs and make command line. */
struct cache *cache = cacheNew();
char *ef1Name = cacheMore(cache, ef1);
char *ef2Name = cacheMore(cache, ef2);
preloadCache(conn, cache);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), 
    "eap_%s %s %s %s %s%s", 
    analysisStep, assembly->name, ef1Name, ef2Name, tempDir, outFileName);

/* Make up eapRun record and schedule it*/
unsigned inFileIds[2] = {ef1->id, ef2->id};
char *inTypes[2] = {"peaks1", "peaks2"};
scheduleStep(conn, tempDir, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes);

}

void replicaPeakAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp, char *peakType)
/* Run analysis on replicated peaks */
{
char *outputType = vf->outputType;
verbose(2, "replicaPeakAnalysis(outputType=%s)\n", outputType);
if (sameString(outputType, "macs2_narrow_peaks") || sameString(outputType, "hotspot_narrow_peaks")
    || sameString(outputType, "hotspot_broad_peaks"))
    {
    struct edwFile *pool = listOutputTypeForExp(conn, 
				vf->experiment, vf->outputType, vf->format, vf->ucscDb);
    if (slCount(pool) > 1)
        {
	/* Output is sorted, so if we are first we are youngest (highest id) and responsible. 
	 * (Since scheduler is called on all in pool, don't want all to try to do this.) */
	if (pool->id == ef->id)   
	    {
	    scheduleReplicatedPeaks(conn, peakType, exp, vf, pool);
	    }
	}
    }
}

void runReplicateAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run analysis we hold off on until we have replicates. */
{
verbose(2, "run replicate analysis format %s, dataType %s\n", vf->format, exp->dataType);
if (vf->replicateQaStatus > 0)
    {
    if (sameWord("bigWig", vf->format))
        replicaWigAnalysis(conn, ef, vf, exp);
    else if (sameWord("broadPeak", vf->format) || sameWord("narrowPeak", vf->format))
        replicaPeakAnalysis(conn, ef, vf, exp, vf->format);
    }
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
    if (vf != NULL && wildMatch(clInFormat, vf->format))
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
noJob = optionExists("noJob");
ignoreQa = optionExists("ignoreQa");
justLink = optionExists("justLink");
dry = optionExists("dry");
clStep = optionVal("step", clStep);
clInFormat = optionVal("inFormat", clInFormat);
clMax = optionInt("max", clMax);
edwScheduleAnalysis(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
