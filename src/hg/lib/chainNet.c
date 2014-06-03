/* chainNet - read/write and free nets which are constructed
 * of chains of genomic alignments. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "chainNet.h"


struct cnFill *cnFillNew()
/* Return fill structure with some basic stuff filled in */
{
struct cnFill *fill;
AllocVar(fill);
fill->tN = fill->qN = fill->tR = fill->qR = 
	fill->tNewR = fill->qNewR = fill->tOldR = fill->qOldR = 
	fill->qTrf = fill->tTrf = 
	fill->qOver = fill->qFar = fill->qDup = -1;
return fill;
}

void cnFillFreeList(struct cnFill **pList);

void cnFillFree(struct cnFill **pFill)
/* Free up a fill structure and all of it's children. */
{
struct cnFill *fill = *pFill;
if (fill != NULL)
    {
    if (fill->children != NULL)
        cnFillFreeList(&fill->children);
    freez(pFill);
    }
}

void cnFillFreeList(struct cnFill **pList)
/* Free up a list of fills. */
{
struct cnFill *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cnFillFree(&el);
    }
*pList = NULL;
}

int cnFillCmpTarget(const void *va, const void *vb)
/* Compare to sort based on target. */
{
const struct cnFill *a = *((struct cnFill **)va);
const struct cnFill *b = *((struct cnFill **)vb);
return a->tStart - b->tStart;
}

void chainNetFree(struct chainNet **pNet)
/* Free up a chromosome net. */
{
struct chainNet *net = *pNet;
if (net != NULL)
    {
    freeMem(net->name);
    cnFillFreeList(&net->fillList);
    hashFree(&net->nameHash);
    freez(pNet);
    }
}

void chainNetFreeList(struct chainNet **pList)
/* Free up a list of chainNet. */
{
struct chainNet *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    chainNetFree(&el);
    }
*pList = NULL;
}

struct cnFill *cnFillFromLine(struct hash *nameHash, 
	struct lineFile *lf, char *line)
/* Create cnFill structure from line.  This will chop up
 * line as a side effect. */
{
static char *words[64];
int i, wordCount;
enum {basicFields = 7};
struct cnFill *fill;

wordCount = chopLine(line, words);
lineFileExpectAtLeast(lf, basicFields, wordCount);
fill = cnFillNew();
fill->tStart = lineFileNeedNum(lf, words, 1);
fill->tSize = lineFileNeedNum(lf, words, 2);
fill->qName = hashStoreName(nameHash, words[3]);
fill->qStrand = words[4][0];
fill->qStart = lineFileNeedNum(lf, words, 5);
fill->qSize = lineFileNeedNum(lf, words, 6);
for (i=basicFields; i<wordCount; i += 2)
    {
    char *name = words[i];

    if (sameString(name, "score"))
	fill->score = atof(words[i+1]);
    else if (sameString(name, "type"))
	fill->type = hashStoreName(nameHash, words[i+1]);
    else
	{
	/* Cope with integer values. */
	int iVal = lineFileNeedNum(lf, words, i+1);
	if (sameString(name, "id"))
	    fill->chainId = iVal;
	else if (sameString(name, "ali"))
	    fill->ali = iVal;
	else if (sameString(name, "tN"))
	    fill->tN = iVal;
	else if (sameString(name, "qN"))
	    fill->qN = iVal;
	else if (sameString(name, "tR"))
	    fill->tR = iVal;
	else if (sameString(name, "qR"))
	    fill->qR = iVal;
	else if (sameString(name, "tNewR"))
	    fill->tNewR = iVal;
	else if (sameString(name, "qNewR"))
	    fill->qNewR = iVal;
	else if (sameString(name, "tOldR"))
	    fill->tOldR = iVal;
	else if (sameString(name, "qOldR"))
	    fill->qOldR = iVal;
	else if (sameString(name, "tTrf"))
	    fill->tTrf = iVal;
	else if (sameString(name, "qTrf"))
	    fill->qTrf = iVal;
	else if (sameString(name, "qOver"))
	    fill->qOver = iVal;
	else if (sameString(name, "qFar"))
	    fill->qFar = iVal;
	else if (sameString(name, "qDup"))
	    fill->qDup = iVal;
	}
    }
return fill;
}

