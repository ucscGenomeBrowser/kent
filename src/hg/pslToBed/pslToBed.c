/* pslToBed -- tranform a psl format file to a bed format file */

#include "common.h"
#include "psl.h"
#include "bed.h"
#include "options.h"

static struct optionSpec optionSpecs[] =  
/* option for pslToBed */
{
    {NULL, 0}
};

void usage()
/* print usage infomation and exit */
{
errAbort("pslToBed: tranform a psl format file to a bed format file.\n"
         "usage:\n"
         "    pslToBed psl bed\n");
} 

void pslToBed(char *pslFile, char *bedFile)
/* pslToBed -- tranform a psl format file to a bed format file */
{
struct lineFile *pslLf = pslFileOpen(pslFile);
FILE *bedFh = mustOpen(bedFile, "w");
struct psl *psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    struct bed *bed = bedFromPsl(psl);
    bedTabOutN(bed, 12, bedFh);
    bedFree(&bed);
    pslFree(&psl);
    }
carefulClose(&bedFh);
lineFileClose(&pslLf);
}

int main(int argc, char* argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
pslToBed(argv[1], argv[2]);
return 0;
}
    

