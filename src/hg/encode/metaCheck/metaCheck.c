/* metaCheck - a program to validate that tables, files, and trackDb entries exist. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "mdb.h"
#include "portable.h"
#include "../inc/encodePatchTdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaCheck - a program to validate that tables, files, and trackDb entries exist\n"
  "usage:\n"
  "   metaCheck database composite\n"
  "arguments:\n"
  "   database            assembly database\n"
  "   composite           name of track composite\n"
  "options:\n"
  "   -outMdb=file.ra     output cruft-free metaDb ra file\n"
  "   -onlyCompTdb        only check trackDb entries that start with composite\n"
  "   -release            set release state, default alpha\n"
  "   -releaseNum=        specify which release directory to look in (e.g. -releaseNum=2)\n"
  "   -metaDb=            specify a path for the metaDb, by default, this looks in <database>'s metaDb\n"
  "   -trackDb=           specify a path for the trackDb, by default, this is looks in <database>'s trackDb\n"
  "   -downloadDir=       specify a path for the downloads directory\n"
  "   -help               print out extended information about what metaCheck is doing\n"
  );
}

char *outMdb = NULL;
boolean onlyCompTdb = FALSE;
char *release = "alpha";
int releaseNum = 0;

static struct optionSpec options[] = {
   {"outMdb", OPTION_STRING},
   {"help", OPTION_BOOLEAN},
   {"onlyCompTdb", OPTION_BOOLEAN},
   {"release", OPTION_STRING},
   {"metaDb", OPTION_STRING},
   {"trackDb", OPTION_STRING},
   {"downloadDir", OPTION_STRING},
   {"releaseNum", OPTION_INT},
   {NULL, 0},
};

void checkMetaTables(struct mdbObj *mdbObj, char *database)
{
struct sqlConnection *conn = sqlConnect(database);

verbose(1, "----------------------------------------------\n");
verbose(1, "Checking that tables specified in metaDb exist in database\n");
verbose(1, "----------------------------------------------\n");
for(; mdbObj != NULL; mdbObj=mdbObj->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "objType");
    if (mdbVar == NULL)
        {
        warn("objType not found in object %s", mdbObj->obj);
        continue;
        }
    if (differentString(mdbVar->val, "table"))
        continue;

    mdbObj->deleteThis = FALSE;
    mdbVar = hashFindVal(mdbObj->varHash, "tableName");

    if (mdbVar == NULL)
        {
        mdbObj->deleteThis = TRUE;
        warn("tableName not found in object %s", mdbObj->obj);
        continue;
        }

    char *tableName = mdbVar->val;

    if (!startsWith(mdbObj->obj, tableName))
	{
	warn("tableName %s does not start with object name %s", tableName, mdbObj->obj);
	}


    struct mdbVar *atticVar = hashFindVal(mdbObj->varHash, "attic");

    if (!sqlTableExists(conn, tableName))
        {
	if (atticVar)
	    {
	    warn("attic metaDb table %s not found in database %s", tableName, database);
	    continue;
	    }
        mdbObj->deleteThis = TRUE;
        warn("metaDb table %s not found in database %s", tableName, database);
        }
    }
sqlDisconnect(&conn);
}

void checkMetaFiles(struct mdbObj *mdbObj, char *downDir, struct hash *allNames)
{
verbose(1, "----------------------------------------------\n");
verbose(1, "Checking that files specified in metaDb exist in download dir\n");
verbose(1, "----------------------------------------------\n");
for(; mdbObj != NULL; mdbObj=mdbObj->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "objType");
    if (mdbVar == NULL)
        {
        warn("objType not found in object %s", mdbObj->obj);
        continue;
        }
    if (sameString(mdbVar->val, "composite"))  // skip objType composite
        continue;

    mdbObj->deleteThis = FALSE;
    mdbVar = hashFindVal(mdbObj->varHash, "composite");

    if (mdbVar == NULL)
        {
        warn("composite not found in object %s", mdbObj->obj);
        continue;
        }
    // char *composite = mdbVar->val;

    mdbVar = hashFindVal(mdbObj->varHash, "fileName");

    if (mdbVar == NULL)
        {
        mdbObj->deleteThis = TRUE;
        warn("fileName not found in object %s", mdbObj->obj);
        continue;
        }

    char *fileName = mdbVar->val;
    char buffer[10 * 1024];

    struct hash *bamNames = hashNew(8);
    struct slName *list = slNameListFromString(fileName, ','), *el;
    for(el=list; el; el=el->next)
	{
	if (hashLookup(allNames, el->name))
	    {
	    warn("duplicate fileName entry: %s", el->name);
	    }
	else
	    {
	    hashAdd(allNames, el->name, NULL);
	    }
        if (endsWith(el->name,".bam"))
	    {
	    hashAdd(bamNames, el->name, NULL);
	    }
        if (endsWith(el->name,".bam.bai"))
	    {
	    el->name[strlen(el->name)-4] = 0;
	    struct hashEl *hel = hashLookup(bamNames, el->name);
	    el->name[strlen(el->name)] = '.';
	    if (hel == NULL)
		{
		warn(".bam.bai without corresponding .bam: %s", el->name);
		}
	    else
		{
		hel->val = (void *)1;
		}
	    }
	}


    // see if we have to add any .bam.bai to the list 
    for(el=list; el; el=el->next)
	{
        if (endsWith(el->name,".bam"))
	    {
	    struct hashEl *hel = hashLookup(bamNames, el->name);
	    if (hel->val == NULL)
		{  // we have to add a .bam.bai to the list 
		char *bambai = addSuffix(el->name, ".bai");
		warn(".bam.bai not found for corresponding .bam in meta.fileName: %s", el->name);
		slNameAddTail(&list, bambai);		
		if (hashLookup(allNames, bambai))
		    {
		    warn("duplicate fileName entry: %s", bambai);
		    }
		else
		    hashAdd(allNames, bambai, NULL);		
		}
	    }
	}

    // make sure the files are there
    for(el=list; el; el=el->next)
	{
	if (!startsWith(mdbObj->obj, el->name))
	    {
	    warn("fileName %s does not start with object name %s", el->name, mdbObj->obj);
	    }

	safef(buffer, sizeof buffer, "%s/%s", downDir, el->name);

	verbose(2, "checking for fileExists %s\n", buffer);
	if (!fileExists(buffer))
	    {
	    mdbObj->deleteThis = TRUE;
	    warn("metaDb file %s not found in download dir %s",buffer, downDir);
	    }
	}
    }
}

void checkDownloadOrphans(char *downDir, char *composite, struct hash *allDownloadNames)
/* check for orphans in the download directory. */
{
verbose(1, "-----------------------------------------------------------------\n");
verbose(1, "Checking that wgEncode* files in the download dir exist in metaDb\n");
verbose(1, "-----------------------------------------------------------------\n");
char buffer[10 * 1024];
safef(buffer, sizeof buffer, "%s", downDir);
struct slName *list = listDir(buffer, "wgEncode*"), *el;
for(el=list;el;el=el->next)
    {
    if (!hashLookup(allDownloadNames, el->name))
	{
	warn("orphan download file %s/%s not found in metaDb\n", buffer, el->name);
	}
    }
}


