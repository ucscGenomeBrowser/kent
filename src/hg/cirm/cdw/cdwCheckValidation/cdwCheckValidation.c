/* cdwCheckValidation - Check if a cdwSubmit validation step has completed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwCheckValidation - Check if a cdwSubmit validation step has completed\n"
  "usage:\n"
  "   cdwCheckValidation XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

/*
struct sqlConnection *conn5 = sqlConnect("cdw"); 
char query5[1024]; 
sqlSafef(query5, 1024, "select * from cdwStepOut where stepRunId = '%i';", out1->stepRunId);
struct cdwStepOut *out5 = cdwStepOutLoadByQuery(conn5, query5);
*/

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
printf("This user has an userId %i, this is the email provided %s.\n", submitter->id, submitter->email);  
// Use the userId to key into cdwSubmit and get the last submission id 
char query2[1024];
sqlSafef(query2, 1024, "select * from cdwSubmit where userId=%i order by id desc limit 1", submitter->id);  
struct cdwSubmit *submission = cdwSubmitLoadByQuery(conn2, query2);
sqlDisconnect(&conn); 
sqlDisconnect(&conn2); 
return submission->id; 

}


void printValidationStats(struct cdwJob *cdwJobCompleteList, int files, int filesValidated)
/* Takes in a linked list of fileId's and a float describing how much of the submission is done 
 * prints out stats and summarizes the total submission*/ 
{
struct sqlConnection *conn = sqlConnect("cdw"); 
struct cdwJob *iter;
int failedFiles = 0; 
float totEnrich = 0.0;  // Keep track of the total enrichment for averaging later.  

uglyf("whats going on, this is the command line %s \n", cdwJobCompleteList->commandLine); 



// Unpack the list of cdwJob entries and spit out some stats.  
for (iter = cdwJobCompleteList; iter->next !=NULL; iter = iter->next)
    {
    uglyf("whats going on \n"); 
    // The command line field in cdwJob has a very specific format that is being expoited here.  The field will always
    // follow this format "cdwQaAgent <fileId>".  
    struct slName *commandLine = charSepToSlNames(iter->commandLine, *" ");
    // Look at the command line field, cut it on white space and store it as two slName elements in a list. 
    char *fileId =  commandLine->next->name; // Grab the second slName in the list, this is the fileID.
    // Verify the file is not in cdqQaFail. 
    char query[1024]; 
    sqlSafef(query, 1024, "select * from cdwQaFail where id = %s", fileId); 
    struct cdwQaFail *failedFile = cdwQaFailLoadByQuery(conn, query); 
    uglyf("whats going on \n"); 
    if (failedFile)
	{
	printf("This file with id = %s did not pass validation and was found in cdwQaFail.\n", fileId); 
	failedFiles += 1; 
	continue;
	}


    // Mine stats from a good validated file. 
    char query2[1024]; 
    sqlSafef(query2, 1024, "select * from cdwQaEnrich where fileId = %s", fileId); 
    struct cdwQaEnrich *enrichStats = cdwQaEnrichLoadByQuery(conn, query2);
    totEnrich += enrichStats->enrichment; 
    printf("Some quick stats from qaEnrich, enrichment:  %f\n", enrichStats->enrichment); //Mine and print stats
    }
uglyf("whats going on \n"); 

printf("Found %i files in the submission.",files); 
printf("%i have completed validation, this is %f percent of the total submission.\n", filesValidated, (float)(filesValidated)/(float)(files));
//printf("%i have failed validation, this is %f percent of the validated submission and %f of the total submission.\n", failedFiles, (float)(failedFiles)/(float)() ); 
printf("The average coverage across the validated files is %f.\n", (float) totEnrich/(float) filesValidated); 

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

// Grab all cdwJob entries that have completed.  
char query2[1024]; 
sqlSafef(query2, 1024, "select * from cdwJob where submitId = '%i' and startTime < endTime;", submitId);
struct cdwJob *cdwJobCompletedList = cdwJobLoadByQuery(conn2, query2);

sqlDisconnect(&conn); 
sqlDisconnect(&conn2); 

// Go through the completed job entries and mine stats. Print some overall stats as well. 
printValidationStats(cdwJobCompletedList, slCount(cdwJobList), slCount(cdwJobCompletedList)); 

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwCheckValidation(argv[1]);
return 0;
}
