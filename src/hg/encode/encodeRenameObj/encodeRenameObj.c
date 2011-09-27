/* encodeRenameObj - a program to rename mdb obj including many related entries as detailed below: 
 *
 * During pass 0,  it validates for oldObj 
 *  composite metaData .ra, metaDb_$USER table, composite trackDb .ra, trackDb.wgEncode.ra, 
 *  tables, files, symlinks, latest releaseN subdirectory under downloads.
 * And that the newObj name is also unoccupied in all those things as well to prevent name collisions.
 *
 * During pass 1, the oldObj is renamed to newObj for all those things.
 * Errors and warnings are sent both to stderr and to the encodeRenamerLog table field "errors".
 * Changes to the objects are also sent both to stderr and to the encodeRenamerLog table field "changes".
 * It updates the metaDb and trackDb .ra files, creating a new composite-trackDb.new{N}.ra if necessary,
 *  and will also git-add (but not commit) any new composite.new{N}.ra file created.
 * It updates the includer trackDb.wgEncode.ra if needed, maintaining release tags.
 * It uses the encodePatchTdb library to edit trackDb .ra files while preserving indentation and comments.
 * The program exits with 0 if errorCount == 0.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "mdb.h"
#include "hdb.h"
#include "net.h"
#include "../inc/encodePatchTdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeRenameObj - a program to rename mdb obj including related tables, files, and trackDb entries.\n"
  "usage:\n"
  "   encodeRenameObj database oldObj newObj\n"
  "arguments:\n"
  "   database            assembly database\n"
  "   oldObj              old mdb object name\n"
  "   newObj              new mdb object name\n"
  "options:\n"
  "   -help               print out extended information about what metaCheck is doing\n"
  "\n"
  "Example:\n"
  " encodeRenameObj hg19 wgEncodeHaibMethylRrbsAg04449UwstamgrowprotSitesRep1 wgEncodeHaibMethylRrbsAg04449UwSitesRep1\n"
  "You must update your meta data (metaDb_$USER):\n"
  "  make update DBS=hg19\n"
  "(or whatever db you are working on).\n"
  "The program needs to be run from inside your sandbox, and it is better to \n"
  "have a clean sandbox so that other work is not mixed in with your renaming changes.\n"
  );
}

char *release = "alpha";

int errorCount = 0;
int pass = 0;

char *oldObj = NULL;
char *newObj = NULL;

struct sqlConnection *conn = NULL;

unsigned int encodeRenamerLogId = 0;

char metaUser[1024];
boolean hasMetaUserTable = FALSE;

static struct optionSpec options[] = {
   {"help", OPTION_BOOLEAN},
   {NULL, 0},
};

int defaultVerboseLevel = 2;

static void logVaChanges(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) 
    {
    struct dyString *dy = dyStringNew(256);
    dyStringVaPrintf(dy, format, args);
    verbose(defaultVerboseLevel, "%s", dy->string);
    struct dyString *dySql = dyStringNew(256);
    char *escStr = sqlEscapeString(dy->string);
    dyStringPrintf(dySql, "update encodeRenamerLog set changes = concat(changes, '%s') where id = %d", 
	escStr, encodeRenamerLogId);
    sqlUpdate(conn, dySql->string);
    freeMem(escStr);
    dyStringFree(&dy);
    dyStringFree(&dySql);
    }
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void logChange(char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
logVaChanges(format, args);
va_end(args);
}


static void logVaWarn(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) 
    {
    struct dyString *dy = dyStringNew(256);
    dyStringVaPrintf(dy, format, args);
    dyStringAppend(dy,"\n");
    struct dyString *dySql = dyStringNew(256);
    char *escStr = sqlEscapeString(dy->string);
    dyStringPrintf(dySql, "update encodeRenamerLog set errors = concat(errors, '%s') where id = %d", 
	escStr, encodeRenamerLogId);
    sqlUpdate(conn, dySql->string);
    freeMem(escStr);
    dyStringFree(&dy);
    dyStringFree(&dySql);
    }
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void logErrnoWarn(char *format, ...)
/* Prints error message from UNIX errno first, then does rest of warning. */
{
char fbuf[512];
va_list args;
va_start(args, format);
sprintf(fbuf, "%s\n%s", strerror(errno), format);
vaWarn(fbuf, args);
va_end(args);
va_start(args, format);
logVaWarn(fbuf, args);
va_end(args);
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void logWarn(char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
va_start(args, format);
logVaWarn(format, args);
va_end(args);
}

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void logErrAbort(char *format, ...)
/* Issue a abort message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
va_start(args, format);
logVaWarn(format, args);
va_end(args);
noWarnAbort();
}




void updateEncodeRenamerLogRecord(char *state)
/* Update record status */
{
char sql[1024];
safef(sql, sizeof sql, "update encodeRenamerLog set state = '%s' where id = %d", state, encodeRenamerLogId);
sqlUpdate(conn, sql);
}

unsigned int initializeEncodeRenamerLogRecord()
/* Create a record with the information */
{
char sql[1024];
safef(sql, sizeof sql, "insert into encodeRenamerLog (oldObj, newObj, user, `when`, state, errors, changes) values ('%s','%s','%s',now(),'starting','','')", 
    oldObj, newObj, getenv("USER"));
sqlUpdate(conn, sql);
return sqlLastAutoId(conn);
}

void checkEncodeRenamerLogTable(char *src)
/* Check if encode renamer log table exists, create if needed */
{
if (!sqlTableExists(conn, "encodeRenamerLog"))
    {
    char sqlFile[1024];
    safef(sqlFile, sizeof sqlFile, "%s/hg/encode/encodeRenameObj/encodeRenamerLog.sql", src);
    struct dyString *dy = netSlurpUrl(sqlFile);
    sqlMaybeMakeTable(conn, "encodeRenamerLog", dy->string);
    }
}


char *findNewestReleaseDir(char *downDir, char *composite)
/* Find the release dir with highest number N (if any) 
 * because the releaseLatest symlink might not really be the latest in use.
 * return NULL if none found. */
{
char buffer[1024];
char releaseTest[1024];
char *releaseN = NULL;
int i;
for (i=1; i < 10; ++i)
    {
    safef(releaseTest, sizeof releaseTest, "release%d", i);
    safef(buffer, sizeof buffer, "%s/%s/%s", downDir, composite, releaseTest);
    if (fileExists(buffer)) // dir exists?
	{
	releaseN = cloneString(releaseTest);
	}
    else
	break;
    }
return releaseN;
}

void checkMetaTableName(struct mdbObj *mdbObj, char *database)
{

verbose(1, "\n");
verbose(1, "---------------------------------------------------------\n");
verbose(1, "Checking that table specified in metaDb exist in database\n");
verbose(1, "---------------------------------------------------------\n");

char *tableName =  mdbObjFindValue(mdbObj, "tableName");
if (!tableName)
    {
    logWarn("tableName not found in object %s", mdbObj->obj);
    ++errorCount;
    return;
    }

if (differentString(oldObj, tableName))
    {
    logWarn("tableName %s does not match oldObj %s",tableName, oldObj);
    ++errorCount;
    return;
    }

if (!sqlTableExists(conn, tableName))
    {
    logWarn("metaDb table %s not found in database %s",tableName, database);
    ++errorCount;
    return;
    }
else
    verbose(2,"table %s found\n", tableName);

}

void checkMetaFileNameAndDownloads(struct mdbObj *mdbObj, char *downDir, char *composite)
{
verbose(1, "\n");
verbose(1, "-------------------------------------------------------------\n");
verbose(1, "Checking that files specified in metaDb exist in download dir\n");
verbose(1, "-------------------------------------------------------------\n");

char *fileName =  mdbObjFindValue(mdbObj, "fileName");
if (!fileName)
    {
    logWarn("fileName not found in object %s", mdbObj->obj);
    ++errorCount;
    return;
    }

if (!startsWith(oldObj, fileName))
    {
    logWarn("fileName %s does not start with oldObj %s", fileName, oldObj);
    ++errorCount;
    return;
    }
char buffer[10 * 1024];
char *newFileName = NULL;
char newBuffer[10 * 1024];
newFileName = replaceChars(fileName, oldObj, newObj);

int i = 0;
for(i=0; i < 2; ++i)
    {
    if (i==0)
	{
	safef(buffer, sizeof buffer, "%s/%s/%s", downDir, composite, fileName); 
	safef(newBuffer, sizeof newBuffer, "%s/%s/%s", downDir, composite, newFileName); 
	}
    else
	{ /* check for files in newest release dir found */ 
	char *releaseN = findNewestReleaseDir(downDir,composite);
	if (!releaseN)
	    break;
	safef(buffer, sizeof buffer, "%s/%s/%s/%s", downDir, composite, releaseN, fileName); 
	safef(newBuffer, sizeof newBuffer, "%s/%s/%s/%s", downDir, composite, releaseN, newFileName); 
	freeMem(releaseN);
	}

    if (fileExists(newBuffer))
	{
	logWarn("Error: Renaming collision %s already exists in download dir %s", newBuffer, downDir);
	++errorCount;
	return;
	}

    if (!fileExists(buffer))
	{
	logWarn("metaDb file %s not found in download dir %s",buffer, downDir);
	++errorCount;
	return;
	}
    else
	{
	if (pass == 1)
	    {
	    if (rename(buffer, newBuffer) != 0)
		{
		logErrnoWarn("Failed to rename %s to %s\n", buffer, newBuffer);
		++errorCount;
		return;
		}
	    else
		logChange("renamed %s to %s\n", buffer, newBuffer);
	    }
	else
	    verbose(2, "fileExists %s\n", buffer);
	}

    /* Note we can't use fileIndex var for .bai but it's not always present in mdb
     so we'll just hard-wire in the .bai support */
    // check the .bai index file
    if (endsWith(buffer,".bam"))
	{
     
	safecat(buffer, sizeof buffer, ".bai"); 
	safecat(newBuffer, sizeof newBuffer, ".bai"); 

	if (fileExists(newBuffer))
	    {
	    logWarn("Error: Renaming collision %s already exists in download dir %s", newBuffer, downDir);
	    ++errorCount;
	    return;
	    }

	if (!fileExists(buffer))
	    {
	    logWarn("metaDb file %s not found in download dir %s",buffer, downDir);
	    ++errorCount;
	    return;
	    }
	else
	    {
	    if (pass == 1)
		{
		if (rename(buffer, newBuffer) != 0)
		    {
		    logErrnoWarn("Failed to renamed %s to %s\n", buffer, newBuffer);
		    ++errorCount;
		    return;
		    }
		else
		    logChange("renamed %s to %s\n", buffer, newBuffer);
		}
	    else
		verbose(2, "fileExists %s\n", buffer);
	    }


	}

    }

if (pass == 1)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "fileName");
    freeMem(mdbVar->val);
    mdbVar->val = newFileName;
    logChange("renamed mdbObj->fileName from %s to %s\n", oldObj, newObj);

    // update mdb_$USER
    char sql[1024];
    safef(sql, sizeof sql, 
	"update %s set val=concat('%s',substring(val,1+length('%s'))) where obj='%s' and var='fileName'",
    	metaUser, newObj, oldObj, oldObj);
    if (sqlUpdateRows(conn, sql, NULL) == 0)
	{
	logWarn("error: no rows changed for: %s", sql);	
	++errorCount;
	return;
	}
    else
	logChange("%s\n",sql);


    mdbVar = hashFindVal(mdbObj->varHash, "fileIndex");
    if (mdbVar) 
	{
	char *newFileIndex = replaceChars(mdbVar->val, oldObj, newObj);
	freeMem(mdbVar->val);
	mdbVar->val = newFileIndex;
	verbose(2, "renamed mdbObj->fileIndex to %s\n", newFileIndex);
        // It looks like they are dropping fileIndex field from mdb anyway.

    	// update mdb_$USER
	char sql[1024];
    	safef(sql, sizeof sql, 
	    "update %s set val=concat('%s',substring(val,1+length('%s'))) where obj='%s' and var='fileIndex'",
	    metaUser, newObj, oldObj, oldObj);
    	if (sqlUpdateRows(conn, sql, NULL) == 0)
    	    {
	    logWarn("error: no rows changed for: %s", sql);	
    	    ++errorCount;
    	    return;
    	    }
	else
	    logChange("%s\n",sql);

	}

    }

}

