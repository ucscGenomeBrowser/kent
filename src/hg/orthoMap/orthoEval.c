/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "orthoEval.h"
#include "borf.h"
#include "bed.h"
#include "altGraphX.h"


void orthoEvalTabOut(struct orthoEval *ev, FILE *f)
/* Tab out an orthoEval record. Skipping the agxBed, and agx records. */
{
int i=0;
assert(ev);
assert(ev->orthoBedName);
assert(ev->borf);
assert(ev->agxBedName);
fprintf(f, "%s\t", ev->orthoBedName);
bedCommaOutN(ev->orthoBed, 12, f);
fprintf(f, "\t");
borfCommaOut(ev->borf, f);
fprintf(f, "\t");
fprintf(f, "%d\t", ev->basesOverlap);
fprintf(f, "%s\t", ev->agxBedName);
fprintf(f, "%d\t", ev->numIntrons);
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%d,", ev->inCodInt[i]);
fprintf(f, "\t");
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%d,", ev->orientation[i]);
fprintf(f, "\t");
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%d,", ev->support[i]);
fprintf(f, "\t");
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%s,", ev->agxNames[i]);
fprintf(f, "\n");
}

struct orthoEval *orthoEvalLoad(char *row[])
/* Load a row of strings to an orthoEval. */
{
struct orthoEval *oe = NULL;
int sizeOne = 0;
AllocVar(oe);
oe->orthoBedName = cloneString(row[0]);
oe->orthoBed = bedCommaInN(&row[1], NULL, 12);
oe->borf = borfCommaIn(&row[2], NULL);
oe->basesOverlap = sqlUnsigned(row[3]);
oe->agxBedName = cloneString(row[4]);
oe->numIntrons = sqlUnsigned(row[5]);
sqlSignedDynamicArray(row[6], &oe->inCodInt, &sizeOne);
assert(sizeOne == oe->numIntrons);
sqlSignedDynamicArray(row[7], &oe->orientation, &sizeOne);
assert(sizeOne == oe->numIntrons);
sqlSignedDynamicArray(row[8], &oe->support, &sizeOne);
assert(sizeOne == oe->numIntrons);
sqlStringDynamicArray(row[9], &oe->agxNames, &sizeOne);
assert(sizeOne == oe->numIntrons);
return oe;
}

void orthoEvalFree(struct orthoEval **pEv)
{
struct orthoEval *ev = *pEv;
borfFree(&ev->borf);
freez(&ev->inCodInt);
freez(&ev->orientation);
freez(&ev->support);
freez(&ev->agxNames);
altGraphXFree(&ev->agx);
bedFree(&ev->agxBed);
freez(&ev);
*pEv = NULL;
}

struct orthoEval *orthoEvalLoadAll(char *fileName) 
/* Load all orthoEval from a whitespace-separated file.
 * Dispose of this with orthoEvalFreeList(). */
{
struct orthoEval *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[10];

while (lineFileRow(lf, row))
    {
    el = orthoEvalLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}
