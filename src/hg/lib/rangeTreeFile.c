/* rangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals). It is
 * based on the self-balancing rbTree code.  Use it in
 * place of a bitmap when the total number of ranges
 * is significantly smaller than the number of bits would
 * be. 
 * Beware the several static/global variables which can be
 * changed by various function calls. */

#include "common.h"
#include "limits.h"
#include "localmem.h"
#include "rangeTree.h"
#include "rangeTreeFile.h"


struct range rangeReadOne(FILE *f, boolean isSwapped)
/* Returns a single range from the file */
{
struct range r = {NULL, 0, 0, NULL};
r.start = readBits32(f, isSwapped);
r.end = r.start + readBits32(f, isSwapped);
return r;
}

void rangeWriteOne(struct range *r, FILE *f)
/* Write out one range structure to binary file f.
 * This only writes start and size. */
{
bits32 start = r->start;
bits32 size = r->end-r->start;
writeOne(f, start);
writeOne(f, size);
}

void rangeReadArray(FILE *f, struct rangeStartSize *r, int n, boolean isSwapped)
/* Read 'n' elements of range array (start,size) from file 'f'. */
{
mustRead(f, r, n*sizeof(*r));
if (isSwapped)
    {
    while (n)
	{
	r->start = byteSwap32(r->start);
	r->size = byteSwap32(r->size);
	--n;
	++r;
	}
    }
}

void rangeWriteArray(struct rangeStartSize *r, int n, FILE *f)
/* Write 'n' elements of range array (start,size) to file 'f'. */
{
mustWrite(f, r, n*sizeof(*r));
}

void rangeWriteArrayToBed(char *chrom, struct rangeStartSize *r, int n, boolean withId, boolean mergeAdjacent, FILE *f)
/* Write 'n' elements of range array (start,size) to file 'f' in bed format.
 * If withId then adds an id in the name field in the format 'chrom.N' .
 * If mergeAdjacent then any ranges which are adjacent, and would otherwise appear 
 * on multiple bed lines, are merged into a single bed line. */
{
int i, id = 0;
int start, end;
if (n==0)
    return;
start = r[0].start;
end = r[0].start+r[0].size;
for (i=1 ; i<n ; ++i)
    {
    if (end > r[i].start)
	errAbort("overlapping bed records in chrom %s (start,end=%d) (start=%d,end=%d)\n", chrom, end, r[i].start, r[i].start+r[i].size);
    if (mergeAdjacent && end == r[i].start) /* keep the previous start and dont write bed */
	{
	end = r[i].start+r[i].size;
	}
    else /* print the previous range and update */
	{
	if (withId)
	    fprintf(f, "%s\t%d\t%d\t%s.%d\n", chrom, start, end, chrom, ++id);
	else
	    fprintf(f, "%s\t%d\t%d\n", chrom, start, end);
	start = r[i].start;
	end = r[i].start+r[i].size;
	}
    }
/* print the last item */
if (withId) 
    fprintf(f, "%s\t%d\t%d\t%s.%d\n", chrom, start, end, chrom, ++id);
else
    fprintf(f, "%s\t%d\t%d\n", chrom, start, end);
}

unsigned rangeArraySize(struct rangeStartSize *r, int n)
/* calculate the total size of the array */
{
unsigned size = 0;
int i;
for ( i = 0 ; i<n ; ++i )
    {
    size += r[i].size;
    }
return size;
}

void rangeReadWriteN(FILE *inF, int n, boolean isSwapped, FILE *outF)
/* Read 'n' ranges in from file 'inF' and write them to file 'outF'.
 * Reads and writes ranges one at a time. */
{
int i;
struct range r;
for (i=0 ; i<n ; ++i )
    {
    r = rangeReadOne(inF, isSwapped);
    rangeWriteOne(&r, outF);
    }
}

struct range rangeReadOneWithVal(FILE *f, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped))
/* Read one range structure from binary file f, including range val.
 * Returns start, end.
 * valWriteOne should point to a function which writes the value (not called if null).
 * Returns val if valWriteOne is not null */
{
struct range r = {NULL, 0, 0, NULL};
r.start = readBits32(f, isSwapped);
r.end = r.start + readBits32(f, isSwapped);
if (valReadOne)
    r.val = valReadOne(f, isSwapped);
return r;
}

void rangeWriteOneWithVal(struct range *r, FILE *f, void (*valWriteOne)(void *val, FILE *f))
/* Write out one range structure to binary file f.
 * Writes start and size.
 * valWriteOne should point to a function which writes the value (not called if null). */
{   
rangeWriteOne(r, f);
if (valWriteOne)
	valWriteOne(r->val, f);
}