void checkBbiOrphans(char *database, char *composite, struct hash *allBbiNames)
/* check for orphans in the download directory. */
{
verbose(1, "-----------------------------------------------------------------\n");
verbose(1, "Checking that <composite>* files in the bbi dir exist in metaDb\n");
verbose(1, "-----------------------------------------------------------------\n");
char buffer[10 * 1024];
char fullPath[10 * 1024];
safef(buffer, sizeof buffer, "/gbdb/%s/bbi", database);
struct slName *list = listDir(buffer, addSuffix(composite,"*")), *el;
for(el=list;el;el=el->next)
    {
    safef(fullPath, sizeof fullPath, "%s/%s", buffer, el->name);
    if (!hashLookup(allBbiNames, fullPath))
	{
	warn("orphan bbi file %s/%s not found in metaDb\n", buffer, el->name);
	}
    }
}


// TODO this might get removed if not used, was copied from encodeRenameObj.c
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

void checkMetaTrackDb(struct mdbObj *mdbObj, struct hash *trackHash)
{
verbose(1, "----------------------------------------------\n");
verbose(1, "Checking that tables specified in metaDb exist in trackDb\n");
verbose(1, "----------------------------------------------\n");
for(; mdbObj != NULL; mdbObj=mdbObj->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObj->varHash, "objType");
    if (mdbVar == NULL)
        {
        warn("objType not found in object %s", mdbObj->obj);
        continue;
        }
    if (differentString(mdbVar->val, "table"))
        continue;

    if (mdbObj->deleteThis)
        continue;

    mdbVar = hashFindVal(mdbObj->varHash, "tableName");
    if (mdbVar == NULL)
        {
        warn("tableName not found in object %s", mdbObj->obj);
        continue;
        }

    char *tableName = mdbVar->val;

    struct mdbVar *atticVar = hashFindVal(mdbObj->varHash, "attic");
    struct mdbVar *statusVar = hashFindVal(mdbObj->varHash, "objStatus");
    char *reason = NULL;
    if (atticVar)
	reason = "attic";
    if (statusVar)
	{
	if (startsWith("renamed", statusVar->val))
	    reason = "renamed";
	if (startsWith("replaced", statusVar->val))
	    reason = "replaced";
	if (startsWith("revoked", statusVar->val))
	    reason = "revoked";
	}

    if (hashLookup(trackHash, tableName))
        {
	if (reason)
	    {
	    warn("%s table %s: should NOT be found in trackDb", reason, tableName);
	    continue;
	    }
        }
    else
        {
	if (reason)
	    { // ok because attic, replaced, revoked, renamed should not be in trackDb
	    continue;
	    }
        warn("table %s: not found in trackDb",tableName);
        }
    }
}

