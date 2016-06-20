/* cdwCheckValidation - Check if a cdwSubmit validation step has completed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "obscure.h"

// Options and an int to keep track of the validation. 
int gSubmitId = -1; 
boolean gWait = FALSE; 
boolean gLong = FALSE; 


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwCheckValidation - Check if a cdwSubmit validation step has completed and print some \n"
  "\t\t     file metrics for the submission. Returns 0 if the submission has completed \n"
  "\t\t     and -1 otherwise. \n"
  "usage:\n"
  "\tcdwCheckValidation command user@email.address outputFile\n"
  "commands are one of:\n"
  "   status - print information\n"
  "   failed - list failed files\n"
  "   retry - rerun validation on failed files\n"
  "options:\n"
  "\t-submitId - Over ride the auto selection and use this specific id, this ignores the email"
  "provided. \n"
  "\t-wait - Wait until all files have been processed by cdwQaAgent, check every 5 seconds or" 
  "so.\n"
  "\t-long - Prints file-by-file status as well as overall status. \n");
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"submitId", OPTION_STRING}, 
    {"wait", OPTION_BOOLEAN}, 
    {"long", OPTION_BOOLEAN}, 
    {NULL, 0},
};


int getSubmitId(FILE *f, char *cdwUser, char *command)
/* Use the user email to get the userId out of cdwUser, then use the cdwUser id to key into 
 * cdwSubmit and grab the last submission */ 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
// Query the cdwUser table to find the userId associated with the email address. 
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwUser where email = '%s';", cdwUser);
struct cdwUser *submitter = cdwUserLoadByQuery(conn,query); 
if (submitter == NULL)
    uglyAbort("There are no users associated with the provided email %s",cdwUser); 

// Use the userId to key into cdwSubmit and get the last submission id. 
sqlSafef(query, sizeof(query), "select * from cdwSubmit where userId=%i order by id desc limit 1",
	    submitter->id);  
struct cdwSubmit *submission = cdwSubmitLoadByQuery(conn, query);
if (submission == NULL) uglyAbort("There are no submissions associated with the provided id.");

// Print out stats. 
if (startsWith("status",command)) 
    {
    fprintf(f,"User email:\t%s\nUser id:\t%i\n",submitter->email, submitter->id); 
    fprintf(f,"Submission id:\t%i\n",submission->id); 
    }
sqlDisconnect(&conn); 

return submission->id; 
}

int secondsUsed(int submitId)
/* Lets figure out how long things have taken. */
{
struct sqlConnection *conn = sqlConnect("cdw"); 
char query[1024]; 

// Get when the first job started. 
sqlSafef(query, sizeof(query), "select min(startTime) from cdwJob where submitId = '%i'"
	" and startTime != 0", submitId);
int batchStart = sqlQuickNum(conn, query);

// Get when the last job finished. 
sqlSafef(query, sizeof(query), "select max(endTime) from cdwJob where submitId = '%i'"
	" and endTime != 0", submitId);
int batchEnd = sqlQuickNum(conn, query);

sqlDisconnect(&conn); 
return batchEnd - batchStart; 
}

void printEnrichmentStats(FILE *f, struct cdwValidFile *validFile)
// Print out the enrichment stats for a valid file. 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
char query[1024];
fprintf(f, " Enrich");
// Return if no ucscDb is specified. 
if (!validFile->ucscDb)
    return;

// Get the assembly id, default to hg19.  
char ucscDb[128] = "hg19";
if (!startsWith(validFile->ucscDb, " "))
    safef(ucscDb, sizeof(ucscDb), "%s", validFile->ucscDb); 
sqlSafef(query, sizeof(query), "select * from cdwAssembly where ucscDb = '%s'", ucscDb);
struct cdwAssembly *assembly = cdwAssemblyLoadByQuery(conn, query);
if (!assembly) return;


// Get the list of enrichment targets for this valid file.  
char enrichedIn[128] = "exon";
if (!startsWith(validFile->enrichedIn," "))
    safef(enrichedIn, sizeof(enrichedIn), "%s", validFile->enrichedIn); 
sqlSafef(query, sizeof(query), "select * from cdwQaEnrichTarget where assemblyId = %i and name in" 
	"('%s', 'chrX', 'chrY', 'chrM')", assembly->id, enrichedIn); 
struct cdwQaEnrichTarget *targetList = cdwQaEnrichTargetLoadByQuery(conn, query);
if (!targetList) 
    return;

