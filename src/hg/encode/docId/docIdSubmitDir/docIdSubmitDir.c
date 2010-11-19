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


boolean editInput = FALSE;

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
if (blob == NULL)
    errAbort("no report blob");

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

while((hel = hashNext(&cook)) != NULL)
    {
    struct hash *blockHash = hel->val;
    struct mdbObj *mdbObj = findMetaObject(mdbObjs, hel->name);
    hashAdd(mapHash, mdbObj->obj, blockHash);

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
        errAbort("object doesn't have submitPath");
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

    // step through the path and submit each file
    while(submitPath != NULL)
        {
        char buffer[10 * 1024];
        char *space = strchr(submitPath, ' ');
        if (space)
            {
            *space = 0;
            space++;
            }
        
        if (subPartVar)
            {
            safef(buffer, sizeof buffer, "%d", subPart);
            subPartVar->val = cloneString(buffer);
            }

        submitPathVar->val = cloneString(submitPath);
        mdbObjPrintToFile(mdbObj, TRUE, tempFile);
        docIdSub.metaData = readBlob(tempFile);	
        unlink(tempFile);


        safef(file, sizeof file, "%s/%s", submitDir, submitPath);
        docIdSub.submitPath = cloneString(file);
        printf("submitPath %s\n", docIdSub.submitPath);

        safef(buffer, sizeof buffer, "%s.report", file);
        docIdSub.valReport = readBlob(buffer);	
        docIdSub.valVersion = getReportVersion(docIdSub.valReport);

        char *docId = docIdSubmit(conn, &docIdSub, docIdDir, type);
        char *decoratedDocId = docIdDecorate(atoi(docId));

        addMdbTxtVar(mdbObj, "docId", decoratedDocId);

        submitPath = space;
        subPart++;

        if (editInput)
            {
            // output load.ra and docIdMetaDb.ra stanzas
            //outLoadStanza(loadRaF, mapHash, mdbObj);
            fprintf(sedF, "s/%s/%s/g\n", mdbObj->obj, decoratedDocId);

            mdbObj->obj = decoratedDocId;
            struct mdbVar *fileNameVar = mdbObjFind(mdbObj, "fileName") ;
            char *fileName = fileNameVar->val;
            char *suffix = strchr(fileName, '.');
            char buffer[10 * 1024];

            safef(buffer, sizeof buffer, "%s%s",decoratedDocId,suffix);
            fileNameVar->val = buffer;

            struct mdbVar *tableNameVar = mdbObjFind(mdbObj, "tableName") ;
            tableNameVar->val = decoratedDocId;

            mdbObjPrintToStream(mdbObj, TRUE, metaRaF);

            }
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