struct hash *getMetaDbHash(char *metaDb, struct mdbObj **head)
{
boolean validated = FALSE;
struct mdbObj *mdbObjs = mdbObjsLoadFromFormattedFile(metaDb, &validated), *mdbObj;
struct hash *hash = newHash(10);

for(mdbObj = mdbObjs; mdbObj; mdbObj = mdbObj->next)
    hashAdd(hash, mdbObj->obj, mdbObj);

*head = mdbObjs;
return hash;
}

struct hash *getTrackDbHash(char *trackDb, struct trackDb **head, char *release)
{
struct trackDb *trackObjs = trackDbFromRa(trackDb, release), *trackObj;
struct hash *hash = newHash(10);

for(trackObj = trackObjs; trackObj; trackObj = trackObj->next)
    {
    char *table = trackObj->table;
    if (table == NULL)
        table = trackObj->track;
    hashAdd(hash, table, trackObj);
    }

*head = trackObjs;
return hash;
}

void checkMetaTableInTrackDb(struct hash *trackHash)
{
verbose(1, "\n");
verbose(1, "---------------------------------------------------------\n");
verbose(1, "Checking that table specified in metaDb exists in trackDb\n");
verbose(1, "---------------------------------------------------------\n");

struct trackDb *trackObj = hashFindVal(trackHash, oldObj);
if (!trackObj)
    {
    logWarn("table %s: not found in trackDb",oldObj);
    ++errorCount;
    return;
    }
else
    verbose(2, "tableName %s found in trackDb\n", oldObj);

// this was copied from another removed routine, not sure if it is needed
char *compSetting = trackDbSetting(trackObj, "compositeTrack");
if ((compSetting != NULL) && sameString("on", compSetting))
    {
    logWarn("expected compositeTrack on for table %s in trackDb",oldObj);
    ++errorCount;
    return;
    }

// this was copied from another removed routine, not sure if it is needed
char *viewSetting = trackDbSetting(trackObj, "view");
if (viewSetting != NULL)
    {
    logWarn("expected view setting for table %s in trackDb",oldObj);
    ++errorCount;
    return;
    }

if (hashLookup(trackHash, newObj) != NULL)
    {
    logWarn("Renaming collision: trackDb entry %s already exists", newObj);
    ++errorCount;
    }
}