/* Some file-globals to be used by rangeTreeWriteFile, rangeTreeWriteFileWithVal, rangeTreeSizeInFileWithVal */
static FILE *tempF = NULL;
static void (*tempFuncWriteNodesWithVal)(void *val, FILE *f) = NULL;
static int tempValSize = 0;
static int (*tempFuncRangeValSize)(void *val) = NULL;

static void rangeWriteFile(void *item)
/* Utility function to pass file handle. See rangeTreeWriteOne() below.
 * Not thread-safe. */
{
rangeWriteOne(item, tempF);
}

static void rangeWriteFileWithVal(void *item)
/* Utility function to pass file handle and func ptr. See rangeTreeWriteOneWithVal() below.
 * Not thread-safe. */
{
rangeWriteOneWithVal(item, tempF, tempFuncWriteNodesWithVal);
}

static void rangeValSize(void *val) 
/* Utility function to calculate binary file size of range val.
 * Not thread-safe. */
{
tempValSize += tempFuncRangeValSize(val);
}

void rangeTreeReadNodes(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped)
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * Does not read range val.  */
{
struct range r;
int i;
for (i = 0 ; i<numNodes ; ++i)
    {
    r = rangeReadOne(f, isSwapped);
    rangeTreeAdd(rt, r.start, r.end);
    }
}

void rangeTreeWriteNodes(struct rbTree *tree, FILE *f)
/* Write out one rangeTree structure to binary file f. 
 * Note this does not include the name, which is stored only in index. 
 * Ranges are written in start sequence (depth-first tree traversal).
 * Writes start and size but not val. 
 * Not thread-safe. */
{
tempF = f;
rbTreeTraverse(tree, rangeWriteFile);
}

static void *assignVal(void *existing, void *new)
/* Local helper function to assign range val in case of reading rangeTree 
 * with guaranteed non-overlapping ranges on disk (no merging of values would occur).
 * Simply assignes new value and ignores null existing value.
 * Produces error if existing value is non-null. */
{
if (existing != NULL)
    errAbort("assignVal found existing val (%p) where NULL expected\n", existing);
return new;
}

void rangeTreeReadNodesWithVal(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped))
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * If rt contains no nodes, and since rangeTree was saved to disk 
 * implying its ranges are already non-overlapping, it is safe 
 * to use a mergeVal function which simply assigns the stored value 
 * to the range since the existing val must always be null.
 * Produces an error if the rt contains nodes already. */
{
if (rt->n > 0)
    errAbort("Range tree already contains %d ranges. Use rangeTreeReadOneWithValMerge().\n", rt->n);
rangeTreeReadNodesWithValMerge(f, rt, numNodes, isSwapped, valReadOne, assignVal);
}

void rangeTreeReadNodesWithValMerge(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped), void *(*mergeVals)(void *existing, void *new) )
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * Reads range values using valReadOne function, if function is non-null.
 * Input rangeTree rt could already have nodes, so mergeVals is called 
 * to merge values read from disk to values in the tree.  */
{
struct range r;
int i;
for (i = 0 ; i<numNodes ; ++i)
    {
    r = rangeReadOneWithVal(f, isSwapped, valReadOne);
    rangeTreeAddVal(rt, r.start, r.end, r.val, mergeVals);
    }
}

void rangeTreeWriteNodesWithVal(struct rbTree *tree, FILE *f, void (*valWriteOne)(void *val, FILE *f))
/* Write out one rangeTree structure to binary file f. 
 * Note this does not include the name, which is stored only in index. 
 * Ranges are written in start sequence (depth-first tree traversal).
 * Writes start and size.
 * valWriteOne should be a function which writes the range val. Not called if null. 
 * Not thread-safe. */
{
tempF = f;
tempFuncWriteNodesWithVal = valWriteOne;
rbTreeTraverse(tree, rangeWriteFileWithVal);
}

int rangeTreeSizeInFile(struct rbTree *tree)
/* Returns size of rangeTree written in binary file format.
 * Includes start and size. 
 * Does not include val. */
{
return sizeof(bits32)*2*tree->n;
}

int rangeTreeSizeInFileWithVal(struct rbTree *tree, int (*rangeValSizeInFile)(void *val))
/* Returns size of rangeTree written in binary file format.
 * Includes start, size, and val. 
 * rangeValSizeInFile should refer to a function which calculates the size of the val 
 * in a binary file. Not called if null. */
{
tempValSize = 0;
tempFuncRangeValSize = rangeValSizeInFile;
rbTreeTraverse(tree, rangeValSize);
return rangeTreeSizeInFile(tree) + tempValSize;
}

