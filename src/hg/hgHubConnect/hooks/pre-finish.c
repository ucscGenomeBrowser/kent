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
            userName = hubSpaceUserNameForApiKey(NULL, apiKey);
            if (!userName)
                errAbort("You are not logged in. Please navigate to My Data -> My Sessions and log in or create an account.");
            }
        fprintf(stderr, "userName='%s'\n", userName);
        // NOTE: All Upload.MetaData values are strings
        // Check multiple possible metadata keys for filename (Uppy sends 'filename' and 'name' by default,
        // our JS code also sets 'fileName' - try all to handle resumed uploads with old metadata)
        char *rawFileName = jsonQueryString(req, "", "Event.Upload.MetaData.fileName", NULL);
        if (!rawFileName)
            rawFileName = jsonQueryString(req, "", "Event.Upload.MetaData.filename", NULL);
        if (!rawFileName)
            rawFileName = jsonQueryString(req, "", "Event.Upload.MetaData.name", NULL);
        fileName = rawFileName ? cgiEncodeFull(rawFileName) : NULL;
        fileSize = jsonQueryInt(req, "",  "Event.Upload.Size", 0, NULL);
        fileType = jsonQueryString(req, "", "Event.Upload.MetaData.fileType", NULL);
        db = jsonQueryString(req, "", "Event.Upload.MetaData.genome", NULL);
        // Blocks newline injection into the synthesized hub.txt.
        // The allowed character class must match sanitizeGenomeName() in
        // src/hg/js/hgMyData.js.
        if (db && db[0])
            {
            char *p;
            for (p = db; *p; p++)
                if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == '.'))
                    errAbort("Invalid genome name '%s': only letters, digits, '.', '_' and '-' are allowed", db);
            }
        reqLm = jsonQueryString(req, "", "Event.Upload.MetaData.lastModified", NULL);
        if (reqLm)
            lastModified = sqlLongLong(reqLm) / 1000; // yes Javascript dates are in millis
        else
            lastModified = time(NULL); // fallback to current time if not provided
        parentDir = jsonQueryString(req, "", "Event.Upload.MetaData.parentDir", NULL);
        // must match what pre-create did to this value, or we build a different path
        parentDir = normalizeParentDir(parentDir);
        fprintf(stderr, "parentDir = '%s'\n", parentDir ? parentDir : "(null)");
        // strip out plain leading '.' and '/' components
        // middle '.' components are dealt with later
        if (parentDir && (startsWith("./", parentDir) || startsWith("/", parentDir)))
            parentDir = skipBeyondDelimit(parentDir, '/');
        // check the value we are about to build paths from, after the strip. pre-create
        // applies these same checks, but an upload created before they existed can
        // still finish here, and a hub name of "", ".", ".." or "/" ends up pointing
        // outside the hub
        if (isEmpty(parentDir))
            errAbort("No hub name for this upload, please give the hub a name");
        if (!isValidParentDir(parentDir))
            errAbort("Hub name '%s' can only contain letters, numbers, periods and "
                    "underscores, in '/' separated components. Please rename the hub.",
                    parentDir);
        tusFile = jsonQueryString(req, "", "Event.Upload.Storage.Path", NULL);
        tusInfo = jsonQueryString(req, "", "Event.Upload.Storage.InfoPath", NULL);
        if (fileName == NULL)
            {
            errAbort("No filename found in upload metadata (checked fileName, filename, and name)");
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
            // resolve any symlinks in the path, because tusd sets the path as
            // the command line specified dataDir + pre-create's ChangeFileInfo
            // this was leading to a bug where the uploaded file had the symlinked
            // path, but the containing hub.txt and directory row had the realpath,
            // which was causing confusion in the UI code
            char *canonicalPath = realpath(tusFile, NULL);
            if (canonicalPath != NULL)
                row->location = canonicalPath;
            else
                {
                // all upload data should have been received and thus the realpath
                // should not fail, but just in case, put something valid here
                row->location = tusFile;
                }
            row->md5sum = md5HexForFile(row->location);
            row->parentDir = encodedParentDir ? encodedParentDir : "";
            // Derive hubType server-side; never trust the client's hubType.
            // A 2bit always promotes its hub to assembly. Otherwise inherit
            // the existing hub's type, defaulting to trackHub.
            // both lookups below are about the hub as a whole, whose row and hub.txt
            // live at the top level, so use the hub component of parentDir
            char *parentDirForCheck = encodedParentDir ? hubRootFromParentDir(encodedParentDir) : "";
            char *hubDir = encodedParentDir ?
                hubPathFromParentDir(encodedParentDir, userDataDir) : NULL;
            if (sameOk(fileType, "2bit"))
                row->hubType = "assemblyHub";
            else
                {
                char *existingType = existingHubTypeForDir(userName, parentDirForCheck);
                row->hubType = existingType ? existingType : "trackHub";
                }
            char *batchHasHubTxtStr = jsonQueryString(req, "", "Event.Upload.MetaData.batchHasHubTxt", NULL);
            boolean batchHasHubTxt = sameOk(batchHasHubTxtStr, "true");
            boolean userOwnNamedHubTxt = userHasOwnNamedHubTxtInDir(userName, parentDirForCheck, hubDir);
            boolean userAuth = batchHasHubTxt || userOwnNamedHubTxt;
            boolean isHubTxt = sameOk(fileType, "hub.txt");
            boolean isTwoBit = sameOk(fileType, "2bit");

            // Serialize hub.txt read-modify-write across parallel pre-finish
            // processes for the same hub. flock is held for the entire
            // decision + action so writeHubText's fileExists check and the
            // upgrade's read-rewrite are atomic with respect to siblings.
            // Lock the directory the hub.txt is in, so uploads into different
            // subdirectories of one hub still serialize against each other.
            int hubLockFd = -1;
            if (hubDir)
                hubLockFd = lockHubDir(hubDir);
            if (!isHubToolsUpload && !isHubTxt)
                {
                if (!userAuth)
                    {
                    if (isTwoBit)
                        {
                        // createNewTempHubForUpload is a no-op when the hub.txt and its
                        // row are already there, and it backfills the row when they are not
                        createNewTempHubForUpload(reqId, row, userDataDir);
                        upgradeExistingHubToAssembly(row, userDataDir);
                        }
                    else
                        createNewTempHubForUpload(reqId, row, userDataDir);
                    }
                else if (isTwoBit)
                    {
                    // user's hub.txt is authoritative; just flip rows to assemblyHub.
                    upgradeExistingHubToAssembly(row, userDataDir);
                    }
                }
            // still under the hub lock: makeParentDirRows checks for a row and then
            // inserts it, so two uploads to one hub would otherwise both insert the
            // same directory row
            // first make the parentDir rows
            // the directory rows carry the upload's own timestamp. row->lastModified
            // holds it as a GMT clock string, which sqlDateToUnixTime would read as
            // local time, so pass the seconds directly
            makeParentDirRows(row->userName, lastModified, row->db, row->parentDir, userDataDir, row->hubType);
            row->parentDir = encodedParentDir ? hubLeafFromPath(encodedParentDir) : "";
            addHubSpaceRowForFile(row);
            unlockHubDir(hubLockFd);
            fprintf(stderr, "added hubSpace row for file '%s'\n", fileName);
            fflush(stderr);

            // Send the client the hub as it now stands, so it can show the new file,
            // its directories and the hub.txt. The upload has already succeeded at
            // this point, so catch errors here rather than let them reject it: the
            // worst case is a client that shows nothing new until the page is reloaded
            struct errCatch *respCatch = errCatchNew(0);
            if (errCatchStart(respCatch))
                {
                if (encodedParentDir)
                    {
                    char *hubName = hubRootFromParentDir(encodedParentDir);
                    setUploadedFileList(response, userName, listFilesInHubDir(userName, hubName));
                    freeMem(hubName);
                    }
                }
            errCatchEnd(respCatch);
            if (respCatch->gotError)
                fprintf(stderr, "could not list hub for response: %s\n",
                        respCatch->message->string);
            errCatchFree(&respCatch);
            }
        }
    // pop the handlers before handling the error, the cleanup below can itself
    // errAbort, which would longjmp back into this same block
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        // App-level reject: exit 0 + RejectUpload=true is the tusd protocol for
        // forwarding HTTPResponse verbatim. Non-zero gets wrapped.
        rejectUpload(response, "%s", errCatch->message->string);
        // clear the partial upload so the user can try again. pre-create hands tusd a
        // ChangeFileInfo, so tusFile is the file in the user's directory, not a temp
        // copy, and tusInfo is tusd's .info alongside it. remove() rather than
        // mustRemove(): this is the error path, and an abort here exits before the
        // response is printed, leaving the client with a bare 500 instead of the
        // reason the upload failed
        if (tusFile && remove(tusFile) != 0)
            fprintf(stderr, "could not remove '%s': %s\n", tusFile, strerror(errno));
        if (tusInfo && remove(tusInfo) != 0)
            fprintf(stderr, "could not remove '%s': %s\n", tusInfo, strerror(errno));
        // TODO: if the first mysql request in createNewTempHubForUpload() works but then
        // either of makeParentDirRows() or addHubSpaceRowForFile() fails, we need to also
        // drop any rows we may have added because the upload didn't full go through
        exitStatus = 0;
        }
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
