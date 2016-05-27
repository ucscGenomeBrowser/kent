/*
 *
 */

#include "common.h"
//#include "cheapcgi.h"
//#include "jksql.h"
#include "dystring.h"
#include "hash.h"
//#include "pipeline.h"
#include "hgConfig.h"


char *sessionThumbnailFileName(char *userIdx, char *encSessionName,
                               char *firstUse)
/* Return a pointer to a string containing the name of the thumbnail image that
 * would be associated with the given user and session name.  Result must be freed.
 * userIdx is a presumed unique ID suitable for being part of a filename.
 * encSessionName is the cgi-encoded session name, and firstUse is the
 * mysql-formatted time string for the session's creation date */
{
struct dyString *fileName = dyStringCreate("hgPS_%s_%u_%ld.png",
    userIdx, hashString(encSessionName), dateToSeconds(firstUse, "%Y-%m-%d %T"));
return dyStringCannibalize(&fileName);
}

char *sessionThumbnailFilePath(char *userIdx, char *encSessionName,
                               char *firstUse)
/* Returns NULL if the image directory for session thumbnails hasn't been
 * set in hg.conf.  Otherwise, returns the filename and path for the
 * thumbnail of the specified session */
{
char *imgDir = cfgOption("sessionThumbnail.imgDir");
if (imgDir == NULL)
    return NULL;

struct dyString *filePath = dyStringCreate("%s", imgDir);
if (lastChar(imgDir) != '/')
    dyStringAppendC(filePath, '/');
dyStringAppend(filePath, sessionThumbnailFileName(userIdx, encSessionName, firstUse));
return dyStringCannibalize(&filePath);
}

char *sessionThumbnailFileUri(char *userIdx, char *encSessionName,
                              char *firstUse)
/* Returns NULL if the web path to session thumbnails hasn't been
 * defined in hg.conf.  Otherwise, returns the path and filename for the
 * thumbnail of the specified session */
{
char *pathBase = cfgOption("sessionThumbnail.webPath");
if (pathBase == NULL)
    return NULL;

struct dyString *fileUri = dyStringCreate("%s", pathBase);
if (lastChar(pathBase) != '/')
    dyStringAppendC(fileUri, '/');
dyStringAppend(fileUri, sessionThumbnailFileName(userIdx, encSessionName, firstUse));
return dyStringCannibalize(&fileUri);
}
