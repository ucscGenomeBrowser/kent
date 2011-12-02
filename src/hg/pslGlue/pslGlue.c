/* pslGlue - reduce a psl file to only the gluing components. */
#include "common.h"
#include "portable.h"
#include "obscure.h"
#include "linefile.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort("pslGlue - reduce a psl mRNA alignment file to only the components\n"
         "that might be involved in gluing\n"
	 "usage:\n"
	 "   pslGlue in.lst pslDir\n"
	 "This will create mrna.psl, est.psl, bacEnd.psl, pairedRead.psl and\n"
	 "mrna.glu and est.glue and bacEnd.psl in the current directory\n"
	 "   pslGlue mrnaIn.psl mrnaOut.psl mrnaOut.glu\n"
	 "This takes only the 'gluing' mrnas from mrnaIn and puts in mrnaOut.psl");
}

int outCount = 0;
int ltot = 0;
int mtot = 0;

boolean output(FILE *out, FILE *glue, struct psl **pList)
/* Write out relevant bits of pList and free pList. 
 * Return TRUE if wrote anything. */
{
struct psl *psl, *lastPsl;
struct psl *list;
int minDiff = 15;
boolean isRelevant = FALSE;

slReverse(pList);
++outCount;
ltot += slCount(*pList);

/* Return if size of list less than 2. */
if ((lastPsl = list = *pList) == NULL)
    return FALSE;
if (list->next == NULL)
    return FALSE;

++mtot;
for (psl = lastPsl->next; psl != NULL; psl = psl->next)
    {
    if (psl->qStart - lastPsl->qStart >= minDiff 
    	&& psl->qEnd - lastPsl->qEnd >= minDiff)
	{
	isRelevant = TRUE;
	break;
	}
    }

if (isRelevant)
    {
    for (psl = list; psl != NULL; psl = psl->next)
	{
	pslTabOut(psl, out);
	fprintf(glue, "%s\t%d\t%d\t%s %s\t%d\t%d\n",
	    psl->qName, psl->qStart, psl->qEnd,
	    psl->tName, psl->strand, psl->tStart, psl->tEnd);
	}
    }
pslFreeList(pList);
return isRelevant;
}

struct slName *getFileList(char *listFile, char *bulkDir)
/* Get list of files to work on from listFile. */
{
char **faFiles;
char *faBuf;
int faCount;
int i;
char path[512], dir[256], name[128], extension[64];
struct slName *list = NULL, *el;

readAllWords(listFile, &faFiles, &faCount, &faBuf);
for (i = 0; i<faCount; ++i)
    {
    splitPath(faFiles[i], dir, name, extension);
    sprintf(path, "%s/%s.%s",
	bulkDir,
	name, "psl");
    if (fileExists(path))
	{
	el = newSlName(path);
	slAddHead(&list, el);
	}
    }
slReverse(&list);
return list;
}

void pslGlueRna(char *listFile, char *partDir, char *pslName, char *gluName)
/* Reduce a psl files for only the gluing mRNA/EST components. */
{
FILE *pslOut;
FILE *gluOut;
struct psl *pslList = NULL, *psl, *nextPsl;
struct psl *localList = NULL;
int glueCount = 0;
int pslCount = 0;
struct slName *inList, *inEl;

inList = getFileList(listFile, partDir);
for (inEl = inList; inEl != NULL; inEl = inEl->next)
    {
    char *inName = inEl->name;
    struct lineFile *lf = pslFileOpen(inName);
    while ((psl = pslNext(lf)) != NULL)
	{
	slAddHead(&pslList, psl);
	++pslCount;
	}
    lineFileClose(&lf);
    }
slSort(&pslList, pslCmpQuery);

pslOut = mustOpen(pslName, "w");
gluOut = mustOpen(gluName, "w");
pslWriteHead(pslOut);

/* Chop this up into chunks that share the same query. */
for (psl = pslList; psl != NULL; psl = nextPsl)
    {
    nextPsl = psl->next;
    if (localList != NULL)
	{
	if (!sameString(localList->qName, psl->qName))
	    {
	    glueCount += output(pslOut, gluOut, &localList);
	    localList = NULL;
	    }
	}
    slAddHead(&localList, psl);
    }
glueCount += output(pslOut, gluOut, &localList);
printf("Got %d gluing mRNAs out of %d psls in %d bundles %d ltot %d mtot to %s\n", 
	glueCount, pslCount, outCount, ltot, mtot, gluName);
fclose(pslOut);
fclose(gluOut);
}

