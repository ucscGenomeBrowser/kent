/* sessionData - functions for moving user data out of trash into permanent storage
 *
 * Copyright (C) 2019-2024 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "customComposite.h"
#include "customTrack.h"
#include "hdb.h"
#include "hgConfig.h"
#include "md5.h"
#include "trashDir.h"
#include "sessionData.h"
#include "quickLift.h"

INLINE boolean isTrashPath(char *path)
/* Return TRUE if path starts with trashDir. */
{
return startsWith(trashDir(), path);
}

static char *sessionDataPathFromTrash(char *trashPath, char *sessionDir)
/* Make a new path from a trash path -- replace "../trash" with safe location. */
{
if (!isTrashPath(trashPath))
    errAbort("sessionDataPathFromTrash: input is non-trash path '%s'", trashPath);
return replaceChars(trashPath, trashDir(), sessionDir);
}

static char *maybeReadlink(char *path)
/* If path is a symbolic link, then alloc & return the link target, otherwise NULL. */
{
char *linkTarget = NULL;
struct stat stat;
if (lstat(path, &stat) != 0)
    // expired file
    return NULL;
if (S_ISLNK(stat.st_mode))
    {
    linkTarget = needMem(stat.st_size + 1);
    int len = readlink(path, linkTarget, stat.st_size);
    if (len < 0)
        errnoAbort("maybeReadlink: lstat says '%s' is symbolic link but readlink failed", path);
    else if (len != stat.st_size)
        errAbort("maybeReadLink: st_size is %d but readlink read %d bytes", (int)stat.st_size, len);
    // readlink doesn't null-terminate
    linkTarget[len] = '\0';
    }
return linkTarget;
}

static void makeDirsForFile(char *path)
/* If path has directories before filename, create them if they don't already exist. */
{
if (path && strchr(path, '/'))
    {
    char pathCopy[strlen(path)+1];
    safecpy(pathCopy, sizeof(pathCopy), path);
    char *p = strrchr(pathCopy, '/');
    *p = '\0';
    makeDirsOnPath(pathCopy);
    }
}

static void moveAndLink(char *oldPath, char *newPath)
/* Make a hard link from newPath to oldPath; unlink oldPath; symlink oldPath to newPath. */
{
if (link(oldPath, newPath) != 0)
    errnoAbort("moveAndLink: link(oldPath='%s', newPath='%s') failed", oldPath, newPath);
if (unlink(oldPath) != 0)
    errnoAbort("moveAndLink: unlink(oldPath='%s') failed", oldPath);
if (symlink(newPath, oldPath) != 0)
    errnoAbort("moveAndLink: symlink(newPath='%s', oldPath='%s') failed", newPath, oldPath);
}

char *sessionDataSaveTrashFile(char *trashPath, char *sessionDir)
/* If trashPath exists and is not already a soft-link to sessionDir, alloc and return a new path in
 * sessionDir; move trashPath to new path and soft-link from trashPath to new path.
 * If trashPath is already a soft-link, return the path that it links to.
 * Return NULL if trashPath does not exist (can happen with expired custom track files). */
{
char *newPath = NULL;
if (fileExists(trashPath))
    {
    char *existingLink = maybeReadlink(trashPath);
    if (existingLink)
        {
        // It may be a multi-directory-level relative symlink created by the trashCleaner scripts
        if (existingLink[0] != '/')
            {
            char trashPathDir[PATH_LEN];
            splitPath(trashPath, trashPathDir, NULL, NULL);
            char fullLinkPath[strlen(trashPathDir) + strlen(existingLink) + 1];
            safef(fullLinkPath, sizeof fullLinkPath, "%s%s", trashPathDir, existingLink);
            newPath = realpath(fullLinkPath, NULL);
            }
        else
            newPath = existingLink;
        }
    else
        {
        newPath = sessionDataPathFromTrash(trashPath, sessionDir);
        if (fileExists(newPath))
            {
            if (unlink(newPath) != 0)
                errnoAbort("sessionDataSaveTrashFile: newPath='%s' already existed but unlink failed",
                           newPath);
            fprintf(stderr, "sessionDataSaveTrashFile: new path '%s' already exists; overwriting", newPath);
            }
        makeDirsForFile(newPath);
        moveAndLink(trashPath, newPath);
        }
    }
return newPath;
}

