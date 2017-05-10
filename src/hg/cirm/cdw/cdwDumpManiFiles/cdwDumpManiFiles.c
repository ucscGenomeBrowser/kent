/* cdwDumpManiFiles - Dump the manifest files associated with a data_set_id. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwStep.h"
#include "cart.h"
#include "cdwLib.h" 
#include "obscure.h" 

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwDumpManiFiles - Dump the manifest files associated with a data_set_id\n"
  "usage:\n"
  "   cdwDumpManiFiles dataSetId directory\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};



struct slManiPair
/* Struct for string name coupled to int values. */ 
    {
    struct slManiPair *next;	/* Next in list. */
    char *url;			/* URL */
    int id;			/* The id of the manifest file. */
    };

struct slManiPair *slManiPairNew(char *url, int id)
/* Allocate new url/value pair. */
{
struct slManiPair *el;
AllocVar(el);
el->url = cloneString(url);
el->id = id;
return el;
}

void slManiPairAdd(struct slManiPair **pList, char *url, int id)
/* Add new slManiPair to head of list. */
{
struct slManiPair *el = slManiPairNew(url, id);
slAddHead(pList, el);
}

struct slManiPair *slManiPairFind(struct slManiPair *pList, char *url)
/* Given a slManiPair list and URL find the slManiPair the URL is associated with. */ 
{
struct slManiPair *iter; 
for (iter = pList; iter != NULL; iter = iter->next)
    {
    if (!strcmp(iter->url,url))
	{
	return iter; 
	}
    }
return iter = NULL; 
}

struct slManiPair *makeManiList(struct sqlConnection *conn, char *dataSetId)
/* Make a list of slManiPairs (url and manifestId) for each unique url.  If there are duplicate 
 * url's keep the one with the highest manifestId. */ 
{
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwSubmit where url like '%%%s%%'", dataSetId);  
verbose(2, "%s\n", query);
struct cdwSubmit *cS, *cSList = cdwSubmitLoadByQuery(conn, query);  
struct slManiPair *maniList = NULL; 

if (cSList == NULL)
    errAbort("Can't find any submissions for %s", dataSetId);
for (cS = cSList; cS != NULL; cS = cS->next)
    {
    struct slManiPair *cur = slManiPairFind(maniList, cS->url); // Look for the URL in the list. 
    if (cur == NULL) // If it's not there add it.
	{
	slManiPairAdd(&maniList, cS->url, cS->manifestFileId); 
	}
    else{ //If its already there compare id values and keep the bigger one.
	if (cur->id < cS->manifestFileId)
	    {
	    slRemoveEl(&maniList, cur);
	    slManiPairAdd(&maniList, cS->url, cS->manifestFileId); 
	    }
	}
    }
return maniList; 
}

int getMeta(struct sqlConnection *conn, char *dataSetId)
/* Get the most recent meta file's id. */ 
{
char query[1024]; 
sqlSafef(query, sizeof(query), "select * from cdwSubmit where url like '%%%s%%'", dataSetId);  
struct cdwSubmit *cS, *cSList = cdwSubmitLoadByQuery(conn, query);  
int result = 0; 

for (cS = cSList; cS != NULL; cS = cS->next)
    {
    if (cS->metaFileId > result)
	result = cS->metaFileId; 
    }
return result; 
}

char *stripFilePath(char *url)
// Grab the string past the last '/'.
{
struct slName *pathPieces = charSepToSlNames(url, '/');
slReverse(&pathPieces); 
return (pathPieces->name); 
}

void serveUpFiles(struct sqlConnection *conn, char *directory, struct slManiPair *maniList, int metaFileId)
// Copy the distinct manifest files and most recent meta file to directory. 
{
char cwd[1024];
getcwd(cwd, sizeof(cwd)); 
char fullpath[PATH_LEN]; 
safef(fullpath, sizeof(fullpath), "%s%s", cwd, directory); 
struct stat st; 
if (stat(directory, &st) == -1)  
    mkdir(directory, 0755);
else 
    errAbort("The directory %s is already in use. Aborting.", directory); 

char query[1024]; 
char cmd[1024]; 
struct slManiPair *iter;
printf("Copying %i distinct manifest files and 1 meta file.\n", slCount(maniList)); 

//Go through the list of manifest files and copy them over.
for (iter = maniList; iter != NULL; iter = iter->next)
    {
    sqlSafef(query, sizeof(query), "select * from cdwFile where id='%i'", iter->id); 
    struct cdwFile *cF = cdwFileLoadByQuery(conn, query); 
    if (cF== NULL) 
	continue;
    char result[1024]; 
    safef (result, sizeof(result), "%s/%s", directory , stripFilePath(iter->url));
    safef(cmd, sizeof(cmd), "cp /data/cirm/cdw/%s %s", cF->cdwFileName, result); 
    printf("%s\n",cmd);  
    mustSystem(cmd);
    }

// Copy over the most recent meta file 
sqlSafef(query, sizeof(query), "select * from cdwFile where id='%i'", metaFileId); 
struct cdwFile *cF = cdwFileLoadByQuery(conn, query); 
char result[1024]; 
safef (result, sizeof(result), "%s/%s", directory , stripFilePath(cF->submitFileName));
safef(cmd, sizeof(cmd), "cp /data/cirm/cdw/%s %s", cF->cdwFileName, result); 
printf("%s\n",cmd);  
mustSystem(cmd);
}

void cdwDumpManiFiles(char *dataSetId, char *directory)
/* cdwDumpManiFiles - Dump the distinct manifest files and most recent meta file associated with a 
 * data_set_id. */
{
struct sqlConnection *conn = cdwConnect(); 
struct slManiPair *maniList = makeManiList(conn, dataSetId);
int metaFileId = getMeta(conn, dataSetId);
serveUpFiles(conn, directory, maniList, metaFileId); 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwDumpManiFiles(argv[1], argv[2]);
return 0;
}
