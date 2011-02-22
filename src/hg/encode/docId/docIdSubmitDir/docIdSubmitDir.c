/* docIdSubmitDir - put ENCODE submission dir into docIdSub table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "mdb.h"
#include "ra.h"
#include "docId.h"
#include "cheapcgi.h"
#include "portable.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "docIdSubmitDir - put ENCODE submission dir into docIdSub table\n"
  "usage:\n"
  "   docIdSubmitDir database submitDir docIdDir\n"
  "options:\n"
  "   -table=docIdSub  specify table to use (default docIdSub)\n"
  "   -editInput       edit the load.ra and mdb.txt files\n"
  );
}


char *docIdTable = DEFAULT_DOCID_TABLE;
boolean editInput = FALSE;
char *toBeDecided = "not yet assigned";

static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"editInput", OPTION_BOOLEAN},
   {NULL, 0},
};

struct mdbObj *findMetaObject(struct mdbObj *mdbObjs, char *table)
{
struct mdbObj *mdbObj = mdbObjs;

for(; mdbObj; mdbObj = mdbObj->next)
    {
    if (sameString(mdbObj->obj, table))
        return mdbObj;
    }

errAbort("couldn't find metaObject called %s\n",table);
return mdbObj;
}

struct mdbVar * addMdbTxtVar(struct mdbObj *mdbObj, char *var, void *val)
{
struct mdbVar * mdbVar;

AllocVar(mdbVar);
mdbVar->var     = cloneString(var);
mdbVar->varType = vtTxt;
mdbVar->val     = cloneString(val);
hashAdd(mdbObj->varHash, mdbVar->var, mdbVar); // pointer to struct to resolve type
slAddHead(&(mdbObj->vars),mdbVar);
//verbose(2, "added %s to obj %s\n", mdbVar->var, mdbObj->obj);

return mdbVar;
}

struct mdbVar *addVar(struct mdbObj *mdbObj, struct hash *blockHash,
    char *stringInBlock, char *stringInMdb)
{
struct hashEl *hel2;

if ((hel2 = hashLookup(blockHash, stringInBlock)) == NULL)
    errAbort("cannot find '%s' tag in load block %s\n", 
        stringInBlock, mdbObj->obj);

char *value = hel2->val;
struct mdbVar *mdbVar = addMdbTxtVar(mdbObj, stringInMdb, value);

return mdbVar;
}

char *getReportVersion(char *blob)
{
char *start = strchr(blob, '+');
if (start == NULL)
    errAbort("no plus in report blob");

start++;
char *end = strchr(start, '+');
if (end == NULL)
    errAbort("no second plus in report blob");

char save = *end;
*end = 0;

char *ret = cloneString(start);
*end = save;

return ret;
}

struct hash *readLoadRa(struct mdbObj *mdbObjs, char *submitDir)
{
char loadRa[10 * 1024];

safef(loadRa, sizeof loadRa, "%s/out/load.ra", submitDir);

struct hash *loadHash =  raReadAll(loadRa, "tablename");
struct hashCookie cook = hashFirst(loadHash);
struct hashEl *hel;
struct hash *mapHash = newHash(5);
struct hash *fileHash = newHash(5);

while((hel = hashNext(&cook)) != NULL)
    {
    struct hash *blockHash = hel->val;
    struct mdbObj *mdbObj = findMetaObject(mdbObjs, hel->name);
    hashAdd(mapHash, mdbObj->obj, blockHash);

    char *fileVal = hashMustFindVal(blockHash, "files");
    if (hashLookup(fileHash, fileVal))
        errAbort("more than one load stanza uses the same files value");
    hashStore(fileHash, fileVal);

    struct mdbVar *mdbVar = addVar(mdbObj, blockHash, "files", "submitPath");
    char *files = mdbVar->val; 
    char *ptr = strchr(files, ',');
    if (ptr != NULL)
        errAbort("don't support multiple files in block %s\n", hel->name);

    addVar(mdbObj, blockHash, "assembly", "assembly");
    addVar(mdbObj, blockHash, "type", "type");

    slSort(&(mdbObj->vars),&mdbVarCmp); // Should be in determined order
    }

return mapHash;
}


struct mdbObj *readMdb(char *submitDir)
{
char metaDb[10 * 1024];
boolean validated;

safef(metaDb, sizeof metaDb, "%s/out/mdb.txt", submitDir);
return mdbObjsLoadFromFormattedFile(metaDb, &validated);
}

#ifdef NOTNOW
char *calcMd5Sum(char *file)
{
verbose(2, "should calculate md5sum for %s\n", file);
return "ffafasfafaf";
}
#endif

char *readBlob(char *file)
{
off_t size = fileSize(file);

if (size < 0)
    return NULL;
FILE *f = fopen(file, "r");
if (f == NULL)
    return NULL;

char *blob = needMem(size + 1);
blob[size] = 0;

mustRead(f, blob, size);
fclose(f);
char *outBlob = cgiEncode(blob);
freez(&blob);
return outBlob;
}

void outLoadStanza(FILE *loadRaF, struct hash *mapHash, struct mdbObj *mdbObj)
{
struct hashEl *hel;
struct hash *blockHash = hashMustFindVal(mapHash, mdbObj->obj);
struct hashCookie cook = hashFirst(blockHash);

fprintf(loadRaF, "tablename %s\n", mdbObjFindValue(mdbObj, "docId"));
while((hel = hashNext(&cook)) != NULL)
    {
    if (!sameString(hel->name, "tablename"))
        fprintf(loadRaF, "%s %s\n", hel->name, (char *)hel->val);
    }
fprintf(loadRaF, "\n");
}

void submitToDocId(struct sqlConnection *conn, struct mdbObj *mdbObjs, 
    char *submitDir, char *docIdDir, struct hash *mapHash)
{
struct mdbObj *mdbObj = mdbObjs, *nextObj;
struct docIdSub docIdSub;
char file[10 * 1024];
struct tempName tn;
makeTempName(&tn, "metadata", ".txt");
char *tempFile = tn.forHtml;
//printf("tempFile is %s\n", tempFile);
/*
char loadRa[10 * 1024];
char loadRaBack[10 * 1024];
FILE *loadRaF = NULL;
*/
char metaRa[10 * 1024];
FILE *metaRaF = NULL;
char sedName[10 * 1024];
FILE *sedF = NULL;

