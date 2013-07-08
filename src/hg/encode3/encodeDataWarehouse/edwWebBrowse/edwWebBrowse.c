/* edwWebBrowse - Browse ENCODE data warehouse.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebBrowse is a cgi script not meant to be run from command line.\n"
  );
}

char *userEmail = NULL; /* User's email handle. */

boolean queryIntoTable(struct sqlConnection *conn, char *query, char *title)
/* Make query and show result in a html table.  Return FALSE (and make no output)
 * if there is no result to query. */
{
boolean didHeader = FALSE;
struct sqlResult *sr = sqlGetResult(conn, query);
int colCount = sqlCountColumns(sr);
if (colCount > 0)
    {
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (!didHeader)
	    {
	    printf("<H3>%s</H3>\n", title);
	    printf("<TABLE>\n");
	    printf("<TR>");
	    char *field;
	    while ((field = sqlFieldName(sr)) != NULL)
		printf("<TH>%s</TH>", field);
	    printf("</TR>\n");
	    didHeader = TRUE;
	    }
	printf("<TR>");
	int i;
	for (i=0; i<colCount; ++i)
	    printf("<TD>%s</TD>", naForNull(row[i]));
	printf("</TR>\n");
	}
    printf("</TABLE>\n");
    }
sqlFreeResult(&sr);
return didHeader;
}

void printSubmitState(struct edwSubmit *submit, struct sqlConnection *conn)
/* Try and summarize submission status in a short statement. */
{
if (!isEmpty(submit->errorMessage))
    printf("has problems");
else if (submit->endUploadTime != 0)
    {
    int validCount = edwSubmitCountNewValid(submit, conn);
    if (validCount == submit->newFiles)
        {
	printf("is uploaded and validated");  // Would be nice to report enrichment/qa status here
	}
    else
        {
	printf("is uploaded and is being validated");
	}
    }
else   /* Upload not complete. */
    {
    long long lastAlive = edwSubmitMaxStartTime(submit, conn);
    struct edwFile *ef = edwFileInProgress(conn, submit->id);
    if (ef != NULL)
        {
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s%s", edwRootDir, ef->edwFileName);
	lastAlive = fileModTime(path);
	}
    long long now = edwNow();
    long long duration = now - lastAlive;


    int oneHourInSeconds = 60*60;
    if (duration < oneHourInSeconds)  
        {
	struct dyString *time = edwFormatDuration(now - submit->startUploadTime);
	printf("has been in progress for %s", time->string);
	}
    else
	{
	struct dyString *time = edwFormatDuration(duration);
	printf("has been stalled for %s", time->string);
	dyStringFree(&time);
	}
    }
}

struct edwValidFile *newReplicatesList(struct edwSubmit *submit, struct sqlConnection *conn)
/* Return list of new files in submission that are supposed to be part of replicated set. */
{
char query[256];
safef(query, sizeof(query), 
    "select v.* from edwFile f,edwValidFile v"
    "  where f.id = v.fileId and f.submitId=%u and v.replicate != ''",
    submit->id);
return edwValidFileLoadByQuery(conn, query);
}

boolean matchInListExceptSelf(struct edwValidFile *v, struct edwValidFile *list)
/* Return TRUE if there's something in list that is a different replica than ours */
{
struct edwValidFile *el;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->format, v->format) && sameString(el->outputType, el->outputType)
        && sameString(el->experiment, v->experiment) && !sameString(el->replicate, v->replicate))
	{
	return TRUE;
	}
    }
return FALSE;
}


static void countPairings(struct sqlConnection *conn, struct edwValidFile *replicatesList, 
			  int *retOldPairs, int *retInnerPairs, int *retUnpaired, 
			  int *retPairCount, int *retPairsDone)
/* Show users files grouped by submission sorted with most recent first. */
{
int oldPairs = 0, innerPairs = 0, unpaired = 0, pairCount = 0, pairsDone = 0;
struct edwValidFile *v;
for (v = replicatesList; v != NULL; v = v->next)
    {
    struct edwValidFile *elderList = edwFindElderReplicates(conn, v);
    boolean isInternallyReplicated = matchInListExceptSelf(v, replicatesList);
    if (isInternallyReplicated)
        ++innerPairs;
    else if (elderList != NULL)
        ++oldPairs;
    else
        ++unpaired;
    struct edwValidFile *ev;
    for (ev = elderList; ev != NULL; ev = ev->next)
        {
	char query[256];
	boolean onePairDone = FALSE;
	safef(query, sizeof(query), 
	    "select count(*) from edwQaPairCorrelation"
	    " where elderFileId=%lld and youngerFileId=%lld"
	    , (long long)ev->fileId, (long long)v->fileId);
	if (sqlQuickNum(conn, query) > 0)
	    onePairDone = TRUE;
	else
	    {
	    safef(query, sizeof(query), 
		"select count(*) from edwQaPairSampleOverlap"
		" where elderFileId=%lld and youngerFileId=%lld"
		, (long long)ev->fileId, (long long)v->fileId);
	    if (sqlQuickNum(conn, query) > 0)
		onePairDone = TRUE;
	    }
	if (onePairDone)
	    ++pairsDone;
	pairCount += 1;
	}
    }
*retOldPairs = oldPairs, *retInnerPairs=innerPairs; 
*retUnpaired = unpaired, *retPairCount = pairCount, *retPairsDone = pairsDone;
}

