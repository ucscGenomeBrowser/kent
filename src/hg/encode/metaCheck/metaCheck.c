/* metaCheck - a program to validate that tables, files, and trackDb entries exist. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "mdb.h"

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
  "   -metaDb=            specify a path for the metaDb, by default, this looks in <database>'s metaDb.\n"
  "   -trackDb=           specify a path for the trackDb, by default, this is looks in <database>'s trackDb.\n"
  "   -downloadDir=       specify a path for the downloads directory\n"
  "   -help               print out extended information about what metaCheck is doing\n"
  );
}

char *outMdb = NULL;
boolean onlyCompTdb = FALSE;
char *release = "alpha";

static struct optionSpec options[] = {
   {"outMdb", OPTION_STRING},
   {"help", OPTION_BOOLEAN},
   {"onlyCompTdb", OPTION_BOOLEAN},
   {"release", OPTION_STRING},
   {"metaDb", OPTION_STRING},
   {"trackDb", OPTION_STRING},
   {"downloadDir", OPTION_STRING},
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

    if (!sqlTableExists(conn, tableName))
        {
        mdbObj->deleteThis = TRUE;
        warn("metaDb table %s not found in database %s",tableName, database);
        }
    }
sqlDisconnect(&conn);
}

void checkMetaFiles(struct mdbObj *mdbObj, char *downDir)
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
    //if (differentString(mdbVar->val, "file"))
        //continue;

    mdbObj->deleteThis = FALSE;
    mdbVar = hashFindVal(mdbObj->varHash, "composite");

    if (mdbVar == NULL)
        {
        warn("composite not found in object %s", mdbObj->obj);
        continue;
        }
    char *composite = mdbVar->val;

    mdbVar = hashFindVal(mdbObj->varHash, "fileName");

    if (mdbVar == NULL)
        {
        mdbObj->deleteThis = TRUE;
        warn("fileName not found in object %s", mdbObj->obj);
        continue;
        }

    char *fileName = mdbVar->val;
    char buffer[10 * 1024];

    safef(buffer, sizeof buffer, "%s/%s/%s", downDir, composite, fileName);

    verbose(2, "checking for fileExists %s\n", buffer);
    if (!fileExists(buffer))
        {
        mdbObj->deleteThis = TRUE;
        warn("metaDb file %s not found in download dir %s",buffer, 
            downDir);
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

    if (hashLookup(trackHash, tableName) == NULL)
        {
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

void checkFilename(struct sqlConnection *conn, char *table)
{
char buffer[10 * 1024];
char fileName[10 * 1024];

verbose(2, "checking fileName for table %s \n", table);
safef(buffer, sizeof buffer, "select fileName from %s limit 1", table);
if (sqlQuickQuery(conn, buffer, fileName, sizeof fileName) != NULL)
    {
    verbose(2,"got fileName %s\n", fileName);

    FILE *f = fopen(fileName, "r");
    if (f == NULL)
        warn("fileName %s from table %s can't be opened", fileName, table);
    else
        fclose(f);

    char *base = strrchr(fileName, '/');
    if (base == NULL)
        warn("fileName %s in table %s not absolute path", fileName, table);
    else
        {
        base++;
        char *dot = strchr(base, '.');
        if (dot == NULL)
            warn("fileName %s in table %s does not have suffix", fileName, table);
        else
            {
            *dot = 0;
            if (!sameString(table, base))
                warn("fileName %s doesn't match table  %s", base, table);
            }
        }
    }
}

void checkTable(struct sqlConnection *conn, char *table, struct hash *mdbHash)
// check to see if table is represented in the metaDb.ra
// Also check gbdb referencing tables to see if files exist and have
// the correct name
{
verbose(2, "checking table %s\n", table);
if (hashLookup(mdbHash, table) == NULL)
    warn("table %s is not in metaDb", table);

int result = sqlFieldIndex(conn, table, "fileName");

if (result >= 0)
    checkFilename(conn, table);
}

void checkDbTables(char *database, char *composite, struct hash *mdbHash)
// search the database for tables that begin with composite and call checkTable
{
struct sqlConnection *conn = sqlConnect(database);
char buffer[10 * 1024];

verbose(1, "----------------------------------------------\n");
verbose(1, "Checking that tables starting with composite in db are in metaDb\n");
verbose(1, "----------------------------------------------\n");
safef(buffer, sizeof buffer, "show tables like '%s%%'", composite);

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
    checkTable(conn, list->name, mdbHash);
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

void getDbPath(char *path, char *database, char *composite, char *type){
    strcat(path,"/kent/src/hg/makeDb/trackDb/");
    if (strncmp(database,"mm", 2) == 0){
        strcat(path,"mouse/");
    } else {
        strcat(path,"human/");
    }
    strcat(path,database);
    strcat(path,"/");
    if (strcmp(type,"metaDb") == 0){
        strcat(path,"metaDb/alpha/");
        printf("%s\n",path);
    }
    strcat(path,composite);
    strcat(path,".ra");
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
checkMetaFiles(mdbObjs, downDir);
checkMetaTrackDb(mdbObjs, trackHash);
trackHash = NULL;

checkDbTables(database, composite, mdbHash);
checkTrackDb(trackDbObjs, composite, mdbHash);

if (outMdb)
    {
    mdbObjs = dropDeleted(mdbObjs);
    mdbObjPrintToFile(mdbObjs, TRUE, outMdb);
    }
}

void printHelp()
{
fprintf(stderr,
"Metacheck tries to report on inconsistencies between all the various data stores that ENCODE uses.\n"
"\n"
"The checks are divided into four passes:\n"
"  - checking that metaDb objects of type \"table\" are tables in mySQL\n"
"  - checking that metaDb objects of type \"file\" are files in download dir\n"
"  - checking that metaDb objects of type \"table\" are found in trackDb.ra\n"
"  - checking that all tables in assembly that start with \"composite\" appear in metaDb\n"
"  - checking that all tables in assembly that start with \"composite\" and have a     field called \"fileName\" are links to a file that can be opened\n"

"\n"
);
usage();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("help")){
    printHelp();
}
    
if (argc != 3) {
    usage();
}
outMdb = optionVal("outMdb", outMdb);
onlyCompTdb = optionExists("onlyCompTdb");
release = optionVal("release", release);

char *database = argv[1];
char *composite = argv[2];

char *home = strdup(getenv("HOME"));

/* If user doesn't provide a metaDB, assume the path using the database and composite  */
char *metaDb = malloc(256);
if(!optionExists("metaDb")){
    strcpy(metaDb,home);
    getDbPath(metaDb,database,composite,"metaDb");
/* Else get the one they provided */
} else {
    strcpy(metaDb,optionVal("metaDb",metaDb));
    if(strchr(metaDb,'~')!=NULL){
        char *dup = malloc(256);
        strcpy(dup,home);
        strcat(dup,(strchr(metaDb,'~')+1)); //replace '~' with $HOME
        strcpy(metaDb,dup);
        free(dup);
    }
}

