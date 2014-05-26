/* ntContig.h - A chain of clones in an NT contig. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "ntContig.h"


static int cmpNtClonePos(const void *va, const void *vb)
/* Compare to sort ntClonePos. */
{
const struct ntClonePos *a = *((struct ntClonePos **)va);
const struct ntClonePos *b = *((struct ntClonePos **)vb);
return a->pos - b->pos;
}

struct ntContig *readNtFile(char *fileName, 
	struct hash *ntContigHash, struct hash *ntCloneHash)
/* Read in NT contig info. (NT contigs are contigs of finished clones.) */
{
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[8];
struct ntContig *contigList = NULL, *contig = NULL;
struct ntClonePos *pos;
char *contigName;
struct hashEl *hel;

/* Parse file into ntContig/ntClonePos data structures. */
lf = lineFileOpen(fileName, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 5)
        errAbort("Expecting 5 words line %d of %s", lf->lineIx, lf->fileName);
    contigName = words[0];
    if (contig == NULL || !sameString(contigName, contig->name))
        {
	AllocVar(contig);
	hel = hashAddUnique(ntContigHash, contigName, contig);
	contig->name = hel->name;
	slAddHead(&contigList, contig);
	}
    AllocVar(pos);
    hel = hashAddUnique(ntCloneHash, words[1], pos);
    pos->name = hel->name;
    pos->ntContig = contig;
    pos->pos = atoi(words[2]);
    pos->orientation = ((words[3][0] == '-') ? -1 : 1);
    pos->size = atoi(words[4]);
    slAddHead(&contig->cloneList, pos);
    }
lineFileClose(&lf);

/* Make sure everything is nicely sorted and sized. */
for (contig = contigList; contig != NULL; contig = contig->next)
    {
    slSort(&contig->cloneList, cmpNtClonePos);
    pos = slLastEl(contig->cloneList);
    contig->size = pos->pos + pos->size;
    }

slReverse(&contigList);
return contigList;
}