void checkFilename()
{
char buffer[10 * 1024];
char fileName[10 * 1024];
char oldSymlink[10 * 1024];

char *newFileName = NULL;
char *newSymlink = NULL;

verbose(2, "checking for fileName field in table %s \n", oldObj);

// see if this is even a bbi table 
boolean bbiTable = FALSE;
struct slName *fnames = sqlFieldNames(conn, oldObj);
if ((slCount(fnames) == 1) && (sameString(fnames->name, "fileName")))
    bbiTable = TRUE;
slFreeList(&fnames);
if (!bbiTable)
    return;

safef(buffer, sizeof buffer, "select fileName from %s limit 1", oldObj);
if (sqlQuickQuery(conn, buffer, fileName, sizeof fileName) != NULL)
    {
    while(1)  // loop to catch .bai as well as .bam
	{
	verbose(2,"got table.fileName %s\n", fileName);
	newFileName = replaceChars(fileName, oldObj, newObj);

	if (pass == 0)  // in pass 1 the symlink target has already been renamed, so fopen and fileExists (via stat) will fail.
	    {
	    // file exists
	    FILE *f = fopen(fileName, "r");
	    if (f == NULL)
		{
		logWarn("fileName %s from table %s can't be opened", fileName, oldObj);
		++errorCount;
		return;
		}
	    else
		fclose(f);
	    }

        // check that the filename and object base match
	char *base = strrchr(fileName, '/');
	if (base == NULL)
	    {
	    logWarn("fileName %s in table %s not absolute path", fileName, oldObj);
	    ++errorCount;
	    return;
	    }
	else
	    {
	    base++;
	    char *dot = strchr(base, '.');
	    if (dot == NULL)
		{
		logWarn("fileName %s in table %s does not have suffix", fileName, oldObj);
		++errorCount;
		return;
		}
	    else
		{
		char saveChar = *dot;
		*dot = 0;
		if (!sameString(oldObj, base))
		    {
		    logWarn("fileName %s doesn't match table  %s", base, oldObj);
		    ++errorCount;
		    return;
		    }
		*dot = saveChar;
		}
	    }

        // this file is really a symlink, so check its link target
	ssize_t bufRead = readlink(fileName, oldSymlink, sizeof oldSymlink);
	if (bufRead == -1)
	    {
	    logErrnoWarn("error reading symlink %s", fileName);
	    ++errorCount;
	    return;
	    }
	else
	    {		
	    oldSymlink[bufRead] = 0;  // needs termination.
	    newSymlink = replaceChars(oldSymlink, oldObj, newObj);
	    if (pass == 0)
		{
		if (!fileExists(oldSymlink))
		    {
		    logWarn("symlink target %s does not exist!", oldSymlink);
		    ++errorCount;
		    return;
		    }
		else
		    verbose(2,"got symlink %s\n", oldSymlink);
    		if (fileExists(newSymlink)) 
		    {
		    logWarn("symlink target %s already exists!", newSymlink);
		    ++errorCount;
		    return;
		    }
		}
	    else
		{
    		if (!fileExists(newSymlink)) 
		    {
		    logWarn("symlink target %s does not exist!", newSymlink);
		    ++errorCount;
		    return;
		    }
		}
	    }

        // check that the symlink and object base match
	base = strrchr(oldSymlink, '/');
	if (base == NULL)
	    {
	    logWarn("symlink %s in fileName %s not absolute path",oldSymlink, fileName);
	    ++errorCount;
	    return;
	    }
	else
	    {
	    base++;
	    char *dot = strchr(base, '.');
	    if (dot == NULL)
		{
		logWarn("symlink %s in fileName %s does not have suffix", oldSymlink, fileName);
		++errorCount;
		return;
		}
	    else
		{
		char saveChar = *dot;
		*dot = 0;
		if (!sameString(oldObj, base))
		    {
		    logWarn("symlink %s doesn't match table  %s", base, oldObj);
		    ++errorCount;
		    return;
		    }
		*dot = saveChar;
		}
	    }

	if (pass == 1)
	    {
	    if (unlink(fileName) != 0)
		{
		logErrnoWarn("Failed to remove old symlink %s pointing to %s\n", fileName, oldSymlink);
		++errorCount;
		return;
		}
	    else
		logChange("Removed old symlink %s\n", fileName);
	    if (symlink(newSymlink, newFileName) != 0)
		{
		logErrnoWarn("Failed to make new symlink %s pointing to %s\n", newFileName, newSymlink);
		++errorCount;
		return;
		}
	    else
		logChange("Made new symlink %s pointing to %s\n", newFileName, newSymlink);
	    // check new symlink works
	    FILE *f = fopen(newFileName, "r");
	    if (f == NULL)
		{
		logWarn("new symlink fileName %s from table %s can't be opened", newFileName, oldObj);
		++errorCount;
		return;
		}
	    else
		fclose(f);
	    }

        /* Note we should be able to use the fileIndex for .bai but it's not always present in mdb
         so we'll just hard-wire in the .bai support */
	if (!endsWith(fileName, ".bam"))
	    break;
	safecat(fileName, sizeof(fileName), ".bai");
	}

    if (pass == 1)  // Update the table with the new fileName
	{
	if (endsWith(newFileName, ".bai"))
	    chopSuffix(newFileName);  // trim it off
	logChange("updating fileName field of table %s to %s\n", oldObj, newFileName);
	char query[256];
	safef(query, sizeof query, "update %s set fileName = '%s'", oldObj, newFileName);
	logChange("query: [%s]\n", query);
	sqlUpdate(conn, query);

	freeMem(newFileName);
	freeMem(newSymlink);
	}
    }
}

