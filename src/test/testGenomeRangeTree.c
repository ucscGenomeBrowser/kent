#include "common.h"
#include "options.h"
#include "genomeRangeTree.h"

/* Test program for genomeRangeTree library functions (author: mikep) */

struct genomeRangeTree *t1, *t2;
struct genomeRangeTreeFile *tf1, *tf2;
struct rbTree *rt1, *rt2;
struct range *r1, *r2;
int n;
FILE *f;

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};


void testNewAndFind()
{
verbose(1,"testNewAndFind()\n");
t1 = genomeRangeTreeNew();
if (!t1) 
    errAbort("Error: genomeRangeTreeNew() failed\n");
else
    verbose(1,"OK: genomeRangeTreeNew()\n");
if ( t1->next) 
    errAbort("Error: genomeRangeTreeNew()->next not null (%p)\n",t1->next);
else
    verbose(1,"OK: genomeRangeTreeNew()->next\n");

t2 = genomeRangeTreeNewSize(19);
if (!t2 || t2->hash->size != 1<<19) 
    errAbort("Error: genomeRangeTreeNewSize(19) failed (got size=%d != %d)\n", t2->hash->size, 1<<19);
else
    verbose(1,"OK: genomeRangeTreeNewSize(19)\n");
if ( t2->next) 
    errAbort("Error: genomeRangeTreeNewSize()->next not null (%p)\n",t2->next);
else
    verbose(1,"OK: genomeRangeTreeNewSize()->next\n");

genomeRangeTreeFree(&t2);
if (t2) 
    errAbort("Error: genomeRangeTreeFree failed\n");
else
    verbose(1,"OK: genomeRangeTreeFree\n");

if (t1->hash->elCount != 0) 
    errAbort("Error: genomeRangeTreeNew() should return 0 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeNew() zero elements\n");

rt1 = genomeRangeTreeFindRangeTree(t1, "chrom not here\n");
if (rt1)
    errAbort("Error: genomeRangeTreeFindRangeTree() \n");
else
    verbose(1,"OK: genomeRangeTreeFindRangeTree()\n");

if (t1->hash->elCount != 0)
    errAbort("Error: genomeRangeTreeFindRangeTree() should not create elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeFindRangeTree() 0 elements\n");

rt1 = genomeRangeTreeFindOrAddRangeTree(t1, "test chrom");
if (!rt1)
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree(t1, \"test chrom\") could not create new rbTree\n");
if (t1->hash->elCount != 1) 
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree() should create 1 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree() created 1 elements\n");
if (rt1->next) 
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree()->next not null (%p)\n",rt1->next);
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree()->next\n");

rt2 = (struct rbTree *)hashFindVal(t1->hash, "test chrom");
if (!rt2 || rt1 != rt2)
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree(t1, \"test chrom\") added wrong value\n");
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree() added hash value\n");

rt2 = genomeRangeTreeFindOrAddRangeTree(t1, "test chrom");
if (t1->hash->elCount != 1) 
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree() should leave 1 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree() left 1 elements\n");
if (!rt2 || rt1 != rt2)
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree(t1, \"test chrom\") did not find value\n");
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree() found hash value\n");

rt2 = genomeRangeTreeFindRangeTree(t1, "test chrom");
if (!rt2 || rt1 != rt2)
    errAbort("Error: genomeRangeTreeFindRangeTree(t1, \"test chrom\") did not find value\n");
else
    verbose(1,"OK: genomeRangeTreeFindRangeTree() found hash value\n");

genomeRangeTreeFree(&t1);
if (t1) 
    errAbort("Error: genomeRangeTreeFree failed\n");
else
    verbose(1,"OK: genomeRangeTreeFree\n");
}

