/* subsetTraces - Build subset of mouse traces that actually align. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "psl.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetTraces - Build subset of mouse traces that actually align\n"
  "usage:\n"
  "   subsetTraces traceDir pslDir subset.fa\n"
  "options:\n"
  "   -abbr=junk - abbreviate names in fa files.\n"
  "   '-tracePat=*.fa' - just use .fa files in traceDir\n"
  "   '-pslPat=*.psl' - just use .psl files in pslDir\n"
  );
}

struct traceTracker
/* Keep track of one psl alignment. */
    {
    struct traceTracker *next;		/* Next in list. */
    char *name;				/* Name of associated trace. */
    boolean found;			/* Trace found? */
    };

void trackTraces(char *pslFile, struct hash *hash, struct traceTracker **pList)
/* Read psl file and add all new query sequence names into hash/list. */
{
struct lineFile *lf = pslFileOpen(pslFile);
struct psl *psl;
struct traceTracker *tt;
uglyf("processing %s\n", pslFile);
while ((psl = pslNext(lf)) != NULL)
    {
    if (hashLookup(hash, psl->qName) == NULL)
        {
	AllocVar(tt);
	hashAddSaveName(hash, psl->qName, tt, &tt->name);
	slAddHead(pList, tt);
	}
    pslFree(&psl);
    }
lineFileClose(&lf);
}

void saveHashedSeqs(char *fileName, struct hash *hash, FILE *f, char *abbr)
/* Read fasta file and copy records that are in hash to f. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
char *line;
int lineSize;
int nameMaxSize = 8;
char *firstLine = needMem(nameMaxSize);
char *name;
int abbrSize = strlen(abbr);
struct traceTracker *tt = NULL;

uglyf("processing %s\n", fileName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	/* Grab first word of line past '>' into 'firstLine' */
	if (lineSize >= nameMaxSize)
	    {
	    freeMem(firstLine);
	    nameMaxSize <<= 1;
	    if (nameMaxSize <= lineSize)
	        nameMaxSize = lineSize + 1;
	    firstLine = needMem(nameMaxSize);
	    }
	memcpy(firstLine, line+1, lineSize-1);
	firstLine[lineSize] = 0;
	name = firstWordInLine(firstLine);
	if (abbrSize > 0 && startsWith(abbr, name))
	    name += abbrSize;
	tt = hashFindVal(hash, name);
	if (tt != NULL)
	    tt->found = TRUE;
	}
    if (tt)
       mustWrite(f, line, lineSize);
    }


freeMem(firstLine);
lineFileClose(&lf);
}

void subsetTraces(char *traceDir, char *pslDir, char *outName)
/* subsetTraces - Build subset of mouse traces that actually align. */
{
char *abbr = cgiUsualString("abbr", "");
char *tracePat = cgiUsualString("tracePat", "*");
char *pslPat = cgiUsualString("pslPat", "*");
struct fileInfo *traceList = NULL, *traceEl, *pslList = NULL, *pslEl;
struct hash *traceHash = newHash(18);
struct traceTracker *ttList = NULL, *tt;
FILE *f = mustOpen(outName, "w");

/* Make hash/list of all traces used in alignments. */
pslList = listDirX(pslDir, pslPat, TRUE);
for (pslEl = pslList; pslEl != NULL; pslEl = pslEl->next)
    {
    if (!pslEl->isDir)
	trackTraces(pslEl->name, traceHash, &ttList);
    }
slReverse(&ttList);
printf("Need %d traces\n", slCount(ttList));

/* Scan through fasta files, saving traces that match. */
traceList = listDirX(traceDir, tracePat, TRUE);
for (traceEl = traceList; traceEl != NULL; traceEl = traceEl->next)
    {
    if (!traceEl->isDir)
        saveHashedSeqs(traceEl->name, traceHash, f, abbr);
    }

/* Check for traces that aren't in files. */
for (tt = ttList; tt != NULL; tt = tt->next)
    if (!tt->found)
        errAbort("%s not found in %s", tt->name, traceDir);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
subsetTraces(argv[1], argv[2], argv[3]);
return 0;
}