static char *nextTrashPath(char *string, char *trashDirPrefix)
/* Alloc & return the next file path in string that starts with "../trash/", or NULL. */
{
char *trashPath = NULL;
if (isNotEmpty(string))
    {
    char *pathStart = stringIn(trashDirPrefix, string);
    if (pathStart)
        {
        char *end = pathStart + strlen(trashDirPrefix);
        // Assume our trash paths don't contain spaces, quotes, '+' or '&', and will be followed by
        // a space, quote, '+', '&', or end of string.
        while (*end && !isspace(*end) && *end != '\'' && *end != '"' &&
               *end != '+' && *end != '&')
            end++;
        trashPath = cloneStringZ(pathStart, (end - pathStart));
        }
    }
return trashPath;
}

struct stealthFile
/* Info for detecting stealth files, i.e. files not explicitly named in the cart or track lines,
 * but whose names are just a suffix added to a trash file that is explicitly named. */
    {
    char *contains;  // Trash path contains this string
    char *ending;    // Trash path ends with this
    char *suffix;    // Stealth file is trash path plus this suffix
    };

static struct stealthFile stealthFiles[] = { { "custRgn", ".bed", ".sha1" },
                                             { "hggUp", ".cgb", ".cgm" },
                                           };


static void saveStealthFile(char *trashPath, char *sessionDir)
/* Some trash files have shadow files -- similarly named, but not in any cart var. */
{
int i;
for (i = 0;  i < ArraySize(stealthFiles);  i++)
    {
    struct stealthFile *sf = &stealthFiles[i];
    if (endsWith(trashPath, sf->ending) && stringIn(sf->contains, trashPath))
        {
        char stealthPath[strlen(trashPath) + strlen(sf->suffix) + 1];
        safef(stealthPath, sizeof stealthPath, "%s%s", trashPath, sf->suffix);
        sessionDataSaveTrashFile(stealthPath, sessionDir);
        break;
        }
    }
}

static void saveTrashPaths(char **retString, char *sessionDir, boolean urlEncoded)
/* If sessionDir is provided, then for each instance of "../trash" in *retString, move
 * the trash file into an analogous location in sessionDir and replace the path in retString.
 * If urlEncoded, look for encoded "..%2ftrash" and replace with encoded new path. */
{
if (retString && sessionDir)
    {
    char *trashDirPrefix = urlEncoded ? cgiEncode(trashDir()) : trashDir();
    char *encTrashPath;
    while ((encTrashPath = nextTrashPath(*retString, trashDirPrefix)) != NULL)
        {
        int encLen = strlen(encTrashPath);
        char trashPath[encLen+1];
        if (urlEncoded)
            cgiDecode(encTrashPath, trashPath, encLen);
        else
            safecpy(trashPath, sizeof(trashPath), encTrashPath);
        char *newPath = sessionDataSaveTrashFile(trashPath, sessionDir);
        if (newPath)
            {
            saveStealthFile(trashPath, sessionDir);
            char *encNewPath = urlEncoded ? cgiEncode(newPath) : newPath;
            char *newString = replaceChars(*retString, encTrashPath, encNewPath);
            freez(retString);
            *retString = newString;
            if (urlEncoded)
                freeMem(encNewPath);
            }
        else
            {
            // No new path -- trash file doesn't exist.  Remove from retString to avoid inf loop.
            char *newString = replaceChars(*retString, encTrashPath, "");
            freez(retString);
            *retString = newString;
            }
        freeMem(encTrashPath);
        freeMem(newPath);
        }
    if (urlEncoded)
        freeMem(trashDirPrefix);
    }
}

static char *sessionDataDbTableName(char *tableName, char *sessionDataDbPrefix, char *dbSuffix)
/* Alloc and return a new table name that includes a db derived from sessionDataDbPrefix. */
{
struct dyString *dy = dyStringCreate("%s%s.%s", sessionDataDbPrefix, dbSuffix, tableName);
return dyStringCannibalize(&dy);
}