void pslCopyInClones(char *listFile, char *partDir, char *outName)
/* Copy in the .psl files corresponding to the clones named in listFile. */
{
struct slName *inList, *inEl;
FILE *out = mustOpen(outName, "w");
struct psl *psl;
int pslCount = 0;
int fileCount = 0;

pslWriteHead(out);
inList = getFileList(listFile, partDir);
for (inEl = inList; inEl != NULL; inEl = inEl->next)
    {
    char *inName = inEl->name;
    struct lineFile *lf = pslFileOpen(inName);
    ++fileCount;
    while ((psl = pslNext(lf)) != NULL)
	{
	pslTabOut(psl, out);
	pslFree(&psl);
	++pslCount;
	}
    lineFileClose(&lf);
    }
printf("%d psls in %d files written to %s\n", pslCount, fileCount, outName);
fclose(out);
}

boolean simpleOut(FILE *out, FILE *glue, struct psl **pList)
/* Write out relevant bits of pList and free pList.
 * Return TRUE if wrote anything. */
{
struct psl *psl, *lastPsl;
struct psl *list;
int minDiff = 15;
boolean isRelevant = FALSE;


slReverse(pList);
++outCount;
ltot += slCount(*pList);

/* Return if size of list less than 2. */
if ((lastPsl = list = *pList) == NULL)
    return FALSE;
if (list->next == NULL)
    return FALSE;

++mtot;
for (psl = lastPsl->next; psl != NULL; psl = psl->next)
    {
    if (psl->qStart - lastPsl->qStart >= minDiff
        && psl->qEnd - lastPsl->qEnd >= minDiff)
        {
        isRelevant = TRUE;
        break;
        }
    }

if (isRelevant)
    {
    for (psl = list; psl != NULL; psl = psl->next)
        {
        pslTabOut(psl, out);
        fprintf(glue, "%s\t%d\t%d\t%s %s\t%d\t%d\n",
            psl->qName, psl->qStart, psl->qEnd,
            psl->tName, psl->strand, psl->tStart, psl->tEnd);
        }
    }
pslFreeList(pList);
return isRelevant;
}

void pslGlue(char *inNames[], int inCount, char *outName, char *glueName)
/* Reduce a psl file to only the gluing components. */
{
FILE *out;
FILE *glue;
struct psl *pslList = NULL, *psl, *nextPsl;
int i;
struct psl *localList = NULL;
int glueCount = 0;

int pslCount = 0;

printf("Reading");
for (i=0; i<inCount; ++i)
    {
    char *inName = inNames[i];
    struct lineFile *lf = pslFileOpen(inName);
    printf(" %s", inName);
    fflush(stdout);
    while ((psl = pslNext(lf)) != NULL)
        {
        slAddHead(&pslList, psl);
        ++pslCount;
        }
    lineFileClose(&lf);
    }
printf("\n");
slSort(&pslList, pslCmpQuery);

out = mustOpen(outName, "w");
glue = mustOpen(glueName, "w");
pslWriteHead(out);

/* Chop this up into chunks that share the same query. */
for (psl = pslList; psl != NULL; psl = nextPsl)
    {
    nextPsl = psl->next;
    if (localList != NULL)
        {
        if (!sameString(localList->qName, psl->qName))
            {
            glueCount += simpleOut(out, glue, &localList);
            localList = NULL;
            }
        }
    slAddHead(&localList, psl);
    }
glueCount += simpleOut(out, glue, &localList);
printf("Got %d gluing mRNAs out of %d psls in %d bundles %d ltot %d mtot\n",
        glueCount, pslCount, outCount, ltot, mtot);
fclose(out);
fclose(glue);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc == 3)
    {
    char subDir[512];
    char *pslDir = argv[2];
    char *inLst = argv[1];
    sprintf(subDir, "%s/mrna", pslDir);
    pslGlueRna(inLst, subDir, "mrna.psl", "mrna.glu");
    sprintf(subDir, "%s/est", pslDir);
    pslGlueRna(inLst, subDir, "est.psl", "est.glu");
    sprintf(subDir, "%s/bacEnds", pslDir);
    pslCopyInClones(inLst, subDir, "bacEnd.psl");
    sprintf(subDir, "%s/pairs", pslDir);
    pslCopyInClones(inLst, subDir, "pairedReads.psl");
    }
else if (argc == 4)
    {
    pslGlue(&argv[1], 1, argv[2], argv[3]);
    }
else
    usage();
return 0;
}
