/* faCount - count base statistics and CpGs in FA files */
#include "common.h"
#include "fa.h"
#include "dnautil.h"
#include "options.h"

static char const rcsid[] = "$Id: faCount.c,v 1.2 2003/05/06 07:41:05 kate Exp $";

void usage()
/* Print usage info and exit. */
{
errAbort("faCount - count base statistics and CpGs in FA files.\n"
	 "usage:\n"
	 "   faCount file(s).fa\n");
}

void faCount(char *faFiles[], int faCount)
/* faCount - count bases. */
{
int i, j;
struct dnaSeq seq;
unsigned long long totalLength = 0;
unsigned long long totalBaseCount[5];
unsigned long long totalCpgCount = 0;
struct lineFile *lf;
ZeroVar(&seq);

for (i = 0; i < ArraySize(totalBaseCount); i++)
    totalBaseCount[i] = 0;

printf("#seq\tlen\tA\tC\tG\tT\tN\tcpg\n");

dnaUtilOpen();
for (i = 0; i<faCount; ++i)
    {
    lf = lineFileOpen(faFiles[i], FALSE);
    while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
        {
        int prevBase = -1;
        unsigned long long length = 0;
        unsigned long long baseCount[5];
        unsigned long long cpgCount = 0;
        for (i = 0; i < ArraySize(baseCount); i++)
            baseCount[i] = 0;
	for (j=0; j<seq.size; ++j)
	    {
            int baseVal = ntVal5[seq.dna[j]];
            length++;
            baseCount[baseVal]++;
            if ((prevBase == C_BASE_VAL) && (baseVal == G_BASE_VAL))
                cpgCount++;
            prevBase = baseVal;
	    }
        printf("%s\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
               seq.name, length,
               baseCount[A_BASE_VAL], baseCount[C_BASE_VAL],
               baseCount[G_BASE_VAL], baseCount[T_BASE_VAL],
               baseCount[N_BASE_VAL], cpgCount);
        totalLength += length;
        totalCpgCount += cpgCount;
        for (i = 0; i < ArraySize(baseCount); i++)
            totalBaseCount[i] += baseCount[i];
	}
    lineFileClose(&lf);
    }
printf("total\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
       totalLength,
       totalBaseCount[A_BASE_VAL], totalBaseCount[C_BASE_VAL],
       totalBaseCount[G_BASE_VAL], totalBaseCount[T_BASE_VAL],
       totalBaseCount[N_BASE_VAL], totalCpgCount);
}

int main(int argc, char *argv[])
/* Process command line . */
{
optionHash(&argc, argv);
if (argc < 2)
    usage();
faCount(argv+1, argc-1);
return 0;
}