void testAddAndList()
{
verbose(1,"testAddAndList()\n");
t1 = genomeRangeTreeNew();
if (!t1) 
    errAbort("Error: genomeRangeTreeNew() failed\n");
else
    verbose(1,"OK: genomeRangeTreeNew()\n");
if (t1->next) 
    errAbort("Error: genomeRangeTreeNew()->next not null (%p)\n",t1->next);
else
    verbose(1,"OK: genomeRangeTreeNew()->next\n");

rt1 = genomeRangeTreeFindOrAddRangeTree(t1, "chr1");
if (!rt1)
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree(t1, chr1) could not create new rangeTree\n");
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree(t1,chr1)\n");
if (t1->hash->elCount != 1) 
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree() should create 1 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree(t1,chr1) left 1 chrom\n");
if (rt1->next) 
    errAbort("Error: genomeRangeTreeFindOrAddRangeTree()->next not null (%p)\n",rt1->next);
else
    verbose(1,"OK: genomeRangeTreeFindOrAddRangeTree()->next\n");

r1 = rangeTreeAdd(rt1, 1, 10);
if (!r1)
    errAbort("Error: rangeTreeAdd(rt1,1,10) could not add range\n");
else
    verbose(1,"OK: rangeTreeAdd(rt1,1,10) #a1\n");
if (r1->next) 
    errAbort("Error: rangeTreeAdd(rt1, 1, 10)->next not null (%p)\n",r1->next);
else
    verbose(1,"OK: rangeTreeAdd(rt1, 1, 10)->next\n");
if (r1->start != 1 || r1->end != 10)
    errAbort("Error: rangeTreeAdd(rt1,1,10) failed start/end [got (%d,%d) wanted (%d,%d)]\n", r1->start, r1->end, 1,10);
else
    verbose(1,"OK: rangeTreeAdd(rt1,1,10) #a2\n");
if (t1->hash->elCount != 1) 
    errAbort("Error: rangeTreeAdd(rt1,1,10) should create 1 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: rangeTreeAdd(rt1,1,10) #a3\n");

r1 = genomeRangeTreeAdd(t1, "chr1", 20, 30);
if (!r1)
    errAbort("Error: genomeRangeTreeAdd(t1,chr1,20,30) could not add range\n");
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr1,20,30) #b1\n");
if (r1->start != 20 || r1->end != 30)
    errAbort("Error: genomeRangeTreeAdd(t1,chr1,20,30) failed start/end [got (%d,%d) wanted (%d,%d)]\n", r1->start, r1->end, 20,30);
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr1,20,30) #b2\n");
if (t1->hash->elCount != 1) 
    errAbort("Error: genomeRangeTreeAdd(t1,chr1,20,30) should have 1 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr1,20,30) #b3\n");

r1 = genomeRangeTreeAdd(t1, "chr2", 1, 10);
if (!r1)
    errAbort("Error: genomeRangeTreeAdd(t1,chr2,1,10) could not add range\n");
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr2,1,10) #c1\n");
if (r1->start != 1 || r1->end != 10)
    errAbort("Error: genomeRangeTreeAdd(t1,chr2,1,10) failed start/end [got (%d,%d) wanted (%d,%d)]\n", r1->start, r1->end, 1,10);
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr2,1,10) #c2\n");
if (t1->hash->elCount != 2) 
    errAbort("Error: genomeRangeTreeAdd(t1,chr2,1,10) should have 2 elements (got %d)\n", t1->hash->elCount);
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr2,1,10) #c3\n");

r1 = genomeRangeTreeList(t1,"chr3");
if (r1)
    errAbort("Error: genomeRangeTreeList(t1,chr3) returned non-null list\n");
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr3) #1\n");

r1 = genomeRangeTreeList(t1,"chr2");
if (!r1)
    errAbort("Error: genomeRangeTreeList(t1,chr2) returned null list\n");
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr2) #2\n");
if (r1->start != 1 || r1->end != 10 || r1->next)
    errAbort("Error: genomeRangeTreeList(t1,chr2) did not return right values [wanted (1,10,null) got (%d,%d,%p)]\n", r1->start, r1->end, r1->next);
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr2) #3\n");

r1 = genomeRangeTreeAdd(t1,"chr1",10,15);
if (!r1)
    errAbort("Error: genomeRangeTreeAdd(t1,chr1,10,15) returned null \n");
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr1,10,15)\n");
//if (r1->start != 1 || r1->end != 15 || r1->next )
//    errAbort("Error: genomeRangeTreeAdd(t1,chr1,9,15) did not merge [wanted (1,15,null) got (%d,%d,%p)]\n", r1->start, r1->end, r1->next);
if (r1->start != 10 || r1->end != 15 || r1->next )
    errAbort("Error: genomeRangeTreeAdd(t1,chr1,10,15) did not merge [wanted (10,15,null) got (%d,%d,%p)]\n", r1->start, r1->end, r1->next);
else
    verbose(1,"OK: genomeRangeTreeAdd(t1,chr1,10,15)\n");

r1 = genomeRangeTreeList(t1,"chr1");
if (!r1)
    errAbort("Error: genomeRangeTreeList(t1,chr1) returned null list\n");
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr1) #4\n");
if (slCount(r1) != 3)
    errAbort("Error: genomeRangeTreeList(t1,chr1) returned wrong number of values (got %d, wanted %d)\n", slCount(r1),3);
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr1) #5\n");
if (r1->start != 1 || r1->end != 10 || !r1->next)
    errAbort("Error: genomeRangeTreeList(t1,chr1) #1 did not return right values [wanted (1,10,0xNNNNNNNN) got (%d,%d,%p)]\n", r1->start, r1->end, r1->next);
else if (r1->next->start != 10 || r1->next->end != 15 || !r1->next->next)
    errAbort("Error: genomeRangeTreeList(t1,chr1) #2 did not return right values [wanted (10,15,0xNNNNNNNN) got (%d,%d,%p)]\n", r1->start, r1->end, r1->next);
