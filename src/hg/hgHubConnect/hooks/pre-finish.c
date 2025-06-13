/* pre-finish  - tus daemon pre-finish hook program. Reads
 * a JSON encoded request to finsh an upload from a tus
 * client and moves a downloaded file to a specific user
 * directory. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "wikiLink.h"
#include "customTrack.h"
#include "userdata.h"
#include "jsonQuery.h"
#include "jsHelper.h"
#include "errCatch.h"
#include "obscure.h"
#include "hooklib.h"
#include "jksql.h"
#include "hdb.h"
#include "hubSpace.h"
#include "hubSpaceKeys.h"
#include "md5.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pre-finish - tus daemon pre-finish hook program\n"
  "usage:\n"
  "   pre-finish < input\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int preFinish()
/* pre-finish hook for tus daemon. Read JSON encoded hook request from
 * stdin and write a JSON encoded hook to stdout. Writing to stderr
 * will be redirected to the tusd log and not seen by the user, so for
 * errors that the user needs to see, they need to be in the JSON response */
{
// TODO: create response object and do all error catching through that
char *reqId = getenv("TUS_ID");
// always return an exit status to the daemon and print to stdout, as
// stdout gets sent by the daemon back to the client
int exitStatus = 0;
struct jsonElement *response = makeDefaultResponse();
if (!(reqId))
    {
    rejectUpload(response, "not a TUS request");
    exitStatus = 1;
    }
else
    {
    char *tusFile = NULL, *tusInfo = NULL;
    struct errCatch *errCatch = errCatchNew(0);
    if (errCatchStart(errCatch))
        {
        // the variables for the row entry for this file, some can be NULL
        char *userName = NULL;
        char *dataDir = NULL, *userDataDir = NULL;
        char *fileName = NULL;
        long long fileSize = 0;
        char *fileType = NULL;
        char *db = NULL;
        char *reqLm = NULL;
        time_t lastModified = 0;
        boolean isHubToolsUpload = FALSE;
        char *parentDir = NULL, *encodedParentDir = NULL;

        struct lineFile *lf = lineFileStdin(FALSE);
        char *request = lineFileReadAll(lf);
        struct jsonElement *req = jsonParse(request);
        fprintf(stderr, "Hook request:\n");
        jsonPrintToFile(req, NULL, stderr, 0);
        char *reqCookie= jsonQueryString(req, "", "Event.HTTPRequest.Header.Cookie[0]", NULL);
        if (reqCookie)
            {
            setenv("HTTP_COOKIE", reqCookie, 0);
            }
        fprintf(stderr, "reqCookie='%s'\n", reqCookie);
        userName = getUserName();
        if (!userName)
            {
            // maybe an apiKey was provided, use that instead to look up the userName
            char *apiKey = jsonQueryString(req, "", "Event.Upload.MetaData.apiKey", NULL);
            userName = userNameForApiKey(apiKey);
            if (!userName)
                errAbort("You are not logged in. Please navigate to My Data -> My Sessions and log in or create an account.");
            }
        fprintf(stderr, "userName='%s'\n'", userName);
        // NOTE: All Upload.MetaData values are strings
        fileName = cgiEncodeFull(jsonQueryString(req, "", "Event.Upload.MetaData.fileName", NULL));
        fileSize = jsonQueryInt(req, "",  "Event.Upload.Size", 0, NULL);
        fileType = jsonQueryString(req, "", "Event.Upload.MetaData.fileType", NULL);
        db = jsonQueryString(req, "", "Event.Upload.MetaData.genome", NULL);
        reqLm = jsonQueryString(req, "", "Event.Upload.MetaData.lastModified", NULL);
        lastModified = sqlLongLong(reqLm) / 1000; // yes Javascript dates are in millis
        parentDir = jsonQueryString(req, "", "Event.Upload.MetaData.parentDir", NULL);
        fprintf(stderr, "parentDir = '%s'\n", parentDir);
        fflush(stderr);
        // strip out plain leading '.' and '/' components
        // middle '.' components are dealt with later
        if (startsWith("./", parentDir) || startsWith("/", parentDir))
            parentDir = skipBeyondDelimit(parentDir, '/');
        fprintf(stderr, "parentDir = '%s'\n", parentDir);
        fflush(stderr);
        tusFile = jsonQueryString(req, "", "Event.Upload.Storage.Path", NULL);
        tusInfo = jsonQueryString(req, "", "Event.Upload.Storage.InfoPath", NULL);
        if (fileName == NULL)
            {
            errAbort("No Event.Upload.fileName setting");
            }
        else if (tusFile == NULL)
            {
            errAbort("No Event.Path setting");
            }
        else
            {
            userDataDir = dataDir = getDataDir(userName);
            // if parentDir provided we are throwing the files in there
            if (parentDir)
                {
                encodedParentDir = encodePath(parentDir);
                if (!endsWith(encodedParentDir, "/"))
                    encodedParentDir = catTwoStrings(encodedParentDir, "/");
                dataDir = catTwoStrings(dataDir, encodedParentDir);
                }
            // the directory may not exist yet
            int oldUmask = 00;
            if (!isDirectory(dataDir))
                {
                fprintf(stderr, "making directory '%s'\n", dataDir);
                // the directory needs to be 777, so ignore umask for now
                oldUmask = umask(0);
                makeDirsOnPath(dataDir);
                // restore umask
                umask(oldUmask);
                }
            mustRemove(tusInfo);
            }

        // we've passed all the checks so we can write a new or updated row
        // to the mysql table and return to the client that we were successful
        if (exitStatus == 0)
            {
            // create a hub for this upload, which can be edited later
            struct hubSpace *row = NULL;
            AllocVar(row);
            row->userName = userName;
            row->fileName = fileName;
            row->fileSize = fileSize;
            row->fileType = fileType;
            row->creationTime = NULL; // automatically handled by mysql
            row->lastModified = sqlUnixTimeToDate(&lastModified, TRUE);
            row->db = db;
            row->location = tusFile;
            row->md5sum = md5HexForFile(row->location);
            row->parentDir = encodedParentDir ? encodedParentDir : "";
            if (!isHubToolsUpload && !(sameString(fileType, "hub.txt")))
                {
                createNewTempHubForUpload(reqId, row, userDataDir, encodedParentDir);
                fprintf(stderr, "added hub.txt and hubSpace row for hub for file: '%s'\n", fileName);
                fflush(stderr);
                }
            // first make the parentDir rows
            makeParentDirRows(row->userName, sqlDateToUnixTime(row->lastModified), row->db, row->parentDir, userDataDir);
            row->parentDir = hubNameFromPath(encodedParentDir);
            addHubSpaceRowForFile(row);
            fprintf(stderr, "added hubSpace row for file '%s'\n", fileName);
            fflush(stderr);
            }
        }
    if (errCatch->gotError)
        {
        rejectUpload(response, errCatch->message->string);
        // must remove the tusd temp files so if the users tries again after a temp error
        // the upload will work
        if (tusFile)
            {
            mustRemove(tusFile);
            mustRemove(tusInfo);
            }
        // TODO: if the first mysql request in createNewTempHubForUpload() works but then
        // either of makeParentDirRows() or addHubSpaceRowForFile() fails, we need to also
        // drop any rows we may have added because the upload didn't full go through
        exitStatus = 1;
        }
    errCatchEnd(errCatch);
    }
// always print a response no matter what
jsonPrintToFile(response, NULL, stdout, 0);
return exitStatus;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
return preFinish();
}