static char *findTableInSessionDataDbs(struct sqlConnection *conn, char *sessionDataDbPrefix,
                                       char *tableName)
/* Given a tableName (no db. prefix), if it exists in any of the sessionDataPrefix* dbs
 * then return the full db.tableName, otherwise NULL. */
{
int day;
for (day = 1;  day <= 31;  day++)
    {
    char dbDotTable[strlen(sessionDataDbPrefix) + 3 + strlen(tableName) + 1];
    safef(dbDotTable, sizeof dbDotTable, "%s%02d.%s", sessionDataDbPrefix, day, tableName);
    if (sqlTableExists(conn, dbDotTable))
        return cloneString(dbDotTable);
    }
return NULL;
}

static char *saveTrashTable(char *tableName, char *sessionDataDbPrefix, char *dbSuffix)
/* Move trash tableName out of customTrash to a sessionDataDbPrefix database, unless that
 * has been done already.  If table does not exist in either customTrash or customData*,
 * then return NULL; otherwise return the new database.table name. */
{
char *newDbTableName = sessionDataDbTableName(tableName, sessionDataDbPrefix, dbSuffix);
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
if (! sqlTableExists(conn, newDbTableName))
    {
    if (! sqlTableExists(conn, tableName))
        {
        // It's possible that this table was already saved and moved out of customTrash as part
        // of some other saved session.  We don't have a way of leaving a symlink in customTrash.
        newDbTableName = findTableInSessionDataDbs(conn, sessionDataDbPrefix, tableName);
        }
    else
        {
        struct dyString *dy = sqlDyStringCreate("rename table %s to %s", tableName, newDbTableName);
        sqlUpdate(conn, dy->string);
        dyStringFree(&dy);
        }
    }
else if (sqlTableExists(conn, tableName))
    errAbort("saveTrashTable: both %s and %s exist", tableName, newDbTableName);
hFreeConn(&conn);
return newDbTableName;
}

static void replaceColumnValue(struct sqlConnection *conn, char *tableName, char *columnName,
                               char *newVal)
/* Replace all tableName.columnName values with newVal. */
{
struct dyString *dy = sqlDyStringCreate("update %s set %s = '%s'",
                                        tableName, columnName, newVal);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static char *fileColumnNames[] = { "file",     // wiggle tables
                                   "extFile",  // maf tables
                                   "fileName", // vcf tables
                                 };

static void updateSessionDataTablePaths(char *tableName, char *sessionDir)
/* If table contains a trash path and sessionDir is given, then replace
 * the old trash path in the table with the new sessionDir location.
 * NOTE: this supports only wiggle, maf and vcf customTrash tables, and relies
 * on the assumption that each customTrash table refers to only one trash path in all rows.  */
{
if (sessionDir)
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    int ix;
    for (ix = 0;  ix < ArraySize(fileColumnNames);  ix++)
        {
        char *columnName = fileColumnNames[ix];
        if (sqlFieldIndex(conn, tableName, columnName) >= 0)
            {
            struct dyString *dy = sqlDyStringCreate("select %s from %s limit 1",
                                                    columnName, tableName);
            char *trashPath = sqlQuickString(conn, dy->string);
            if (trashPath)
                {
                // For some reason, customTrash tables' filename paths can begin with "./../trash"
                char *actualTrashPath = trashPath;
                if (startsWith("./", trashPath) && isTrashPath(trashPath+2))
                    actualTrashPath = trashPath+2;
                if (isTrashPath(actualTrashPath))
                    {
                    char *newPath = sessionDataSaveTrashFile(actualTrashPath, sessionDir);
                    if (newPath)
                        replaceColumnValue(conn, tableName, columnName, newPath);
                    freeMem(newPath);
                    }
                }
            dyStringFree(&dy);
            freeMem(trashPath);
            break;
            }
        }
    hFreeConn(&conn);
    }
}

static void saveDbTableName(char **retString, char *sessionDataDbPrefix, char *dbSuffix,
                            char *sessionDir)
