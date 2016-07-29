/* cdwCheckDataset - Look at all the submissions for a given dataset and print helpful stats.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "obscure.h"

char *gWorkingDir = NULL;  

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwCheckDataset - Look at all the submissions for a given dataset and print helpful stats.\n"
  "\t\tThe data set is identified by a submission directory which defaults to the current \n"
  "\t\tworking directory.\n"
  "usage:\n"
  "   cdwCheckDataset outputFile\n"
  "options:\n"
  "   -submitDir - Provide a specific submission directory from cdwSubmitDir.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"submitDir", OPTION_STRING}, 
    {NULL, 0},
};

void cdwCheckDataset(char *outputFile)
/* cdwCheckDataset - Look at all the submissions for a given dataset and print helpful stats.. */
{

FILE *f = mustOpen(outputFile,"w"); 
struct sqlConnection *conn = sqlConnect("cdw"); 

char query[1024]; 
struct cdwSubmitDir *submitDir; 
// Get the id associated with the submission directory. 
if (gWorkingDir == NULL)
    {
    char cwd[1024]; 
    getcwd(cwd, sizeof(cwd)); 
    sqlSafef(query, sizeof(query), "select * from cdwSubmitDir where url = '%s';", cwd);
    submitDir = cdwSubmitDirLoadByQuery(conn, query); 
    }
else
    {
    sqlSafef(query, sizeof(query), "select * from cdwSubmitDir where url = '%s';", gWorkingDir);
    submitDir = cdwSubmitDirLoadByQuery(conn, query); 
    }

if (!submitDir)
    uglyAbort("There are no submissions associated with the submission directory provided.");

// Get all cdwSubmit entries associated with the submission directory. 
sqlSafef(query, sizeof(query), "select * from cdwSubmit where submitDirId = '%i';", submitDir->id); 
struct cdwSubmit *submitList = cdwSubmitLoadByQuery(conn, query);

struct cdwSubmit *submission;
// Keep track of the # of submissions, meta files and mani files. 
int count = 0; 
long long int totalFiles = 0, totalBytes = 0;	// Count the total files and bytes. 
struct slName *submitters = NULL, *user;    // Keep track of the submitters.   
// Loop over all cdwSubmit entries and gather/print out stats.  

for (submission = submitList; ; submission = submission->next)
    {
    ++count;
    sqlSafef(query,sizeof(query), "select * from cdwUser where id = '%i';", (int)submission->userId); 
    struct cdwUser *user = cdwUserLoadByQuery(conn,query); 
    if(!slNameInList(submitters, user->email)) // If a submitter has not been seen add it. 
	slNameAddHead(&submitters, user->email);
    // Print out submission stats. 
    fprintf(f, "Sub #: %i | Sub id: %i | New files: %i | New bytes: %lld | Mani id: %i |",  
	    count, submission->id, submission->newFiles, submission->newBytes, submission->manifestFileId);  
    fprintf(f, " Meta id: %i | Files with new meta data values: %i | User email: %s | Wrangler: %s\n",
	    submission->metaFileId, submission->metaChangeCount, user->email, submission->wrangler); 
    totalFiles += submission->newFiles; 
    totalBytes += submission->newBytes; 
    if (submission->next == NULL)
	break; 
    }
sqlSafef(query, sizeof(query), "select distinct metaFileId from cdwSubmit where submitDirId=%i", submitDir->id); 
fprintf(f,"Total submissions:\t%i\n", count); 
fprintf(f,"Total files:\t%lld\n",totalFiles);
// Math magic for pretty printing the file size. 
long long int tBytes, gBytes, mBytes, kBytes, bytes; 
tBytes = totalBytes/1000000000000;
gBytes = (totalBytes - tBytes * 1000000000000)/1000000000;
mBytes = (totalBytes - tBytes * 1000000000000 - gBytes * 1000000000)/1000000;
kBytes = (totalBytes - tBytes * 1000000000000 - gBytes * 1000000000 - mBytes * 1000000)/1000;
bytes = totalBytes - tBytes * 1000000000000 - gBytes * 1000000000 - mBytes * 1000000 - kBytes * 1000;

fprintf(f,"Total bytes:\t%lld T %lld G %lld M %lld K %lld B\n",  tBytes, gBytes, mBytes, kBytes, bytes); 

fprintf(f,"Submitters: "); 
// Print out the submitters, ideally it will only be one person but in practice... Not so much. 
for (user = submitters; ; user = user->next)
    {
    fprintf(f,"%s", user->name); 
    if (user->next == NULL)
	break;
    fprintf(f,", ");
    }
fprintf(f,"\n"); 
sqlDisconnect(&conn); 

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
gWorkingDir = optionVal("submitDir", gWorkingDir);
if (argc != 2)
    usage();
cdwCheckDataset(argv[1]);
return 0;
}