unsigned rangeTreeFileIntersection(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, struct rangeStartSize **pRange, boolean saveMem, struct bed **pBed, char *chrom, struct lm *lm, int *n)
/* Create intersection of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges (start,size) in r2.
 * If pRange is not null, the ranges are saved into an allocated array 'r' 
 * and then returned in *pRange. 
 * Note that the ranges in *pRange are as stored on disk (start,size), 
 * not as in the rangeTree (start,end).
 * If saveMem is true, tries to shrink size of array r before returning it.
 * Free array with freez(&r).
 * If pBed is not null, the ranges are saved as a list of bed records
 * and returned in *pBed. The bed->chrom field is set to chrom. Memory 
 * for bed records is allocated from lm. Free using lmCleanup.
 * The number of merged ranges is stored in n.
 * If no ranges, returns NULL in *pRange and *pBed.
 * Returns total size of ranges in intersection or zero if none. */
{
int r1end, r2end, i = 0;
unsigned totSize = 0;
struct rangeStartSize *r = NULL;
int rSize = max(n1,n2); /* max size needed is larger of two */
struct bed *bList = NULL;
if (pBed && !lm)
    errAbort("specify localmem when requesting bed output");
if (pBed && !chrom)
    errAbort("specify chrom when requesting bed output");
if (pRange)
    {
    AllocArray(r, rSize);
    }
/* loop around merging while we have ranges in both lists */
while ( n1 > 0 && n2 > 0)
    {
    verbose(2,"loop n1=%d n2=%d\n", n1, n2);
    r1end = r1->start+r1->size;
    r2end = r2->start+r2->size;
    /* check which should be merged or output */
    if ( r1end <= r2->start ) /* r1 is upstream, get next one */
	{
	--n1;
	++r1;
	}
    else if (r2end <= r1->start) /* r2 is upstream, get next one */
	{
	--n2;
	++r2;
	}
    else 
    /* Overlap: take the max(start) and min(end) as the intersection */
    /* Overlap: keep the one with rightmost end, and extend its start position
     * to the left if need be. Note that the next element from this rangeTree 
     * will not overlap. 
     * Read the next element from the other rangeTree as it may overlap. 
     * If they both end on the same base, output the largest of the two
     * and read in the next from each rangeTree. */
	{ 
	if (r1end > r2end) /* 1 is rightmost, get highest start, fix size, and read next r2 */
	    {
	    int start = max(r1->start,r2->start);
	    int l = r2end-start;
	    if(pBed)
		{
		struct bed *b;
		lmAllocVar(lm, b);
		b->chrom = chrom;
		b->chromStart = start;
		b->chromEnd = r2end;
		slAddHead(&bList, b);
		}
	    if (r)
		{
		r[i].start = start;
		r[i].size = l;
		}
	    totSize += l;
	    ++i;
	    --n2;
	    ++r2;
	    }
	else if (r2end > r1end) /* 2 is rightmost, set lowest start, fix size, and read next r1 */
	    {
	    int start = max(r1->start,r2->start);
	    int l = r1end-start;
            if(pBed)
                {
		struct bed *b;
                lmAllocVar(lm, b);
                b->chrom = chrom;
                b->chromStart = start;
                b->chromEnd = r1end;
                slAddHead(&bList, b);
                }
	    if (r)
		{
		r[i].start = start;
		r[i].size = l;
		}
	    totSize += l;
	    ++i;
	    --n1;
	    ++r1;
	    }
	else /* 1 and 2 both end on same base, write out smallest one, and read both */
	    {
            int start = max(r1->start,r2->start);
            int l = r1end-start;
            if(pBed)
                {
		struct bed *b;
                lmAllocVar(lm, b);
                b->chrom = chrom;
                b->chromStart = start;
                b->chromEnd = r1end;
                slAddHead(&bList, b);
                }
	    if (r)
		{
		r[i].start = start;
		r[i].size = l;
		}
            totSize += l;
            ++i;
	    --n1;
	    ++r1;
	    --n2;
	    ++r2;
	    }
	}
    }
*n = i;
if (r && *n == 0) /* if no results, free this memory */
    freez(&r);
else if (r && saveMem && *n < rSize/2) /* we seriously overestimated mem reqts so free it up */
    {
    struct rangeStartSize *r0 = CloneArray(r, *n);
    freez(&r);
    r = r0;
    }
if (pBed)
    {
    slReverse(&bList);
    *pBed = bList;
    }
if (pRange)
    *pRange = r;
return totSize;
}

