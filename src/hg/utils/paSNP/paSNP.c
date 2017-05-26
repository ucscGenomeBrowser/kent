#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "axt.h"
#include "math.h"
#include "pa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paSNP - given species list and protein alignments, generate bed file with SNPs\n"
  "usage:\n"
  "   paSNP order.lst alignments.fa out.spec\n"
  "arguments:\n"
  "   order.lst        order of species in fasta\n"
  "   alignments.fa    fasta protein alignments\n"
  "   out.spec         file to put spectra in\n"
  "options:\n"
  "   -binCol          consider only binary columns\n"
  "   -fullCol         consider only full (no dash,X,or Z) columns\n"
  );
}

static struct optionSpec options[] = {
   {"binCol", OPTION_BOOLEAN},
   {"fullCol", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean binCol = FALSE;
boolean fullCol = FALSE;


char **speciesNames;
int totalCountS[1000];
int aaCountS[1000][26];
int aaCount[26];
int dashCountS[1000];
int numSpecies;

void countAA( struct alignDetail *detail, int cNum, void *closure)
{
int ii;
FILE *f = (FILE *) closure;

char firstChar = 0;
char *position = NULL;
for(ii=0; ii < detail->numSpecies; ii++)
    {
    struct seqBuffer *sb = &detail->seqBuffers[ii];

    if (ii == 0)
	{
	position = sb->position;
	firstChar = sb->buffer[cNum];
	}
    else
	if ((firstChar != sb->buffer[cNum]) && (sb->buffer[cNum] != '-'))
	    {
	    char strand = position[strlen(position) - 1];
	    fprintf(f, "%s %s 1\n", 
		getPosString(position, strand, cNum, detail->startFrame, detail->endFrame),
		 sb->species);
		 }
	
	
    }
}


void paSnp(char *orderFilename, char *fastaFile, char *outFile)
{
FILE *f = mustOpen(outFile, "w");

alignFunc afunc = allColumns;
columnFunc cfunc = countAA;

if (binCol)
    afunc = binColumns;
else if (fullCol)
    afunc = fullColumns;

parseAli(orderFilename, fastaFile, afunc, cfunc, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 4)
    usage();

binCol = optionExists("binCol");
fullCol = optionExists("fullCol");

if (binCol && fullCol)
    errAbort("cannot set both binCol and fullCol");

paSnp(argv[1],argv[2],argv[3]);
return 0;
}