void checkDbTableAndSymlinks(struct mdbObj *mdbObj)
// check to see if table is represented in the metaDb.ra
// Also check gbdb referencing tables to see if files exist and have
// the correct name
{
  
verbose(1, "\n");
verbose(1, "-------------------------------------------------------\n");
verbose(1, "Checking the table, fileName field and symlinks for %s\n", oldObj);
verbose(1, "-------------------------------------------------------\n");

struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "tableName");
if (mdbVar == NULL)
    {
    logWarn("tableName not found in object %s", oldObj);
    ++errorCount;
    return;
    }

char *tableName = mdbVar->val; 

if (differentString(oldObj, tableName))
    {
    logWarn("tableName %s does not match oldObj %s",tableName, oldObj);
    ++errorCount;
    return;
    }

if (!sqlTableExists(conn, tableName))
    {
    logWarn("metaDb table %s not found.",tableName);
    ++errorCount;
    return;
    }
else
    verbose(2,"table %s found\n", tableName);

if (sqlTableExists(conn, newObj))
    {
    logWarn("Destination table %s already exists, cannot rename table %s to it\n", newObj, oldObj);
    ++errorCount;
    return;
    }

checkFilename();

if (errorCount > 0)
    return;

if (pass == 1)
    {
    sqlRenameTable(conn, oldObj, newObj);
    logChange("Renamed table %s to %s\n", oldObj, newObj);
    if (!sqlTableExists(conn, newObj))
	{
	logWarn("Unexpected error: table with new name %s should exist after rename from %s, but not found.", newObj, oldObj);
	++errorCount;
	return;
	}
    freeMem(mdbVar->val); 
    mdbVar->val = newObj; 
    logChange("renamed mdbObj->tableName from %s to %s\n", oldObj, newObj);

    // update mdb_$USER
    char sql[1024];
    safef(sql, sizeof sql, 
	"update %s set val=concat('%s',substring(val,1+length('%s'))) where obj='%s' and var='tableName'",
    	metaUser, newObj, oldObj, oldObj);
    if (sqlUpdateRows(conn, sql, NULL) == 0)
	{
	logWarn("error: no rows changed for: %s", sql);	
	++errorCount;
	return;
	}
    else
	logChange("%s\n",sql);


    }

}


