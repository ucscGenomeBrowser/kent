/* Snof.c Sorted Name Offset File.
 * This accesses a file of name/offset pairs that are sorted by
 * name.  Does a binary search of file to find the offset given name.
 * Most typically this is used to do a quick lookup given an index file. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "snof.h"
#include "snofmake.h"


void snofClose(struct snof **pSnof)
/* Close down the index file. */
{
struct snof *snof = *pSnof;
if (snof != NULL)
    {
    if (snof->file != NULL)
        fclose(snof->file);
    if (snof->first != NULL)
        freeMem(snof->first);
    freeMem(snof);
    *pSnof = NULL;
    }
}

static boolean makeSnofName(char *snofName, char *rootName)
/* Figure out whether it's .xi or .ix by looking at
 * signature.  Usually just need to open a file to
 * do this once per program run. */
{
static char *suffixes[2] = {".ix", ".xi"};
static char *suff = NULL;
int sigBuf[4];
FILE *f;
int i;

if (suff == NULL)
    {
    for (i=0; i<ArraySize(suffixes); ++i)
	{
	sprintf(snofName, "%s%s", rootName, suffixes[i]);
	if ((f = fopen(snofName, "rb")) != NULL)
	    {
	    if ((fread(sigBuf, sizeof(sigBuf), 1, f)) == 1)
		{
		if (isSnofSig(&sigBuf))
		    {
		    suff = suffixes[i];
		    }
		}
	    fclose(f);
	    }
	if (suff != NULL)
	    break;
	}
    }
if (suff == NULL)
    return FALSE;
sprintf(snofName, "%s%s", rootName, suff);
return TRUE;
}

struct snof *snofOpen(char *indexName)
/* Open up the index file.  Returns NULL if there's any problem. */
{
struct snof *snof;
int sigBuf[4];
FILE *f;
char fileName[512];

if (!makeSnofName(fileName, indexName))
    return NULL;
if ((snof = needMem(sizeof(*snof))) == NULL)
    return NULL;

if ((snof->file = f = fopen(fileName, "rb")) == NULL)
    {
    freeMem(snof);
    return NULL;
    }
if ((fread(sigBuf, sizeof(sigBuf), 1, f)) != 1)
    {
    snofClose(&snof);
    return NULL;
    }
if (!isSnofSig(&sigBuf))
    {
    snofClose(&snof);
    return NULL;
    }
if ((fread(&snof->maxNameSize, sizeof(snof->maxNameSize), 1, f)) != 1)
    {
    snofClose(&snof);
    return NULL;
    }
snof->headSize = ftell(f);
snof->itemSize = snof->maxNameSize + sizeof(unsigned);
if ((snof->first = needMem(5*snof->itemSize)) == NULL)
    {
    snofClose(&snof);
    return NULL;
    }
snof->last = snof->first + snof->itemSize;
snof->less = snof->last + snof->itemSize;
snof->mid = snof->less + snof->itemSize;
snof->more = snof->mid + snof->itemSize;

if (fread(snof->first, snof->itemSize, 1, f) != 1)
    {
    snofClose(&snof);
    return NULL;
    }
fseek(f, -snof->itemSize, SEEK_END);
snof->endIx = (ftell(f)-snof->headSize)/snof->itemSize;
if (fread(snof->last, snof->itemSize, 1, f) != 1)
    {
    snofClose(&snof);
    return NULL;
    }
return snof;
}

struct snof *snofMustOpen(char *indexName)
/* Open up index file or die. */
{
struct snof *snof = snofOpen(indexName);
if (snof == NULL)
    errAbort("Couldn't open index file %s", indexName);
return snof;
}

static long retrieveOffset(char *item, int itemSize)
{
unsigned offset;
memcpy(&offset, item+itemSize-sizeof(offset), sizeof(offset));
return offset;
}

static int snofCmp(char *prefix, char *name, int maxSize, boolean isPrefix)
{
if (isPrefix)
    return memcmp(prefix, name, maxSize);
else
    return strcmp(prefix, name);
}

static boolean snofSearch(struct snof *snof, char *name, int nameSize, 
    boolean isPrefix, int *pIx, char **pNameOffset)