struct mdbObj *dropDeleted(struct mdbObj *head)
{
struct mdbObj *next, *mdbObj, *prev;
for(mdbObj = head; mdbObj != NULL; mdbObj = next)
    {
    next = mdbObj->next;
    if (mdbObj->deleteThis)
        {
        if (prev == NULL)
            head = next;
        else 
            prev->next = next;
        }
    else
        prev = mdbObj;
    }

return head;
}


void checkFilename(struct sqlConnection *conn, char *table, struct hash *allBbiNames)
{
char buffer[10 * 1024];
char fileName[10 * 1024];
char oldSymlink[10 * 1024];

verbose(2, "checking for fileName field in table %s \n", table);

// see if this is even a bbi table 
boolean bbiTable = FALSE;
struct slName *fnames = sqlFieldNames(conn, table);
if ((slCount(fnames) == 1) && (sameString(fnames->name, "fileName")))
    bbiTable = TRUE;
slFreeList(&fnames);
if (!bbiTable)
    return;

sqlSafef(buffer, sizeof buffer, "select fileName from %s limit 1", table);
if (sqlQuickQuery(conn, buffer, fileName, sizeof fileName) != NULL)
    {
    while(1)  // loop to catch .bai as well as .bam
	{


	hashAdd(allBbiNames, fileName, NULL);

	verbose(2,"got table.fileName %s\n", fileName);

	// file exists
	FILE *f = fopen(fileName, "r");
	if (f == NULL)
	    {
	    warn("fileName %s from table %s can't be opened", fileName, table);
	    return;
	    }
	else
	    fclose(f);

        // check that the filename and object base match
	char *base = strrchr(fileName, '/');
	if (base == NULL)
	    {
	    warn("fileName %s in table %s not absolute path", fileName, table);
	    return;
	    }
	else
	    {
	    base++;
	    char *dot = strchr(base, '.');
	    if (dot == NULL)
		{
		warn("fileName %s in table %s does not have suffix", fileName, table);
		return;
		}
	    else
		{
		char saveChar = *dot;
		*dot = 0;
		if (!sameString(table, base))
		    {
		    warn("fileName %s doesn't match table  %s", base, table);
		    return;
		    }
		*dot = saveChar;
		}
	    }

        // this file is really a symlink, so check its link target
	ssize_t bufRead = readlink(fileName, oldSymlink, sizeof oldSymlink);
	if (bufRead == -1)
	    {
	    errnoWarn("error reading symlink %s", fileName);
	    return;
	    }
	else
	    {		
	    oldSymlink[bufRead] = 0;  // needs termination.
	    if (!fileExists(oldSymlink))
		{
		warn("symlink target %s does not exist!", oldSymlink);
		return;
		}
	    else
		verbose(2,"got symlink %s\n", oldSymlink);
	    }

        // check that the symlink and object base match
	base = strrchr(oldSymlink, '/');
	if (base == NULL)
	    {
	    warn("symlink %s in fileName %s not absolute path",oldSymlink, fileName);
	    return;
	    }
	else
	    {
	    base++;
	    char *dot = strchr(base, '.');
	    if (dot == NULL)
		{
		warn("symlink %s in fileName %s does not have suffix", oldSymlink, fileName);
		return;
		}
	    else
		{
		char saveChar = *dot;
		*dot = 0;
		if (!sameString(table, base))
		    {
		    warn("symlink %s doesn't match table  %s", base, table);
		    return;
		    }
		*dot = saveChar;
		}
	    }

        /* Note "fileIndex" for .bai has been made obsolete
         so we'll just hard-wire in the .bai support */
	if (!endsWith(fileName, ".bam"))
	    break;
	safecat(fileName, sizeof(fileName), ".bai");
	}

    }
}