void checkTrackDbPatch(struct raFile *tdbFile)
/* Check trackDb for patching */
{
verbose(1, "\n");
verbose(1, "-----------------------------------------------------\n");
verbose(1, "Checking that track is in trackDb for renaming-patch \n");
verbose(1, "-----------------------------------------------------\n");

char *warnMsg = NULL;
if (renameTrack(tdbFile, oldObj, newObj, pass, &warnMsg))
    {
    if (pass == 0)
    	verbose(2, "Found track %s in trackDb for renaming\n", oldObj);
    else
    	logChange("Renamed track %s to %s in trackDb patching.\n", oldObj, newObj);
    }
else
    {
    logWarn("%s", warnMsg);
    freeMem(warnMsg);
    ++errorCount;
    }

}


int myMdbObjCmp(const void *va, const void *vb)
/* Compare to sort on label. */
{
const struct mdbObj *a = *((struct mdbObj **)va);
const struct mdbObj *b = *((struct mdbObj **)vb);
//return strcasecmp(a->obj, b->obj);
return strcmp(a->obj, b->obj);
}


void renameObj(struct mdbObj *mdbObj)
/* Rename the mdb obj from old to new */
{
verbose(1, "\n");
verbose(1, "----------------------------------------------\n");
verbose(1, "Renaming mdb->obj from %s to %s\n", oldObj, newObj);
verbose(1, "----------------------------------------------\n");
freeMem(mdbObj->obj);  
mdbObj->obj = cloneString(newObj);
logChange("Renamed mdb->obj %s to %s\n", oldObj, newObj);

// update mdb_$USER
char sql[1024];
safef(sql, sizeof sql, 
    "update %s set obj='%s' where obj='%s'",
    metaUser, newObj, oldObj);
if (sqlUpdateRows(conn, sql, NULL) == 0)
    {
    logWarn("error: no rows changed for: %s", sql);	
    ++errorCount;
    return;
    }
else
    logChange("%s\n",sql);

return;
}



char *findCompositeRa(struct raFile *tdbFile, char *composite, int *pNumTagsFound)
/* Find correct trackDb.ra file for the includer (trackDb.wgEncode.ra) tdbFile
 * and alpha. If problems, give warnings and return NULL.*/
{
char *result = NULL;
verbose(1, "\n");
verbose(1, "--------------------------------------------------------------------\n");
verbose(1, "Checking that track is in trackDb.wgEncode.ra with alpha tag or none\n");
verbose(1, "--------------------------------------------------------------------\n");
char *warnMsg = NULL;
result = findCompositeInIncluder(tdbFile, composite, pNumTagsFound, FALSE, &warnMsg);
if (result)
    {
    verbose(2, "Found <composite>.ra %s in trackDb.wgEncode.ra for composite %s\n", result, composite);
    }
else
    {
    logWarn("%s", warnMsg);
    ++errorCount;
    freeMem(warnMsg);
    }
return result;
}