if (editInput)
    {
    // need to backup load.ra into load.ra.preDocId
#ifdef NOTNOW
    safef(loadRa, sizeof loadRa, "%s/out/load.ra", submitDir);
    safef(loadRaBack, sizeof loadRaBack, "%s/out/load.ra.preDocId", submitDir);
    if (rename(loadRa, loadRaBack) != 0)
        errAbort("could not rename %s to %s", loadRa, loadRaBack);

    // overwrite existing file
    loadRaF = mustOpen(loadRa, "w");
#endif
    safef(sedName, sizeof sedName, "%s/out/edit.sed", submitDir);
    sedF = mustOpen(sedName, "w");

    // open file for metaDb
    safef(metaRa, sizeof metaRa, "%s/out/docIdMetaDb.ra", submitDir);
    metaRaF = mustOpen(metaRa, "w");
    }

for(; mdbObj; mdbObj = nextObj)
    {
    // save the next pointer because we need to nuke it to allow
    // us to write one metaDb object at a time
    nextObj = mdbObj->next;
    mdbObj->next = NULL;

    docIdSub.md5sum = NULL;
    docIdSub.valReport = NULL;
    docIdSub.submitDate = mdbObjFindValue(mdbObj, "dateSubmitted") ;
    docIdSub.submitter = mdbObjFindValue(mdbObj, "lab") ;
    docIdSub.assembly = mdbObjFindValue(mdbObj, "assembly") ;

    char *type = mdbObjFindValue(mdbObj, "type") ;
    struct mdbVar *submitPathVar = mdbObjFind(mdbObj, "submitPath") ;
    if (submitPathVar == NULL)
        errAbort("object %s doesn't have submitPath", mdbObj->obj);
    char *submitPath = cloneString(submitPathVar->val);
    char *space = strchr(submitPath, ' ');
    struct mdbVar * subPartVar = NULL;
    int subPart = 0;

    // unfortunately, submitPath might be a space separated list of files
    if (space)
        {
        // we have a space, so add a new metadata item, subPart, that
        // has the number of the file in the list
        subPartVar = addMdbTxtVar(mdbObj, "subPart", NULL);
        }

    boolean multipleFiles = FALSE;
    boolean firstTime = TRUE;
    char *masterDocId = NULL;
    char *oldObjName = NULL;
    struct mdbVar *tmpVar;
    tmpVar = mdbObjFind(mdbObj, "type") ;
    char *suffix = docDecorateType(tmpVar->val);

    // step through the path and submit each file
    while(submitPath != NULL)
        {
        char buffer[10 * 1024];
        char *space = strchr(submitPath, ' ');
        if (space)
            {
            multipleFiles = TRUE;
            *space = 0;
            space++;
            }
        submitPathVar->val = cloneString(submitPath);

        if (subPartVar)
            subPartVar->val = toBeDecided;
        tmpVar = mdbObjFind(mdbObj, "fileName") ;
        if (tmpVar != NULL)
            tmpVar->val = toBeDecided;
        tmpVar = mdbObjFind(mdbObj, "tableName") ;
        if (tmpVar != NULL)
            tmpVar->val = toBeDecided;
        oldObjName = mdbObj->obj;
        mdbObj->obj = toBeDecided;
        mdbObjPrintToFile(mdbObj, TRUE, tempFile);
        docIdSub.metaData = readBlob(tempFile);	
        unlink(tempFile);

        safef(file, sizeof file, "%s/%s", submitDir, submitPath);
        docIdSub.submitPath = cloneString(file);
        printf("submitPath %s\n", docIdSub.submitPath);

        safef(buffer, sizeof buffer, "%s.report", file);
        docIdSub.valReport = readBlob(buffer);	
        if (docIdSub.valReport == NULL)
            warn("no report blob for object %s", oldObjName);
        else
            docIdSub.valVersion = getReportVersion(docIdSub.valReport);

        char *docId = docIdSubmit(conn, docIdTable, &docIdSub, docIdDir, type);
        char *composite = mdbObjFindValue(mdbObj, "composite");
        if (composite == NULL)
            errAbort("could not find composite name in metadata");
        char *cellType = mdbObjFindValue(mdbObj, "cell");
        if (cellType == NULL)
            errAbort("could not find cell type in metadata");
        char *decoratedDocId = docIdDecorate(composite, cellType, atoi(docId));

        if (firstTime)
            addMdbTxtVar(mdbObj, "docId", decoratedDocId);
        else 
            {
            struct mdbVar *docIdVar = mdbObjFind(mdbObj, "docId") ;
            docIdVar->val = cloneString(decoratedDocId);
            }

        submitPath = space;
        subPart++;

        if (firstTime)
            masterDocId = cloneString(decoratedDocId);

        if (editInput)
            {
            // output load.ra and docIdMetaDb.ra stanzas
            //outLoadStanza(loadRaF, mapHash, mdbObj);

            mdbObj->obj = decoratedDocId;
            struct mdbVar *fileNameVar = mdbObjFind(mdbObj, "fileName") ;
            //char *fileName = fileNameVar->val;
            char buffer[10 * 1024];

            safef(buffer, sizeof buffer, "%s.%s",decoratedDocId,suffix);
            fileNameVar->val = cloneString(buffer);

            struct mdbVar *tableNameVar = mdbObjFind(mdbObj, "tableName") ;
            if (tableNameVar != NULL)
                tableNameVar->val = decoratedDocId;

            if (subPartVar)
                {
                if (firstTime)
                    safef(buffer, sizeof buffer, "%d.%s", subPart, "master");
                else
                    safef(buffer, sizeof buffer, "%d.%s", subPart, masterDocId );
                subPartVar->val = cloneString(buffer);
                }
            mdbObjPrintToStream(mdbObj, TRUE, metaRaF);

            if (firstTime || !multipleFiles)
                fprintf(sedF, "s/%s/%s/g\n", oldObjName, decoratedDocId);
            }

        firstTime = FALSE;
        } 
    }
}

void docIdSubmitDir(char *database, char *submitDir, char *docIdDir)
/* docIdSubmitDir - put ENCODE submission dir into docIdSub table. */
{
struct mdbObj *mdbObjs = readMdb(submitDir);
struct hash *mapHash = readLoadRa(mdbObjs, submitDir);
struct sqlConnection *conn = sqlConnect(database);

submitToDocId(conn, mdbObjs, submitDir, docIdDir, mapHash);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
editInput = optionExists("editInput");
docIdSubmitDir(argv[1], argv[2], argv[3]);
return 0;
}