else if (r1->next->next->start != 20 || r1->next->next->end != 30 || r1->next->next->next)
    errAbort("Error: genomeRangeTreeList(t1,chr1) #3 did not return right values [wanted (1,10,0xNNNNNNNN) got (%d,%d,%p)]\n", r1->start, r1->end, r1->next);
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr1) #6\n");
}

void testOverlaps()
{
verbose(1,"testOverlaps()\n");
/* Overlaps 
 * ranges: chr1:1-10,val=3  10-15,val=1  20-30,val=1 ; chr2:1-10
 */
t1 = genomeRangeTreeNew();
r1 = genomeRangeTreeAddValCount(t1,"chr1", 1,10);
r1 = genomeRangeTreeAddValCount(t1,"chr1", 1, 5);
r1 = genomeRangeTreeAddValCount(t1,"chr1", 3, 6);
r1 = genomeRangeTreeAddValCount(t1,"chr1",10,15);
r1 = genomeRangeTreeAddValCount(t1,"chr1",20,30);
r1 = genomeRangeTreeAdd(t1,"chr2", 1,10);
/* Check all ranges there */
r1 = genomeRangeTreeList(t1,"chr1");
if (!r1)
    errAbort("Error: genomeRangeTreeList(t1,chr1) returned null list\n");
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr1) #a1\n");
if (slCount(r1) != 3)
    errAbort("Error: genomeRangeTreeList(t1,chr1) returned wrong number of values (got %d, wanted %d)\n", slCount(r1),3);
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr1) #a2\n");
r1 = genomeRangeTreeList(t1,"chr2");
if (!r1)
    errAbort("Error: genomeRangeTreeList(t1,chr2) returned null list\n");
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr2) #a3\n");
if (slCount(r1) != 1)
    errAbort("Error: genomeRangeTreeList(t1,chr2) returned wrong number of values (got %d, wanted %d)\n", slCount(r1),1);
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr2) #a4\n");
r1 = genomeRangeTreeList(t1,"chr3");
if (r1)
    errAbort("Error: genomeRangeTreeList(t1,chr2) returned non-null list\n");
else
    verbose(1,"OK: genomeRangeTreeList(t1,chr3) #a5\n");

/* Check overlap */
if (genomeRangeTreeOverlaps(t1,"chr3",1,100)) /* no overlap */
    errAbort("Error: genomeRangeTreeOverlaps(t1,chr3,1,100) should not overlap\n");
else
    verbose(1,"OK: genomeRangeTreeOverlaps(t1,chr3,1,100) = false\n");

if (genomeRangeTreeOverlaps(t1,"chr2",0,1)) /* no overlap */
    errAbort("Error: genomeRangeTreeOverlaps(t1,chr2,0,1) should not overlap\n");
else
    verbose(1,"OK: genomeRangeTreeOverlaps(t1,chr2,0,1) = false\n");

if (genomeRangeTreeOverlaps(t1,"chr1",16,19)) /* no overlap */
    errAbort("Error: genomeRangeTreeOverlaps(t1,chr1,16,19) should not overlap\n");
else
    verbose(1,"OK: genomeRangeTreeOverlaps(t1,chr1,16,19) = false\n");

if (!genomeRangeTreeOverlaps(t1,"chr1",2,9)) /* overlap */
    errAbort("Error: genomeRangeTreeOverlaps(t1,chr1,2,9) overlap\n");
else
    verbose(1,"OK: genomeRangeTreeOverlaps(t1,chr1,2,9) = true\n");

if (!genomeRangeTreeOverlaps(t1,"chr1",9,21)) /* overlap */
    errAbort("Error: genomeRangeTreeOverlaps(t1,chr1,9,21) overlap\n");
else
    verbose(1,"OK: genomeRangeTreeOverlaps(t1,chr1,9,21) = true\n");

/* Find Enclosing */
r1 = genomeRangeTreeFindEnclosing(t1, "chr3",2,3);
if (r1) /* no overlap */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr3,2,3) not enclosed\n");
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr3,2,3) not enclosed\n");
r1 = genomeRangeTreeFindEnclosing(t1, "chr2",2,3);
if (!r1) /* overlap */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr2,2,3) enclosed by (1-10)\n");
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr3,2,3) enclosed by (1-10)\n");
if (r1->next || r1->start!=1 || r1->end!=10 || r1->val) /* no overlap */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr2,2,3) enclosed wrong [got (%p,%d,%d,%p), wanted ((nil),1,10,(nil))\n",r1->next,r1->start,r1->end,r1->val);
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr2,2,3) enclosed by ((nil),1,10,(nil))\n");

