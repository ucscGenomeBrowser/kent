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

#define IMGDIR_OPTION "sessionThumbnail.imgDir"
#define WEBPATH_OPTION "sessionThumbnail.webPath"

void sessionBuildThumbnailFilePaths(struct tempName *thumbnailPath, char *userIdx,
                                  char *encSessionName, char *firstUse)
/* Populate thumbnailPath with paths to the thumbnail image for the specified session.
 * The basename of the thumbnail image is based on several pieces of data on the session:
 * userIdx is a presumed unique ID suitable for being part of a filename,
 * encSessionName is the cgi-encoded session name,
 * and firstUse is the mysql-formatted time string for the session's creation date.
 * Leaks memory via a dyString. */
{
struct dyString *base = dyStringCreate("hgPS_%s_%u_%ld",
    userIdx, hashString(encSessionName), dateToSeconds(firstUse, "%Y-%m-%d %T"));
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

char *sessionThumbnailFilePath(char *userIdx, char *encSessionName,
                               char *firstUse)
/* Returns the path to the thumbnail image of the specified session as seen by CGIs.
 * Result must be freed. */
{
struct tempName thumbnailPath;
sessionBuildThumbnailFilePaths(&thumbnailPath, userIdx, encSessionName, firstUse);
return cloneString(thumbnailPath.forCgi);
}

char *sessionThumbnailFileUri(char *userIdx, char *encSessionName,
                              char *firstUse)
/* Returns the path to the thumbnail image of the specified session as seen by web viewers.
 * Result must be freed. */
{
struct tempName thumbnailPath;
sessionBuildThumbnailFilePaths(&thumbnailPath, userIdx, encSessionName, firstUse);
return cloneString(thumbnailPath.forHtml);
}
