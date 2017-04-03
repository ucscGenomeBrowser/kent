/* sessionThumbnail - get the filename, filesystem path, and web URI
 * for the thumbnail image associated with a session.
 *
 * Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hash.h"
#include "trashDir.h"
#include "hgConfig.h"
#include "md5.h"

#define IMGDIR_OPTION "sessionThumbnail.imgDir"
#define WEBPATH_OPTION "sessionThumbnail.webPath"

void sessionBuildThumbnailFilePaths(struct tempName *thumbnailPath, char *userIdentifier,
                                  char *encSessionName, char *firstUse)
/* Populate thumbnailPath with paths to the thumbnail image for the specified session.
 * The basename of the thumbnail image is based on several pieces of data on the session:
 * userIdentifier is an ID for the user that is suitable for being part of a filename and
 * presumed to be unique,
 * encSessionName is the cgi-encoded session name,
 * and firstUse is the mysql-formatted time string for the session's creation date.
 * Leaks memory via a dyString. */
{
struct dyString *base = dyStringCreate("hgPS_%s_%u_%ld",
    userIdentifier, hashString(encSessionName), dateToSeconds(firstUse, "%Y-%m-%d %T"));
char *imgDir = cfgOption(IMGDIR_OPTION);
char *webPath = cfgOption(WEBPATH_OPTION);
if (imgDir != NULL && webPath != NULL)
    {
    makeDirsOnPath(imgDir);
    safef(thumbnailPath->forCgi, sizeof(thumbnailPath->forCgi), "%s%s%s.png", imgDir,
        lastChar(thumbnailPath->forCgi) == '/' ? "" : "/",
        dyStringContents(base));
    safef(thumbnailPath->forHtml, sizeof(thumbnailPath->forCgi), "%s%s%s.png", imgDir,
        lastChar(thumbnailPath->forCgi) == '/' ? "" : "/",
        dyStringContents(base));
    }
else if (imgDir != NULL || webPath != NULL)
    {
    errAbort("Error with session thumbnail path configuration.  "
        "Either both %s and %s should be set in hg.conf or neither.",
        IMGDIR_OPTION, WEBPATH_OPTION);
    }
else
    trashDirReusableFile(thumbnailPath, "hgPS", dyStringContents(base), ".png");
}

char *sessionThumbnailFilePath(char *userIdentifier, char *encSessionName,
                               char *firstUse)
/* Returns the path to the thumbnail image of the specified session as seen by CGIs.
 * Result must be freed. */
{
struct tempName thumbnailPath;
sessionBuildThumbnailFilePaths(&thumbnailPath, userIdentifier, encSessionName, firstUse);
return cloneString(thumbnailPath.forCgi);
}

char *sessionThumbnailFileUri(char *userIdentifier, char *encSessionName,
                              char *firstUse)
/* Returns the path to the thumbnail image of the specified session as seen by web viewers.
 * Result must be freed. */
{
struct tempName thumbnailPath;
sessionBuildThumbnailFilePaths(&thumbnailPath, userIdentifier, encSessionName, firstUse);
return cloneString(thumbnailPath.forHtml);
}


char *sessionThumbnailGetUserIdentifier(char *userName, char *userIdx)
{
/* If the user index is NULL (e.g., because the authentication server is
 *  remote and it couldn't be retrieved), fake a unique identifier with an md5sum.
 *  Be nice to just go straight to the md5sum, but must be backwards compatible with
 *  thumbnails generated with the old identifier strings. */
if (userIdx != NULL)
    return cloneString(userIdx);
else
    return md5HexForString(userName);
}