/* If sessionDataDbPrefix is given then scan for dbTableName setting; if found, move table and
 * update retString with new location.  Also, if table contains a trash path and sessionDir
 * is given, then replace the old trash path in the table with the new sessionDir location. */
{
char *prefix = "dbTableName";
if (sessionDataDbPrefix)
    {
    int prefixLen = strlen(prefix);
    char *setting = stringIn(prefix, *retString);
    if (setting &&
        (setting[prefixLen] == '=' || setting[prefixLen] == ' '))
        {
        char *start = setting + prefixLen + 1;
        char quote = *start;
        if (quote == '\'' || quote == '"')
            start++;
        else
            quote = '\0';
        char *end = start;
        while (*end && ((quote && *end != quote) || (!quote && !isspace(*end))))
            end++;
        if (stringIn(prefix, end))
            errAbort("saveDbTableName: encountered two instances of '%s', expected 0 or 1",
                     prefix);
        char *tableName = cloneStringZ(start, (end - start));
        if (!startsWith(sessionDataDbPrefix, tableName))
            {
            char *newDbTableName = saveTrashTable(tableName, sessionDataDbPrefix, dbSuffix);
            if (newDbTableName)
                {
                updateSessionDataTablePaths(newDbTableName, sessionDir);
                char *newString = replaceChars(*retString, tableName, newDbTableName);
                freez(retString);
                *retString = newString;
                freeMem(newDbTableName);
                }
            }
        freeMem(tableName);
        }
    }
}

static char *newCtTrashFile()
/* Alloc and return the name of a new trash file to hold custom track metadata. */
{
struct tempName tn;
trashDirFile(&tn, "ct", CT_PREFIX, ".ctfile");
return cloneString(tn.forCgi);
}

static char *saveTrackFile(struct cart *cart, char *varName, char *oldFile,
                           char *sessionDataDbPrefix, char *dbSuffix, char *sessionDir)
/* oldFile contains custom track lines or track collection hub trackDb; scan for trashDir paths
 * and/or customTrash tables and move files and tables to safe locations per sessionDataDbPrefix and
 * sessionDir.  If oldFile does not exist or has already been saved, return NULL. */
{
char *newFile = NULL;
if (fileExists(oldFile))
    {
    if (isTrashPath(oldFile))
        {
        struct lineFile *lf = lineFileOpen(oldFile, TRUE);
        if (isNotEmpty(sessionDir))
            newFile = sessionDataPathFromTrash(oldFile, sessionDir);
        else
            newFile = newCtTrashFile();
        if (fileExists(newFile))
            fprintf(stderr, "saveTrackFile: new file '%s' already exists", newFile);
        makeDirsForFile(newFile);
        FILE *newF = mustOpen(newFile, "w");
        char *line;
        while (lineFileNext(lf, &line, NULL))
            {
            char *s = skipLeadingSpaces(line);
            if (*s != '\0' && *s != '#')
                {
                char *trackLine = cloneString(line);
                saveTrashPaths(&trackLine, sessionDir, FALSE);
                saveDbTableName(&trackLine, sessionDataDbPrefix, dbSuffix, sessionDir);
                fprintf(newF, "%s\n", trackLine);
                freeMem(trackLine);
                }
            else
                fprintf(newF, "%s\n", line);
            }
        carefulClose(&newF);
        fprintf(stderr, "Wrote new file %s\n", newFile);
        if (isNotEmpty(sessionDir))
            {
            if (unlink(oldFile) != 0)
                errnoAbort("saveTrackFile: unlink(oldFile='%s') failed", oldFile);
            if (symlink(newFile, oldFile) != 0)
                errnoAbort("saveTrackFile: symlink(newFile='%s', oldFile='%s') failed",
                           newFile, oldFile);
            fprintf(stderr, "symlinked %s to %s\n", oldFile, newFile);
            }
        cartSetString(cart, varName, newFile);
        }
    }
else
    cartRemove(cart, varName);
return newFile;
}

