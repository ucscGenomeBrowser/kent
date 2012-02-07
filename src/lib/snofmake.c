/* snofmake - Write out an index file. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "localmem.h"
#include "snofmake.h"
#include "errabort.h"


static jmp_buf errRecover;

static void ourErrAbort()
/* Default error handler. Prints message and exits
 * program. */
{
longjmp(errRecover, -1);
}

static struct lm *lm;

static void initMem()
{
lm = lmInit((1<<16));
}

static void cleanupMem()
{
lmCleanup(&lm);
}

static void *localNeedMem(int size)
{
return lmAlloc(lm, size);
}

struct offsetList
    {
    struct offsetList *next;
    char *name;
    unsigned offset;
    };

static struct offsetList *newOffset(char *name, int nameLen)
/* Return a fresh name list entry. */
{
struct offsetList *nl = localNeedMem(sizeof(*nl));
nl->name = localNeedMem(nameLen+1);
memcpy(nl->name, name, nameLen);
nl->name[nameLen] = 0;
return nl;
}

static int cmpOffsetPointers(const void *va, const void *vb)
/* comparison function for qsort on an array of offset pointers*/
{
struct offsetList **pa, **pb;
struct offsetList *a, *b;
pa = (struct offsetList **)va;
pb = (struct offsetList **)vb;
a = *pa;
b = *pb;
return strcmp(a->name, b->name);
}

static int longestNameSize(struct offsetList *list)
{
struct offsetList *el;
int size, longestSize = 0;

for (el = list; el != NULL; el = el->next)
    {
    size = strlen(el->name);
    if (size > longestSize) 
        {
        longestSize = size;
        }
    }
return longestSize;
}

static struct offsetList *makeOffsetList(FILE *inFile, 
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), void *data)
/* Build up a list of records and their offsets into file. */
{
struct offsetList *list = NULL;
struct offsetList *newEl;
for (;;)
    {
    long offset = ftell(inFile);
    char *name;
    int nameLen;
    if (!nextRecord(inFile, data, &name, &nameLen))
        break;
    if (nameLen > 0)
        {
        newEl = newOffset(name, nameLen);
        newEl->offset = offset;
        newEl->next = list;
        list = newEl;
        }
    }
slReverse(&list);
return list;
}

boolean warnAboutDupes(struct offsetList **array, int size)
/* Since list is sorted it's easy to warn about duplications. */
{
char *name, *prevName;
int i;
boolean ok = TRUE;

if (size < 2)
    return FALSE;
prevName = array[0]->name;
for (i=1; i<size; ++i)
    {
    name = array[i]->name;
    if (!differentWord(name, prevName))
	{
        warn("Duplicate strings: %s %s", prevName, name);
	ok = FALSE;
	}
    prevName = name;
    }
return ok;
}

static void makeIndex(FILE *in, FILE *out, 
    boolean (*nextRecord)(FILE *in, void *data, char **rName, int *rNameLen), void *data,
    boolean dupeOk)
/* Make index.  Throw error if there's a problem. */
{
struct offsetList *list;
int listSize;
struct offsetList **array;
int i;
struct offsetList *el;
int nameSize;
char *nameBuf;
char *indexSig;
int indexSigSize;

printf("Reading input\n");
list = makeOffsetList(in, nextRecord, data);
listSize = slCount(list);
array = localNeedMem(listSize * sizeof(*array));
nameSize = longestNameSize(list)+1;

printf("Got %d offsets %d nameSize.  Sorting...\n", listSize, nameSize);
nameBuf = localNeedMem(nameSize);

/* Make an array of pointers, one for each element in list. */
for (i=0, el = list; i<listSize; i+=1, el = el->next)
    array[i] = el;

/* Sort alphabetically based on name. */
qsort(array, listSize, sizeof(array[0]), cmpOffsetPointers);
if (!dupeOk)
    warnAboutDupes(array, listSize);

/* Write out file header */
printf("Writing index file \n");
snofSignature(&indexSig, &indexSigSize);
mustWrite(out, indexSig, indexSigSize);
mustWrite(out, &nameSize, sizeof(nameSize));
    
/* Write out sorted output */
for (i=0; i<listSize; ++i)
    {
    zeroBytes(nameBuf, nameSize);
    strcpy(nameBuf, array[i]->name);
    mustWrite(out, nameBuf, nameSize);
    mustWrite(out, &array[i]->offset, sizeof(array[i]->offset));
    }
}

boolean snofDupeOkIndex(FILE *inFile, char *outName, 
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), 
    void *data, boolean dupeOk)
/* Make an index file, as in snofMakeIndex, but optionally allow duplicates
 * without complaining. */
{
FILE *outFile;
int status;

/* Initialize */
if ((outFile = fopen(outName, "wb")) == NULL)
    {
    fprintf(stderr, "Couldn't create index file %s\n", outName);
    return FALSE;
    }
initMem();

/* Wrap error recovery around main routine. */
status = setjmp(errRecover);
if (status == 0)
    {
    pushAbortHandler(ourErrAbort);
    makeIndex(inFile, outFile, nextRecord, data, dupeOk);
    }
popAbortHandler();

/* Cleanup. */
fclose(outFile);
cleanupMem();
return status == 0;
}

boolean snofMakeIndex(FILE *inFile, char *outName, 
    boolean (*nextRecord)(FILE *inFile, void *data, char **rName, int *rNameLen), 
    void *data)
/* Make an index file - name/offset pairs that are sorted by name.
 * Inputs:
 *     inFile - open file that you're indexing with header read and verified.
 *     outName - name of index file to create
 *     nextRecord - function that reads next record in file you're indexing
 *                  and returns the name of that record.
 *     data - void pointer passed through to nextRecord.
 *
 * In this implementation this function just is an error recovery wrapper
 * around the local function makeIndex, which does the real work. */
{
return snofDupeOkIndex(inFile, outName, nextRecord, data, FALSE);
}

