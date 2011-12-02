/* fofFa - create a fof index for a list of FA files. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "localmem.h"
#include "sig.h"
#include "fof.h"


struct fofRecord
/* This holds a record of an index file. */
    {
    bits32 offset;  /* Start offset within file. Must be first element.*/
    bits32 size;    /* Sizer within file. */
    UBYTE fileIx;   /* Which file it's in. */
    char name[1];   /* Dynamically allocated to fit actual size. */
    };

struct fof
/* Manage a file offset file - an index which includes the file,
 * the offset, and the size of each item. */
    {
    char *name;                  /* Name of fof given to fofOpen. */
    char *relDir;                /* Directory to apply to index files. */
    char **fileNames;            /* Names of files being indexed. */
    FILE **files;                /* Possibly null file handles of files being indexed. */
    FILE *f;                     /* Index file handle. */
    int fileCount;               /* The number of files being indexed. */
    int endIx;                   /* Last index. */
    int maxNameSize;             /* Size allocated for index field in index. */
    int itemSize;                /* Size of index record. */
    long headSize;               /* Offset to first index record. */
    struct fofRecord *rec;       /* Buffer space for one index record. */
    struct fofRecord *first;     /* First record. */
    struct fofRecord *last;      /* Last record. */
    };


static void readStringZ(FILE *f, char *s, int maxLen)
/* Read in a zero terminated string from file. */
{
int c;
int ix;
maxLen -= 1;    /* room for zero tag. */

for (ix = 0; ix <maxLen; ++ix)
    {
    if ((c = fgetc(f)) == EOF)
        errAbort("Unexpected EOF in readStringZ");
    if (c == 0)
        break;
    s[ix] = c;
    }
if (ix == maxLen)
    errAbort("String too long in readStringZ");
s[ix] = 0;
}


struct fof *fofOpen(char *fofName, char *fofDir)
/* Open up the named fof. fofDir may be NULL.  It should include 
 * trailing '/' if non-null. */
{
bits32 sig, elCount;
bits16 fileCount, maxNameSize;
FILE *f;
char nameBuf[512];
char pathBuf[512];
struct fof *fof;
int i;

/* Handle directory either being something or NULL, and
 * either ending with a slash or not. */
if (fofDir == NULL)
    {
    fofDir = "";
    }

/* Open file, verify signature. */
safef(pathBuf, sizeof(pathBuf), "%s%s", fofDir, fofName);
f = mustOpen(pathBuf, "rb");
mustReadOne(f, sig);
if (sig != fofSig)
    errAbort("Bad signature on %s", pathBuf);
mustReadOne(f, elCount);

/* Read size info and allocate basic fof structure. */
mustReadOne(f, fileCount);
if (fileCount > 12)
    warn("%d files indexed in fof %s!?", fileCount, fofName);
mustReadOne(f, maxNameSize);
if (maxNameSize > 40)
    warn("%d maxName size in fof %s!?", maxNameSize, fofName);
AllocVar(fof);
fof->name = cloneString(fofName);
fof->relDir = cloneString(fofDir);
fof->fileNames = needMem(fileCount * sizeof(fof->fileNames[0]));
fof->files = needMem(fileCount * sizeof(fof->files[0]));
fof->f = f;
fof->fileCount = fileCount;
fof->endIx = elCount-1;
fof->maxNameSize = maxNameSize;
fof->itemSize = sizeof(bits32) +sizeof(bits32) + sizeof(UBYTE) + maxNameSize;
fof->rec = needMem(sizeof(*fof->rec) + maxNameSize);
fof->first = needMem(sizeof(*fof->rec) + maxNameSize);
fof->last = needMem(sizeof(*fof->rec) + maxNameSize);

/* Read in names of files being indexed and figure header size. */
for (i=0; i<fileCount; ++i)
    {
    readStringZ(f, nameBuf, sizeof(nameBuf));
    safef(pathBuf, sizeof(pathBuf), "%s%s", fofDir, nameBuf);
    fof->fileNames[i] = cloneString(pathBuf);
    }
fof->headSize = ftell(f);

/* Read in first and last records. */
mustRead(f, fof->first, fof->itemSize);
fseek(f, fof->headSize + fof->endIx*fof->itemSize, SEEK_SET);
mustRead(f, fof->last, fof->itemSize);

/* All done (files will be opened as needed, not here). */
return fof;
}


