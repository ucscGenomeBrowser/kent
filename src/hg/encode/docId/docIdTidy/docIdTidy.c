/* docIdTidy - tidy up the docId library by compressing and md5suming where appropriate. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "docId.h"
#include "portable.h"
#include "trashDir.h"
#include "mdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "docIdTidy - tidy up the docId library by compressing and md5suming where appropriate\n"
  "usage:\n"
  "   docIdTidy database docIdRoot\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *docIdTable = DEFAULT_DOCID_TABLE ;

#define NEEDS_COMPRESSION       (1<<0)
#define NEEDS_REPORT            (1<<1)
#define NEEDS_MD5SUM            (1<<2)

struct toDoList
{
struct toDoList *next;
unsigned int docId;
char *path;
int needs;
};

static struct optionSpec options[] = {
   {NULL, 0},
};

void doCompression(struct toDoList *toDoList)
{
struct toDoList *todo = toDoList;
printf("compressing these\n");
for(; todo; todo = todo->next)
    {
    if (todo->needs & NEEDS_COMPRESSION)
	{
        printf("%d %s\n", todo->docId, todo->path);
	}
    }
}

void doReports(struct toDoList *toDoList)
{
struct toDoList *todo = toDoList;
printf("report these\n");
for(; todo; todo = todo->next)
    if (todo->needs & NEEDS_REPORT)
        printf("%d %s\n", todo->docId, todo->path);
}

void parseMd5sumFile(struct sqlConnection *conn, char *file, struct hash *pathHash)
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *words[2];

while(lineFileRow(lf, words))
    {
    struct hashEl *hel = hashLookup(pathHash, words[1]);

    if (hel == NULL)
	errAbort("cannot find docId for path %s", words[1]);

    char *docId = hel->val;

    char query[10 * 1024];

    sqlSafef(query, sizeof query, "update %s set md5sum='%s' where ix=%s",
	docIdTable, words[0], docId);
    verbose(2,"query %s\n", query);
    char *result = sqlQuickString(conn, query);

    printf("result %s\n", result);
    }
}

void doMd5Summing( struct sqlConnection *conn, struct toDoList *toDoList)
{
struct toDoList *todo = toDoList;
struct dyString *dy = dyStringNew(50);
boolean haveOne = FALSE;
struct hash *pathHash = newHash(5);

dyStringPrintf(dy, "ssh swarm paraMd5sum ");

for(; todo; todo = todo->next)
    if (todo->needs & NEEDS_MD5SUM)
	{
	char buffer[100];

	safef(buffer, sizeof buffer, "%d", todo->docId);
	dyStringPrintf(dy, "%s ",  todo->path);
	hashAdd(pathHash, todo->path, cloneString(buffer));
	haveOne = TRUE;
	}

if (!haveOne)
    return;

struct tempName tn;
trashDirFile(&tn, "docIdTmp", "paraMd5sum", ".txt");
char *tempFile = tn.forCgi;

dyStringPrintf(dy, " > %s\n",  tempFile);

verbose(2, "systeming %s\n",dy->string);
mustSystem(dy->string);
parseMd5sumFile(conn, tempFile, pathHash);
unlink(tempFile);
}

struct toDoList *getItem(struct toDoList *toDoItem, unsigned function,
    char *path, struct docIdSub *docIdSub)
{
if (toDoItem == NULL) AllocVar(toDoItem);
toDoItem->needs |= function;
toDoItem->path = path;
toDoItem->docId = docIdSub->ix;

return toDoItem;
}


void docIdTidy(char *database, char *docIdDir)
/* docIdTidy - tidy up the docId library by compressing and md5suming where appropriate. */
{
struct sqlConnection *conn = sqlConnect(database);
sqlGetLock(conn, "docIdTidy");

char query[10 * 1024];
struct sqlResult *sr;
char **row;
struct tempName tn;
trashDirFile(&tn, "docId", "meta", ".txt");
char *tempFile = tn.forCgi;
struct toDoList *toDoList = NULL;

sqlSafef(query, sizeof query, "select * from %s", docIdTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct docIdSub *docIdSub = docIdSubLoad(row);

    cgiDecode(docIdSub->metaData, docIdSub->metaData, strlen(docIdSub->metaData));
    FILE *f = mustOpen(tempFile, "w");
    fwrite(docIdSub->metaData, strlen(docIdSub->metaData), 1, f);
    fclose(f);
//    boolean validated;
    struct mdbObj *mdbObj = mdbObjsLoadFromFormattedFile(tempFile, NULL);
    unlink(tempFile);

    char *docIdType = mdbObjFindValue(mdbObj, "type");
    char buffer[10 * 1024];
    safef(buffer, sizeof buffer, "%d", docIdSub->ix);
    char *path = docIdGetPath(buffer, docIdDir, docIdType, NULL);
    struct toDoList *toDoItem = NULL;

    if (!fileIsCompressed(path))
	toDoItem = getItem(toDoItem, NEEDS_COMPRESSION, path, docIdSub);

    if (sameString(docIdSub->md5sum, ""))
	toDoItem = getItem(toDoItem, NEEDS_MD5SUM, path, docIdSub);

    if (sameString(docIdSub->valReport,""))
	toDoItem = getItem(toDoItem, NEEDS_REPORT, path, docIdSub);

    if (toDoItem)
        slAddHead(&toDoList, toDoItem);
    }
sqlFreeResult(&sr);

doCompression(toDoList);
doReports( toDoList);
doMd5Summing(conn, toDoList);

sqlReleaseLock(conn, "docIdTidy");
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

docIdTidy(argv[1],argv[2]);
return 0;
}