/* Find index of name by binary search.
 * Returns FALSE if no such name in the index file. */
 {
char *startName, *endName, *midName;
int startIx, endIx, midIx;
int cmp;
int itemSize = snof->itemSize;
FILE *f = snof->file;
int headSize = snof->headSize;

/* Truncate name size if necessary. */
if (nameSize > snof->maxNameSize)
    nameSize = snof->maxNameSize;

/* Set up endpoints of binary search */
startName = snof->less;
memcpy(startName, snof->first, itemSize);
endName = snof->more;
memcpy(endName, snof->last, itemSize);
midName = snof->mid;

startIx = 0;
endIx = snof->endIx;

/* Check for degenerate initial case */
if (snofCmp(name, startName, nameSize, isPrefix) == 0)
    {
    *pIx = startIx;
    *pNameOffset = startName;
    return TRUE;
    }
if (snofCmp(name, endName, nameSize, isPrefix) == 0)
    {
    *pIx = endIx;
    *pNameOffset =  endName;
    return TRUE;
    }

/* Do binary search. */
for (;;)
    {
    midIx = (startIx + endIx ) / 2;
    if (midIx == startIx || midIx == endIx)
	{
        *pIx = -1;
        return FALSE;
        }
    fseek(f, headSize + midIx*itemSize, SEEK_SET);
    if (fread(midName, itemSize, 1, f) < 1)
        {
        *pIx = 0;
        return FALSE;
        }
    cmp = snofCmp(name, midName, nameSize, isPrefix);
    if (cmp == 0)
        {
        *pIx = midIx;
        *pNameOffset = midName;
        return TRUE;
        }
    else if (cmp > 0)
	{
	memcpy(startName, midName, itemSize);
	startIx = midIx;
	}
    else
	{
	memcpy(endName, midName, itemSize);
	endIx = midIx;
	}
    }
}

boolean snofFindOffset(struct snof *snof, char *name, long *pOffset)
/* Find offset corresponding with name.  Returns FALSE if no such name
 * in the index file. */
{
char *nameOffset;
int matchIx;
if (!snofSearch(snof, name, strlen(name), FALSE,  &matchIx, &nameOffset))
    {
    *pOffset = matchIx; /* Pass along error code such as it is. */
    return FALSE;
    }
*pOffset = retrieveOffset(nameOffset, snof->itemSize);
return TRUE;
}

boolean snofFindFirstStartingWith(struct snof *snof, char *prefix, int prefixSize,
    int *pSnofIx)
/* Find first index in snof file whose name begins with prefix. */
{
char *nameOffset;
int matchIx;
if (!snofSearch(snof, prefix, prefixSize, TRUE,  &matchIx, &nameOffset))
    {
    *pSnofIx = matchIx; /* Pass along error code such as it is. */
    return FALSE;
    }
while (--matchIx >= 0)
    {
    nameOffset = snofNameAtIx(snof, matchIx);
    if (snofCmp(prefix, nameOffset, prefixSize, TRUE) != 0)
        break;
    }
++matchIx;
*pSnofIx = matchIx;
return TRUE;
}

int snofElementCount(struct snof *snof)
/* How many names are in snof file? */
{
return snof->endIx + 1;
}

long snofOffsetAtIx(struct snof *snof, int ix)
/* The offset of a particular index in file. */
{
char *nameOffset = snofNameAtIx(snof, ix);
return retrieveOffset(nameOffset, snof->itemSize);
}

char *snofNameAtIx(struct snof *snof, int ix)
/* The name at a particular index in file.  (This will be overwritten by
 * later calls to snof system. Strdup if you want to keep it.)
 */
{
fseek(snof->file, snof->headSize + ix*snof->itemSize, SEEK_SET);
if (fread(snof->mid, snof->itemSize, 1, snof->file) != 1 && ferror(snof->file))
    errAbort("snofNameAtIx: fread failed: %s", strerror(ferror(snof->file)));
return snof->mid;
}

void snofNameOffsetAtIx(struct snof *snof, int ix, char **pName, long *pOffset)
/* Get both name and offset for an index. */
{
char *nameOffset = snofNameAtIx(snof, ix);
*pName = nameOffset;
*pOffset = retrieveOffset(nameOffset, snof->itemSize);
}