unsigned rangeTreeFileUnion(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, struct rangeStartSize **pRange, boolean saveMem, struct bed **pBed, char *chrom, struct lm *lm, int *n)
/* Create union of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges (start,size) in r2.
 * If pRange is not null, the ranges are saved into an allocated array 'r' 
 * and then returned in *pRange. 
 * Note that the ranges in *pRange are as stored on disk (start,size), 
 * not as in the rangeTree (start,end).
 * If saveMem is true, tries to shrink size of array r before returning it.
 * Free array with freez(&r).
 * If pBed is not null, the ranges are saved as a list of bed records
 * and returned in *pBed. The bed->chrom field is set to chrom. Memory 
 * for bed records is allocated from lm. Free using lmCleanup.
 * The number of merged ranges is stored in n.
 * If no ranges, returns NULL in *pRange and *pBed.
 * Returns total size of ranges in intersection or zero if none. */
{
int r1end, r2end, i = 0; 
unsigned totSize = 0;
struct rangeStartSize *r = NULL;
int rSize = n1 + n2; /* worst case these dont overlap at all */
struct bed *bList = NULL;
if (pBed && !lm)
    errAbort("specify localmem when requesting bed output");
if (pBed && !chrom)
    errAbort("specify chrom when requesting bed output");
if (pRange)     
    {           
    AllocArray(r, rSize);
    }       
/* loop around merging while we have ranges in both lists */
while ( n1 > 0 && n2 > 0)
    {
    r1end = r1->start+r1->size;
    r2end = r2->start+r2->size;
    /* check which should be merged or output */
    if ( r1end <= r2->start ) /* r1 is upstream, write it and get next one */
	{
        if(pBed)
            {
            struct bed *b;
            lmAllocVar(lm, b);
            b->chrom = chrom;
            b->chromStart = r1->start;
            b->chromEnd = r1->start+r1->size;
            slAddHead(&bList, b);
            }
	if (r)
	    {
	    r[i].start = r1->start;
	    r[i].size = r1->size;
	    }
	totSize += r1->size;
	++i;
	--n1;
	++r1;
	}
    else if (r2end <= r1->start) /* r2 is upstream, write it and get next one */
	{
        if(pBed)
            {
            struct bed *b;
            lmAllocVar(lm, b);
            b->chrom = chrom;
            b->chromStart = r2->start;
            b->chromEnd = r2->start+r2->size;
            slAddHead(&bList, b);
            }
	if (r)
	    {
	    r[i].start = r2->start;
	    r[i].size = r2->size;
	    }
	totSize += r2->size;
	++i;
	--n2;
	++r2;
	}
    else 
    /* Overlap: keep the one with rightmost end, and extend its start position
     * to the left if need be. Note that the next element from this rangeTree 
     * will not overlap. 
     * Read the next element from the other rangeTree as it may overlap. 
     * If they both end on the same base, output the largest of the two
     * and read in the next from each rangeTree. */
	{ 
	if (r1end > r2end) /* 1 is rightmost, set lowest start, fix size, and read next r2 */
	    {
	    if (r2->start < r1->start)
		{
		r1->start = r2->start;
		r1->size = r1end - r1->start;
		}
	    --n2;
	    ++r2;
	    }
	else if (r2end > r1end) /* 2 is rightmost, set lowest start, fix size, and read next r1 */
	    {
	    if (r1->start < r2->start)
		{
		r2->start = r1->start;
		r2->size = r2end - r2->start;
		}
	    --n1;
	    ++r1;
	    }
	else /* 1 and 2 both end on same base, write out left-most one */
	    {
	    if (r1->start <= r2->start)
		{
		if(pBed)
		    {
		    struct bed *b;
		    lmAllocVar(lm, b);
		    b->chrom = chrom;
		    b->chromStart = r1->start;
		    b->chromEnd = r1->start+r1->size;
		    slAddHead(&bList, b);
		    }
		if (r)
		    {
		    r[i].start = r1->start;
		    r[i].size = r1->size;
		    }
		totSize += r1->size;
		}
	    else
		{
                if(pBed)
                    {
                    struct bed *b;
                    lmAllocVar(lm, b);
                    b->chrom = chrom;
                    b->chromStart = r2->start;
                    b->chromEnd = r2->start+r2->size;
                    slAddHead(&bList, b);
                    }
		if (r)
		    {
		    r[i].start = r2->start;
		    r[i].size = r2->size;
		    }
		totSize += r2->size;
		}
	    ++i;
	    --n1;
	    ++r1;
	    --n2;
	    ++r2;
	    }
	}
    }
/* write out any remaining nodes that are either in n1 or n2 */
while (n1)
    {
    if(pBed)
        {
        struct bed *b;
        lmAllocVar(lm, b);
        b->chrom = chrom;
        b->chromStart = r1->start;
        b->chromEnd = r1->start+r1->size;
        slAddHead(&bList, b);
        }
    if (r)
	{
	r[i].start = r1->start;
	r[i].size = r1->size;
	}
    totSize += r1->size;
    ++i;
    --n1;
    ++r1;
    }
while (n2)
    {
    if(pBed)
        {
        struct bed *b;
        lmAllocVar(lm, b);
        b->chrom = chrom;
        b->chromStart = r2->start;
        b->chromEnd = r2->start+r2->size;
        slAddHead(&bList, b);
        }
    if (r)
	{
	r[i].start = r2->start;
	r[i].size = r2->size;
	}
    totSize += r2->size;
    ++i;
    --n2;
    ++r2;
    }
if (i == 0) /* if no results, free this memory */
    freez(&r);
else if (saveMem && i < rSize/2) /* we seriously overestimated mem reqts so free it up */
    {
    struct rangeStartSize *r0 = CloneArray(r, i);
    freez(&r);
    r = r0;
    }
*n = i;
if (pBed)
    {
    slReverse(&bList);
    *pBed = bList;
    }
if (pRange)
    *pRange = r;
return totSize;
}