char *findNextAvailableNewRaName(char *composite, char *trackDb)
/* Find the next available name for a New composite .ra
 * Try .new, .new2, .new3 etc */
{
char base[1024];
char newName[1024];
safecpy(base, sizeof base, trackDb);
char *zTerm = strrchr(base, '/');
if (!zTerm)
    logErrAbort("unexpected error parsing base of trackDb %s, does not have '/'", trackDb);
*(++zTerm) = 0; // keep trailing slash
int n;
for(n=1; n < 100; ++n)
    {
    char countStr[256];
    safef(countStr, sizeof countStr, "%d", n);
    if (n == 1)
	countStr[0] = 0;  // special case leave empty
    safef(newName, sizeof newName, "%s%s.new%s.ra", base, composite, countStr);
    if (!(fileExists(newName) || sameString(newName, trackDb)))
	break;
    }
if (n >= 100)
    logErrAbort("unexpected error: can't find free filename for new composite .ra %s", composite);
return cloneString(newName);
}

char *editCompositeRaList(struct raFile *tdbFile, char *composite, char *trackDb)
/* Remove the alpha tag from the old trackDb.ra file for the includer (trackDb.wgEncode.ra) tdbFile
 * or add beta,public if there were no tags.
 * Clone the file.  Git add the file.  Insert new record with tag alpha into tdbFile. 
 * If problems, give warnings and return NULL.*/
{
char *result = NULL;
verbose(1, "\n");
verbose(1, "---------------------------------------------------------\n");
verbose(1, "Editing trackDb.wgEncode.ra to split into alpha and other\n");
verbose(1, "---------------------------------------------------------\n");
// Eliminate the old alpha tag (or add beta,public if none)
char *warnMsg = NULL;
result = findCompositeInIncluder(tdbFile, composite, NULL, TRUE, &warnMsg);
if (result)
    {
    logChange("Removed alpha tag from includer:  %s in trackDb.wgEncode.ra for composite %s\n", result, composite);
    }
else
    {
    logWarn("%s", warnMsg);
    freeMem(warnMsg);
    ++errorCount;
    return NULL;
    }
// Find a new available composite .ra name 
char *newCompositeRaName = findNextAvailableNewRaName(composite, trackDb);
char *newNameOnly = strrchr(newCompositeRaName, '/');
if (!newNameOnly)
    logErrAbort("unexpected error parsing base of trackDb %s, does not have '/'", newCompositeRaName);
++newNameOnly;

// Insert new composite .ra to Includer with tag alpha
warnMsg = NULL;
boolean success = addCompositeToIncluder(tdbFile, composite, newNameOnly, &warnMsg);
if (success)
    {
    logChange("Inserted new composite .ra %s with alpha tag into includer trackDb.wgEncode.ra for composite %s\n", 
	newNameOnly, composite);
    logChange("New output .ra file: %s\n", newCompositeRaName);
    }
else
    {
    logWarn("%s", warnMsg);
    freeMem(warnMsg);
    ++errorCount;
    return NULL;
    }
return newCompositeRaName;
}


