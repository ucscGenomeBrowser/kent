#include "common.h"
#include "options.h"
#include "gbFa.h"
#include "gbFileOps.h"
#include <stdio.h>

static unsigned gVerbose = 0;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

/* global count of errors encountered */
unsigned gErrorCnt = 0;

/* array of bases valid in mRNA sequences */
char gValidMRnaBases[256];

void printMrnaErr(struct gbFa* fa, char* msg, ...)
/* print an mrna related error */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void printMrnaErr(struct gbFa* fa, char* msg, ...)
/* print an mrna related error */
{
va_list args;
fprintf(stderr, "Error: %s:%d: %s: ", fa->fileName, fa->recLineNum, fa->id);
va_start(args, msg);
vfprintf(stderr, msg, args);
va_end(args);
fputc('\n', stderr);
gErrorCnt++;
}

void checkMrnaSeq(struct gbFa* fa)
/* validate one mrna sequence */
{
int numInvalidBases = 0;
char* bases = gbFaGetSeq(fa);
char invalidChars[256];
zeroBytes(invalidChars, sizeof(invalidChars));

if (strlen(bases) == 0)
    printMrnaErr(fa, "zero length sequence");
while (*bases != '\0')
    {
    char b = *bases++;
    if (!gAllowedRNABases[(int)b])
        {
        numInvalidBases++;
        invalidChars[(int)b] = b;
        }
    }
if (numInvalidBases > 0)
    {
    printMrnaErr(fa, "invalid base letters: %d of %d", numInvalidBases,
                 fa->seqLen);
    if (gVerbose >= 1)
        {
        int i;
        fprintf(stderr, "   invalid chars: ");
        for (i = 0; i < ArraySize(invalidChars); i++)
            if (invalidChars[i] != 0)
                fprintf(stderr, "%c", i);
        fprintf(stderr, "\n");
        }
    }
}

void gbFaCheck(char *faFile)
/* check a fasta file */
{
struct gbFa* fa = gbFaOpen(faFile, "r");
while (gbFaReadNext(fa))
    {
    checkMrnaSeq(fa);
    }
gbFaClose(&fa);
}

void usage()
/* print usage and exit */
{
errAbort("   gbFaCheck fa [...]\n"
         "\n"
         "Validate the contents of genbank fa files. \n"
         "\n"
         " Options:\n"
         "     -verbose=n - enable verbose output, values > 1\n"
	 "      increase verbosity:\n"
         "      1 - print invalid characters in each sequence,\n"
         "\n");
}

int main(int argc, char* argv[])
{
int argi;

optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
gVerbose = optionInt("verbose", 0);
allowedRNABasesInit();

for (argi = 1; argi < argc; argi++)
    gbFaCheck(argv[argi]);

return (gErrorCnt > 0) ? 1 : 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

