/* faCount - count base statistics and CpGs in FA files */
#include "common.h"
#include "fa.h"
#include "dnautil.h"
#include "options.h"

static char const rcsid[] = "$Id: faCount.c,v 1.7 2006/03/18 01:51:36 angie Exp $";

bool summary = FALSE;
void usage()
/* Print usage info and exit. */
{
errAbort("faCount - count base statistics and CpGs in FA files.\n"
	 "usage:\n"
	 "   faCount file(s).fa\n"
         "     -summary  show only summary statistics\n"
             );
}

void faCount(char *faFiles[], int faCount)
/* faCount - count bases. */
{
int f, i, j;
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
for (f = 0; f<faCount; ++f)
    {
    lf = lineFileOpen(faFiles[f], FALSE);
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
            int baseVal = ntVal5[(int)(seq.dna[j])];
            assert(baseVal != -1);
            assert(baseVal <= 4);
            length++;
            baseCount[baseVal]++;
            if ((prevBase == C_BASE_VAL) && (baseVal == G_BASE_VAL))
                cpgCount++;
            prevBase = baseVal;
	    }
        if (!summary)
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
if (summary)
    printf("percent\t%10.1f\t%10.4f\t%10.4f\t%10.4f\t%10.4f\t%10.6f\t%10.4f\n",
       (float)totalLength/totalLength,
       ((float)totalBaseCount[A_BASE_VAL])/(float)totalLength, ((float)totalBaseCount[C_BASE_VAL])/(float)totalLength,
       ((float)totalBaseCount[G_BASE_VAL])/(float)totalLength, ((float)totalBaseCount[T_BASE_VAL])/(float)totalLength,
       ((float)totalBaseCount[N_BASE_VAL])/(float)totalLength, (float)totalCpgCount/(float)totalLength);
}

int main(int argc, char *argv[])
/* Process command line . */
{
optionHash(&argc, argv);
summary = optionExists("summary");
if (argc < 2)
    usage();
faCount(argv+1, argc-1);
return 0;
}
