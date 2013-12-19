/* edwScheduleAnalysis - Schedule analysis runs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "bamFile.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

boolean again = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwScheduleAnalysis - Schedule analysis runs.\n"
  "usage:\n"
  "   edwScheduleAnalysis startFileId endFileId\n"
  "options:\n"
  "   -again - if set schedule it even if it's been run once\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"again", OPTION_BOOLEAN},
   {NULL, 0},
};

static int countAlreadyScheduled(struct sqlConnection *conn, char *analysisStep, 
    long long firstInput)
/* Return how many of the existing job are already scheduled */
{
if (again)
    return 0;
char query[512];
sqlSafef(query, sizeof(query),
    "select count(*) from edwAnalysisRun where firstInputId=%lld and analysisStep='%s'",
    firstInput, analysisStep);
return sqlQuickNum(conn, query);
}

static char *newTempDir(char *name)
/* Return a freshly created temp dir where name occurs somewhere in it's name. */
{
char *tempDir = cloneString(rTempName(eapTempDir, name, ""));
makeDir(tempDir);
return tempDir;
}

char *scheduleStep(struct sqlConnection *conn, 
    char *analysisStep, char *commandLine, char *experiment, struct edwAssembly *assembly,
    int inCount, unsigned inputIds[], char *inputTypes[],
    int outCount, char *outputNames[], char *outputTypes[], char *outputFormats[])
/* Schedule a step.  Returns temp dir it is to be run in. */
{
edwAnalysisCheckVersions(conn, analysisStep);

/* Grab temp dir */
char *tempDir = newTempDir(analysisStep);
verbose(1, "scheduling %s on %u in %s\n", analysisStep, inputIds[0], tempDir);

/* Wrap command line in cd to temp dir */
char fullCommandLine[strlen(commandLine)+128];
safef(fullCommandLine, sizeof(fullCommandLine), 
    "bash -exc 'cd %s; nice %s'", tempDir, commandLine);

/* Make up edwAnalysisRun record */
struct edwAnalysisRun *run;
AllocVar(run);
run->jobId = edwAnalysisJobAdd(conn, fullCommandLine);
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

return tempDir;
}

struct edwAssembly *getBwaIndex(struct sqlConnection *conn, char *target, char indexPath[PATH_LEN])
/* Target for now is same as UCSC DB.  Returns assembly ID */
{
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, target);
edwBwaIndexPath(assembly, indexPath);
return assembly;
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
struct edwAssembly *assembly = getBwaIndex(conn, vf1->ucscDb, indexPath);

/* Make up job and command line */
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), 
    "%s %s %s%s %s%s %s",
    "eap_run_bwa_pe", indexPath, edwRootDir, ef1->edwFileName, edwRootDir, 
    ef2->edwFileName, "out.bam");

/* Make up edwAnalysisRun record and schedule it*/
unsigned inFileIds[2] = {ef1->id, ef2->id};
char *inTypes[2] = {"read1", "read2"};
char *outNames[1] = {"out.bam"};
char *outTypes[1] = {"alignments"};
char *outFormats[1] = {"bam"};
scheduleStep(conn, analysisStep, commandLine, exp->accession, assembly,
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
struct edwAssembly *assembly = getBwaIndex(conn, vf->ucscDb, indexPath);

/* Make up job and command line */
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s%s %s",
    "eap_run_bwa_se", indexPath, edwRootDir, ef->edwFileName, "out.bam");

/* Make up edwAnalysisRun record and schedule it*/
unsigned inFileIds[1] = {ef->id};
char *inTypes[1] = {"reads"};
char *outNames[1] = {"out.bam"};
char *outTypes[1] = {"alignments"};
char *outFormats[1] = {"bam"};
scheduleStep(conn, analysisStep, commandLine, exp->accession, assembly,
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
    if (one.core.flag&BAM_FPAIRED)
       {
       isPaired = TRUE;
       break;
       }
    int err = bam_read1(sf->x.bam, &one);
    if (err < 0)
	break;
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

verbose(2, "schedulingMacsDnase on %s step %s, script %s\n", bamName, analysisStep, scriptName);
/* Make sure that we don't schedule it again and again */
if (countAlreadyScheduled(conn, analysisStep, ef->id))
    return;

/* Make command line */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "%s %s %s %s %s",
    scriptName, assembly->ucscDb, bamName, "out.narrowPeak.bigBed", "out.bigWig");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"alignments"};
