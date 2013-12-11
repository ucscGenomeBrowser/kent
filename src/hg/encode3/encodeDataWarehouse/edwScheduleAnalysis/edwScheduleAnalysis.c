/* edwScheduleAnalysis - Schedule analysis runs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwScheduleAnalysis - Schedule analysis runs.\n"
  "usage:\n"
  "   edwScheduleAnalysis startFileId endFileId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static int countAlreadyScheduled(struct sqlConnection *conn, char *scriptName, long long firstInput)
/* Return how many of the existing job are already scheduled */
{
char query[512];
sqlSafef(query, sizeof(query),
    "select count(*) from edwAnalysisRun where firstInputId=%lld and scriptName='%s'",
    firstInput, scriptName);
return sqlQuickNum(conn, query);
}

static int addAnalysisJob(struct sqlConnection *conn, char *commandLine)
/* Add job to edwAnalyisJob table and return job ID. */
{
struct edwAnalysisJob job =
   {
   .commandLine = commandLine,
   };
edwAnalysisJobSaveToDb(conn, &job, "edwAnalysisJob", 0);
return sqlLastAutoId(conn);
}

void schedulePairedBwa(struct sqlConnection *conn, 
    struct edwValidFile *vf1, struct edwValidFile *vf2, struct edwExperiment *exp)
/* If it hasn't already been done schedule bwa analysis of the two files. */
{
/* Make sure that we don't schedule it again and again */
char *scriptName = "eap_run_bwa_pe.bash";
if (countAlreadyScheduled(conn, scriptName, vf1->fileId))
    return;

/* Get ef records */
struct edwFile *ef1 = edwFileFromIdOrDie(conn, vf1->fileId);
struct edwFile *ef2 = edwFileFromIdOrDie(conn, vf2->fileId);

/* Get target assembly (will redo this more precisely at some point) */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf1->ucscDb);

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
edwBwaIndexPath(assembly, indexPath);

/* Figure out temp dir and create it */
char *tempDir = rTempName(eapTempDir, vf1->licensePlate, "");
makeDir(tempDir);

/* Make up job and command line */
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "cd %s; nice %s %s %s%s %s%s %s",
    tempDir, scriptName, indexPath, edwRootDir, ef1->edwFileName, edwRootDir, 
    ef2->edwFileName, "output.bam");
int jobId = addAnalysisJob(conn, commandLine);

/* Make up edwAnalysisRun record */
unsigned inputFiles[2] = {ef1->id, ef2->id};
char *outputFiles[1] = {"output.bam"};
struct edwAnalysisRun *run;
AllocVar(run);
run->jobId = jobId;
safef(run->experiment, sizeof(run->experiment), "%s",  exp->accession);
run->scriptName = scriptName;
run->tempDir = tempDir;
run->firstInputId = ef1->id;
run->inputFileCount = 2;
run->inputFiles = inputFiles;
run->outputFileCount = 1;
run->outputFiles = outputFiles;
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
/* Make sure that we don't schedule it again and again */
char *scriptName = "eap_run_bwa_se.bash";
if (countAlreadyScheduled(conn, scriptName, ef->id))
    return;

/* Get target assembly (will redo this more precisely at some point) */
struct edwAssembly *assembly = edwAssemblyForUcscDb(conn, vf->ucscDb);

/* Figure out path to BWA index */
char indexPath[PATH_LEN];
edwBwaIndexPath(assembly, indexPath);

/* Figure out temp dir and create it */
char *tempDir = rTempName(eapTempDir, vf->licensePlate, "");
makeDir(tempDir);

/* Make up job and command line */
char commandLine[4*PATH_LEN];
safef(commandLine, sizeof(commandLine), "cd %s; nice %s %s %s%s %s",
    tempDir, scriptName, indexPath, edwRootDir, ef->edwFileName, "output.bam");
int jobId = addAnalysisJob(conn, commandLine);

/* Make up edwAnalysisRun record */
unsigned inputFiles[1] = {ef->id};
char *outputFiles[1] = {"output.bam"};
struct edwAnalysisRun *run;
AllocVar(run);
run->jobId = jobId;
safef(run->experiment, sizeof(run->experiment), "%s",  exp->accession);
run->scriptName = scriptName;
run->tempDir = tempDir;
run->firstInputId = ef->id;
run->inputFileCount = 1;
run->inputFiles = inputFiles;
run->outputFileCount = 1;
run->outputFiles = outputFiles;
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
uglyf("Theoretically running single analysis on %u\n", ef->id);
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
edwScheduleAnalysis(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}