struct cnFill *cnFillRead(struct chainNet *net, struct lineFile *lf)
/* Recursively read in list and children from file. */
{
char *line;
int d, depth = 0;
struct cnFill *fillList = NULL;
struct cnFill *fill = NULL;

for (;;)
    {
    if (!lineFileNextReal(lf, &line))
	break;
    d = countLeadingChars(line, ' ');
    if (fill == NULL)
        depth = d;
    if (d < depth)
	{
	lineFileReuse(lf);
	break;
	}
    if (d > depth)
        {
	lineFileReuse(lf);
	fill->children = cnFillRead(net, lf);
	}
    else
        {
	fill = cnFillFromLine(net->nameHash, lf, line);
	slAddHead(&fillList, fill);
	}
    }
slReverse(&fillList);
return fillList;
}

void cnFillWrite(struct cnFill *fillList, FILE *f, int depth)
/* Recursively write out fill list. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    char *type = (fill->chainId ? "fill" : "gap");
    spaceOut(f, depth);
    fprintf(f, "%s %d %d %s %c %d %d", type, fill->tStart, fill->tSize,
    	fill->qName, fill->qStrand, fill->qStart, fill->qSize);
    if (fill->chainId)
        fprintf(f, " id %d", fill->chainId);
    if (fill->score > 0)
        fprintf(f, " score %1.0f", fill->score);
    if (fill->ali > 0)
        fprintf(f, " ali %d", fill->ali);
    if (fill->qOver >= 0)
        fprintf(f, " qOver %d", fill->qOver);
    if (fill->qFar >= 0)
        fprintf(f, " qFar %d", fill->qFar);
    if (fill->qDup >= 0)
        fprintf(f, " qDup %d", fill->qDup);
    if (fill->type != NULL)
        fprintf(f, " type %s", fill->type);
    if (fill->tN >= 0)
        fprintf(f, " tN %d", fill->tN);
    if (fill->qN >= 0)
        fprintf(f, " qN %d", fill->qN);
    if (fill->tR >= 0)
        fprintf(f, " tR %d", fill->tR);
    if (fill->qR >= 0)
        fprintf(f, " qR %d", fill->qR);
    if (fill->tNewR >= 0)
        fprintf(f, " tNewR %d", fill->tNewR);
    if (fill->qNewR >= 0)
        fprintf(f, " qNewR %d", fill->qNewR);
    if (fill->tOldR >= 0)
        fprintf(f, " tOldR %d", fill->tOldR);
    if (fill->qOldR >= 0)
        fprintf(f, " qOldR %d", fill->qOldR);
    if (fill->tTrf >= 0)
        fprintf(f, " tTrf %d", fill->tTrf);
    if (fill->qTrf >= 0)
        fprintf(f, " qTrf %d", fill->qTrf);
    fputc('\n', f);
    if (fill->children)
        cnFillWrite(fill->children, f, depth+1);
    }
}

void chainNetWrite(struct chainNet *net, FILE *f)
/* Write out chain net. */
{
fprintf(f, "net %s %d\n", net->name, net->size);
cnFillWrite(net->fillList, f, 1);
}

struct chainNet *chainNetRead(struct lineFile *lf)
/* Read next net from file. Return NULL at end of file.*/
{
char *line, *words[3];
struct chainNet *net;
int wordCount;

if (!lineFileNextReal(lf, &line))
   return NULL;
if (!startsWith("net ", line))
   errAbort("Expecting 'net' first word of line %d of %s", 
   	lf->lineIx, lf->fileName);
AllocVar(net);
wordCount = chopLine(line, words);
lineFileExpectAtLeast(lf, 3, wordCount);
net->name = cloneString(words[1]);
net->size = lineFileNeedNum(lf, words, 2);
net->nameHash = hashNew(6);
net->fillList = cnFillRead(net, lf);
return net;
}


static void rMarkUsed(struct cnFill *fillList, Bits *bits, int maxId)
/* Recursively mark bits that are used. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId != 0)
	{
	if (fill->chainId >= maxId)
	    errAbort("chainId %d, can only handle up to %d", 
	    	fill->chainId, maxId);
        bitSetOne(bits, fill->chainId);
	}
    if (fill->children != NULL)
	rMarkUsed(fill->children, bits, maxId);
    }
}

void chainNetMarkUsed(struct chainNet *net, Bits *bits, int bitCount)
/* Fill in a bit array with 1's corresponding to
 * chainId's used in net file. */
{
rMarkUsed(net->fillList, bits, bitCount);
}

