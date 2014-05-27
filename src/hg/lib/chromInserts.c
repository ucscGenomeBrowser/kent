/* chromInserts - this module helps handle centromeres, heterochromatic regions
 * and other large gaps in chromosomes. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "chromInserts.h"


static char *dashIsNull(char *s)
/* Return cloned copy of string, or NULL if it's just a dash. */
{
if (sameString(s, "-"))
    return NULL;
else
    return cloneString(s);
}

struct chromInserts *chromInsertsRead(char *fileName, struct hash *insertsHash)
/* Read in inserts file and process it. */
{
struct chromInserts *chromInsertsList = NULL, *chromInserts;
struct bigInsert *insert;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[16], *chrom;
int wordCount;
struct hashEl *hel;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 5, wordCount);
    chrom = words[0];
    if ((chromInserts = hashFindVal(insertsHash, chrom)) == NULL)
        {
	AllocVar(chromInserts);
	hel = hashAdd(insertsHash, chrom, chromInserts);
	chromInserts->chrom = hel->name;
	slAddHead(&chromInsertsList, chromInserts);
	}
    AllocVar(insert);
    insert->ctgBefore = dashIsNull(words[1]);
    insert->ctgAfter = dashIsNull(words[2]);
    insert->size = atoi(words[3]);
    insert->type = cloneString(words[4]);
    if (insert->ctgAfter == NULL)
        chromInserts->terminal = insert;
    else
        {
	slAddTail(&chromInserts->insertList, insert);
	}
    insert->chrom = chromInserts;
    }
lineFileClose(&lf);
slReverse(&chromInsertsList);
return chromInsertsList;
}

struct bigInsert *bigInsertBeforeContig(struct chromInserts *chromInserts, 
	char *contig)
/* Return the big insert (if any) before contig) */
{
struct bigInsert *bi;
if (chromInserts == NULL)
    return NULL;
for (bi = chromInserts->insertList; bi != NULL; bi = bi->next)
    {
    if (sameString(bi->ctgAfter, contig))
        return bi;
    }
return NULL;
}

static int chromInsertsDefaultGapSize = 200000;

void chromInsertsSetDefaultGapSize(int size)
/* Set default gap size. */
{
chromInsertsDefaultGapSize = size;
}

int chromInsertsGapSize(struct chromInserts *chromInserts, char *contig, boolean isFirst)
/* Return size of gap before next contig. */
{
int size = (isFirst ? 0 : chromInsertsDefaultGapSize);
if (chromInserts != NULL)
    {
    struct bigInsert *bi = bigInsertBeforeContig(chromInserts, contig);
    if (bi != NULL)
        size = bi->size;
    }
return size;
}

