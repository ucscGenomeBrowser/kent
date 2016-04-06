/* cdwCheckValidation - Check if a cdwSubmit validation step has completed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "obscure.h"

boolean gStatus = FALSE;
boolean gScripting = FALSE;
int validationComplete = 1; 


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwCheckValidation - Check if a cdwSubmit validation step has completed and print some \n"
  "\t\t     file metrics for the submission. Returns 0 if the submission has completed \n"
  "\t\t     and 1 otherwise. \n"
  "usage:\n"
  "\tcdwCheckValidation user@email.address\n"
  "options:\n"
  "\t-status - Print the status only, skip file metrics. \n"
  "\t-scripting - Print nothing at all."
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"status", OPTION_BOOLEAN},
   {"scripting", OPTION_BOOLEAN},
   {NULL, 0},
};


int getSubmitId(char *cdwUser)
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
if (!gScripting) printf("User email:\t%s\nUser id:\t%i\n",submitter->email, submitter->id); 

// Use the userId to key into cdwSubmit and get the last submission id 
char query2[1024];
sqlSafef(query2, 1024, "select * from cdwSubmit where userId=%i order by id desc limit 1", submitter->id);  
struct cdwSubmit *submission = cdwSubmitLoadByQuery(conn2, query2);
if (submission == NULL) uglyAbort("There are no submissions associated with the provided id.");
if (!gScripting) printf("Submission id:\t%i\n",submission->id); 
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

void printValidationStats(struct cdwJob *cdwJobCompleteList, int files, int totalFiles)
/* Takes in a linked list of fileId's and a float describing how much of the submission is done 
 * prints out stats and summarizes the total submission*/ 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
struct cdwJob *iter;
int failedFiles = 0; 
float totEnrich = 0.0, totMap = 0.0, totUniqMap = 0.0, totCov = 0.0;  // Keep track of the totals for averaging later.  

// Unpack the list of cdwJob entries and spit out some stats. 

    
if (!gStatus) printf("Printing file statistics...\n");
for (iter = cdwJobCompleteList; iter->next !=NULL; iter = iter->next)
    {
    // The command line field in cdwJob has a very specific format that is being expoited here.  The field will always
    // follow this format "cdwQaAgent <fileId>".  
    struct slName *commandLine = charSepToSlNames(iter->commandLine, *" ");
    // Look at the command line field, cut it on white space and store it as two slName elements in a list. 
    char *fileId =  commandLine->next->name; // Grab the second slName in the list, this is the fileID.
    // Verify the file is not in cdqQaFail. 
    
    if (iter->returnCode!=0)
	{
	if (!gStatus)printf("File id: %s | Validation: Fail\n", fileId); 
	failedFiles += 1; 
	continue;
	}
    // Mine stats from a good validated file. 
    //Enrichment stats
    char query[1024]; 
    sqlSafef(query, 1024, "select * from cdwQaEnrich where fileId = %s", fileId); 
    struct cdwQaEnrich *enrichStats = cdwQaEnrichLoadByQuery(conn, query);
    totEnrich += enrichStats->enrichment; 
    //Valid file stats
    char query2[1024]; 
    sqlSafef(query2, 1024, "select * from cdwValidFile where fileId = %s", fileId); 
    struct cdwValidFile *fileStats = cdwValidFileLoadByQuery(conn, query2);
    totMap += fileStats->mapRatio; 
    totUniqMap += fileStats->uniqueMapRatio;
    totCov += fileStats->sampleCoverage;
    if (!gStatus) printf("File id: %s | Validation: Pass | Enrichment: %f | Map ratio: %f | " 
			"Unique map ratio: %f | Sample coverage: %f\n", fileId, enrichStats->enrichment,  
			fileStats->mapRatio, fileStats->uniqueMapRatio, fileStats->sampleCoverage); //Mine and print stats
    }
    

printf("Total files:\t%i\n",files); 
printf("Files that completed cdwQaAgent:\t%i\n\tPercent of submission:\t%f\n", totalFiles, 100*((float)(totalFiles)/(float)(files)));
printf("Failed validation:\t%i\n\tPercent of submission past cdwQaAgent:\t%f\n\tPercent of total submission:\t\t%f\n", failedFiles, 100*(float)(failedFiles)/(float)(totalFiles), 100*(float)(failedFiles)/(float)(files));  

printf("Average enrichment of validated submission:\t%f\n", (float) totEnrich/(float) (totalFiles - failedFiles) );
printf("Average map ratio of validated submission:\t%f\n", (float) totMap/(float) (totalFiles - failedFiles) );
printf("Average unique map ratio of validated submission:\t%f\n", (float) totUniqMap/(float) (totalFiles - failedFiles) );
printf("Average coverage of validated submission:\t\t%f\n", (float) totCov/(float) (totalFiles - failedFiles) );
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

printf("Time so far:\t%ih %im %is\nTime remaining:\t%ih %im %is\n", hours, minutes, seconds, eHours, eMinutes, eSeconds); 
sqlDisconnect(&conn); 
}


void cdwCheckValidation(char *cdwUser)
/* cdwCheckValidation - Check if a cdwSubmit validation step has completed. */
{
int submitId = getSubmitId(cdwUser); // Grab the submit id, lets the user give an email address. 

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
    if (gScripting) exit(1);
    else printf("Status:\t\tRunning\n");
    }
else 
    {
    validationComplete = 0; 
    if (gScripting) exit(0);
    else printf("Status:\t\tComplete\n"); 
    }
// Go through the completed job entries and mine stats. Print some overall stats as well. 
printValidationStats(cdwJobCompletedList, slCount(cdwJobList) , slCount(cdwJobCompletedList) ); 

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
gStatus = optionExists("status");
gScripting = optionExists("scripting");
if (argc != 2)
    usage();
cdwCheckValidation(argv[1]);
return validationComplete;
}