void fofClose(struct fof **pFof)
/* Close down the named fof. */
{
struct fof *fof = *pFof;
if (fof != NULL)
    {
    int fileCount = fof->fileCount;
    int i;

    for (i=0; i<fileCount; ++i)
        {
        freeMem(fof->fileNames[i]);
        carefulClose(&fof->files[i]);
        }
    freeMem(fof->name);
    freeMem(fof->fileNames);
    freeMem(fof->files);
    freeMem(fof->rec);
    freeMem(fof->first);
    freeMem(fof->last);
    carefulClose(&fof->f);
    freez(pFof);
    }
}

int fofElementCount(struct fof *fof)
/* How many names are in fof file? */
{
return fof->endIx + 1;
}

static void fofRecToPos(struct fof *fof, int ix, struct fofRecord *rec, struct fofPos *pos)
/* Convert from record to position, opening file for entry if necessary. */
{
int fileIx = rec->fileIx;
FILE *f;

pos->indexIx = ix;
pos->offset = rec->offset;
pos->size = rec->size;
pos->fileName = fof->fileNames[fileIx];
if ((f = fof->files[fileIx]) != NULL)
    {
    pos->f = f;
    }
else
    {
    pos->f = fof->files[fileIx] = mustOpen(fof->fileNames[fileIx], "rb");
    }
return;
}


static int fofCmp(char *prefix, char *name, int maxSize, boolean isPrefix)
/* Compare either prefix of whole string to name. */
{
if (isPrefix)
    return memcmp(prefix, name, maxSize);
else
    return strcmp(prefix, name);
}

static boolean fofSearch(struct fof *fof, char *name, int nameSize, 
    boolean isPrefix, struct fofPos *retPos)
/* Find index of name by binary search.
 * Returns FALSE if no such name in the index file. */
 {
struct fofRecord *rec = fof->rec;
int startIx, endIx, midIx;
int cmp;
int itemSize = fof->itemSize;
FILE *f = fof->f;
int headSize = fof->headSize;

/* Truncate name size if necessary. */
if (nameSize > fof->maxNameSize)
    nameSize = fof->maxNameSize;

/* Set up endpoints of binary search */
startIx = 0;
endIx = fof->endIx;

/* Check for degenerate initial case */
if (fofCmp(name, fof->first->name, nameSize, isPrefix) == 0)
    {
    fofRecToPos(fof, startIx, fof->first, retPos);
    return TRUE;
    }
if (fofCmp(name, fof->last->name, nameSize, isPrefix) == 0)
    {
    fofRecToPos(fof, endIx, fof->last, retPos);
    return TRUE;
    }

/* Do binary search. */
for (;;)
    {
    midIx = (startIx + endIx ) / 2;
    if (midIx == startIx || midIx == endIx)
        return FALSE;
    fseek(f, headSize + midIx*itemSize, SEEK_SET);
    mustRead(f, rec, itemSize);
    cmp = fofCmp(name, rec->name, nameSize, isPrefix);
    if (cmp == 0)
        {
        fofRecToPos(fof, midIx, rec, retPos);
        return TRUE;
        }
    else if (cmp > 0)
	{
	startIx = midIx;
	}
    else
	{
	endIx = midIx;
	}
    }
}

boolean fofFindFirst(struct fof *fof, char *prefix, 
    int prefixSize, struct fofPos *retPos)
/* Find first element with key starting with prefix. */
{
int ix;
struct fofRecord *rec = fof->rec;
FILE *f = fof->f;
int itemSize = fof->itemSize;
int headSize = fof->headSize;

/* Find some record that starts with prefix. */
if (!fofSearch(fof, prefix, prefixSize, TRUE, retPos))
    return FALSE;

/* Backtrack until find one that doesn't start with prefix. */
ix = retPos->indexIx;
while (--ix >= 0)
    {
    fseek(f, headSize + ix*itemSize, SEEK_SET);
    mustRead(f, rec, itemSize);
    if (memcmp(prefix, rec->name, prefixSize) != 0)
        break;
    }

/* Return the first record that does start with prefix. */
++ix;
fseek(f, headSize + ix*itemSize, SEEK_SET);
mustRead(f, rec, itemSize);
fofRecToPos(fof, ix, rec, retPos);
return TRUE;
}


