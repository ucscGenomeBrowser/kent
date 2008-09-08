#include "common.h"
#include "options.h"
#include "rangeTreeFile.h"

/* Test program for rangeTree library functions (author: mikep) */

struct rbTree *rt1, *rt2;
struct range r1 = {NULL,0,0,NULL}, r2 = {NULL,0,0,NULL};
struct range *r1p, *r2p;
int n,n2;
FILE *f;

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};


void testRangeTreeNewFree()
{
rt1 = rangeTreeNew();
if (!rt1)
    errAbort("Error: rangeTreeNew() failed\n");
else
    verbose(1,"OK: rangeTreeNew()\n");

rangeTreeFree(&rt1);
if (rt1)
    errAbort("Error: rangeTreeFree() failed\n");
else
    verbose(1,"OK: rangeTreeFree()\n");
}

void testRangeReadWriteNoVal()
/* test read/write range WITHOUT VAL */
{
r1.start = 123456;
r1.end = 234567;

f = mustOpen("out/testRangeTree.out","w");
rangeWriteOne(&r1, f);
carefulClose(&f);

if ((n=(int)fileSize("out/testRangeTree.out")) != 8)
    errAbort("Error: rangeWriteOne(r,f) wrote wrong size file (got=%d, expected=8)\n", n);
else
    verbose(1,"OK: rangeWriteOne(r,f) wrote 8 bytes\n");

f = mustOpen("out/testRangeTree.out","r");
r2 = rangeReadOne(f,FALSE);
carefulClose(&f);

if (r2.start != 123456 && r2.end != 123456)
    errAbort("Error: rangeReadOne(f,FALSE) or rangeWriteOne() failed # r1=[start=%0x,size=%0x] r2=[start=%0x,size=%0x] r2(%d,%d)\n",r1.start,r1.end-r1.start,r2.start,r2.end-r2.start,r2.start,r2.end);
else
    verbose(1,"OK: rangeReadOne(f,FALSE) and rangeWriteOne() # r1=[start=%0x,size=%0x] r2=[start=%0x,size=%0x] r2(%d,%d)\n",r1.start,r1.end-r1.start,r2.start,r2.end-r2.start,r2.start,r2.end);

f = mustOpen("out/testRangeTree.out","r");
r2 = rangeReadOne(f,TRUE);
carefulClose(&f);

verbose(2,"# r1=[start=%0x,size=%0x] r2=[start=%0x,size=%0x] r2(%d,%d)\n",r1.start,r1.end-r1.start,r2.start,r2.end-r2.start,r2.start,r2.end);

if (r2.start != 1088553216 && r2.end != 1217659392)
    errAbort("Error: rangeReadOne(f,TRUE) or rangeWriteOne() failed # r1=[start=%0x,size=%0x] r2=[start=%0x,size=%0x] r2(%d,%d)\n",r1.start,r1.end-r1.start,r2.start,r2.end-r2.start,r2.start,r2.end);
else
    verbose(1,"OK: rangeReadOne(f,TRUE) and rangeWriteOne() # r1=[start=%0x,size=%0x] r2=[start=%0x,size=%0x] r2(%d,%d)\n",r1.start,r1.end-r1.start,r2.start,r2.end-r2.start,r2.start,r2.end);
}

void testRangeTreeReadWriteNoVal()
/* test read/write rangeTree WITHOUT VAL */
{
rt1 = rangeTreeNew();

f = mustOpen("out/testRangeTree.out","w");
rangeTreeWriteNodes(rt1,f);
carefulClose(&f);

if ((n=fileSize("out/testRangeTree.out")) != 0)
    errAbort("Error: rangeWriteOne(r,f) wrote wrong size file (got=%d, expected=0)\n", n);
else
    verbose(1,"OK: rangeWriteOne(r,f) wrote 0 bytes\n");

/* tree with 2 nodes (1-10, 20-30) */
rangeTreeAdd(rt1, 1, 9);
rangeTreeAdd(rt1, 3,10);
rangeTreeAdd(rt1,20,30);
if (rt1->n != 2)
    errAbort("Error: rangeTreeAdd() failed for add(1,9)+add(3,10)+add(20,30) (got=%d, expected=2)\n", rt1->n);
else
    verbose(1,"OK: rangeTreeAdd() created 2 nodes\n");

f = mustOpen("out/testRangeTree.out","w");
rangeTreeWriteNodes(rt1,f);
carefulClose(&f);

if ((n=fileSize("out/testRangeTree.out")) != 2*2*sizeof(bits32))
    errAbort("Error: rangeWriteOne(r,f) wrote wrong size file (got=%d, expected=%d)\n", n, 2*2*(int)sizeof(bits32));
else
    verbose(1,"OK: rangeWriteOne(r,f) wrote %d bytes\n", 2*2*(int)sizeof(bits32));

f = mustOpen("out/testRangeTree.out","r");
rt2 = rangeTreeNew();
rangeTreeReadNodes(f, rt2, 2, FALSE);
carefulClose(&f);

if (rt2->n != 2)
    errAbort("Error: rangeTreeReadNodes(f,rt,%d,FALSE) failed (got=%d, expected 2)\n", rt2->n, rt2->n);
else
    verbose(1,"OK: rangeTreeNew()\n");

r1p = rangeTreeList(rt2);
if (slCount(r1p) != 2 || r1p->start != 1 || r1p->end != 10 || r1p->val || !r1p->next)
    errAbort("Error: rangeTreeReadNodes() or rangeTreeWriteNodes() failed. got count=%d, r=(%p,%d,%d,%p); expected count=2, r=(0xNNNNNNNN,1,10,null)\n", slCount(r1p), r1p->next, r1p->start, r1p->end, r1p->val);
else
    verbose(1,"OK: rangeTreeReadNodes()/rangeTreeWriteNodes() #1 r=(1,10) \n");
r1p = r1p->next;
if ( r1p->start != 20 || r1p->end != 30 || r1p->val || r1p->next)
    errAbort("Error: rangeTreeReadNodes() or rangeTreeWriteNodes() failed. got r=(%p,%d,%d,%p); expected r=(null,20,30,null)\n", r1p->next, r1p->start, r1p->end, r1p->val);
else
    verbose(1,"OK: rangeTreeReadNodes()/rangeTreeWriteNodes() #2 r=(20,30) \n");

n = rangeTreeSizeInFile(rt1);
n2 = rangeTreeSizeInFile(rt1);
if (n != 2*2*sizeof(bits32) || n != n2)
    errAbort("Error: rangeTreeSizeInFile() failed. got (n=%d,size(rt2)=%d); expected (n=%d,size(rt2)=%d)\n", n, n2, 2*2*(int)sizeof(bits32), 2*2*(int)sizeof(bits32));
else
    verbose(1,"OK: rangeTreeSizeInFile()=%d \n", 2*2*(int)sizeof(bits32));
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
testRangeTreeNewFree();
testRangeReadWriteNoVal();
testRangeTreeReadWriteNoVal();


verbose(1,"rangeTree OK\n");
return EXIT_SUCCESS;
}