r1 = genomeRangeTreeFindEnclosing(t1, "chr1",12,13);
if (!r1) /* overlap */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed by (10-15)\n");
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed by (10-15)\n");
if ( r1->start!=10 || r1->end!=15 || !r1->val) /* no overlap */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed wrong [got (%d,%d,%p), wanted (10,15,0xNNNNNNNN)\n",r1->start,r1->end,r1->val);
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed by (10,15,0xNNNNNNNN)\n");
if ( *((int *)r1->val) != 1) /* count of ranges in the enclosing range 10-15 */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed wrong [got (%d,%d,(%d)), wanted (10,15,(1))\n",r1->start,r1->end,*((int *)r1->val));
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed by 1 value (10,15,(1))\n");
if ( r1->next) /* this should be zero */
    errAbort("Error: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed wrong [got (%p,%d,%d,%p), wanted ((nil),10,15,(nil))\n",r1->next,r1->start,r1->end,r1->val);
else
    verbose(1,"OK: genomeRangeTreeFindEnclosing(t1,chr1,12,13) enclosed by ((nil),01,15,(nil))\n");

r1 = genomeRangeTreeAllOverlapping(t1, "chr3",1,2);
if (r1) /* no overlap */
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr3,1,2) should not be overlapping\n");
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr3,1,2) not overlapping \n");

r1 = genomeRangeTreeAllOverlapping(t1, "chr2",1,2);
if ( !r1 || r1->next || r1->start!=1 || r1->end!=10 || r1->val) 
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr1,12,13) all overlapping wrong [got (%p,%d,%d,%p), wanted ((nill),1,10,(nil))\n",r1->start,r1->end,r1->val);
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr2,1,2) overlapping (1,10)\n");

r1 = genomeRangeTreeAllOverlapping(t1, "chr1",2,22);
if (!r1) 
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr1,2,22) should be overlapping\n");
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr1,2,22) overlapping \n");

if ( !r1 || !r1->next || r1->start!=1 || r1->end!=10 || !r1->val) 
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr1,2,22) all overlapping wrong [got (%p,%d,%d,%p), wanted (0xNNNNNNNN,1,10,0xNNNNNNNN)\n",r1->next,r1->start,r1->end,r1->val);
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr1,2,22) overlapping (1,10)\n");
if ( *((int *)r1->val) != 3) 
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr1,2,22) all overlapping wrong [got (%p,%d,%d,%p->%d), wanted (0xNNNNNNNN,1,10,0xNNNNNNNN -> 3)\n",r1->next,r1->start,r1->end,r1->val,*((int *)r1->val));
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr1,2,22) overlapping 3 values (1,10)\n");

r1 = r1->next;
if ( !r1 || !r1->next || r1->start!=10 || r1->end!=15 || !r1->val || *((int *)r1->val) != 1) 
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr1,2,22) all overlapping wrong [got (%p,%d,%d,%p->%d), wanted (0xNNNNNNNN,10,15,0xNNNNNNNN->1)\n",r1->next,r1->start,r1->end,r1->val,*((int *)r1->val));
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr1,2,22) overlapping (10,15)\n");

r1 = r1->next;
if ( !r1 || r1->next || r1->start!=20 || r1->end!=30 || !r1->val || *((int *)r1->val) != 1) 
    errAbort("Error: genomeRangeTreeAllOverlapping(t1,chr1,2,22) all overlapping wrong [got (%p,%d,%d,%p->%d), wanted ((nil),20,30,0xNNNNNNNN->1)\n",r1->next,r1->start,r1->end,r1->val,*((int *)r1->val));
else
    verbose(1,"OK: genomeRangeTreeAllOverlapping(t1,chr1,2,22) overlapping (20,30)\n");

r1 = genomeRangeTreeMaxOverlapping(t1, "chr3",2,22);
if (r1) 
    errAbort("Error: genomeRangeTreeMaxOverlapping(t1,chr3,2,22) should not be overlapping\n");
else
    verbose(1,"OK: genomeRangeTreeMaxOverlapping(t1,chr3,2,22) not overlapping \n");

r1 = genomeRangeTreeMaxOverlapping(t1, "chr2",2,3);
if (!r1) 
    errAbort("Error: genomeRangeTreeMaxOverlapping(t1,chr2,2,3) should be overlapping\n");
else
    verbose(1,"OK: genomeRangeTreeMaxOverlapping(t1,chr2,2,3) overlapping \n");
if ( r1->next || r1->start!=1 || r1->end!=10 || r1->val) 
    errAbort("Error: genomeRangeTreeMaxOverlapping(t1,chr2,2,3) max overlapping wrong [got (%p,%d,%d,%p), wanted ((nil),1,10,(nil))\n",r1->next,r1->start,r1->end,r1->val);
else
    verbose(1,"OK: genomeRangeTreeMaxOverlapping(t1,chr2,2,3) overlapping (1,10)\n");

r1 = genomeRangeTreeMaxOverlapping(t1, "chr1",9,22);
if (!r1) 
    errAbort("Error: genomeRangeTreeMaxOverlapping(t1,chr1,9,22) should be overlapping\n");
else
    verbose(1,"OK: genomeRangeTreeMaxOverlapping(t1,chr1,9,22) overlapping \n");