char *outFormats[] = {"narrowPeak", "bigWig"};
char *outNames[] = {"out.narrowPeak.bigBed", "out.bigWig"};
char *outTypes[] = {"macs2_dnase_peaks", "macs2_dnase_signal"};
scheduleStep(conn, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);
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
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
int readLength = 36;	// uglyf - get from fastq record for input
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "eap_run_hotspot %s%s %s %s %d %s",
    edwRootDir, ef->edwFileName, assembly->ucscDb, "DNase-seq", readLength, "out");

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"alignments"};
char *outFormats[] = {"broadPeak", "narrowPeak", "bigWig"};
char *outNames[] = {"out/broadPeaks.bigBed", "out/narrowPeaks.bigBed", "out/density.bigWig"};
char *outTypes[] = {"hotspot_broad_peaks", "hotspot_narrow_peaks", "hotspot_signal"};
scheduleStep(conn, analysisStep, commandLine, exp->accession, assembly,
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

/* Figure out command line */
char commandLine[3*PATH_LEN];
safef(commandLine, sizeof(commandLine), "eap_bam_sort %s%s %s",
    edwRootDir, ef->edwFileName, "out.bam");

struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);

/* Declare step input and output arrays and schedule it. */
unsigned inFileIds[] = {ef->id};
char *inTypes[] = {"unsorted"};
char *outFormats[] = {"bam", };
char *outNames[] = {"out.bam"};
char *outTypes[] = {"alignments",};
scheduleStep(conn, analysisStep, commandLine, exp->accession, assembly,
    ArraySize(inFileIds), inFileIds, inTypes,
    ArraySize(outNames), outNames, outTypes, outFormats);
}


void runFastqAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run fastq analysis, at least on the data types where we can handle it. */
{
char *dataType = exp->dataType;
uglyf("runFastqAnalysis %u %s\n", ef->id, dataType);
if (sameString(dataType, "DNase-seq") || sameString(dataType, "ChIP-seq"))
    {
    uglyf("runFastqAnalysis still going\n");
    if (!isEmpty(vf->pairedEnd))
        {
	uglyf("isPaird\n");
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
	uglyf("notPaired\n");
	scheduleBwaSingle(conn, ef, vf, exp);
	}
    }
}

void runBamAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run fastq analysis, at least on the data types where we can handle it. */
{
uglyf("runBamAnalysis %u %s\n", ef->id, exp->dataType);
if (sameString(exp->dataType, "DNase-seq"))
    {
    scheduleMacsDnase(conn, ef, vf, exp);
    scheduleHotspot(conn, ef, vf, exp);
    }
#ifdef SOON 
#endif /* SOON */
}

void runSingleAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
verbose(2, "run single analysis format %s, dataType %s\n", vf->format, exp->dataType);
if (sameWord("fastq", vf->format))
    runFastqAnalysis(conn, ef, vf, exp);
else if (sameWord("bam", vf->format))
    runBamAnalysis(conn, ef, vf, exp);
}

void runReplicateAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
verbose(2, "run replicate analysis format %s, dataType %s\n", vf->format, exp->dataType);
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
	    if (vf->singleQaStatus > 0)
		runSingleAnalysis(conn, ef, vf, exp);
	    if (vf->replicateQaStatus > 0)
		runReplicateAnalysis(conn, ef, vf, exp);
	    }
	}
    }
edwPokeFifo("edwAnalysis.fifo");
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
again = optionExists("again");
edwScheduleAnalysis(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