boolean metaCheck(char *src, char *org, char *database, char *composite, char *metaDb, char *downDir)
/* metaCheck 
 * For a list of all the checks and updates, see the description at the top of this file.
 * It returns true if errorCount==0 at end of run.*/
{

errorCount = 0;

struct mdbObj *mdbObjs = NULL;
struct hash *mdbHash = getMetaDbHash(metaDb, &mdbObjs);

if (strlen(newObj) > 64)
    {
    ++errorCount;
    logErrAbort("ERROR: new name %s is too long for mysql table name, length %ld exceeds 64 chars in length.", newObj, strlen(newObj));
    }

// Does object exist in mdb
struct mdbObj *mdbObj = (struct mdbObj *)hashFindVal(mdbHash, oldObj);
if (!mdbObj)
    {
    ++errorCount;
    logErrAbort("ERROR: %s is not in metaDb", oldObj);
    }
else
    verbose(2, "%s found in metaDb\n", oldObj);

// new object should not exist in mdb yet
if (hashLookup(mdbHash, newObj))
    {
    ++errorCount;
    logErrAbort("ERROR: name collision: newObj %s already exists in mdb, cannot rename oldObj %s to it.\n", newObj, oldObj);
    }

char *objType = mdbObjFindValue(mdbObj, "objType");
if (sameString(objType, "table") || sameString(objType, "file"))
    {
    verbose(2, "objType %s found", objType);
    }
else
    {
    ++errorCount;
    logErrAbort("only objType table or file is allowed, found %s for obj %s", objType, oldObj);
    }

// this came from the metaCheck code for checking fileName and downloads.
char *compositeDefined = mdbObjFindValue(mdbObj, "composite");
if (!compositeDefined)
    {
    warn("composite not found in object %s", mdbObj->obj);
    ++errorCount;
    }
if (!sameString(composite,compositeDefined))
    {
    warn("composite %s in mdb does not match composite found by caller %s", compositeDefined, composite);
    ++errorCount;
    }

// do we have meta_$USER table?
safef(metaUser, sizeof metaUser, "metaDb_%s", getenv("USER"));
boolean hasMetaUserTable = sqlTableExists(conn, metaUser);
if (!hasMetaUserTable)
    {
    logErrAbort("missing user mdb table %s", metaUser);
    }

// does the old object exist in meta_$USER table?
char sql[1024];
safef(sql, sizeof sql, "select count(*) from metaDb_%s where obj = '%s'", getenv("USER"), oldObj);
if (sqlQuickNum(conn, sql) == 0)
    {
    logErrAbort("missing oldObj %s in user mdb table %s", oldObj,  metaUser);
    }
    

char trackDb[1024];
/* Load encode composite-includer trackDb.wgEncode.ra */
char trackDbIncluder[1024];
safef(trackDbIncluder, sizeof(trackDbIncluder), "%s/hg/makeDb/trackDb/%s/%s/%s", src, org, database, "trackDb.wgEncode.ra");
struct raFile *includerFile = raFileRead(trackDbIncluder);
/* Find the correct trackDb.ra for the composite */
int numTagsFound = -1;
char *compositeName = findCompositeRa(includerFile, composite, &numTagsFound);
if (!compositeName)
    logErrAbort("unable to find composite .ra for the track in trackDb.wgEncode.ra\n");
// if numTagsFound == 1 then a composite .ra with a single alpha tag exists already, 
//  so no further work required on trackDb.wgEncode.ra

safef(trackDb, sizeof(trackDb), "%s/hg/makeDb/trackDb/%s/%s/%s", src, org, database, compositeName);
if (!fileExists(trackDb))
    logErrAbort("composite .ra file %s not found (for the track %s)\n", trackDb, oldObj);
    
verbose(1,"trackDb: %s (number of tags found: %d)\n", trackDb, numTagsFound);

struct trackDb *trackDbObjs = NULL;
struct hash *trackHash = getTrackDbHash(trackDb, &trackDbObjs, release);

/* Load composite trackDb.ra file to patch. */
struct raFile *tdbFile = raFileRead(trackDb);

if (sameString(oldObj, newObj))
    logErrAbort("ERROR NOT ALLOWED: newObj == oldObj\n");

    //TODO if we go to processing whole lists, 
    //  check that oldObj is not in some unique-hash (error if it is), then add it, 
    //   and then do the same for newObj.



pass = 0;
while (pass < 2)
    {

    if (pass == 0)
	updateEncodeRenamerLogRecord("checking");
    else
	updateEncodeRenamerLogRecord("changing");

    checkMetaFileNameAndDownloads(mdbObj, downDir, composite);
    if (errorCount > 0) break;

    if (pass == 0 && sameString(objType, "table"))
	{
	checkMetaTableInTrackDb(trackHash);
	if (errorCount > 0) break;
	}

    if (sameString(objType, "table"))
	{
	checkDbTableAndSymlinks(mdbObj);
    	if (errorCount > 0) break;
	}

    if (pass==1)
	{
	renameObj(mdbObj);  // rename old to new in meta objects
	slSort(&mdbObjs,&myMdbObjCmp);
	if (errorCount > 0) break;
	}

    if (sameString(objType, "table"))
	{
    	checkTrackDbPatch(tdbFile);
    	if (errorCount > 0) break;
	}

    verbose(1, "\n");

    if (pass == 0)
	verbose(2, "pass0 precheck OK\n");
    else
	verbose(2, "pass1 rename OK\n");

    if (pass == 1)
	updateEncodeRenamerLogRecord("done (except for .ra writes");

    ++pass;
    }

if (errorCount != 0)
    {
    if (pass == 0)
	{
	warn("errorCount=%d, unable to proceed with rename of %s to %s\n", errorCount, oldObj, newObj);
	updateEncodeRenamerLogRecord("error checking");
	}
    else
	{
	warn("errorCount=%d, while renaming of %s to %s\n", errorCount, oldObj, newObj);
	updateEncodeRenamerLogRecord("error changing");
	}
    }
verbose(1, "\n");

if (errorCount == 0)  
    {

    defaultVerboseLevel = 1;     // more important information in this section should be verbose 1
    // Save mdb .ra output file
    mdbObjPrintToFile(mdbObjs, TRUE, metaDb);
    logChange("Wrote %s\n", metaDb);

    // Save composite trackDb .ra output file
    if (sameString(objType, "table"))
	{
	if (numTagsFound != 1)
	    {
	    char *newTrackDb = editCompositeRaList(includerFile, composite, trackDb);
	    if (newTrackDb)
		{  // will cause it to be saved to the new composite .ra with tag alpha
		safecpy(trackDb, sizeof trackDb, newTrackDb);
		}
	    }

    	// Save patched composite trackDb.ra output file
    	writeTdbFile(tdbFile, trackDb); 
	logChange("Wrote %s\n", trackDb);

	if (numTagsFound != 1)
	    {
	    // git add the new name. 
	    char *nameOnly = strrchr(trackDb,'/');
	    if (!nameOnly)
		logErrAbort("unexpected error slash / not found in new trackDb name %s", trackDb);
	    *(nameOnly++) = 0;
	    char cmd[1024];
	    safef(cmd, sizeof cmd, "cd %s; git add %s", trackDb, nameOnly);
	    if (system(cmd) != 0) 
		{
		logErrnoWarn("ERROR: system error running cmd=[%s]", cmd);
		}
	    else
		logChange("*** Ran git add %s\n", nameOnly);
	    *(--nameOnly) = '/';  // restore
	    }
	verbose(1, "\n");
	}

    // Save patched trackDb.wgEncode.ra output file
    if (numTagsFound != 1 && sameString(objType, "table"))
	{
    	writeTdbFile(includerFile, trackDbIncluder);
	logChange("Wrote %s\n", trackDbIncluder);
	}

    updateEncodeRenamerLogRecord("done");

    defaultVerboseLevel = 2;  // restore verbose level
    }

return (errorCount == 0);

}


