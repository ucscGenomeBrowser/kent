/* edwScheduleAnalysis - Schedule analysis runs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
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


void schedulePairedBwa(struct sqlConnection *conn, 
    struct edwValidFile *vf1, struct edwValidFile *vf2, struct edwExperiment *exp)
/* If it hasn't already been done schedule bwa analysis of the two files. */
{
/* Make sure that we don't schedule it again and again */
char *analysisStep = "bwa_paired_end";
char *scriptName = "eap_run_bwa_pe";
if (countAlreadyScheduled(conn, analysisStep, vf1->fileId))
    return;
verbose(1, "scheduling paired end bwa analysis on %u and %u\n", vf1->fileId, vf2->fileId);

/* Get ef records */
struct edwFile *ef1 = edwFileFromIdOrDie(conn, vf1->fileId);
struct edwFile *ef2 = edwFileFromIdOrDie(conn, vf2->fileId);

/* Get target assembly (will redo this more precisely at some point) */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf1->ucscDb);
char configuration[128];
safef(configuration, sizeof(configuration), "bwa_%s_generic", assembly->ucscDb);

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
edwBwaIndexPath(assembly, indexPath);

/* Figure out temp dir and create it */
char *tempDir = rTempName(eapTempDir, vf1->licensePlate, "");
makeDir(tempDir);

/* Make up job and command line */
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), 
    "bash -ec 'cd %s; nice %s %s %s%s %s%s %s; edwFinishAnalysis now'",
    tempDir, scriptName, indexPath, edwRootDir, ef1->edwFileName, edwRootDir, 
    ef2->edwFileName, "alignments.bam");
int jobId = edwAnalysisJobAdd(conn, commandLine);

/* Make up edwAnalysisRun record */
unsigned inputFileIds[2] = {ef1->id, ef2->id};
char *inputTypes[2] = {"read1", "read2"};
char *outputNamesInTempDir[1] = {"alignments.bam"};
char *outputFormats[1] = {"bam"};
char *outputTypes[1] = {"alignments"};
struct edwAnalysisRun *run;
AllocVar(run);
run->jobId = jobId;
safef(run->experiment, sizeof(run->experiment), "%s",  exp->accession);
run->analysisStep = analysisStep;
run->configuration = configuration;
run->tempDir = tempDir;
run->firstInputId = ef1->id;
run->inputFileCount = 2;
run->inputFileIds = inputFileIds;
run->inputTypes = inputTypes;
run->outputFileCount = 1;
run->outputNamesInTempDir = outputNamesInTempDir;
run->outputFormats = outputFormats;
run->outputTypes = outputTypes;
run->assemblyId = assembly->id;
run->jsonResult = "";
edwAnalysisRunSaveToDb(conn, run, "edwAnalysisRun", 0);
freez(&run);

/* Clean up */
edwFileFree(&ef1);
edwFileFree(&ef2);
}

void scheduleSingleBwa(struct sqlConnection *conn, 
    struct edwFile *ef, struct edwValidFile *vf, struct edwExperiment *exp)
/* If it hasn't already been done schedule bwa analysis of the two files. */
{
/* Ugh - lots of duplication with schedulePairedBwa.  Maybe should have done it as
 * a single routine with lots of ifs, though that would be messy too.  Remember
 * to update both routines though! */

/* Make sure that we don't schedule it again and again */
char *analysisStep = "bwa_single_end";
char *scriptName = "eap_run_bwa_se";
if (countAlreadyScheduled(conn, analysisStep, ef->id))
    return;

/* Get target assembly (will redo this more precisely at some point) */
verbose(1, "scheduling single end bwa analysis on %u\n", ef->id);
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);
char configuration[128];
safef(configuration, sizeof(configuration), "bwa_%s_generic", assembly->ucscDb);

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
edwBwaIndexPath(assembly, indexPath);

/* Figure out temp dir and create it */
char *tempDir = rTempName(eapTempDir, vf->licensePlate, "");
makeDir(tempDir);

/* Make up job and command line */
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "bash -ec 'cd %s; nice %s %s %s%s %s; edwFinishAnalysis now'",
    tempDir, scriptName, indexPath, edwRootDir, ef->edwFileName, "alignments.bam");
int jobId = edwAnalysisJobAdd(conn, commandLine);

/* Make up edwAnalysisRun record */
unsigned inputFileIds[1] = {ef->id};
char *inputTypes[1] = {"reads"};
char *outputNamesInTempDir[1] = {"alignments.bam"};
char *outputFormats[1] = {"bam"};
char *outputTypes[1] = {"alignments"};
struct edwAnalysisRun *run;
AllocVar(run);
run->jobId = jobId;
safef(run->experiment, sizeof(run->experiment), "%s",  exp->accession);
run->analysisStep = analysisStep;
run->configuration = configuration;
run->tempDir = tempDir;
run->firstInputId = ef->id;
run->inputFileCount = 1;
run->inputFileIds = inputFileIds;
run->inputTypes = inputTypes;
run->outputFileCount = 1;
run->outputNamesInTempDir = outputNamesInTempDir;
run->outputFormats = outputFormats;
run->outputTypes = outputTypes;
run->assemblyId = assembly->id;
run->jsonResult = "";
edwAnalysisRunSaveToDb(conn, run, "edwAnalysisRun", 0);
freez(&run);
}
    
void runFastqAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
/* Run fastq analysis, at least on the data types where we can handle it. */
{
char *dataType = exp->dataType;
if (sameString(dataType, "DNase-seq") || sameString(dataType, "ChIP-seq"))
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
		schedulePairedBwa(conn, vf1, vf2, exp);
		edwQaPairedEndFastqFree(&pair);
		}
	    edwValidFileFree(&vfB);
	    }
	}
    else
        {
	scheduleSingleBwa(conn, ef, vf, exp);
	}
    }
}

void runSingleAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
if (sameWord("fastq", vf->format))
    runFastqAnalysis(conn, ef, vf, exp);
}

void runReplicateAnalysis(struct sqlConnection *conn, struct edwFile *ef, struct edwValidFile *vf,
    struct edwExperiment *exp)
{
uglyf("Theoretically running replicate analysis on %u\n", ef->id);
}

void edwScheduleAnalysis(int startFileId, int endFileId)
/* edwScheduleAnalysis - Schedule analysis runs.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFile *ef, *efList = edwFileAllIntactBetween(conn, startFileId, endFileId);
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	struct edwExperiment *exp = edwExperimentFromAccession(conn, vf->experiment); 
	if (exp != NULL)
	    {
	    if (vf->singleQaStatus > 0)
		runSingleAnalysis(conn, ef, vf, exp);
	    if (vf->replicateQaStatus > 0)
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
again = optionExists("again");
edwScheduleAnalysis(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
