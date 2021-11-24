/* qacIndex - Index a qac file. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "qaSeq.h"

struct qacIndex
    {
    struct qacIndex *next;
    char *name;   /* name of the sequence in the qac file */
    off_t offset; /* offset in the qac file */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "qacIndex - Index a qac file\n"
  "usage:\n"
  "   qacIndex in.qac out.qdx\n"
);
}

void qacIndex(char *inQac, char *outQdx)
/* qacIndex - Index a qac file. */
{
FILE *qacf, *qdxf;
struct qaSeq *qa;
struct qacIndex *indexList = NULL, *qaci;
off_t offset;
boolean isSwapped;
int seqCount = 0;

qacf = qacOpenVerify(inQac, &isSwapped);
if ((offset = ftello(qacf)) == (off_t) -1)
    errnoAbort("ftello() failed\n");

while((qa = qacReadNext(qacf, isSwapped)) != NULL)
    {
    seqCount++;
    AllocVar(qaci);
    qaci->name = cloneString(qa->name);
    qaci->offset = offset;
    slAddHead(&indexList, qaci);
    if ((offset = ftello(qacf)) == (off_t) -1)
        errnoAbort("ftello() failed\n");
    }
carefulClose(&qacf);
slReverse(&indexList);

qdxf = mustOpen(outQdx, "w");
fprintf(qdxf, "%d\n", seqCount);
for(qaci = indexList; qaci != NULL; qaci = qaci->next)
    {
    fprintf(qdxf, "%s\t%ld\n", qaci->name, qaci->offset);
    free(qaci->name);
    }
carefulClose(&qdxf);
slFreeList(&indexList);
}

int main(int argc, char *argv[])
{
if (argc != 3)
    usage();
qacIndex(argv[1], argv[2]);
return EXIT_SUCCESS;
}
