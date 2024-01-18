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
        char *userName = (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
        fprintf(stderr, "userName='%s'\n'", userName);
        if (!userName)
            {
            rejectUpload(response, "not logged in");
            exitStatus = 1;
            }
        else
            {
            long reqFileSize = jsonQueryInt(req, "", "Event.Upload.Size", 0, NULL);
            long currQuota = checkUserQuota(userName);
            long newQuota = currQuota + reqFileSize;
            if (newQuota > MAX_QUOTA)
                {
                rejectUpload(response, "file too large, current stored files is %0.2fgb", currQuota / 1000000000.0);
                exitStatus = 1;
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
return preCreate();
}