boolean fofFind(struct fof *fof, char *name, struct fofPos *retPos)
/* Find element corresponding with name.  Returns FALSE if no such name
 * in the index file. */
{
return fofSearch(fof, name, strlen(name), FALSE, retPos);
}

void *fofFetch(struct fof *fof, char *name, int *retSize)
/* Lookup element in index, allocate memory for it, and read
 * it.  Returns buffer with element in it, which should be
 * freeMem'd when done. Aborts if element isn't there. */
{
struct fofPos pos;
void *s;

if (!fofFind(fof, name, &pos))
    errAbort("Couldn't find %s in %s", name, fof->name);
s = needLargeMem(pos.size);
fseek(pos.f, pos.offset, SEEK_SET);
mustRead(pos.f, s, pos.size);
*retSize = pos.size;
return s;
}

char *fofFetchString(struct fof *fof, char *name, int *retSize)
/* Lookup element in index, allocate memory for it, read it.
 * Returns zero terminated string with element in it, which 
 * should be freeMem'd when done. Aborts if element isn't there. */
{
struct fofPos pos;
char *s;

if (!fofFind(fof, name, &pos))
    errAbort("Couldn't find %s in %s", name, fof->name);
s = needLargeMem(pos.size+1);
fseek(pos.f, pos.offset, SEEK_SET);
mustRead(pos.f, s, pos.size);
s[pos.size] = 0;
*retSize = pos.size;
return s;
}

/* ------------------- Batch read ------------------------*/
static int cmpOnKey(const void *va, const void *vb)
/* Comparison function for qsort on an array of offset pointers.
 * Sorts on key. */
{
const struct fofBatch *a = *((struct fofBatch **)va);
const struct fofBatch *b = *((struct fofBatch **)vb);
return strcmp(a->key, b->key);
}

static int cmpOnFilePos(const void *va, const void *vb)
/* Comparison function for qsort on an array of offset pointers.
 * Sorts on file then file offset. */
{
const struct fofBatch *a = *((struct fofBatch **)va);
const struct fofBatch *b = *((struct fofBatch **)vb);
int dif = a->f - b->f;
if (dif == 0)
    dif = a->offset - b->offset;
return dif;
}

static void elFromRec(struct fof *fof, struct fofRecord *rec, struct fofBatch *el)
/* Fill in a batch element from record. */
{
FILE *ff;
int fileIx = rec->fileIx;
if ((ff = fof->files[fileIx]) != NULL)
    {
    el->f = ff;
    }
else
    {
    el->f = fof->files[fileIx] = mustOpen(fof->fileNames[fileIx], "rb");
    }
el->offset = rec->offset;
el->size = rec->size;
}

struct fofBatch *fofBatchFind(struct fof *fof, struct fofBatch *list)
/* Look up all of members on list. */
{
struct fofBatch *el;
FILE *f = fof->f;
struct fofRecord *rec = fof->rec;
int itemSize = fof->itemSize;
char *lastKey = "";

slSort(&list, cmpOnKey);
fseek(f, fof->headSize, SEEK_SET);
for (el = list; el != NULL; el = el->next)
    {
    char *key = el->key;
    if (sameString(key, lastKey))
        {
        elFromRec(fof, rec, el);
        }
    else
        {
        for (;;)
            {
            mustRead(f, rec, itemSize);
            if (sameString(key, rec->name))
                {
                elFromRec(fof, rec, el);
                lastKey = key;
                break;
                }
            }
        }
    }
slSort(&list, cmpOnFilePos);
return list;
}


/* ------------------- Write Side ------------------------*/

struct lm *localMem;    /* Local (fast) memory pool. */

struct fofRecList
/* This holds a list of records for an index file. */
    {
    struct fofRecList *next;
    struct fofRecord rec;
    };

static int cmpRecList(const void *va, const void *vb)
/* Comparison function for qsort on an array of offset pointers.
 * Sorts first on name, then on fileIx, then on offset. */
{
const struct fofRecList *a = *((struct fofRecList **)va);
const struct fofRecList *b = *((struct fofRecList **)vb);
int dif;
dif = strcmp(a->rec.name, b->rec.name);
if (dif == 0)
    {
    UBYTE ao = a->rec.fileIx;
    UBYTE bo = b->rec.fileIx;
    if (ao < bo)
        dif =  -1;
    else if (ao > bo)
        dif = 1;
    else
        {
        bits32 ao = a->rec.offset;
        bits32 bo = b->rec.offset;
        if (ao < bo)
            dif =  -1;
        else if (ao == bo)
            dif = 0;
        else
            dif = 1;
        }
    }
return dif;
}