if ( r1->next || r1->start!=10 || r1->end!=15 || !r1->val || *((int *)r1->val) != 1) 
    errAbort("Error: genomeRangeTreeMaxOverlapping(t1,chr1,9,22) max overlapping wrong [got (%p,%d,%d,%p->%d), wanted ((nil),10,15,0xNNNNNNNN->1)\n",r1->next,r1->start,r1->end,r1->val,*((int *)r1->val));
else
    verbose(1,"OK: genomeRangeTreeMaxOverlapping(t1,chr1,9,22) overlapping (10,15)\n");

n = genomeRangeTreeOverlapSize(t1, "chr1",9,22);
if ( n != 8)
    errAbort("Error: genomeRangeTreeOverlapSize(t1,chr1,9,22) wrong [got %d, wanted 8)\n",n);
else
    verbose(1,"OK: genomeRangeTreeOverlapSize(t1,chr1,9,22) overlapping 8\n");
n = genomeRangeTreeOverlapSize(t1, "chr3",9,22);
if ( n != 0)
    errAbort("Error: genomeRangeTreeOverlapSize(t1,chr3,9,22) wrong [got %d, wanted 0)\n",n);
else
    verbose(1,"OK: genomeRangeTreeOverlapSize(t1,chr3,9,22) overlapping 0\n");
n = genomeRangeTreeOverlapSize(t1, "chr2",9,22);
if ( n != 1)
    errAbort("Error: genomeRangeTreeOverlapSize(t1,chr2,9,22) wrong [got %d, wanted 1)\n",n);
else
    verbose(1,"OK: genomeRangeTreeOverlapSize(t1,chr2,9,22) overlapping 1\n");
}


void testAddVar()
{
verbose(1,"testAddVar()\n");
/* Overlaps 
 * ranges: chr1:1-10,val=3  10-15,val=1  20-30,val=1 ; chr2:1-10
 */
t1 = genomeRangeTreeNew();
r1 = genomeRangeTreeAddValCount(t1,"chr1", 1,10);
if (!r1)
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr1,1,10) returned null \n");
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr1,1,10) #1\n");
if (*(int *)r1->val != 1)
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr1,1,10) got val=%d, wanted val=1\n",*(int *)r1->val);
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr1,1,10) #2 val=1\n");

r1 = genomeRangeTreeAddValCount(t1,"chr1", 1, 5);
if (!r1)
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr1,1,5) returned null \n");
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr1,1,5) #3\n");
if (*(int *)r1->val != 2)
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr1,1,5) got val=%d, wanted val=2\n",*(int *)r1->val);
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr1,1,5) #4 val=2\n");

r1 = genomeRangeTreeAddValCount(t1,"chr1", 3, 6);
if (!r1)
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr1,3,6) returned null \n");
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr1,3,6) #5\n");
if (*(int *)r1->val != 3)
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr1,3,6) got val=%d, wanted val=3\n",*(int *)r1->val);
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr1,3,6) #6 val=3\n");

/* Test adding lists to lists */
struct slName *n1 = slNameListFromString("jim,fred,bob", ',');
struct slName *n2 = slNameListFromString("sam,max", ',');
struct slName *n3 = slNameListFromString("neddy", ',');

r1 = genomeRangeTreeAddValList(t1,"chr2",1,9,n1);
if (slCount(r1->val) != 3)
    errAbort("Error: genomeRangeTreeAddValList(t1,chr2,1,9,[jim,fred,bob]) got=%d elements, wanted=3\n",slCount(r1->val));
else
    verbose(1,"OK: genomeRangeTreeAddValList(t1,chr2,1,9) #1 list=3\n");
if (!sameString(slNameListToString(r1->val,','), "jim,fred,bob"))
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr2,1,9,[jim,fred,bob]) -> [%s]\n",slNameListToString(r1->val,','));
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr2,1,9) #2 [%s]\n",slNameListToString(r1->val,','));

r1 = genomeRangeTreeAddValList(t1,"chr2",2,3,n2);
if (slCount(r1->val) != 5)
    errAbort("Error: genomeRangeTreeAddValList(t1,chr2,2,3,[sam,max]) got=%d elements, wanted=5\n",slCount(r1->val));
else
    verbose(1,"OK: genomeRangeTreeAddValList(t1,chr2,2,3) #3 list=5\n");
if (!sameString(slNameListToString(r1->val,','), "jim,fred,bob,sam,max"))
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr2,2,3,[sam,max]) -> [%s]\n",slNameListToString(r1->val,','));
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr2,2,3) #4 [%s]\n",slNameListToString(r1->val,','));

r1 = genomeRangeTreeAddValList(t1,"chr2",6,7,n3);
if (slCount(r1->val) != 6)
    errAbort("Error: genomeRangeTreeAddValList(t1,chr2,6,7,[neddy]) got=%d elements, wanted=6\n",slCount(r1->val));