// Loop through the list of enrichment targets and print stats. 
struct cdwQaEnrichTarget *target;
for (target = targetList; ; target = target->next)
    {
    sqlSafef(query, sizeof(query), "select enrichment from cdwQaEnrich where fileId = %i and"
		" qaEnrichTargetId = %i", validFile->fileId, target->id); 
    double enrichment = sqlQuickDouble(conn, query);
    fprintf(f," %s: %0.3f", target->name, enrichment);
    if (target->next == NULL)
	break;
    else 
	fprintf(f,",");
    }
sqlDisconnect(&conn); 
}

void printTimeStats(FILE *f, struct cdwJob *jobList, int finJobs)
// Print out the time so far and an estimate of the time remaining in a pretty fashion.
{
int timeSoFar = secondsUsed(jobList->submitId); 

int hours, minutes, seconds; 
// Do math magic to get the proper hours, minutes and seconds. 
hours = timeSoFar/3600; 
minutes = (timeSoFar - (hours*3600))/ 60; 
seconds = (timeSoFar - (hours*3600) - (minutes*60)); 
assert(slCount(jobList) > 0); 

// Determine average time so far, multiply it by the number of jobs remaining.  
double avgTimeSoFar = (double) timeSoFar / (double) finJobs; 
int jobsRemaining = slCount(jobList) - finJobs; 
double estTimeRemaining = avgTimeSoFar * jobsRemaining; 
    
int eHours, eMinutes, eSeconds; 
eHours = estTimeRemaining/3600; 
eMinutes = (estTimeRemaining - (eHours*3600))/ 60; 
eSeconds = (estTimeRemaining - (eHours*3600) - (eMinutes*60)); 

fprintf(f,"Time so far:\t%ih %im %is\nTime remaining:\t%ih %im %is\n", hours, minutes, seconds,
	    eHours, eMinutes, eSeconds); 
}
		
void printValidFileStats(FILE *f, char *fileId)
// Print out the stats for a valid file. Enrichment stats are handled in their own function. 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwValidFile where fileId = %s", fileId); 
struct cdwValidFile *validFile = cdwValidFileLoadByQuery(conn, query);
sqlDisconnect(&conn);

if (!validFile)
    // Usually indicates an errant cdwAddQaJob in some code some where. 
    {
    fprintf(f,"File id: %s | Status: corrupted | The file passed validation yet has no entry in"
	    " cdwValidFile |  \n", fileId);
    sqlDisconnect(&conn); 
    return; 
    }
// Default these to n/a to prevent seg faults. 

if (!validFile->mapRatio || !validFile->uniqueMapRatio || !validFile->sampleCoverage)
    {
    fprintf(f,"File id: %s | Valid: Yes | Format: %s |\n", fileId, validFile->format);
    return;
    }

fprintf(f,"File id: %s | Valid: Yes | Format: %s | Map ratio (mr): %f | Unique mr: %f| "
		"Coverage: %f |", fileId, validFile->format, validFile->mapRatio, 
		validFile->uniqueMapRatio, validFile->sampleCoverage);
printEnrichmentStats(f, validFile);
fprintf(f," |\n");
return; 
}
    
