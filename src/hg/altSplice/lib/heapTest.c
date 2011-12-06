#include "common.h"
#include "heap.h"


int randomVals[] = { 2, 2, 50, 100, -1, 0, 8, 8, 5, 10, };

struct slInt {
    /* list of ints, should probably switch to slRef but harder to debug */
    struct slInt *next;
    int val;
};

int slIntCmp(const void *va, const void *vb)
/* Compare based on val. */
{
const struct slInt *a = va;
const struct slInt *b = vb;
return a->val - b->val;
}


int main(int argc, char *argv[])
{
int i;
struct heap *h = NULL;
struct slInt *si = NULL;

warn("Making first ordered heap.");
h = newHeap(10, slIntCmp);
warn("Inserting items into heap.");
for(i = 0; i< 10; i++)
    {
    AllocVar(si);
    si->val = i;
    heapMinInsert(h, si);
    }
warn("Printing minimum order.");
while(!heapEmpty(h))
    {
    si = heapExtractMin(h);
    uglyf("%d,",si->val);
    freez(&si);
    }
uglyf("\t Should be in order.\n");
heapFree(&h);

warn("Making second ordered heap.");
h = newHeap(10, slIntCmp);
warn("Inserting items into heap.");
for(i = 0; i< 10; i++)
    {
    AllocVar(si);
    si->val = 10-i;
    heapMinInsert(h, si);
    }
warn("Printing minimum order.");
while(!heapEmpty(h))
    {
    si = heapExtractMin(h);
    uglyf("%d,",si->val);
    freez(&si);
    }
uglyf("\t Should be in order.\n");
heapFree(&h);

warn("Making second ordered heap.");
h = newHeap(10, slIntCmp);
warn("Inserting items into heap.");
for(i = 0; i< 10; i++)
    {
    AllocVar(si);
    si->val = randomVals[i];
    heapMinInsert(h, si);
    }
warn("Printing minimum order.");
while(!heapEmpty(h))
    {
    si = heapExtractMin(h);
    uglyf("%d,",si->val);
    freez(&si);
    }
uglyf("\tShould be in order.\n");
heapFree(&h);



warn("Done");
return 0;
}
      