void checkTable(struct sqlConnection *conn, char *table, struct hash *mdbHash, struct hash *allBbiNames)
// check to see if table is represented in the metaDb.ra
// Also check gbdb referencing tables to see if files exist and have
// the correct name
{
verbose(2, "checking table %s\n", table);
if (hashLookup(mdbHash, table) == NULL)
    warn("table %s is not in metaDb", table);

int result = sqlFieldIndex(conn, table, "fileName");

if (result >= 0)
    checkFilename(conn, table, allBbiNames);
}

void checkDbTables(char *database, char *composite, struct hash *mdbHash, struct hash *allBbiNames)
// search the database for tables that begin with composite and call checkTable
{
struct sqlConnection *conn = sqlConnect(database);
char buffer[10 * 1024];

verbose(1, "----------------------------------------------\n");
verbose(1, "Checking that tables starting with composite in db are in metaDb\n (also checks dummy table and the bbi symlink and its target)\n");
verbose(1, "----------------------------------------------\n");
sqlSafef(buffer, sizeof buffer, "show tables like '%s%%'", composite);

struct sqlResult *sr;
sr = sqlGetResult(conn, buffer);
char **row;
struct slName *list = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct slName *el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);

for(; list; list = list->next)
    checkTable(conn, list->name, mdbHash, allBbiNames);
sqlDisconnect(&conn);
}

void checkTrackDb(struct trackDb *trackObjs, char *composite,
    struct hash *mdbHash)
// check through trackDb for tables that begin with composite
// and check to see if they are represented in the metaDb.ra
{
struct trackDb *trackObj;

verbose(1, "----------------------------------------------\n");
verbose(1, "Checking that tables in trackDb are in metaDb \n");
verbose(1, "----------------------------------------------\n");
for(trackObj = trackObjs; trackObj; trackObj = trackObj->next)
    {
    char *table = trackObj->table;
    if (table == NULL)
        table = trackObj->track;

    if (startsWith(composite, table))
        {
        char *compSetting = trackDbSetting(trackObj, "compositeTrack");
        if ((compSetting != NULL) && sameString("on", compSetting))
            continue;

        char *viewSetting = trackDbSetting(trackObj, "view");
        if (viewSetting != NULL)
            continue;

        if (hashLookup(mdbHash, table) == NULL)
            warn("trackDb entry %s isn't in metaDb", table);
        }
    else
        if (! onlyCompTdb)
            warn("trackDb entry %s doesn't start with %s", table, composite);
    }
}


void metaCheck(char *database, char *composite, char *metaDb, char *trackDb, 
    char *downDir)