void printSubmissionStatistics(FILE *f, struct cdwJob *jobList, char *command)
/* Take a list of jobs and gather stats. Different stats are printed to file f depending on the
 * command given. */
{
if (!jobList)
    uglyAbort("There are no jobs on the joblist"); 

// Keep track of all the possible job types. 
int finJobs = 0, workingJobs = 0, queuedJobs = 0, failedJobs = 0, validFiles = 0; 

if (startsWith("status", command) && gLong) 
    fprintf(f,"Printing file statistics...\n");

// Loop through all jobs and gather stats. 
struct cdwJob *job;
for (job = jobList;; job = job->next)
    {
    char *prefix = "cdwQaAgent ";
    assert(startsWith(prefix, job->commandLine)); 
    char *fileIdString = job->commandLine + strlen(prefix);
    long long fileId = sqlLongLong(fileIdString);
    if (job->startTime > 0)
	{
	if (job->endTime > 0)
	// Finished jobs. 
	    {
	    ++finJobs; 
	    if (job->returnCode == 0)
	    // Jobs that passed validation. 
		{
		++validFiles; 
		if (startsWith("status", command) && gLong) 
		    printValidFileStats(f, fileIdString);
		}
	    else
	    // Jobs that failed validation. 
		{
		++failedJobs;
		if (startsWith("failed", command))
		    fprintf(f,"File id: %s | Valid: No | Error: %s |\n", fileIdString, job->stderr);  
		if (startsWith("status", command) && gLong) 
		   fprintf(f,"File id: %s | Valid: No | Error: %s |\n", fileIdString, job->stderr);  
		if (startsWith("retry",command))
		    {
		    fprintf(f,"Rerunning file %s \n", fileIdString); 
		    struct sqlConnection *conn = sqlConnect("cdw"); 
		    cdwAddQaJob(conn, fileId, job->submitId);
		    sqlDisconnect(&conn); 
		    }
		}
	    }
	else
	// Working jobs.
	    {
	    ++workingJobs;
	    if (startsWith("status", command) && gLong) 
		fprintf(f,"File id: %s | Valid: running | Run time: %i | \n", fileIdString, (int)((unsigned)time(NULL) - job->startTime));  
	    }
	}
    else
    // Queued jobs.  
	{
	++queuedJobs;
	if (startsWith("status", command) && gLong) 
	    fprintf(f,"File id: %s | Valid: queued | \n", fileIdString);  
	}
    if (!job->next) 
	break;
    }

// Print out overall submission stats. 

if (startsWith("status", command))
    {
    fprintf(f,"Total files:\t%i\n",slCount(jobList)); 
    fprintf(f,"Finished validation:\t%i\n", finJobs); 
    fprintf(f,"Passed validation:\t\e[1;32m%i\e[0m\n",  validFiles);  
    fprintf(f,"Failed validation:\t\e[1;31m%i\e[0m\n",  failedJobs);  
    fprintf(f,"Jobs in progress:\t%i\n", workingJobs); 
    fprintf(f,"Jobs queued:\t%i\n", queuedJobs); 
    printTimeStats(f, jobList, finJobs);
    if (slCount(jobList) == finJobs)
	fprintf(f,"Status:\t\e[1;32mCompleted\e[0m\n"); 
    else
	fprintf(f,"Status:\tIn progress\n"); 
    
    }
if (startsWith("retry", command))
    fprintf(f,"Started revalidating %i files.\n", failedJobs); 
if (startsWith("failed", command))
    fprintf(f,"%i files failed validation.\n", failedJobs); 
}
    

void waitLoop(int submitId)
/* Hang out and wait for all files in the submission to pass through cdwQaAgent. */ 
{
for (;;)
    {
    struct sqlConnection *conn = sqlConnect("cdw"); 

    // Grab all cdwJob entries with the submitId. 
    char query[1024]; 
    sqlSafef(query, sizeof(query), "select * from cdwJob where submitId = '%i';", submitId);
    struct cdwJob *jobList = cdwJobLoadByQuery(conn, query);
    if (!jobList) uglyAbort("The submission has no entries in cdwJob.\nThere are several "
		    "possibilities as to why; the submission could be corrupted, submitted before\n"
		    " submitId's were implemented or the submission is still copying files and has "
			"not started validation");
    // Determine how many cdwJob entries that have completed.  
    sqlSafef(query, sizeof(query), "select count(*) from cdwJob where submitId = '%i'"
		" and endTime > 0", submitId);
    int finJobs = sqlQuickNum(conn, query);

    sqlDisconnect(&conn); 
    if (slCount(jobList) != finJobs)
	{
	sleep(5); 
	}
    else 
	{
	break;
	}
    }
}

void cdwCheckValidation(char *command, char *cdwUser, char *outputFile)
/* cdwCheckValidation - Check if a submission has completed validation. */
{

struct sqlConnection *conn = sqlConnect("cdw"); 

FILE *f = mustOpen(outputFile,"w"); 
int submitId;
// If no submitId is given grab the last submission associated with the users email address. 
if (gSubmitId == -1)
    submitId = getSubmitId(f, cdwUser, command);
else submitId = gSubmitId; 

// Grab all cdwJob entries with the submitId. 
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwJob where submitId = '%i';", submitId);
struct cdwJob *jobList = cdwJobLoadByQuery(conn, query);


if (!jobList) uglyAbort("The submission has no entries in cdwJob.\nThere are several "
		    "possibilities as to why; the submission could be corrupted, submitted before\n"
		    " submitId's were implemented or the submission is still copying files and has "
		    "not started validation");
assert(slCount(jobList) > 0);
if (gWait) waitLoop(submitId); 

sqlSafef(query, sizeof(query), "select count(*) from cdwJob where submitId = '%i' and endTime > 0",
	    submitId);
int finJobs = sqlQuickNum(conn, query);
sqlDisconnect(&conn); 

// Go through the jobList, gather and print statistics. 
printSubmissionStatistics(f, jobList, command); 

// Check if the submission has finished validation. 
if (finJobs == slCount(jobList))
    exit(0);
else 
    exit(-1); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
gWait = optionExists("wait");
gLong = optionExists("long"); 
gSubmitId = optionInt("submitId", gSubmitId);
if (argc != 4)
    usage();
cdwCheckValidation(argv[1], argv[2], argv[3]);
return 0;
}