unsigned rangeTreeFileUnionToFile(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, FILE *of, int *n)
/* Create union of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges in r2, writing them to output file 'of' and returning 
 * the number of merged ranges written in 'n'. 
 * Note that the ranges are as stored on disk (start,size)
 * not as in the rangeTree (start,end).
 * Writes the ranges one-by-one.
 * Returns total size of ranges in merged file. */
{
int r1end, r2end;
unsigned size = 0;
*n = 0;
/* loop around merging while we have ranges in both lists */
while ( n1 > 0 && n2 > 0)
    {
    r1end = r1->start+r1->size;
    r2end = r2->start+r2->size;
    /* check which should be merged or output */
    if ( r1end <= r2->start ) /* r1 is upstream, write it and get next one */
	{
	if (of)
	    mustWrite(of, r1, sizeof(*r1));
	size += r1->size;
	++(*n);
	--n1;
	++r1;
	}
    else if (r2end <= r1->start) /* r2 is upstream, write it and get next one */
	{
	if (of)
	    mustWrite(of, r2, sizeof(*r2));
	size += r2->size;
	++(*n);
	--n2;
	++r2;
	}
    else 
    /* Overlap: keep the one with rightmost end, and extend its start position
     * to the left if need be. Note that the next element from this rangeTree 
     * will not overlap. 
     * Read the next element from the other rangeTree as it may overlap. 
     * If they both end on the same base, output the largest of the two
     * and read in the next from each rangeTree. */
	{ 
	if (r1end > r2end) /* 1 is rightmost, set lowest start, fix size, and read next r2 */
	    {
	    if (r2->start < r1->start)
		{
		r1->start = r2->start;
		r1->size = r1end - r1->start;
		}
	    --n2;
	    ++r2;
	    }
	else if (r2end > r1end) /* 2 is rightmost, set lowest start, fix size, and read next r1 */
	    {
	    if (r1->start < r2->start)
		{
		r2->start = r1->start;
		r2->size = r2end - r2->start;
		}
	    --n1;
	    ++r1;
	    }
	else /* 1 and 2 both end on same base, write out left-most one */
	    {
	    if (r1->start <= r2->start)
		{
		if (of)
		    mustWrite(of, r1, sizeof(*r1));
		size += r1->size;
		}
	    else
		{
		if (of)
		    mustWrite(of, r2, sizeof(*r2));
		size += r2->size;
		}
	    ++(*n);
	    --n1;
	    ++r1;
	    --n2;
	    ++r2;
	    }
	}
    }
/* write out any remaining nodes that are either in n1 or n2 */
while (n1)
    {
    if (of)
	mustWrite(of, r1, sizeof(*r1));
    size += r1->size;
    ++(*n);
    --n1;
    ++r1;
    }
while (n2)
    {
    if (of)
	mustWrite(of, r2, sizeof(*r2));
    size += r2->size;
    ++(*n);
    --n2;
    ++r2;
    }
return size;
}


