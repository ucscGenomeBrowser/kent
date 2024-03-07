/* post-finish  - tus daemon post-finish hook program. Reads
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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "post-finish - tus daemon post-finish hook program\n"
  "usage:\n"
  "   post-finish < input\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct jsonElement *makeDefaultResponse()
/* Create the default response json with some fields pre-filled */
{
struct hash *defHash = hashNew(8);
struct jsonElement *response = newJsonObject(defHash);
// only the HTTP Response object is important to have by default, the other
// fields will be created as needed
jsonObjectAdd(response, HTTP_NAME, newJsonObject(hashNew(8)));
return response;
}

int postFinish()
/* post-finish hook for tus daemon. Read JSON encoded hook request from
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
        char *userName = (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
        fprintf(stderr, "userName='%s'\n'", userName);
        if (!userName)
            {
            rejectUpload(response, "not logged in");
            exitStatus = 1;
            }
        else
            {
            char *uploadFname = jsonQueryString(req, "", "Event.Upload.MetaData.filename", NULL);
            char *tusFile = jsonQueryString(req, "", "Event.Upload.Storage.Path", NULL);
            if (uploadFname == NULL)
                {
                rejectUpload(response, "No Event.Upload.filename setting");
                exitStatus = -1;
                }
            else if (tusFile == NULL)
                {
                rejectUpload(response, "No Event.Path setting");
                exitStatus = -1;
                }
            else
                {
                char *tusInfo = catTwoStrings(tusFile, ".info");
                char *dataDir = getDataDir(userName);
                char *newFile = catTwoStrings(dataDir, uploadFname);
                fprintf(stderr, "moving %s to %s\n", tusFile, newFile);
                // TODO: check if file exists or not and let user choose to overwrite
                // and re-call this hook, for now just exit if the file exists
                if (fileExists(newFile))
                    {
                    rejectUpload(response, "file '%s' exists already, not overwriting");
                    exitStatus = 1;
                    }
                else
                    {
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
                    copyFile(tusFile, newFile);
                    // the files definitely should not be executable!
                    chmod(newFile, 0666);
                    mustRemove(tusFile);
                    mustRemove(tusInfo);
                    }
                }
            }

        // we've passed all the checks so we can return that we are safe
        if (exitStatus == 0)
            fillOutHttpResponseSuccess(response);
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
return exitStatus;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
return postFinish();
}
