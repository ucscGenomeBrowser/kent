/* cmpfa.c - compare two fa files. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

int difPos(DNA *as, DNA *bs)
{
int count = 0;
DNA a,b;

for (;;)
    {
    a = *as++;
    b = *bs++;
    if (a != b)
        break;
    if (a == 0 && b == 0)
        return -1;
    if (a == 0 || b == 0)
        break;
    ++count;
    }
return count;
}


int main(int argc, char *argv[])
{
struct dnaSeq *a, *b;
char *aName, *bName;
int pos;

if (argc != 3)
    {
    errAbort("cmpfa - compare two .fa files\n"
             "usage: cmpfa a.fa b.fa");
    }
dnaUtilOpen();
aName = argv[1];
bName = argv[2];
uglyf("Reading %s\n", aName);
a = faReadDna(aName);
uglyf("Reading %s\n", bName);
b = faReadDna(bName);
uglyf("Comparing %s and %s\n", aName, bName);
pos = difPos(a->dna, b->dna);
if (pos < 0)
    printf("%s and %s are identical\n", aName, bName);
else
    printf("%s and %s differ at position %d\n", aName, bName, pos);
return 0;
}