/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixBlastTrack - fixup problems with blast tracks with buggy protein.lft \n"
  "usage:\n"
  "   fixBlastTrack XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void fixBlastTrack(char *query, char *target, char *outFile)
{
struct lineFile *qlf = pslFileOpen(query);
struct lineFile *tlf = pslFileOpen(target);
struct psl *psl, *queryPsl, *newPslList;
struct hash *pslHash = newHash(0);  
FILE *outStream = mustOpen(outFile, "w");

while ((psl = pslNext(qlf)) != NULL)
    {
    queryPsl = hashFindVal(pslHash, psl->qName);
    if (queryPsl != NULL)
	errAbort("each qName in query psl file must be unique (%s)",psl->qName);

    hashAdd(pslHash, psl->qName, psl);
    }

while ((psl = pslNext(tlf)) != NULL)
    {
    queryPsl = hashFindVal(pslHash, psl->qName);
    if (queryPsl == NULL)
	errAbort("can't find %s in query file",psl->qName);

    if ((queryPsl->qStarts[0] != 0) && (psl->qStarts[0] < queryPsl->blockSizes[0]))
	{
	int qStart, qEnd, tBlock;
	assert(queryPsl->qStart == queryPsl->qStarts[0]);
	qStart =  0;// queryPsl->qStarts[0];
	qEnd = qStart + queryPsl->blockSizes[0];
	psl->qStarts[0] += queryPsl->qStart;
	psl->qStart = psl->qStarts[0];
	tBlock = 1;
	while((tBlock < psl->blockCount) && (psl->qStarts[tBlock] >= qStart) && (psl->qStarts[tBlock] < qEnd))
	    psl->qStarts[tBlock++] += queryPsl->qStart;
	psl->qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	}
    pslTabOut(psl, outStream);
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
fixBlastTrack(argv[1], argv[2], argv[3]);
return 0;
}
