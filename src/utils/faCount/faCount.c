/* faCount - count base statistics and CpGs in FA files */
#include "common.h"
#include "fa.h"
#include "dnautil.h"
#include "options.h"


static struct optionSpec optionSpecs[] =
{
    {"summary", OPTION_BOOLEAN},
    {"dinuc", OPTION_BOOLEAN},
    {"strands", OPTION_BOOLEAN},
    {NULL, 0},
};

bool summary = FALSE;
bool dinuc = FALSE;
bool strands = FALSE;
void usage()
/* Print usage info and exit. */
{
errAbort("faCount - count base statistics and CpGs in FA files.\n"
	 "usage:\n"
	 "   faCount file(s).fa\n"
         "     -summary  show only summary statistics\n"
         "     -dinuc    include statistics on dinucletoide frequencies\n"
         "     -strands  count bases on both strands\n"
             );
}

void faCount(char *faFiles[], int faCount)
/* faCount - count bases. */
{
int f, i, j, k;
struct dnaSeq seq;
unsigned long long totalLength = 0;
unsigned long long totalBaseCount[5];
unsigned long long totalDinucleotideCount[5][5];
unsigned long long totalCpgCount = 0;
struct lineFile *lf;
ZeroVar(&seq);

for (i = 0; i < ArraySize(totalBaseCount); i++)
    totalBaseCount[i] = 0;

for (i = 0; i < ArraySize(totalDinucleotideCount); i++)
    for (j = 0; j < ArraySize(totalDinucleotideCount[i]); j++)
        totalDinucleotideCount[i][j] = 0;

printf("#seq\tlen\tA\tC\tG\tT\tN\tcpg");
if (dinuc)
    printf("\tAA\tAC\tAG\tAT\tCA\tCC\tCG\tCT\tGA\tGC\tGG\tGT\tTA\tTC\tTG\tTT");
printf("\n");

dnaUtilOpen();
for (f = 0; f<faCount; ++f)
    {
    lf = lineFileOpen(faFiles[f], FALSE);
    while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
        {
        int prevBase = -1;
        int prevRcBase = -1;
        unsigned long long length = 0;
        unsigned long long baseCount[5];
        unsigned long long dinucleotideCount[5][5];
        unsigned long long cpgCount = 0;
        for (i = 0; i < ArraySize(baseCount); i++)
            baseCount[i] = 0;
        for (i = 0; i < ArraySize(dinucleotideCount); i++)
            for (j = 0; j < ArraySize(dinucleotideCount[i]); j++)
                dinucleotideCount[i][j] = 0;
    	for (j=0; j<seq.size; ++j)
	        {
            int baseVal = ntVal5[(int)(seq.dna[j])];
            int rcBaseVal;
            assert(baseVal != -1);
            assert(baseVal <= 4);
            length++;
            switch(baseVal)
                {
                case A_BASE_VAL: rcBaseVal = T_BASE_VAL; break;
                case C_BASE_VAL: rcBaseVal = G_BASE_VAL; break;
                case G_BASE_VAL: rcBaseVal = C_BASE_VAL; break;
                case T_BASE_VAL: rcBaseVal = A_BASE_VAL; break;
                default: rcBaseVal = N_BASE_VAL; break;
                }
            baseCount[baseVal]++;
            if ((prevBase == C_BASE_VAL) && (baseVal == G_BASE_VAL))
                cpgCount++;
            if (prevBase != -1)
                dinucleotideCount[prevBase][baseVal]++;
            if (strands)
                {
                length++;
                baseCount[rcBaseVal]++;
                if ((prevRcBase == G_BASE_VAL) && (rcBaseVal == C_BASE_VAL))
                    cpgCount++;
                if (prevRcBase != -1)
                    dinucleotideCount[rcBaseVal][prevRcBase]++;
                }
            prevBase = baseVal;
            prevRcBase = rcBaseVal;
            }
        if (!summary)
            {
            printf("%s\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",
            seq.name, length,
            baseCount[A_BASE_VAL], baseCount[C_BASE_VAL],
            baseCount[G_BASE_VAL], baseCount[T_BASE_VAL],
            baseCount[N_BASE_VAL], cpgCount);
            if (dinuc)
                printf("\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",
                    dinucleotideCount[A_BASE_VAL][A_BASE_VAL], dinucleotideCount[A_BASE_VAL][C_BASE_VAL],
                    dinucleotideCount[A_BASE_VAL][G_BASE_VAL], dinucleotideCount[A_BASE_VAL][T_BASE_VAL],
                    dinucleotideCount[C_BASE_VAL][A_BASE_VAL], dinucleotideCount[C_BASE_VAL][C_BASE_VAL],
                    dinucleotideCount[C_BASE_VAL][G_BASE_VAL], dinucleotideCount[C_BASE_VAL][T_BASE_VAL],
                    dinucleotideCount[G_BASE_VAL][A_BASE_VAL], dinucleotideCount[G_BASE_VAL][C_BASE_VAL],
                    dinucleotideCount[G_BASE_VAL][G_BASE_VAL], dinucleotideCount[G_BASE_VAL][T_BASE_VAL],
                    dinucleotideCount[T_BASE_VAL][A_BASE_VAL], dinucleotideCount[T_BASE_VAL][C_BASE_VAL],
                    dinucleotideCount[T_BASE_VAL][G_BASE_VAL], dinucleotideCount[T_BASE_VAL][T_BASE_VAL]);
            printf("\n");
            }
        totalLength += length;
        totalCpgCount += cpgCount;
        for (i = 0; i < ArraySize(baseCount); i++)
            totalBaseCount[i] += baseCount[i];
        for (i = 0; i < ArraySize(dinucleotideCount); i++)
            for (k = 0; k < ArraySize(dinucleotideCount[i]); k++)
                totalDinucleotideCount[i][k] += dinucleotideCount[i][k];
        }
    lineFileClose(&lf);
	}


printf("total\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",
       totalLength,
       totalBaseCount[A_BASE_VAL], totalBaseCount[C_BASE_VAL],
       totalBaseCount[G_BASE_VAL], totalBaseCount[T_BASE_VAL],
       totalBaseCount[N_BASE_VAL], totalCpgCount);
if (dinuc)
    printf("\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",
        totalDinucleotideCount[A_BASE_VAL][A_BASE_VAL], totalDinucleotideCount[A_BASE_VAL][C_BASE_VAL],
        totalDinucleotideCount[A_BASE_VAL][G_BASE_VAL], totalDinucleotideCount[A_BASE_VAL][T_BASE_VAL],
        totalDinucleotideCount[C_BASE_VAL][A_BASE_VAL], totalDinucleotideCount[C_BASE_VAL][C_BASE_VAL],
        totalDinucleotideCount[C_BASE_VAL][G_BASE_VAL], totalDinucleotideCount[C_BASE_VAL][T_BASE_VAL],
        totalDinucleotideCount[G_BASE_VAL][A_BASE_VAL], totalDinucleotideCount[G_BASE_VAL][C_BASE_VAL],
        totalDinucleotideCount[G_BASE_VAL][G_BASE_VAL], totalDinucleotideCount[G_BASE_VAL][T_BASE_VAL],
        totalDinucleotideCount[T_BASE_VAL][A_BASE_VAL], totalDinucleotideCount[T_BASE_VAL][C_BASE_VAL],
        totalDinucleotideCount[T_BASE_VAL][G_BASE_VAL], totalDinucleotideCount[T_BASE_VAL][T_BASE_VAL]);
printf("\n");

if (summary)
    {
    printf("prcnt\t%-5.1f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f",
       (float)totalLength/totalLength,
       ((float)totalBaseCount[A_BASE_VAL])/(float)totalLength, ((float)totalBaseCount[C_BASE_VAL])/(float)totalLength,
       ((float)totalBaseCount[G_BASE_VAL])/(float)totalLength, ((float)totalBaseCount[T_BASE_VAL])/(float)totalLength,
       ((float)totalBaseCount[N_BASE_VAL])/(float)totalLength, (float)totalCpgCount/(float)totalLength);
    if (dinuc)
        printf("\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f\t%-5.4f",
            (float)totalDinucleotideCount[A_BASE_VAL][A_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[A_BASE_VAL][C_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[A_BASE_VAL][G_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[A_BASE_VAL][T_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[C_BASE_VAL][A_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[C_BASE_VAL][C_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[C_BASE_VAL][G_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[C_BASE_VAL][T_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[G_BASE_VAL][A_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[G_BASE_VAL][C_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[G_BASE_VAL][G_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[G_BASE_VAL][T_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[T_BASE_VAL][A_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[T_BASE_VAL][C_BASE_VAL]/(float)totalLength,
            (float)totalDinucleotideCount[T_BASE_VAL][G_BASE_VAL]/(float)totalLength, (float)totalDinucleotideCount[T_BASE_VAL][T_BASE_VAL]/(float)totalLength);
    printf("\n");
    }
}

int main(int argc, char *argv[])
/* Process command line . */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
summary = optionExists("summary");
dinuc = optionExists("dinuc");
strands = optionExists("strands");
faCount(argv+1, argc-1);
return 0;
}
