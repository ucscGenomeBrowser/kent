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

extern char *docIdTable;

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
    if (todo->needs & NEEDS_COMPRESSION)
        printf("%d %s\n", todo->docId, todo->path);
}

void doReports(struct toDoList *toDoList)
{
struct toDoList *todo = toDoList;
printf("report these\n");
for(; todo; todo = todo->next)
    if (todo->needs & NEEDS_REPORT)
        printf("%d %s\n", todo->docId, todo->path);
}

void doMd5Summing(struct toDoList *toDoList)
{
struct toDoList *todo = toDoList;
printf("md5sum these\n");
for(; todo; todo = todo->next)
    if (todo->needs & NEEDS_MD5SUM)
        printf("%d %s\n", todo->docId, todo->path);
}

void docIdTidy(char *database, char *docIdDir)
/* docIdTidy - tidy up the docId library by compressing and md5suming where appropriate. */
{
char query[10 * 1024];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect(database);
struct tempName tn;
trashDirFile(&tn, "docId", "meta", ".txt");
char *tempFile = tn.forCgi;
struct toDoList *toDoList = NULL;

safef(query, sizeof query, "select * from %s", docIdTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct docIdSub *docIdSub = docIdSubLoad(row);

    cgiDecode(docIdSub->metaData, docIdSub->metaData, strlen(docIdSub->metaData));
    FILE *f = mustOpen(tempFile, "w");
    fwrite(docIdSub->metaData, strlen(docIdSub->metaData), 1, f);
    fclose(f);
    boolean validated;
    //printf("metadata is %s\n", docIdSub->metaData);
    struct mdbObj *mdbObj = mdbObjsLoadFromFormattedFile(tempFile, &validated);
    unlink(tempFile);

    char *docIdType = mdbObjFindValue(mdbObj, "type");
    char buffer[10 * 1024];
    safef(buffer, sizeof buffer, "%d", docIdSub->ix);
    char *path = docIdGetPath(buffer, docIdDir, docIdType, NULL);
        //docIdDecorate(docIdSub->ix));
    printf("path %s\n", path);
    struct toDoList *toDoItem = NULL;

    if (!fileIsCompressed(path))
        {
        if (toDoItem == NULL) AllocVar(toDoItem);
        printf("foo\n");
        toDoItem->needs |= NEEDS_COMPRESSION;
        toDoItem->path = path;
        toDoItem->docId = docIdSub->ix;
        }

    printf("docId %d md5sum %s valReport %s\n",docIdSub->ix, docIdSub->md5sum, docIdSub->valReport);
    //if (docIdSub->md5sum == NULL)
    if (sameString(docIdSub->md5sum, ""))
        {
        printf("mdsum\n");
        if (toDoItem == NULL) AllocVar(toDoItem);
        toDoItem->needs |= NEEDS_MD5SUM;
        toDoItem->path = path;
        toDoItem->docId = docIdSub->ix;
        }

    //if (docIdSub->valReport == NULL)
    if (sameString(docIdSub->valReport,""))
        {
        printf("report\n");
        if (toDoItem == NULL) AllocVar(toDoItem);
        toDoItem->needs |= NEEDS_REPORT;
        toDoItem->path = path;
        toDoItem->docId = docIdSub->ix;
        }

    if (toDoItem)
        slAddHead(&toDoList, toDoItem);
    }
sqlFreeResult(&sr);

doCompression(toDoList);
doReports(toDoList);
doMd5Summing(toDoList);
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
