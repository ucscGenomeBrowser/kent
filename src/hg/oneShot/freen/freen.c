/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include <unistd.h>
#include <time.h>
#include <dirent.h>

struct numObj
/* A number on a list. */
     {
     struct numObj *next;
     int num;
     };


static void numObjSort2(struct numObj **ptArray, int n);

/* Some variables used by recursive function numObjSort2
 * across all incarnations. */
static struct numObj **nosTemp, *nosSwap;

static void numObjSort2(struct numObj **ptArray, int n)
/* This is a fast recursive sort that uses a temporary
 * buffer (nosTemp) that has to be as big as the array
 * that is being sorted. */
{
struct numObj **tmp, **pt1, **pt2;
int n1, n2;

/* Divide area to sort in two. */
n1 = (n>>1);
n2 = n - n1;
pt1 = ptArray;
pt2 =  ptArray + n1;

/* Sort each area separately.  Handle small case (2 or less elements)
 * here.  Otherwise recurse to sort. */
if (n1 > 2)
    numObjSort2(pt1, n1);
else if (n1 == 2 && pt1[0] > pt1[1])
    {
    nosSwap = pt1[1];
    pt1[1] = pt1[0];
    pt1[0] = nosSwap;
    }
if (n2 > 2)
    numObjSort2(pt2, n2);
else if (n2 == 2 && pt2[0] > pt2[1])
    {
    nosSwap = pt2[1];
    pt2[1] = pt2[0];
    pt2[0] = nosSwap;
    }

/* At this point both halves are internally sorted. 
 * Do a merge-sort between two halves copying to temp
 * buffer.  Then copy back sorted result to main buffer. */
tmp = nosTemp;
while (n1 > 0 && n2 > 0)
    {
    if ((*pt1)->num <= (*pt2)->num)
	{
	--n1;
	*tmp++ = *pt1++;
	}
    else
	{
	--n2;
	*tmp++ = *pt2++;
	}
    }
/* One or both sides are now fully merged. */

/* If some of first side left to merge copy it to end of temp buf. */
if (n1 > 0)
    memcpy(tmp, pt1, n1 * sizeof(*tmp));

/* If some of second side left to merge, we finesse it here:
 * simply refrain from copying over it as we copy back temp buf. */
memcpy(ptArray, nosTemp, (n - n2) * sizeof(*ptArray));
}

void numObjSort(struct numObj **pList)
/* Sort a singly linked list with Qsort and a temporary array. */
{
struct numObj *list = *pList;
if (list != NULL && list->next != NULL)
    {
    int count = slCount(list);
    struct numObj *el;
    struct numObj **array;
    int i;
    int byteSize = count*sizeof(*array);
    array = needLargeMem(byteSize);
    nosTemp = needLargeMem(byteSize);
    for (el = list, i=0; el != NULL; el = el->next, i++)
        array[i] = el;
    numObjSort2(array, count);
    list = NULL;
    for (i=0; i<count; ++i)
        {
        array[i]->next = list;
        list = array[i];
        }
    freez(&array);
    freez(&nosTemp);
    slReverse(&list);
    *pList = list;       
    }
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My Pet Freen\n"
  "usage:\n"
  "   freen inFile\n");
}

int numObjCmp(const void *va, const void *vb)
/* Compare to sort based . */
{
const struct numObj *a = *((struct numObj **)va);
const struct numObj *b = *((struct numObj **)vb);
return (a->num - b->num);
}

int binaryDigits(unsigned val)
/* Figure out how many binary digits needed to represent val. */
{
int digits = 0;
while (val > 0)
    {
    ++digits;
    val >>= 1;
    }
return digits;
}

void doubleSort(struct numObj **pList, int maxVal)
/* Do a preliminary bucket sort followed by a bunch of 
 * qsorts. */
{
int rangeDigits = binaryDigits(maxVal);
int shift = rangeDigits - 14;
struct numObj **buckets;
int i, bucketCount;
int itemCount;
struct numObj *el, *next, *list = NULL;
int ix;

itemCount = slCount(*pList);
// uglyf("MaxVal %d, rangeDigits %d, items %d\n", maxVal, rangeDigits, itemCount);
if (shift < 1 || itemCount < 1024*8)
    {
    slSort(pList, numObjCmp);
    return;
    }
bucketCount = (maxVal >> shift) + 1;
AllocArray(buckets, bucketCount);

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    slAddHead(&buckets[el->num>>shift], el);
    }
for (i=0; i<bucketCount; ++i)
    {
    slSort(&buckets[i], numObjCmp);
    }
for (i=bucketCount-1; i >= 0; i -= 1)
    {
    for (el = buckets[i]; el != NULL; el = next)
        {
	next = el->next;
	slAddHead(&list, el);
	}
    }
slReverse(&list);
freeMem(buckets);
// uglyf("%d items after\n", slCount(list));
*pList = list;
}

void sortTime(int size, boolean doDouble)
/* Make a list of random items of the given size and
 * time how long it takes to sort it. */
{
int i;
struct numObj *list = NULL, *el;
clock_t allocStart, allocEnd, sortStart, sortEnd, 
	countStart, countEnd, freeStart, freeEnd;
struct lm *lm = lmInit(0);

allocStart = clock();
for (i=0; i<size; ++i)
    {
    el = lmAlloc(lm, sizeof(*el));
    el->num = (random()&(0x3fffffff));
    slAddHead(&list, el);
    }
allocEnd = countStart = clock();
slCount(list);
countEnd = sortStart = clock();
if (doDouble)
//    doubleSort(&list, 0x3fffffff);
     numObjSort(&list);
else
    slSort(&list, numObjCmp);
sortEnd = freeStart = clock();
lmCleanup(&lm);
// slFreeList(&list);
freeEnd = clock();

printf("%c %8d %8d %8d %8d %8d\n", 
	(doDouble ?  'd' : 'q'),
	size, allocEnd - allocStart, sortEnd - sortStart,
	freeEnd - freeStart, countEnd - countStart);
}

void freen(int maxSize)
/* freen - My Pet Freen. */
{
int size;
for (size = 1; size <= maxSize; size *= 2)
    {
    sortTime(size, FALSE);
    sortTime(size, TRUE);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2 || !isdigit(argv[1][0]))
    usage();
freen(atoi(argv[1]));
return 0;
}
