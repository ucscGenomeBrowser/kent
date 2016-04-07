/* cdwCheckValidation - Check if a cdwSubmit validation step has completed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "obscure.h"

boolean gWait = FALSE; 
boolean gRetry = FALSE; 
boolean gStatus = FALSE;
boolean gScripting = FALSE;
boolean gFailed = FALSE; 
int gSubmitId = -1; 
int validationComplete = -1; 


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwCheckValidation - Check if a cdwSubmit validation step has completed and print some \n"
  "\t\t     file metrics for the submission. Returns 0 if the submission has completed \n"
  "\t\t     and -1 otherwise. \n"
  "usage:\n"
  "\tcdwCheckValidation user@email.address outputFile\n"
  "options:\n"
  "\t-submitId - Over ride the auto selection and use this specific id, this ignores the email provided. \n"
  "\t-wait - Wait until all files have been processed by cdwQaAgent, check every 5 seconds or so.\n"
  "\t-retry - Requeue the files that failed, highly recommended to run with the -failed option first to see what your about to queue up.\n"
  "\t-status - Print the status only, skip file metrics. \n"
  "\t-scripting - Print nothing at all.\n"
  "\t-failed - Print out just the failed files."
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"submitId", OPTION_STRING}, 
    {"wait", OPTION_BOOLEAN}, 
    {"retry", OPTION_BOOLEAN}, 
    {"status", OPTION_BOOLEAN},
    {"scripting", OPTION_BOOLEAN},
    {"failed", OPTION_BOOLEAN},
    {NULL, 0},
};


int getSubmitId(char *cdwUser, FILE *f)
/* Use the user email to get the userId out of cdwUser, then use the cdwUser id to key into 
 * cdwSubmit and grab the last submission */ 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
struct sqlConnection *conn2 = sqlConnect("cdw"); 
// Query the cdwUser table to find the userId
char query[1024]; 
sqlSafef(query, 1024, "select * from cdwUser where email = '%s';", cdwUser);
struct cdwUser *submitter = cdwUserLoadByQuery(conn,query); 
if (submitter == NULL) uglyAbort("There are no users associated with the provided email %s",cdwUser); 
if (!gScripting && !gStatus && !gFailed && !gRetry) fprintf(f,"User email:\t%s\nUser id:\t%i\n",submitter->email, submitter->id); 

// Use the userId to key into cdwSubmit and get the last submission id 
char query2[1024];
sqlSafef(query2, 1024, "select * from cdwSubmit where userId=%i order by id desc limit 1", submitter->id);  
struct cdwSubmit *submission = cdwSubmitLoadByQuery(conn2, query2);
if (submission == NULL) uglyAbort("There are no submissions associated with the provided id.");
if (!gScripting && !gStatus && !gFailed && !gRetry) fprintf(f,"Submission id:\t%i\n",submission->id); 
sqlDisconnect(&conn); 
sqlDisconnect(&conn2); 

return submission->id; 
}

int secondsUsed(struct cdwJob *jobList)
/* Lets figure out how long things have taken. */
{
struct cdwJob *iter; 
int totTime = 0;
for (iter = jobList; iter->next != NULL; iter = iter->next)
    {
    totTime += (iter->endTime - iter->startTime);
    }
return totTime; 
}

void printEnrichmentStats(FILE *f, struct cdwValidFile *validFile)
/* Enrichment stats require a bit of table jummping. The flow goes like this,
 * 0. start with a cdwValidFile.fileId and get the cdwValidFile.enrichedIn and cdwValidFile.ucscDb values. 
 * 1.1 use cdwValidFile.ucscDb value to query into cdwAssembly.ucscDb value
 * 1.2 use cdwAssembly.ucscDb value to get cdwAssembly.id
 * 2.1 use cdwAssembly.id value to get cdwQaEnrichTarget.assemblyId 
 * 2.2 use cdwValidFile.enrichedIn to get cdwQaEnrichTarget.name. 
 * NOTE that here we add in chrX, chrY and chrM (sometimes) as possible cdwAsEnrichTarget.name possibilities.
 * 2.3 use cdwQaEnrichTarget.assemblyId and cdwQaEnrichTarget.name values to get cdwQaEnrichTarget.id values
 * 3.1 use cdwValidFile.fileId to query into cdwQaEnrich.fileId
 * 3.2 use cdwQaEnrichTarget.id to query into cdwQaEnrich.qaEnrichTargetId
 * This gives us the enrichment values we want for a given file id.  */ 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
char query[1024], query2[1024];
fprintf(f, " Enrich");
if (!validFile->ucscDb) return;
// Step 1
sqlSafef(query, 1024, "select * from cdwAssembly where ucscDb = '%s'", validFile->ucscDb); 
struct cdwAssembly *assembly = cdwAssemblyLoadByQuery(conn, query);
// Step 2
if (!assembly) return;
sqlSafef(query2, 1024, "select * from cdwQaEnrichTarget where assemblyId = %i and (name = '%s' "
	"or name = 'chrX' or name = 'chrY' or name = 'chrM') ", assembly->id, validFile->enrichedIn); 