static bits16 maxNameSize;      /* Maximum size we've seen. */

struct fofRecList *newFofRecEl(int fileIx, long offset, long size, char *name, int nameLen)
/* Create a new offset list element. */
{
struct fofRecList *fr;

if (maxNameSize < nameLen)
    maxNameSize = nameLen;
fr = lmAlloc(localMem, sizeof(*fr) + nameLen);
fr->rec.offset = offset;
fr->rec.size = size;
fr->rec.fileIx = fileIx;
memcpy(fr->rec.name, name, nameLen);
return fr;
}

void fofMake(char *inFiles[], int inCount, char *outName, 
    boolean (*readHeader)(FILE *inFile, void *data),
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), 
    void *data, boolean dupeOk)
/* Make an index file
 * Inputs:
 *     inFiles - List of files that you're indexing with header read and verified.
 *     inCount - Size of file list.
 *     outName - name of index file to create
 *     readHeader - function that sets up file to read first record.  May be NULL.
 *     nextRecord - function that reads next record in file you're indexing
 *                  and returns the name of that record.  Returns FALSE at
 *                  end of file.  Can set *rNameLen to zero you want indexer
 *                  to ignore the record. 
 *     data - void pointer passed through to nextRecord.
 *     dupeOk - set to TRUE if you want dupes to not cause squawking
 */
{
FILE *out;
bits32 sig = fofSig;
bits32 elCount = 0;
bits16 fileCount = inCount;
struct fofRecList *recList = NULL, *rl;
int i, fileIx, itemSize;
char *lastName = "";
int maxMod = 10000;

/* Initialize. */
localMem = lmInit(0);
maxNameSize = 0;

/* Read in all records and sort by name. */
for (fileIx = 0; fileIx<inCount; ++fileIx)
    {
    char *inName = inFiles[fileIx];
    FILE *in = mustOpen(inName, "rb");
    bits32 start, end;
    char *name;
    int nameLen;
    int mod = maxMod;

    printf("Processing %s\n", inName);
    if (readHeader)
        readHeader(in, data);
    start = ftell(in);
    while (nextRecord(in, data, &name, &nameLen))
        {
	if (--mod == 0)
	    {
	    putc('.', stdout);
	    fflush(stdout);
	    mod = maxMod;
	    }
        end = ftell(in);
        if (nameLen > 0)
            {
            rl = newFofRecEl(fileIx, start, end-start, name, nameLen);
            slAddHead(&recList, rl);
            }
        start = end;
        }
    fclose(in);
    printf("\n");
    }

printf("sorting\n");
slSort(&recList, cmpRecList);

/* Count up names. */
if (dupeOk)
    elCount = slCount(recList);
else
    {
    lastName = "";
    for (rl = recList; rl != NULL; rl = rl->next)
        {
        char *name = rl->rec.name;
        if (!sameString(name, lastName))
            {
            ++elCount;
            lastName = name;
            }
        }
    }

/* Write out index file. */
printf("Writing %s\n", outName);
out = mustOpen(outName, "wb");
writeOne(out, sig);
writeOne(out, elCount);
writeOne(out, fileCount);
writeOne(out, maxNameSize);
itemSize = sizeof(bits32) +sizeof(bits32) + sizeof(UBYTE) + maxNameSize;
for (i=0; i<inCount; ++i)
    {
    char *name = inFiles[i];
    int len = strlen(name)+1;
    mustWrite(out, name, len);
    }
lastName = "";
for (rl = recList; rl != NULL; rl = rl->next)
    {
    if (!dupeOk)
        {
        char *name = rl->rec.name;
        if (sameString(name, lastName))
            {
            warn("Duplicate %s only saving first.", name);
            continue;
            }
        else
            lastName = name;
        }
    writeOne(out, rl->rec.offset);
    writeOne(out, rl->rec.size);
    writeOne(out, rl->rec.fileIx);
    mustWrite(out, rl->rec.name, maxNameSize);
    }
if (fclose(out) != 0)
    errnoAbort("fclose failed");
/* Clean up. */
lmCleanup(&localMem);
}

