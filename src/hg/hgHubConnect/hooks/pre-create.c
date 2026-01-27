/* pre-create - tus daemon pre-create hook program. Reads
 * a JSON encoded request to create an upload from a tus
 * client and verifies that the user is logged in and the
 * to be uploaded file will not exceed the quota. Regardless
 * of success, return a JSON encoded response back to the
 * client. */
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
#include "hubSpaceKeys.h"
#include "htmshell.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pre-create - tus daemon pre-create hook program\n"
  "usage:\n"
  "   pre-create < input\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int preCreate()
/* pre-create hook for tus daemon. Read JSON encoded hook request from
 * stdin and write a JSON encoded hook to stdout. Writing to stderr
 * will be redirected to the tusd log and not seen by the user, so for
 * errors that the user needs to see, they need to be in the JSON response */
{
// TODO: create response object and do all error catching through that
char *reqId = getenv("TUS_ID");
char *reqOffsetEnv = getenv("TUS_OFFSET");
char *reqSizeEnv = getenv("TUS_SIZE");
// always return an exit status to the daemon and print to stdout, as
// stdout gets sent by the daemon back to the client
int exitStatus = 0;
struct jsonElement *response = makeDefaultResponse();
if (!(reqId && reqOffsetEnv && reqSizeEnv))
    {
    rejectUpload(response, "not a TUS request");
    exitStatus = 1;
    }
else
    {
    struct errCatch *errCatch = errCatchNew(0);
    if (errCatchStart(errCatch))
        {
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
        char *userName = getUserName();
        if (!userName)
            {
            // maybe an apiKey was provided, use that instead to look up the userName
            char *apiKey = jsonQueryString(req, "", "Event.Upload.MetaData.apiKey", NULL);
            if (!apiKey)
                {
                errAbort("You are not logged in. Please navigate to My Data -> My Sessions and log in or create an account.");
                }
            else
                {
                userName = userNameForApiKey(NULL, apiKey);
                if (!userName)
                    errAbort("You are not logged in. Please navigate to My Data -> My Sessions and log in or create an account.");
                }
            }
        fprintf(stderr, "userName='%s'\n'", userName);
        long reqFileSize = jsonQueryInt(req, "", "Event.Upload.Size", 0, NULL);
        // Check multiple possible metadata keys for filename (Uppy sends 'filename' and 'name' by default,
        // our JS code also sets 'fileName' - try all to handle resumed uploads with old metadata)
        char *reqFileName = jsonQueryString(req, "", "Event.Upload.MetaData.fileName", NULL);
        if (!reqFileName)
            reqFileName = jsonQueryString(req, "", "Event.Upload.MetaData.filename", NULL);
        if (!reqFileName)
            reqFileName = jsonQueryString(req, "", "Event.Upload.MetaData.name", NULL);
        if (!reqFileName)
            {
            errAbort("No filename found in upload metadata (checked fileName, filename, and name)");
            }
        char *reqParentDir = jsonQueryString(req, "", "Event.Upload.MetaData.parentDir", NULL);
        boolean isHubToolsUpload = FALSE;
        char *hubtoolsStr = jsonQueryString(req, "", "Event.Upload.MetaData.hubtools", NULL);
        if (hubtoolsStr)
            isHubToolsUpload = sameString(hubtoolsStr, "TRUE") || sameString(hubtoolsStr, "true");
        // Check for allowOverwrite metadata from JavaScript (for hub.txt overwrites)
        char *allowOverwriteStr = jsonQueryString(req, "", "Event.Upload.MetaData.allowOverwrite", NULL);
        boolean allowOverwrite = (allowOverwriteStr && (sameString(allowOverwriteStr, "TRUE") || sameString(allowOverwriteStr, "true")));
        boolean forceOverwrite = isHubToolsUpload || allowOverwrite;
        long currQuota = checkUserQuota(userName);
        long newQuota = currQuota + reqFileSize;
        long maxQuota = getMaxUserQuota(userName);
        if (newQuota > maxQuota)
            {
            errAbort("File '%s' is too large, need %s free space but current used space is %s out of %s", reqFileName, prettyFileSize(reqFileSize), prettyFileSize(currQuota), prettyFileSize(maxQuota));
            }
        char *reqFileType = jsonQueryString(req, "", "Event.Upload.MetaData.fileType", NULL);
        if (!isFileTypeRecognized(reqFileType))
            {
            errAbort("File type '%s' for file '%s' is not accepted at this time", reqFileType, reqFileName);
            }
        char *reqGenome = jsonQueryString(req, "", "Event.Upload.MetaData.genome", NULL);
        if (!reqGenome)
            {
            errAbort("Genome selection is NULL for file '%s' is invalid. Please choose the correct genome", reqFileName);
            }

        // we've passed all the checks so we can return that we are good to upload the file
        if (exitStatus == 0)
            {
            // set the location of the upload to the location it will ultimately live
            char *location = setUploadPath(userName, reqFileName, reqParentDir, forceOverwrite);
            if (!location)
                {
                errAbort("Error setting upload path in pre-create for file '%s'. This is an"
                        " issue with our server, please email genome-www@soe.ucsc.edu with your"
                        " userName so we can investigate.", reqFileName);
                }
            struct hash *changeObjHash = hashNew(0);
            struct hash *pathObjHash = hashNew(0);
            struct jsonElement *changeObj = newJsonObject(changeObjHash);
            struct jsonElement *pathObj = newJsonObject(pathObjHash);

            jsonObjectAdd(pathObj, "Path", newJsonString(location));
            jsonObjectAdd(changeObj, "Storage", pathObj);
            jsonObjectAdd(changeObj, "ID", newJsonString(makeRandomKey(128)));
            jsonObjectAdd(response, "ChangeFileInfo", changeObj);
            fillOutHttpResponseSuccess(response);
            }
        }
    if (errCatch->gotError)
        {
        rejectUpload(response, errCatch->message->string);
        exitStatus = 1;
        }
    errCatchEnd(errCatch);
    }
// always print a response no matter what
jsonPrintToFile(response, NULL, stdout, 0);
return 0;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
return preCreate();
}
