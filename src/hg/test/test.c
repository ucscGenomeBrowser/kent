/* test - test out something. */
#include "common.h"
#include "portable.h"
#include "jksql.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"
#include "fa.h"
#include "ens.h"
#include "unfin.h"


char *database = "ensdev";


void usage()
{
errAbort("usage: test \n");
}

void getEnsSeqs(char *seqs[], int seqCount)
/* Get bac sequences and put into corresponding .fa
 * file. */
{
char faName[256];
char *bacAcc;
int i;

for (i = 0; i<seqCount; ++i)
    {
    struct dnaSeq *bacSeq;
    int seqSize;

    bacAcc = seqs[i];
    sprintf(faName, "%s.fa", bacAcc);
    seqSize = ensBacBrowserLength(bacAcc);
    printf("Processing %s into %s (%d bases)\n", bacAcc, faName, seqSize);
    bacSeq = ensDnaInBacRange(bacAcc, 0, seqSize, dnaLower);
    faWrite(faName, bacAcc, bacSeq->dna, seqSize);

    freeDnaSeq(&bacSeq);
    }
}

char *seqList[] = 
   {
   "AB014083",
   "AB023052",
   "AC000001",
   "AC000005",
   "AC000016",
   "AC000033",
   "AC001644",
   "AC002399",
   "AC004256",
   "AC007056",
   "AC007479",
   "AC007666",
   "AC007702",
   "AC009587",
   "AC012191",
   "AL050317",
   };

int test()
{
}

int main(int argc, char *argv[])
{
test();
return 0;
}