char *getSrcDir()
/* Get the kent/src/ dir */
{
char *pwd = getCurrentDir();
char *s = strstr(pwd, "/src/");
if (!s)
    s = strstr(pwd, "/src");
if (!s)
    errAbort("unable to locate src/ directory, must run in your sandbox");
s += strlen("/src");
return cloneStringZ(pwd, s - pwd); 
}

char *getComposite(char *src, char * org, char *database)
/* Get the name of the composite track 
 * Can either use the mdb table or just look in files */
{
char target[1024];
char path[1024];
int i = strlen(oldObj);
int l = strlen("wgEncode");
if (i == 0)
    errAbort("oldObj must not be empty");
if (!startsWith("wgEncode", oldObj))
    errAbort("oldObj should start with 'wgEncode': %s", oldObj);
safef(target, sizeof(target), "%s", oldObj);
while(1)
    {
    if (i <= l)
	errAbort("<composite>.ra not found for oldObj: %s", oldObj);
    safef(path, sizeof(path), "%s/hg/makeDb/trackDb/%s/%s/%s.ra", src, org, database, target);
    if (fileExists(path))
	return cloneString(target);
    target[--i] = 0;
    }
return NULL;  // never gets here
}

void encodeRenameObj(char *database)
/* Try to rename oldObj to newObj in mdb, trackDb, tables, and downloads.
 * The need arises if a mistake in meta data daf/ddf is caught too late
 * to be reloaded in the pipeline. */
{
char *composite = NULL;
char metaDb[1024];
char downDir[1024];
char *src = getSrcDir();
char *org = cloneString(hOrganism(database));
org[0] = tolower(org[0]);
composite = getComposite(src, org, database);
safef(metaDb, sizeof(metaDb), "%s/hg/makeDb/trackDb/%s/%s/metaDb/%s/%s.ra", src, org, database, release, composite);
safef(downDir, sizeof(downDir), "/usr/local/apache/htdocs-hgdownload/goldenPath/%s/encodeDCC", database);

verbose(1,"oldObj: %s\nnewObj: %s\ndatabase: %s\ncomposite: %s\nmetaDb: %s\ndownDir: %s\n",
    oldObj, newObj, database, composite, metaDb, downDir);
conn = sqlConnect(database);

checkEncodeRenamerLogTable(src);
encodeRenamerLogId = initializeEncodeRenamerLogRecord();

verbose(2,"encodeRenamerLog id = %d\n", encodeRenamerLogId);

boolean result = metaCheck(src, org, database, composite, metaDb, downDir);
sqlDisconnect(&conn);

exit( result ? 0 : 1);
}


void printHelp()
{
fprintf(stderr,
"encodeRenameObj tries to rename an object whose daf,ddf were incorrect during pipeline load.\n"
"\n"
"It should be run from inside your sandbox, and you should do \n"
"  make update DBS=hg19\n"
" (or whatever db you are working on) so that metaDb_$USER is up-to-date\n"
"\n"
"pass0:\n"
"It first checks that the old object is found of type table or file,\n"
"It checks that the old object exists for mdb, trackDb, table\n"
" and that the corresponding new objects do not exist yet.\n"
"It checks that the tableName and fileName match the object name.\n"
"It checks the downloads dir for fileName field and the newest releaseN directory found.\n"
"It checks if the table is for a bbi file and checks the corresponding symlink(s) in /gbdb/$db/bbi/\n"
"If fileName is a bam file, it checks for .bam.bai index file as well.\n"
"If fileIndex field is found it is renamed too to keep it from going stale.\n"
"It checks the metaDb_$USER table\n"
"And while it checks that in all those things the oldObj name exists, \n"
" it is also check that the newObj name does not exist to prevent name-collisions.\n"
"\n"
"pass1:\n"
"Once all the checks have succeeded with no errors, it begins the next pass\n"
"during which it renames the mdb.{tableName,fileName,fileIndex} fields, and later the mdb object itself.\n"
"The the table if any is renamed, along with the fileName field in it if found, \n"
"and then the symlinks are removed and re-created.\n"
"It updates the metaDb_$USER table.\n"
"Then the composite trackDb.ra is updated in memory.\n"
"Then the composite-includer trackDb.wgEncode.ra is updated in memory.\n"
"If there are still no errors, then the mdb, trackDb, and composite-includer .ra files are written.\n"
"Status is maintained in table encodeRenamerLog.\n"
"Errors  are written to the 'errors'  field as well as stderr.\n"
"Changes are written to the 'changes' field as well as stderr.\n"
"You may wish to use -verbose=2 and save the output of stdout and stderr.\n"
"Program exits with 0 if there were no errors.\n"
"\n"
);
usage();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("help"))
    printHelp();
    
if (argc != 4)
    usage();

oldObj = argv[2];
newObj = argv[3];
encodeRenameObj(argv[1]);
return 0;
}