else
    verbose(1,"OK: genomeRangeTreeAddValList(t1,chr2,6,7) #5 list=6\n");
if (!sameString(slNameListToString(r1->val,','), "jim,fred,bob,sam,max,neddy"))
    errAbort("Error: genomeRangeTreeAddValCount(t1,chr2,6,7,[neddy]) -> [%s]\n",slNameListToString(r1->val,','));
else
    verbose(1,"OK: genomeRangeTreeAddValCount(t1,chr2,6,7) #6 [%s]\n",slNameListToString(r1->val,','));
}

void testReadWrite()
{
verbose(1,"testReadWrite()\n");
t1 = genomeRangeTreeNew();
genomeRangeTreeAdd(t1,"chr1", 3,10);
genomeRangeTreeAdd(t1,"chr1", 1, 9);
genomeRangeTreeAdd(t1,"chr1",20,30);
genomeRangeTreeAdd(t1,"chr2",2,3);
if (t1->hash->elCount != 2 || genomeRangeTreeFindRangeTree(t1, "chr1")->n != 2 || genomeRangeTreeFindRangeTree(t1, "chr2")->n != 1)
    errAbort("Error: genomeRangeTreeAdd(chr1:1-10,chr1:20-30,chr2:2-3) failed\n");
else
    verbose(1,"OK: genomeRangeTreeAdd(chr1:1-10,chr1:20-30,chr2:2-3) \n");

tf1 = genomeRangeTreeFileNew(t1, "out/testGenomeRangeTree.out");
if (!tf1)
    errAbort("Error: genomeRangeTreeFileNew() returned null\n");
else
    verbose(1,"OK: genomeRangeTreeFileNew() #1\n");

genomeRangeTreeFileWriteHeader(tf1);
verbose(1,"OK: genomeRangeTreeFileWriteHeader() #1\n");

genomeRangeTreeFileWriteData(tf1);
verbose(1,"OK: genomeRangeTreeFileWriteData() #1\n");

genomeRangeTreeFileFree(&tf1);
if (tf1)
    errAbort("Error: genomeRangeTreeFileFree() failed\n");
else
    verbose(1,"OK: genomeRangeTreeFileFree() #1\n");

/* genomeRangeTreeRead */
t2 = genomeRangeTreeRead("out/testGenomeRangeTree.out");
if (!t2)
    errAbort("Error: genomeRangeTreeRead() returned null\n");
else
    verbose(1,"OK: genomeRangeTreeRead() #1\n");
if (t2->hash->elCount != 2)
    errAbort("Error: genomeRangeTreeRead() wrong # chroms (got [%s], expected 2 [chr1,chr2])\n", slNameListToString((struct slName *)hashElListHash(t2->hash),','));
else
    verbose(1,"OK: genomeRangeTreeRead() #2 chrom chr1 \n");
r1 = genomeRangeTreeList(t1,"chr1");
if (slCount(r1) != 2)
    errAbort("Error: genomeRangeTreeRead() wrong # ranges (got %d, expected 2)\n", slCount(r1));
else
    verbose(1,"OK: genomeRangeTreeRead() #3 range count\n");
if (!r1->next || r1->start != 1 || r1->end != 10 || r1->val)
    errAbort("Error: genomeRangeTreeRead() wrong range1 (got (%p,%d,%d,%p), expected (0xNNNNNNNN,1,10,null))\n", r1->next, r1->start, r1->end, r1->val);
else
    verbose(1,"OK: genomeRangeTreeRead() #4 range1 \n");
r1 = r1->next;
if (r1->next || r1->start != 20 || r1->end != 30 || r1->val)
    errAbort("Error: genomeRangeTreeRead() wrong range2 (got (%p,%d,%d,%p), expected (null,20,30,null))\n", r1->next, r1->start, r1->end, r1->val);
else
    verbose(1,"OK: genomeRangeTreeRead() #5 range2 \n");

r1 = genomeRangeTreeList(t1,"chr2");
if (slCount(r1) != 1)
    errAbort("Error: genomeRangeTreeRead() wrong # ranges (got %d, expected 1)\n", slCount(r1));
else
    verbose(1,"OK: genomeRangeTreeRead() #3 range count\n");
if (r1->next || r1->start != 2 || r1->end != 3 || r1->val)
    errAbort("Error: genomeRangeTreeRead() wrong range2 (got (%p,%d,%d,%p), expected (null,2,3,null))\n", r1->next, r1->start, r1->end, r1->val);
else
    verbose(1,"OK: genomeRangeTreeRead() #6 range3 \n");

/* now write it to another file and see if they are the same */
genomeRangeTreeWrite(t2, "out/testGenomeRangeTree.out2");
verbose(1,"OK: genomeRangeTreeWrite(t2, out/testGenomeRangeTree.out2) \n");

/* genomeRangeTreeFileReadHeader */
tf1 = genomeRangeTreeFileReadHeader("out/testGenomeRangeTree.out");
if (!tf1)
    errAbort("Error: genomeRangeTreeFileReadHeader() failed\n", slCount(r1));
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader() #1\n");
if (slCount(tf1->chromList) != 2 || !tf1->file || (tf1->sig != 0xf7fb8104 &&  tf1->sig != 0x0481fbf7) 
    || tf1->headerLen != 32 || tf1->numChroms != 2)
    errAbort("Error: genomeRangeTreeFileReadHeader() failed. got=(%d,%p,(%x),%d,%d), expecting=(2,0xNNNNNNNN,(0xf7fb8104 or 0x0481fbf7), 32, 2).\n", slCount(r1), tf1->file, tf1->sig, tf1->headerLen,  tf1->numChroms);
else
    verbose(1,"OK: genomeRangeTreeReadHeader() #2\n");

/* genomeRangeTreeFileReadData */
t2 = genomeRangeTreeFileReadData(tf1);
if (!t2)
    errAbort("Error: genomeRangeTreeFileReadData() returned null\n");
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #1\n");
if (t2->hash->elCount != 2)
    errAbort("Error: genomeRangeTreeFileReadData() wrong # chroms (got [%s], expected 2 [chr1,chr2])\n", slNameListToString((struct slName *)hashElListHash(t2->hash),','));
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #2 chrom chr1 \n");
r1 = genomeRangeTreeList(t1,"chr1");
if (slCount(r1) != 2)
    errAbort("Error: genomeRangeTreeFileReadData() wrong # ranges (got %d, expected 2)\n", slCount(r1));
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #3 range count\n");
if (!r1->next || r1->start != 1 || r1->end != 10 || r1->val)
    errAbort("Error: genomeRangeTreeFileReadData() wrong range1 (got (%p,%d,%d,%p), expected (0xNNNNNNNN,1,10,null))\n", r1->next, r1->start, r1->end, r1->val);
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #4 range1 \n");
r1 = r1->next;
if (r1->next || r1->start != 20 || r1->end != 30 || r1->val)
    errAbort("Error: genomeRangeTreeFileReadData() wrong range2 (got (%p,%d,%d,%p), expected (null,20,30,null))\n", r1->next, r1->start, r1->end, r1->val);
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #5 range2 \n");

r1 = genomeRangeTreeList(t1,"chr2");
if (slCount(r1) != 1)
    errAbort("Error: genomeRangeTreeFileReadData() wrong # ranges (got %d, expected 1)\n", slCount(r1));
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #3 range count\n");
if (r1->next || r1->start != 2 || r1->end != 3 || r1->val)
    errAbort("Error: genomeRangeTreeFileReadData() wrong range2 (got (%p,%d,%d,%p), expected (null,2,3,null))\n", r1->next, r1->start, r1->end, r1->val);
else
    verbose(1,"OK: genomeRangeTreeFileReadData() #6 range3 \n");

}

