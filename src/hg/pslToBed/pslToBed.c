/** pslToBed -- change a standard psl format file to a bed format file */

#include "common.h"
#include "psl.h"
#include "bed.h"
#include "options.h"

static struct optionSpec optionSpecs[] =  
/* option for pslToBed */
{
    {"help", OPTION_BOOLEAN},
    {"pslFile", OPTION_STRING},
    {"outBed", OPTION_STRING},
    {"del", OPTION_STRING},
    {"cols", OPTION_INT},
    {NULL, 0}
};

static char *optionDescripts[] =
/* Description of our options for usage summary. */
{
    "Display this message.",
    "File containing psl alignments.",
    "File to output beds to.",
    "delimiter(tab or comma) used in the output file.Default tab.",
    "columns of output bed file, default 4."
};


void usage()

/** provide usage information */
{
int i = 0;
warn("pslToBed: tranform a psl format file to a bed format file.\n");
for(; i< ArraySize(optionSpecs) - 1; i++)
    fprintf(stderr, "-%s  -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage: pslToBed [-pslFile= **.psl] [-outBed = **.bed] <-del=><-cols=N>\n");
} 

void pslToBed()
{
char *pslFile = NULL;
char *outBedName = NULL;
FILE *outBed = NULL;
char *del = NULL;
struct psl *pslList=NULL, *psl=NULL;
struct bed *bed = NULL;
char del1;
int col;

pslFile = optionVal("pslFile", NULL);
outBedName = optionVal("outBed", NULL);
del = optionVal("del", "tab");
col = optionInt("cols", 4);

outBed = mustOpen(outBedName, "w");

if(pslFile == NULL || outBed == NULL)
    usage();

warn("Loading psl file");
if(pslFile)
    pslList = pslLoadAll(pslFile);

if(sameWord(del, "tab"))
    {
    del1= '\t';
    }
else if(sameWord(del, "comma") || sameWord(del, ","))
   {
   del1 = ',';
  }
 else
   {
   errAbort("Wrong delim1ter\n");
   }

for(psl = pslList; psl != NULL; psl= psl->next)
    {
    bed = bedFromPsl(psl);
    bedOutputN (bed, col, outBed, del1, '\n');
    bedFree(&bed);
    }
}


int main(int argc, char* argv[])
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
pslToBed();
return 0;
}
    