/* metaCheck - a program to validate that tables, files, and trackDb entries exist. */
{
struct mdbObj *mdbObjs = NULL;
struct hash *mdbHash = getMetaDbHash(metaDb, &mdbObjs);
struct trackDb *trackDbObjs = NULL;
struct hash *trackHash = getTrackDbHash(trackDb, &trackDbObjs, release);

checkMetaTables(mdbObjs, database);
struct hash *allDownloadNames = hashNew(8);
checkMetaFiles(mdbObjs, downDir, allDownloadNames);
checkDownloadOrphans(downDir, composite, allDownloadNames);
checkMetaTrackDb(mdbObjs, trackHash);
trackHash = NULL;

struct hash *allBbiNames = hashNew(8);
checkDbTables(database, composite, mdbHash, allBbiNames);
checkBbiOrphans(database, composite, allBbiNames);
checkTrackDb(trackDbObjs, composite, mdbHash);

if (outMdb)
    {
    mdbObjs = dropDeleted(mdbObjs);
    mdbObjPrintToFile(mdbObjs, TRUE, outMdb);
    }
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

char *findCompositeRa(struct raFile *tdbFile, char *composite, char *release, int *pNumTagsFound)
/* Find correct trackDb.ra file for the includer (trackDb.wgEncode.ra) tdbFile
 * and alpha. If problems, give warnings and return NULL.*/
{
char *result = NULL;
verbose(1, "\n");
verbose(1, "--------------------------------------------------------------------\n");
verbose(1, "Checking that track is in trackDb.wgEncode.ra with %s tag or none\n", release);
verbose(1, "--------------------------------------------------------------------\n");
char *warnMsg = NULL;
result = findCompositeInIncluder(tdbFile, composite, release, pNumTagsFound, FALSE, &warnMsg);
if (result)
    {
    verbose(2, "Found <composite>.ra %s in trackDb.wgEncode.ra for composite %s\n", result, composite);
    }
else
    {
    errAbort("%s", warnMsg);
    }
return result;
}

void replaceTildeWithHome(char **pS)
/* if the path starts with ~ (tilde) char, replace with $HOME */
{
char *s = *pS;
if (s[0] == '~')
    {
    *pS = replaceChars(s, "~", getenv("HOME"));
    }
}



void printHelp()
{
fprintf(stderr,
"metaCheck tries to report on inconsistencies between all the various data stores that ENCODE uses.\n"
"\n"
"The checks are divided into four passes:\n"
"  - checking that metaDb objects of type \"table\" are tables in mySQL\n"
"  - checking that metaDb objects of type \"file\" are files in download dir\n"
"  - checking that metaDb objects of type \"table\" are found in trackDb.ra\n"
"  - checking that all tables in assembly that start with the composite name appear in metaDb\n"
"  - checking that all tables in assembly that start with the composite name and have a field called \"fileName\""
" are links to a file that can be opened\n"
"\n"
);
usage();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("help"))
    {
    printHelp();
    }
    
if (argc != 3) 
    {
    usage();
    }
outMdb = optionVal("outMdb", outMdb);
onlyCompTdb = optionExists("onlyCompTdb");
release = optionVal("release", release);
releaseNum = optionInt("releaseNum", releaseNum);

char *database = argv[1];
char *composite = argv[2];


char defaultMetaDb[1024];
char defaultDownloadDir[1024];
char tempDownloadDir[1024];
char *src = getSrcDir();
char *org = cloneString(hOrganism(database));
org[0] = tolower(org[0]);
/* If user doesn't provide a metaDB, assume the path using the database and composite  */
safef(defaultMetaDb, sizeof(defaultMetaDb), "%s/hg/makeDb/trackDb/%s/%s/metaDb/%s/%s.ra", src, org, database, release, composite);
/* If user doesn't provide a downloadDir, assume the path using the database and composite  */
safef(defaultDownloadDir, sizeof(defaultDownloadDir), "/usr/local/apache/htdocs-hgdownload/goldenPath/%s/encodeDCC/%s", database, composite);
safef(tempDownloadDir, sizeof(tempDownloadDir), "/usr/local/apache/htdocs-hgdownload/goldenPath/%s/encodeDCC/%s", database, composite);
if (releaseNum)
    safef(defaultDownloadDir, sizeof(defaultDownloadDir), "%s/release%d", tempDownloadDir, releaseNum);

/* If user doesn't provide a trackDB, assume the path using the database and composite  */
char defaultTrackDb[1024];
/* Load encode composite-includer trackDb.wgEncode.ra */
char trackDbIncluder[1024];
safef(trackDbIncluder, sizeof(trackDbIncluder), "%s/hg/makeDb/trackDb/%s/%s/%s", src, org, database, "trackDb.wgEncode.ra");
struct raFile *includerFile = raFileRead(trackDbIncluder);
/* Find the correct trackDb.ra for the composite */
int numTagsFound = -1;
char *compositeName = findCompositeRa(includerFile, composite, release, &numTagsFound);
if (!compositeName)
    errAbort("unable to find composite .ra for the track in trackDb.wgEncode.ra\n");
// if numTagsFound == 1 then a composite .ra with a single alpha tag exists already, 
//  so no further work required on trackDb.wgEncode.ra
safef(defaultTrackDb, sizeof(defaultTrackDb), "%s/hg/makeDb/trackDb/%s/%s/%s", src, org, database, compositeName);
    
verbose(1,"database: %s\ncomposite: %s\nrelease %s\ndefault trackDb: %s\ndefault metaDb: %s\ndefault downloadDir: %s\n",
    database, composite, release, defaultTrackDb, defaultMetaDb, defaultDownloadDir);

char *metaDb = optionVal("metaDb",defaultMetaDb);
replaceTildeWithHome(&metaDb);
if (!fileExists(metaDb))
    errAbort("metaDb %s does not exist.", metaDb);

char *trackDb = optionVal("trackDb",defaultTrackDb);
replaceTildeWithHome(&trackDb);
if (!fileExists(trackDb))
    errAbort("trackDb %s does not exist.", trackDb);

char *downloadDir = optionVal("downloadDir",defaultDownloadDir);
replaceTildeWithHome(&downloadDir);
if (!fileExists(downloadDir))
    errAbort("downloadDir %s does not exist.", downloadDir);


printf("metaDb = %s\n trackDb = %s\n downloadDir = %s\n",metaDb,trackDb,downloadDir);

metaCheck(database, composite, metaDb, trackDb, downloadDir);

return 0;
}