void showRecentFiles(struct sqlConnection *conn)
/* Show users files grouped by submission sorted with most recent first. */
{
/* Get id for user. */
char query[1024];
safef(query, sizeof(query), "select id from edwUser where email='%s'", userEmail);
int userId = sqlQuickNum(conn, query);
if (userId == 0)
    {
    printf("No user with email %s.  Contact your wrangler to make an account", userEmail);
    return;
    }

/* Select all submissions, most recent first. */
safef(query, sizeof(query), 
    "select * from edwSubmit where userId=%d and (newFiles != 0 or errorMessage is not NULL)"
    " order by id desc", userId);
struct edwSubmit *submit, *submitList = edwSubmitLoadByQuery(conn, query);

for (submit = submitList; submit != NULL; submit = submit->next)
    {
    printf("<H2><A HREF=\"edwWebSubmit?url=%s&monitor=on\">%s</A> ", 
	cgiEncode(submit->url), submit->url);
    printSubmitState(submit, conn);
    printf("</H2>\n");

    /* Print a little summary information about the submission overall before getting down to the
     * file by file info. */
    if (!isEmpty(submit->errorMessage))
        printf("<B>%s</B><BR>\n", submit->errorMessage);
    printf("%d files in validated.txt including %d already in warehouse<BR>\n", 
	submit->fileCount, submit->oldFiles);
    printf("%d of %d new files are uploaded<BR>\n", submit->newFiles, 
	submit->fileCount-submit->oldFiles);
    int newValid = edwSubmitCountNewValid(submit, conn);
    printf("%d of %d uploaded files are validated<BR>\n", newValid, submit->newFiles);
    struct edwValidFile *replicatesList = newReplicatesList(submit, conn);
    int oldPairs = 0, innerPairs = 0, unpaired = 0, pairsToCompute = 0, pairsDone = 0;
    countPairings(conn, replicatesList, &oldPairs, &innerPairs, &unpaired, &pairsToCompute,
	&pairsDone);
    printf("%d of %d replicate comparisons have been computed<BR>\n", pairsDone, pairsToCompute);
    printf("%d replicates still are unpaired<BR>\n", unpaired);

    /* Get and print file-by-file info. */
    char title[256];
    safef(title, sizeof(title), "Files and enrichments for %d new files", submit->newFiles);
    safef(query, sizeof(query),
        "select v.licensePlate ID, v.itemCount items, v.basesInItems bases,"
	"v.format format,truncate(v.mapRatio,2) 'map ratio', "
	       "v.enrichedIn 'enriched in', truncate(e.enrichment,2) X, "
	       "v.experiment experiment, f.submitFileName 'file name' "
	"from edwFile f left join edwValidFile v on f.id = v.fileId "
	               "left join edwQaEnrich e on v.fileId = e.fileId "
		       "left join edwQaEnrichTarget t on e.qaEnrichTargetId = t.id "
	"where f.submitId = %u and f.tags != ''"
 	" and (v.enrichedIn = t.name or v.enrichedIn is NULL or t.name is NULL)"
	" order by f.id desc"
	, submit->id);
    queryIntoTable(conn, query, title);

    safef(query, sizeof(query), 
	"select ev.experiment,ev.outputType 'output type',ev.format,\n"
	"       yv.replicate repA,ev.replicate repB, ev.licensePlate idA, yv.licensePlate idB,\n"
	"       ev.enrichedIn target,p.sampleSampleEnrichment xEnrichment\n"
	"from edwFile e,edwQaPairSampleOverlap p, edwFile y,edwValidFile ev,edwValidFile yv \n"
        "where e.id=p.elderFileId and y.id=p.youngerFileId and ev.fileId=e.id and yv.fileId=y.id\n"
        "      and y.submitId = %u \n"
        "      order by ev.experiment,'output type',format,repA,repB\n"
	, submit->id);
    queryIntoTable(conn, query, "Cross-enrichment between replicates in target areas");

    safef(query, sizeof(query), 
	"select ev.experiment,ev.outputType 'output type',ev.format,\n"
	"       yv.replicate repA,ev.replicate repB, ev.licensePlate idA, yv.licensePlate idB,\n"
	"       ev.enrichedIn target,p.pearsonInEnriched 'R in targets'\n"
	"from edwFile e,edwQaPairCorrelation p, edwFile y,edwValidFile ev,edwValidFile yv \n"
        "where e.id=p.elderFileId and y.id=p.youngerFileId and ev.fileId=e.id and yv.fileId=y.id\n"
        "      and y.submitId = %u \n"
        "      order by ev.experiment,'output type',format,repA,repB\n"
	, submit->id);
    queryIntoTable(conn, query, "Correlation between replicates in target areas");
    }
}

void doMiddle()
/* Write what goes between BODY and /BODY */
{
printf("<H1>ENCODE Data Warehouse Submission Browser</H1>\n");
userEmail = edwGetEmailAndVerify();
printf("<div id=\"userId\">");
if (userEmail == NULL)
    {
    printf("Please sign in.");
    printf("<INPUT TYPE=SUBMIT NAME=\"signIn\" VALUE=\"sign in\" id=\"signin\">");
    }
else
    {
    printf("Hello %s", userEmail);
    edwPrintLogOutButton();
    }
printf("</div>");

if (userEmail != NULL)
    {
    struct sqlConnection *conn = sqlConnect(edwDatabase);
    showRecentFiles(conn);
    sqlDisconnect(&conn);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
edwWebHeaderWithPersona("ENCODE Data Warehouse Submission Browser");
htmEmptyShell(doMiddle, NULL);
edwWebFooterWithPersona();
return 0;
}