struct cdwQaEnrichTarget *enrichTarget = cdwQaEnrichTargetLoadByQuery(conn, query2);
// Step 3 
if (!enrichTarget) return;
struct cdwQaEnrichTarget *iter;

for (iter = enrichTarget; ; iter = iter->next)
    {
    
    char tQuery[1024]; 
    sqlSafef(tQuery, 1024, "select * from cdwQaEnrich where fileId = %i and qaEnrichTargetId = %i", validFile->fileId, iter->id); 
    //uglyf("whats gucci! %s\n",tQuery); 
    struct cdwQaEnrich *enrich = cdwQaEnrichLoadByQuery(conn, tQuery);
    fprintf(f," %s: %0.3f", iter->name, enrich->enrichment);
    if (iter->next == NULL) break;
    else fprintf(f,",");
    
    }
sqlDisconnect(&conn); 
}

void printTimeStats(struct cdwJob *cdwJobCompleteList, int files, int totalFiles, FILE *f)
{
int timeSoFar = secondsUsed(cdwJobCompleteList); 

int hours, minutes, seconds; 
hours = timeSoFar/3600; 
minutes = (timeSoFar - (hours*3600))/ 60; 
seconds = (timeSoFar - (hours*3600) - (minutes*60)); 

float perComplete = (float)(totalFiles)/(float)(files); 

float estTimeRemaining = ((float)timeSoFar / (float)perComplete) - timeSoFar; 
int eHours, eMinutes, eSeconds; 
eHours = estTimeRemaining/3600; 
eMinutes = (estTimeRemaining - (eHours*3600))/ 60; 
eSeconds = (estTimeRemaining - (eHours*3600) - (eMinutes*60)); 

fprintf(f,"Time so far:\t%ih %im %is\nTime remaining:\t%ih %im %is\n", hours, minutes, seconds, eHours, eMinutes, eSeconds); 
}

void printAllValidationStats(struct cdwJob *cdwJobCompleteList, int files, int totalFiles, FILE *f)
/* Takes in a linked list of fileId's and a float describing how much of the submission is done 
 * prints out stats and summarizes the total submission*/ 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
struct cdwJob *iter;
int failedFiles = 0; 
float totMap = 0.0, totUniqMap = 0.0, totCov = 0.0;  // Keep track of the totals for averaging later.  

// Unpack the list of cdwJob entries and spit out some stats. 
    
if (!gStatus && !gFailed && !gRetry) fprintf(f,"Printing file statistics...\n");
for (iter = cdwJobCompleteList; ; iter = iter->next)
    {
    // The command line field in cdwJob has a very specific format that is being expoited here.  The field will always
    // follow this format "cdwQaAgent <fileId>".  
    struct slName *commandLine = charSepToSlNames(iter->commandLine, *" ");
    // Look at the command line field, cut it on white space and store it as two slName elements in a list. 
    char *fileId =  commandLine->next->name; // Grab the second slName in the list, this is the fileID.
    // Verify the file is not in cdqQaFail. 
    // Mine stats from a good validated file, this typically only works for fastq files. 
    // Get a valid file. 
    char query[1024]; 
    sqlSafef(query, 1024, "select * from cdwValidFile where fileId = %s", fileId); 
    struct cdwValidFile *validFile = cdwValidFileLoadByQuery(conn, query);
    
    if (iter->returnCode!=0)
	{
	if (!gStatus)
	    {
	    if (!gRetry) fprintf(f,"File id: %s | Valid: No\n", fileId); 
	    //if (gRetry) fprintf(f,"cdqQaAgent %s\n",fileId); 
	    if (gRetry) cdwAddQaJob(conn, (long long int) fileId, iter->submitId);
	    }
	failedFiles += 1; 
	continue;
	}
    
    //For all other files there will likely not be enrichment or mapping stats. 
    totMap += validFile->mapRatio; 
    totUniqMap += validFile->uniqueMapRatio;
    totCov += validFile->sampleCoverage;
    
    if (!gStatus && !gFailed && !gRetry) 
	{
	// Print n/a if the file doesn't have the metrics we are looking for. 
	char mapRatio[128] = "n/a";
	char uniqueMapRatio[128] = "n/a";
	char coverage[128] = "n/a";
	if (validFile->mapRatio) safef(mapRatio, 128, "%0.3f", validFile->mapRatio); 
	
	if (validFile->uniqueMapRatio) safef(uniqueMapRatio, 128, "%0.3f", validFile->uniqueMapRatio); 
	if (validFile->sampleCoverage) safef(coverage, 128, "%0.3f", validFile->sampleCoverage); 
	fprintf(f,"File id: %s | Valid: Yes | Format: %s | Map ratio: %s | " 
			"Unique map ratio: %s| Sample coverage: %s |", fileId, validFile->format, 
			mapRatio, uniqueMapRatio, coverage); //Mine and print stats
	printEnrichmentStats(f, validFile);
	fprintf(f," |\n");
	}
    if (iter->next == NULL) break;
    }
    