void testBaseMaskAndOr()
{
t1 = genomeRangeTreeNew();
genomeRangeTreeAdd(t1,"chr1", 3,10);
genomeRangeTreeAdd(t1,"chr1", 1, 9);
genomeRangeTreeAdd(t1,"chr1", 5, 6);
genomeRangeTreeAdd(t1,"chr1",20,22);
genomeRangeTreeAdd(t1,"chr1",26,28);
genomeRangeTreeAdd(t1,"chr1",21,27);
genomeRangeTreeAdd(t1,"chr1",27,30);
genomeRangeTreeAdd(t1,"chr2",2,3);
genomeRangeTreeAdd(t1,"chr3",0xaabbcc,0xaabbcc+0x012345);
if (differentString(genomeRangeTreeToString(t1)->string, "[tree [chr1: (1,10) (20,30)] [chr2: (2,3)] [chr3: (11189196,11263761)]]"))
    errAbort("Error: testBaseMaskAndOr #1 tree doesnt match (got=%s, wanted=%s)\n", genomeRangeTreeToString(t1)->string, "[tree [chr1: (1,10) (20,30)] [chr2: (2,3)] [chr3: (11189196,11263761)]]");
else
    verbose(1,"OK: testBaseMaskAndOr #1 tree=%s\n", genomeRangeTreeToString(t1)->string);
genomeRangeTreeWrite(t1, "out/testGenomeRangeTreeOr1.out");
verbose(1,"OK: genomeRangeTreeWrite() \n");
genomeRangeTreeFree(&t1);
verbose(1,"OK: genomeRangeTreeFree()\n");
t1 = genomeRangeTreeRead("out/testGenomeRangeTreeOr1.out");
if (differentString(genomeRangeTreeToString(t1)->string, "[tree [chr1: (1,10) (20,30)] [chr2: (2,3)] [chr3: (11189196,11263761)]]"))
    errAbort("Error: out/testGenomeRangeTreeOr1.out tree doesnt match (got=%s, wanted=%s)\n", genomeRangeTreeToString(t1)->string, "[tree [chr1: (1,10) (20,30)] [chr2: (2,3)] [chr3: (11189196,11263761)]]");
else
    verbose(1,"OK: out/testGenomeRangeTreeOr1.out tree=%s\n", genomeRangeTreeToString(t1)->string);
if (t1->hash->elCount != 3 || genomeRangeTreeFindRangeTree(t1, "chr1")->n != 2 || genomeRangeTreeFindRangeTree(t1, "chr2")->n != 1 ||  genomeRangeTreeFindRangeTree(t1, "chr3")->n != 1)
    errAbort("Error: genomeRangeTreeAdd(chr1:1-10,chr1:20-30,chr2:2-3,chr3:11189196-11263761) failed\n");
else
    verbose(1,"OK: genomeRangeTreeAdd(chr1:1-10,chr1:20-30,chr2:2-3,chr3:11189196-11263761) \n");

t2 = genomeRangeTreeNew();
genomeRangeTreeAdd(t2,"chr1", 1,10);
genomeRangeTreeAdd(t2,"chr1",20,30);
genomeRangeTreeAdd(t2,"chr2",2,3);
genomeRangeTreeAdd(t2,"chr4",15,255+15);
genomeRangeTreeAdd(t2,"chr5",0x123456,0x123456+0x234567);
genomeRangeTreeWrite(t2, "out/testGenomeRangeTreeOr2.out");
t2 = genomeRangeTreeRead("out/testGenomeRangeTreeOr2.out");
if (differentString(genomeRangeTreeToString(t2)->string, "[tree [chr1: (1,10) (20,30)] [chr2: (2,3)] [chr4: (15,270)] [chr5: (1193046,3504573)]]"))
    errAbort("Error: out/testGenomeRangeTreeOr2.out doesnt match (got=%s, wanted=%s)\n", genomeRangeTreeToString(t2)->string,  "[tree [chr1: (1,10) (20,30)] [chr2: (2,3)] [chr4: (15,270)] [chr5: (1193046,3504573)]]");
else
    verbose(1,"OK: out/testGenomeRangeTreeOr2.out tree=%s\n", genomeRangeTreeToString(t2)->string);

tf1 = genomeRangeTreeFileReadHeader("out/testGenomeRangeTreeOr1.out");
if (!tf1 || tf1->numChroms != 3)
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out) failed\n");
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out) tf1=%p\n", tf1);
if (!tf1->chromList )
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out) failed (tf1->chromList=%p)\n", tf1->chromList);
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out)\n");
if (slCount(tf1->chromList) != 3 )
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out) failed (slCount=%d)\n", slCount(tf1->chromList));
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out)\n");
if (differentString("chr1",tf1->chromList->name) || differentString("chr2",tf1->chromList->next->name)
    || differentString("chr3",tf1->chromList->next->next->name))
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out) failed (name1=%s, name2=%s, name3=%s)\n", tf1->chromList->name, tf1->chromList->next->name, tf1->chromList->next->next->name);
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr1.out) (name1=%s, name2=%s, name3=%s)\n", tf1->chromList->name, tf1->chromList->next->name, tf1->chromList->next->next->name);

