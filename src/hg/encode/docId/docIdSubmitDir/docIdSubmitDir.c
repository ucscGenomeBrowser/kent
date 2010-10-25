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
  );
}


static struct optionSpec options[] = {
   {"table", OPTION_STRING},
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

struct mdbVar *addVar(struct mdbObj *mdbObj, struct hash *blockHash,
    char *stringInBlock, char *stringInMdb)
{
struct hashEl *hel2;

if ((hel2 = hashLookup(blockHash, stringInBlock)) == NULL)
    errAbort("cannot find '%s' tag in load block %s\n", 
        stringInBlock, mdbObj->obj);

char *value = hel2->val;
struct mdbVar * mdbVar;
AllocVar(mdbVar);
mdbVar->var     = cloneString(stringInMdb);
mdbVar->varType = vtTxt;
mdbVar->val     = cloneString(value);

hashAdd(mdbObj->varHash, mdbVar->var, mdbVar); // pointer to struct to resolve type
slAddHead(&(mdbObj->vars),mdbVar);

return mdbVar;
}

void addFiles(struct mdbObj *mdbObjs, char *submitDir)
{
char loadRa[10 * 1024];

safef(loadRa, sizeof loadRa, "%s/out/load.ra", submitDir);

struct hash *loadHash =  raReadAll(loadRa, "tablename");
struct hashCookie cook = hashFirst(loadHash);
struct hashEl *hel;
while((hel = hashNext(&cook)) != NULL)
    {
    struct hash *blockHash = hel->val;
    struct mdbObj *mdbObj = findMetaObject(mdbObjs, hel->name);

    struct mdbVar *mdbVar = addVar(mdbObj, blockHash, "files", "submitPath");
    char *files = mdbVar->val; 
    char *ptr = strchr(files, ',');
    if (ptr != NULL)
        errAbort("don't support multiple files in block %s\n", hel->name);

    addVar(mdbObj, blockHash, "assembly", "assembly");
    addVar(mdbObj, blockHash, "type", "type");

    slSort(&(mdbObj->vars),&mdbVarCmp); // Should be in determined order
    }

}


struct mdbObj *getMdb(char *submitDir)
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

void submitToDocId(struct sqlConnection *conn, struct mdbObj *mdbObjs, 
    char *submitDir, char *docIdDir)
{
struct mdbObj *mdbObj = mdbObjs, *nextObj;
struct docIdSub docIdSub;
char file[10 * 1024];
struct tempName tn;
makeTempName(&tn, "metadata", ".txt");
char *tempFile = tn.forHtml;
//printf("tempFile is %s\n", tempFile);

for(; mdbObj; mdbObj = nextObj)
    {
    nextObj = mdbObj->next;
    mdbObj->next = NULL;

    docIdSub.md5sum = NULL;
    docIdSub.valReport = NULL;
    docIdSub.submitDate = mdbObjFindValue(mdbObj, "dateSubmitted") ;
    docIdSub.submitter = mdbObjFindValue(mdbObj, "lab") ;

    char *type = mdbObjFindValue(mdbObj, "type") ;
    struct mdbVar *submitPathVar = mdbObjFind(mdbObj, "submitPath") ;
    char *submitPath = cloneString(submitPathVar->val);
    char *space = strchr(submitPath, ' ');
    struct mdbVar * subPartVar = NULL;
    int subPart = 0;

    // unfortunately, submitPath might be a space separated list of files
    if (space)
        {
        // we have a space, so add a new metadata item, subPart, that
        // has the number of the file in the list
        AllocVar(subPartVar);
        subPartVar->var     = "subPart";
        subPartVar->varType = vtTxt;

        hashAdd(mdbObj->varHash, subPartVar->var, subPartVar);
        slAddHead(&(mdbObj->vars),subPartVar);
        }

    // step through the path and submit each file
    while(submitPath != NULL)
        {
        char *space = strchr(submitPath, ' ');
        if (space)
            {
            *space = 0;
            space++;
            }
        
        if (subPartVar)
            {
            char buffer[10 * 1024];

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
        docIdSubmit(conn, &docIdSub, docIdDir, type);

        submitPath = space;
        subPart++;
        } 
    }
}

void docIdSubmitDir(char *database, char *submitDir, char *docIdDir)
/* docIdSubmitDir - put ENCODE submission dir into docIdSub table. */
{
struct mdbObj *mdbObjs = getMdb(submitDir);

addFiles(mdbObjs, submitDir);

struct sqlConnection *conn = sqlConnect(database);

submitToDocId(conn, mdbObjs, submitDir, docIdDir);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
docIdSubmitDir(argv[1], argv[2], argv[3]);
return 0;
}