// There was probably a good reason for this at one point. Now there are a bunch of if' statements. 
if (!gFailed && !gRetry)
    {
    fprintf(f,"Total files:\t%i\n",files); 
    fprintf(f,"Files that completed cdwQaAgent:\t%i\n", totalFiles); 
    if (!gStatus) fprintf(f,"\tPercent of submission:\t%f\n", 100*((float)(totalFiles)/(float)(files)));
    fprintf(f,"Passed validation:\t%i\n",  files - failedFiles);  
    }

if (!gRetry) fprintf(f,"Failed validation:\t%i\n", failedFiles);

if (!gStatus && !gFailed && !gRetry)
    {
    fprintf(f,"\tPercent of submission past cdwQaAgent:\t%f\n\tPercent of total submission:\t\t%f\n", 100*(float)(failedFiles)/(float)(totalFiles), 100*(float)(failedFiles)/(float)(files)); 
    fprintf(f,"Average map ratio of validated submission:\t%f\n", (float) totMap/(float) (totalFiles - failedFiles) );
    fprintf(f,"Average unique map ratio of validated submission:\t%f\n", (float) totUniqMap/(float) (totalFiles - failedFiles) );
    fprintf(f,"Average coverage of validated submission:\t\t%f\n", (float) totCov/(float) (totalFiles - failedFiles) );
    printTimeStats(cdwJobCompleteList, files, totalFiles, f);
    }

sqlDisconnect(&conn); 
}

void waitLoop(int submitId)
/* Hang out and wait for all files in the submission to pass through cdwQaAgent. */ 
{
for (;;)
    {
    struct sqlConnection *conn = sqlConnect("cdw"); 
    struct sqlConnection *conn2 = sqlConnect("cdw"); 

    // Grab all cdwJob entries with the submitId. 
    char query[1024]; 
    sqlSafef(query, 1024, "select * from cdwJob where submitId = '%i';", submitId);
    struct cdwJob *cdwJobList = cdwJobLoadByQuery(conn, query);
    if (!cdwJobList) uglyAbort("The submission has no entries in cdwJob, it could be corrupted or submitted before submitId's were implemented.");
    // Grab all cdwJob entries that have completed.  
    char query2[1024]; 
    sqlSafef(query2, 1024, "select * from cdwJob where submitId = '%i' and startTime < endTime;", submitId);
    struct cdwJob *cdwJobCompletedList = cdwJobLoadByQuery(conn2, query2);

    sqlDisconnect(&conn); 
    sqlDisconnect(&conn2); 
    if (slCount(cdwJobList) != slCount(cdwJobCompletedList))
	{
	sleep(5); 
	}
    else 
	{
	break;
	}
    }
}

void cdwCheckValidation(char *cdwUser, char *outputFile)
/* cdwCheckValidation - Check if a cdwSubmit validation step has completed. */
{

struct sqlConnection *conn = sqlConnect("cdw"); 
struct sqlConnection *conn2 = sqlConnect("cdw"); 

FILE *f = mustOpen(outputFile,"w"); 
int submitId; 
if (gSubmitId == -1)
    submitId = getSubmitId(cdwUser, f); // Grab the submit id, lets the user give an email address. 
else submitId = gSubmitId; 

// Grab all cdwJob entries with the submitId. 
char query[1024]; 
sqlSafef(query, 1024, "select * from cdwJob where submitId = '%i';", submitId);
struct cdwJob *cdwJobList = cdwJobLoadByQuery(conn, query);
if (!cdwJobList) uglyAbort("The submission has no entries in cdwJob, it could be corrupted or submitted before submitId's were implemented.");
// Grab all cdwJob entries that have completed.  
char query2[1024]; 
sqlSafef(query2, 1024, "select * from cdwJob where submitId = '%i' and startTime < endTime;", submitId);
struct cdwJob *cdwJobCompletedList = cdwJobLoadByQuery(conn2, query2);

if (gWait) waitLoop(submitId); 

sqlDisconnect(&conn); 
sqlDisconnect(&conn2); 
if (slCount(cdwJobList) != slCount(cdwJobCompletedList))
    {
    if (gScripting) exit(-1);
    else if (!gRetry) fprintf(f,"Status:\t\tRunning\n");
    }
else 
    {
    validationComplete = 0; 
    if (gScripting) exit(0);
    else if (!gRetry) fprintf(f,"Status:\t\tComplete\n"); 
    }
// Go through the completed job entries and mine stats. Print some overall stats as well. 
printAllValidationStats(cdwJobCompletedList, slCount(cdwJobList) , slCount(cdwJobCompletedList), f ); 

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
gRetry = optionExists("retry"); 
gWait = optionExists("wait");
gSubmitId = optionInt("submitId", gSubmitId);
gStatus = optionExists("status");
gScripting = optionExists("scripting");
gFailed = optionExists("failed");
if (argc != 3)
    usage();
cdwCheckValidation(argv[1], argv[2]);
return validationComplete;
}