tf2 = genomeRangeTreeFileReadHeader("out/testGenomeRangeTreeOr2.out");
if (!tf2 || tf2->numChroms != 4 || !tf2->chromList)
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr2.out) failed (tf2=%p)\n");
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr2.out) tf2=%p\n", tf2);
if (slCount(tf2->chromList) != 4 )
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr2.out) failed (slCount=%d)\n", slCount(tf2->chromList));
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr2.out)\n");
if (differentString("chr1",tf2->chromList->name) || differentString("chr2",tf2->chromList->next->name)\
    || differentString("chr4",tf2->chromList->next->next->name) || differentString("chr5",tf2->chromList->next->next->next->name))
    errAbort("Error: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr2.out) failed (name1=%s, name2=%s, name3=%s, name4=%s)\n", tf2->chromList->name, tf2->chromList->next->name, tf2->chromList->next->next->name, tf2->chromList->next->next->next->name);
else
    verbose(1,"OK: genomeRangeTreeFileReadHeader(out/testGenomeRangeTreeOr2.out) (name1=%s, name2=%s, name3=%s, name4=%s)\n", tf2->chromList->name, tf2->chromList->next->name, tf2->chromList->next->next->name, tf2->chromList->next->next->next->name);

genomeRangeTreeFileOr(tf1, tf2, "out/testGenomeRangeTreeOr_1or2.out");
verbose(1,"OK: genomeRangeTreeFileOr(tf1, tf2, out/testGenomeRangeTreeOr_1or2.out)\n");

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

testNewAndFind();
testAddAndList();
testAddVar();
testOverlaps();
testReadWrite();
testBaseMaskAndOr();

verbose(1,"genomeRangeTree OK\n");
return EXIT_SUCCESS;
}
