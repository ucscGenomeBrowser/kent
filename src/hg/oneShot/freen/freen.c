/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "vGfx.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file");
}

struct gfHit
/* A genoFind hit. */
   {
   struct gfHit *next;
   bits32 qStart;		/* Where it hits in query. */
   bits32 tStart;		/* Where it hits in target. */
   bits32 diagonal;		/* tStart + qSize - qStart. */
   };

/* Some variables used by recursive function gfHitSort2
 * across all incarnations. */
static struct gfHit **nosTemp, *nosSwap;
int depth;

static void gfHitSort2(struct gfHit **ptArray, int n)
/* This is a fast recursive sort that uses a temporary
 * buffer (nosTemp) that has to be as big as the array
 * that is being sorted. */
{
struct gfHit **tmp, **pt1, **pt2;
int n1, n2;

++depth;

/* Divide area to sort in two. */
n1 = (n>>1);
n2 = n - n1;
pt1 = ptArray;
pt2 =  ptArray + n1;

   {
   int i;
   spaceOut(uglyOut, depth);
   uglyf("n %d, n1 %d, n2 %d\n", n, n1, n2);
   spaceOut(uglyOut, depth);
   for (i=0; i<n; ++i)
       {
       if (i == n1)
          uglyf(" : ");
       uglyf("%d ", ptArray[i]->diagonal);
       }
   uglyf("\n");
   }

/* Sort each area separately.  Handle small case (2 or less elements)
 * here.  Otherwise recurse to sort. */
if (n1 > 2)
    gfHitSort2(pt1, n1);
else if (n1 == 2 && pt1[0]->diagonal > pt1[1]->diagonal)
    {
    nosSwap = pt1[1];
    pt1[1] = pt1[0];
    pt1[0] = nosSwap;
    }
if (n2 > 2)
    gfHitSort2(pt2, n2);
else if (n2 == 2 && pt2[0]->diagonal > pt2[1]->diagonal)
    {
    nosSwap = pt2[1];
    pt2[1] = pt2[0];
    pt2[0] = nosSwap;
    }

   {
   int i;
   spaceOut(uglyOut, depth);
   for (i=0; i<n; ++i)
       {
       if (i == n1)
          uglyf(" : ");
       uglyf("%d ", ptArray[i]->diagonal);
       }
   uglyf("\n");
   }

/* At this point both halves are internally sorted. 
 * Do a merge-sort between two halves copying to temp
 * buffer.  Then copy back sorted result to main buffer. */
tmp = nosTemp;
while (n1 > 0 && n2 > 0)
    {
    if ((*pt1)->diagonal <= (*pt2)->diagonal)
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
assert(tmp + n1 + n2 == nosTemp + n);
if (n1 > 0) uglyf("n1 %d, pt1 = %d\n", n1, (*pt1)->diagonal);
if (n2 > 0) uglyf("n2 %d, pt2 = %d\n", n2, (*pt2)->diagonal);

/* If some of first side left to merge copy it to end of temp buf. */
if (n1 > 0)
    memcpy(tmp, pt1, n1 * sizeof(*tmp));

/* If some of second side left to merge, we finesse it here:
 * simply refrain from copying over it as we copy back temp buf. */
memcpy(ptArray, nosTemp, (n - n2) * sizeof(*ptArray));
--depth;
}

void gfHitSortDiagonal(struct gfHit **pList)
/* Sort a singly linked list with Qsort and a temporary array. */
{
struct gfHit *list = *pList;
if (list != NULL && list->next != NULL)
    {
    int count = slCount(list);
    struct gfHit *el;
    struct gfHit **array;
    int i;
    int byteSize = count*sizeof(*array);
    array = needLargeMem(byteSize);
    nosTemp = needLargeMem(byteSize);
    for (el = list, i=0; el != NULL; el = el->next, i++)
        array[i] = el;
    gfHitSort2(array, count);
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


void addHit(struct gfHit **pList, int qStart, int tStart, int diag)
/* Add hit to list. */
{
struct gfHit *hit;
AllocVar(hit);
hit->qStart = qStart;
hit->tStart = tStart;
hit->diagonal = diag;
slAddHead(pList, hit);
}

struct gfHit *makeHitList()
/* Make test hit list. */
{
struct gfHit *list = NULL;
addHit(&list, 233, 65483, 66133);
addHit(&list, 315, 125829, 126397);
addHit(&list, 420, 124927, 125390);
addHit(&list, 530, 87087, 87440);
addHit(&list, 533, 76318, 76668);
addHit(&list, 649, 66187, 66421);
slReverse(&list);
}

void freen(char *fileName)
/* Print status code. */
{
struct gfHit *list, *hit;

list = makeHitList();
printf("Before\n");
for (hit = list; hit != NULL; hit = hit->next)
    printf("%d, %d, %d\n", hit->diagonal, hit->qStart, hit->tStart);
printf("\n");
gfHitSortDiagonal(&list);
printf("After\n");
for (hit = list; hit != NULL; hit = hit->next)
    printf("%d, %d, %d\n", hit->diagonal, hit->qStart, hit->tStart);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
