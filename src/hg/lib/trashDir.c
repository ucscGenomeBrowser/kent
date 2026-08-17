/* trashDir.c - temporary file creation and directory creation in /trash */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


#include "common.h"
#include "hash.h"
#include "hgConfig.h"
#include "portable.h"
#include "trashDir.h"

static boolean hasDotDotComponent(char *path)
/* Return TRUE if any '/'-separated component of path is exactly "..", which is the only
 * way a path can climb back above a directory it appears to be inside of. */
{
char *s = path;
while (s != NULL && s[0] != '\0')
    {
    if (s[0] == '.' && s[1] == '.' && (s[2] == '/' || s[2] == '\0'))
        return TRUE;
    s = strchr(s, '/');
    if (s != NULL)
        s += 1;
    }
return FALSE;
}

static boolean pathIsUnderDir(char *dir, char *path)
/* Return TRUE if path names something underneath dir.  A '/' is required at the directory
 * boundary, so a sibling directory whose name merely starts the same way (trashBackup next
 * to trash) does not match.  ".." below the boundary is refused. */
{
if (isEmpty(dir) || isEmpty(path))
    return FALSE;
int dirLen = strlen(dir);
while (dirLen > 0 && dir[dirLen-1] == '/')
    dirLen -= 1;
if (dirLen == 0)
    return FALSE;
if (strncmp(path, dir, dirLen) != 0 || path[dirLen] != '/' || path[dirLen+1] == '\0')
    return FALSE;
return !hasDotDotComponent(path + dirLen + 1);
}

boolean isTrashPath(char *path)
/* Return TRUE if path names a file inside the trash directory. */
{
return pathIsUnderDir(trashDir(), path);
}

boolean isTrashOrSessionDataPath(char *path)
/* Return TRUE if path is under the trash directory, or under one of the durable session-data
 * directories that trash files are moved to when a session is saved. */
{
return isTrashPath(path) ||
       pathIsUnderDir(cfgOption("sessionDataDir"), path) ||
       pathIsUnderDir(cfgOption("sessionDataDirOld"), path);
}

boolean isServerUserFilePath(char *path)
/* Return TRUE if path is under one of the directories where the server keeps files it made
 * for a user: the trash directory, the session-data directories, or a per-feature data
 * directory such as myVariantsDataDir. */
{
return isTrashOrSessionDataPath(path) ||
       pathIsUnderDir(cfgOption("myVariantsDataDir"), path);
}

boolean isRemoteUrl(char *path)
/* Return TRUE if path is a URL fetched over the network rather than a file name.  Only the
 * three protocols the tree actually fetches count; hasProtocol() in net.c is a test for
 * "://" anywhere in the string, which is too loose to decide anything on. */
{
return startsWith("http://", path) ||
       startsWith("https://", path) ||
       startsWith("ftp://", path);
}

boolean isServerUserFileOrUrl(char *path)
/* Return TRUE if path is either a remote URL or a file the server made for a user.
 *
 * A few cart variables legitimately hold either one: the user gives hgTracks a URL for the
 * multi-region BED or pastes the BED itself, and hgSession loads settings from a URL.  The
 * code then decides which it has by looking for a protocol, and treats anything else as a
 * local file name, so "no protocol" has to mean "one of ours" or the local-file branch reads
 * whatever the cart says. */
{
return isRemoteUrl(path) || isServerUserFilePath(path);
}

static void trashDirFileExt(struct tempName *tn, char *dirName, char *base, char *suffix, boolean addDate)
/*	obtain a trash file name trash/dirName/base*.suffix */
{
static struct hash *dirHash = NULL;
char prefix[128];
char buffer[4096];

if (! dirHash)
	dirHash = newHash(0);

/* already created this directory ? */
if (! hashLookup(dirHash,dirName))
    {
    hashAddInt(dirHash, dirName, 1);	/* remember, been here, done that */
    mkdirTrashDirectory(dirName);
    }

if (addDate)
    {
    safef(buffer, sizeof buffer, "%s/%03d", dirName, dayOfYear());
    dirName = buffer;
    if (! hashLookup(dirHash,dirName))
        {
        hashAddInt(dirHash, dirName, 1);	/* remember, been here, done that */
        mkdirTrashDirectory(dirName);
        }
    }

/* no need to duplicate the _ at the end of base, makeTempName is going
 *	to add _ to the given base, some CGIs pass "base_"
 */
if (endsWith(base,"_"))
    {
    char *t = cloneString(base);
    int len = strlen(t);
    t[len-1] = '\0';	/* remove ending _ */
    safef(prefix, sizeof(prefix), "%s/%s", dirName,t);
    freeMem(t);
    }
else
    safef(prefix, sizeof(prefix), "%s/%s", dirName,base);
makeTempName(tn, prefix, suffix);
}

void trashDirFile(struct tempName *tn, char *dirName, char *base, char *suffix)
/*	obtain a trash file name trash/dirName/base*.suffix */
{
trashDirFileExt(tn, dirName, base, suffix, FALSE);
}

void trashDirDateFile(struct tempName *tn, char *dirName, char *base, char *suffix)
/*	obtain a trash file name trash/dirName.dayOfYear/base*.suffix */
{
trashDirFileExt(tn, dirName, base, suffix, TRUE);
}


boolean trashDirReusableFile(struct tempName *tn, char *dirName, char *base, char *suffix)
/*      obtain a resusable trash file name as trash/dirName/base.suffix
 *      returns TRUE if already exists. */
{
trashDirFile(tn,dirName,base,suffix);
// Don't really want the randomized name.
char *cgiName  = rStringIn("/",tn->forCgi );
char *htmlName = rStringIn("/",tn->forHtml);
if (cgiName == NULL)
    cgiName = rStringIn("\\",tn->forCgi);
assert(cgiName != NULL && htmlName != NULL);

cgiName += 1;
htmlName += 1;
boolean addDot = (*suffix != '.');
safef(cgiName, strlen(cgiName), "%s%s%s", base, (addDot?".":""),suffix);// There is room, since
safef(htmlName,strlen(htmlName),"%s%s%s", base, (addDot?".":""),suffix);// tempName: base_*.suffix

// exists?
return fileExists(tn->forCgi);
}

void copyFileToTrash(char **pFileName, char *dirName, char *base, char *suffix)
/* If *pFileName is not NULL and exists, then create a new file in the
 * given dirName of trash/ with the given base and suffix, copy *pFileName's
 * contents to it, and set *pFileName to the new filename. */
{
if (pFileName != NULL && *pFileName != NULL)
    {
    if (fileExists(*pFileName))
	{
	FILE *fIn = mustOpen(*pFileName, "r");
        struct tempName tn;
	trashDirFile(&tn, dirName, base, suffix);
        char *newFileName = tn.forCgi;
	FILE *fOut = mustOpen(newFileName, "w");
	unsigned char buf[16 * 1024];
	size_t sz;
	while ((sz = fread(buf, sizeof(buf[0]), ArraySize(buf), fIn)) > 0)
	    fwrite(buf, sizeof(buf[0]), sz, fOut);
	fclose(fOut);
	fclose(fIn);
	*pFileName = cloneString(newFileName);
	}
    }
}