char *sessionDirFromNames(char *sessionDataDir, char *encUserName, char *encSessionName)
/* Alloc and return session data directory:
 * sessionDataDir/2ByteHashOfEncUserName/encUserName/8ByteHashOfEncSessionName
 * 2ByteHashOfEncUserName spreads userName values across up to 256 subdirectories because
 * we have ~15000 distinct namedSessionDb.userName values in 2019.
 * 8ByteHashOfEncSessionName because session names can be very long.  */
{
char *dir = NULL;
if (isNotEmpty(sessionDataDir))
    {
    if (sessionDataDir[0] != '/')
        errAbort("config setting sessionDataDir must be an absolute path (starting with '/')");
    char *userHash = md5HexForString(encUserName);
    userHash[2] = '\0';
    char *sessionHash = md5HexForString(encSessionName);
    sessionHash[8] = '\0';
    struct dyString *dy = dyStringCreate("%s/%s/%s/%s",
                                         sessionDataDir, userHash, encUserName, sessionHash);
    dir = dyStringCannibalize(&dy);
    freeMem(sessionHash);
    }
return dir;
}

INLINE boolean cartVarIsLocalHub(char *cartVar)
/* Return TRUE if cartVar starts with "customComposite-" or "hubQuickLift-". */
{
return startsWith(quickLiftCartName "-", cartVar) || startsWith(customCompositeCartName "-", cartVar);
}

static char *dayOfMonthString()
/* Return a two-character string with the current day of the month [01..31].  Do not free.
 * (Yeah, not [0..30]!  See man 3 localtime.) */
{
static char dayString[16];
time_t now = time(NULL);
struct tm *tm = localtime(&now);
safef(dayString, sizeof dayString, "%02u", tm->tm_mday);
return dayString;
}

void sessionDataSaveSession(struct cart *cart, char *encUserName, char *encSessionName,
                            char *dbSuffix)
/* If hg.conf specifies safe places to store files and/or tables that belong to user sessions,
 * then scan cart for trashDir files and/or customTrash tables, store them in safe locations,
 * and update cart to point to the new locations. */
{
char *sessionDataDbPrefix = cfgOption("sessionDataDbPrefix");
char *sessionDataDir = cfgOption("sessionDataDir");
// Use (URL-encoded) userName and sessionName to make directory hierarchy under sessionDataDir
char *sessionDir = sessionDirFromNames(sessionDataDir, encUserName, encSessionName);
if (isNotEmpty(sessionDataDbPrefix) || isNotEmpty(sessionDir))
    {
    if (isNotEmpty(sessionDataDbPrefix) && dbSuffix == NULL)
        dbSuffix = dayOfMonthString();
    struct slPair *allVars = cartVarsLike(cart, "*");
    struct slPair *var;
    for (var = allVars;  var != NULL;  var = var->next)
        {
        if (startsWith(CT_FILE_VAR_PREFIX, var->name) ||
            cartVarIsLocalHub(var->name) )
            {
            // val is file that contains references to trash files and customTrash db tables;
            // replace with new file containing references to saved files and tables.
            char *oldTrackFile = cloneString(var->val);
            char *newTrackFile = saveTrackFile(cart, var->name, var->val,
                                               sessionDataDbPrefix, dbSuffix, sessionDir);
            if (newTrackFile && cartVarIsLocalHub(var->name))
                cartReplaceHubVars(cart, var->name, oldTrackFile, newTrackFile);
            freeMem(oldTrackFile);
            freeMem(newTrackFile);
            }
        else
            {
            // Regular cart var; save trash paths (possibly encoded) in value, if any are found.
            char *newVal = cloneString(var->val);
            saveTrashPaths(&newVal, sessionDir, FALSE);
            saveTrashPaths(&newVal, sessionDir, TRUE);
            // If the variable would end up with an empty value, leave the old deleted trash file
            // name in place because the CGIs know how to deal with that but may error out if the
            // value is just empty.
            if (newVal != var->val && isNotEmpty(skipLeadingSpaces(newVal)) &&
                differentString(newVal, var->val))
                cartSetString(cart, var->name, newVal);
            freeMem(newVal);
            }
        }
    }
}