/* If user doesn't provide a trackDB, assume the path using the database and composite  */
char *trackDb = malloc(256);
if(!optionExists("trackDb")){
    strcpy(trackDb,home);
    getDbPath(trackDb,database,composite,"trackDb");
} else {
    strcpy(trackDb,optionVal("trackDb",trackDb));
    if(strchr(trackDb,'~')!=NULL){
        char *dup = malloc(256);
        strcpy(dup,home);
        strcat(dup,(strchr(trackDb,'~')+1));//replace '~' with $HOME
        strcpy(trackDb,dup);
        free(dup);
    }
}

char *downloadDir = malloc(256);
if(!optionExists("downloadDir")){
    strcpy(downloadDir,"/hive/groups/encode/dcc/analysis/ftp/pipeline/");
    strcat(downloadDir, database);
    strcat(downloadDir, "/");
//    strcat(dup,(strchr(downloadDir, I need to add the assembly here
} else {
    downloadDir = optionVal("downloadDir",downloadDir);
    if(strchr(downloadDir,'~')!=NULL){
        char *dup = malloc(256);
        strcpy(dup,home);
        strcat(dup,(strchr(downloadDir,'~')+1));
        strcpy(downloadDir,dup);
        free(dup);
    }
}


printf("metaDb = %s\n trackDb = %s\n downloadDir = %s\n",metaDb,trackDb,downloadDir);
metaCheck(database, composite, metaDb, trackDb, downloadDir);
free(home);
free(trackDb);
free(downloadDir);
free(metaDb);
return 0;
}
