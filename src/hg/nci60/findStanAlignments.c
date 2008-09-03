#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "linefile.h"
#include "hdb.h"
#include "psl.h"
#include "imageClone.h"
#include "stanMad.h"

static char const rcsid[] = "$Id: findStanAlignments.c,v 1.6 2008/09/03 19:20:40 markd Exp $";

void usage() 
{
errAbort("findStanAlignments - takes a stanford microarray experiment file and\n"
	 "tries to look up an alignment for the relevant clone in the database.\n"
	 "Starts by trying to look up the longest genbank clone from image id, \n"
	 "then tries to look up the 5' and 3' accesion numbers.\n"
	 "usage:\n"
	 "\tfindStanAlignments <db version> <stanFile> <imageCloneFile> <alignmentOutFile>\n");
}


struct imageClone *imageCloneLoadAllNonZero(char *fileName) 
/* Load all imageClone from a tab-separated file where there are genbank ids.
 * Dispose of this with imageCloneFreeList(). */
{
struct imageClone *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];

while (lineFileRow(lf, row))
    {
    el = imageCloneLoad(row);
    if(el->numGenbank != 0) 
	{
	slAddHead(&list, el);
	}
    else
	{
	imageCloneFree(&el);
	}
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

/* read all of the entries in an imageClone file into
   a hash for easy access */
void readInImageHash(struct hash *iHash, char *image)
{
struct imageClone *icList =NULL, *ic = NULL;
char buff[256];
int count =0;

icList = imageCloneLoadAllNonZero(image);
for(ic = icList; ic != NULL; ic = ic->next)
    {
    if((count++ % 10000) ==0)
	{
	printf(".");
	fflush(stdout);
	}
	sprintf(buff, "%d", ic->id);
	hashAdd(iHash, buff, ic);
    }
printf("\n");
}

struct psl *pslLoadByQuery(struct sqlConnection *conn, char *query)
/* Load all psl from table that satisfy the query given.  
 * Where query is of the form 'select * from example where something=something'
 * or 'select example.* from example, anotherTable where example.something = 
 * anotherTable.something'.
 * Dispose of this with pslFreeList(). */
{
struct psl *list = NULL, *el;
struct sqlResult *sr;
char **row;
int offSet = 0;
sr = sqlGetResult(conn, query);
offSet = sqlCountColumns(sr) - 21;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = pslLoad(row+offSet);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
return list;
}


/* create object on the heap, copy values from psl and return, don't forget to free */
struct psl *copyPsl(struct psl *psl)
{
struct psl *ret = NULL;
AllocVar(ret);
ret->next = NULL;
ret->match = psl->match;
ret->misMatch = psl->misMatch;
ret->repMatch = psl->repMatch;
ret->nCount = psl->nCount;
ret->qNumInsert = psl->qNumInsert;
ret->qBaseInsert = psl->qBaseInsert;
ret->tNumInsert = psl->tNumInsert;
ret->tBaseInsert = psl ->tBaseInsert;
strcpy(ret->strand, psl->strand);
ret->qName = cloneString(psl->qName);
ret->qSize = psl->qSize;
ret->qStart = psl->qStart;
ret->qEnd = psl->qEnd;
ret->tName = cloneString(psl->tName);
ret->tSize = psl->tSize;
ret->tStart = psl->tStart;
ret->tEnd = psl->tEnd;
ret->blockCount = psl->blockCount;
ret->blockSizes = CloneArray(psl->blockSizes, psl->blockCount);
ret->qStarts = CloneArray(psl->qStarts, psl->blockCount);
ret->tStarts = CloneArray(psl->tStarts, psl->blockCount);
return ret;
}


void outputAlignmentForStan(struct sqlConnection *conn, struct stanMad *sm, struct hash *iHash, FILE *out)
{
struct psl *pslList, *bestPsl = NULL;
char buff[1024];
int i;
struct imageClone *ic = NULL;
sprintf(buff, "%d", sm->clid);
printf("Looking for %s\n", buff);
ic = hashFindVal(iHash, buff);
if(ic != NULL) 
    {
    /* first try looking for the image clones themselves... */
    for(i=0; i<ic->numGenbank; i++) 
	{
	sprintf(buff, "select * from all_est where qName='%s'", ic->genbankIds[i]);
	pslList = pslLoadByQuery(conn, buff);
	if(pslList != NULL) 
	    {
	    slSort(&pslList, pslCmpScore);	
	    if(bestPsl == NULL || (pslScore(pslList) > pslScore(bestPsl)))
		pslFree(&bestPsl);
		bestPsl = copyPsl(pslList);
	    }
	
	pslFreeList(&pslList);
	}

    if(bestPsl != NULL)
	{    
	freez(&bestPsl->qName);
	sprintf(buff, "%d", sm->clid);
	bestPsl->qName = cloneString(buff);
	pslTabOut(bestPsl,out);
	}
    else 
	{
	fprintf(out, "%d\talignment unknown\n", sm->clid);
	}
    
    }
else 
    {
    fprintf(out, "%d\tunknown\n", sm->clid);
    }
}

void findStanAlignments(char *db, char *stan, char *image, char *pslOut)
{
struct hash *iHash = newHash(5);
struct stanMad *smList = NULL, *sm = NULL;
FILE *out = mustOpen(pslOut, "w");
int count =0;
struct sqlConnection *conn = NULL;
warn("Getting sql Connection...");
conn = hAllocConn(db);
warn("Reading in image clones...");
readInImageHash(iHash, image);
warn("Loading Stanford Alignments..");
smList = stanMadLoadAll(stan);
warn("Finding best Alignments...");
for(sm = smList; sm != NULL; sm = sm->next)
    {
    if(differentString(sm->type,"Control"))
	{
	if((count++ % 10000) ==0)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	outputAlignmentForStan(conn, sm, iHash, out);
	}
    }
printf("\n");
warn("Done. Cleaning up...");
stanMadFreeList(&smList);
freeHash(&iHash);
hFreeConn(&conn);

}


int main(int argc, char *argv[]) 
{
if(argc != 5)
    usage();
findStanAlignments(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
