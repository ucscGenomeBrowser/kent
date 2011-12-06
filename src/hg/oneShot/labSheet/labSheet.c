/* labSheet - Make a tab-separated file for turning into a lab spreadsheet.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "labSheet - Make a tab-separated file for turning into a lab spreadsheet.\n"
  "usage:\n"
  "   labSheet outFile.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void labSheet(char *outFile)
/* labSheet - Make a tab-separated file for turning into a lab spreadsheet.. */
{
int row;
FILE *f = mustOpen(outFile, "w");
static char *fPrimers[] = {
   "fJunk.1fwdA",
   "fNogDown.1fwdA",
   "uxf.357fwdA",
   "uxf.482fwdA",
   "uxf.336fwdA",
   "uxf.355fwdA",
   "uxf.123fwdA",
   "uxf.124fwdA",
   "uxf.125fwdA",
   "uxf.126fwdA",
   "uxf.322fwdA",
   "uxf.323fwdA",
   };
static char *rPrimers[] = {
   "fJunk.1revA",
   "fNogDown.1revA",
   "uxf.357revA",
   "uxf.482revA",
   "uxf.336revA",
   "uxf.355revA",
   "uxf.123revA",
   "uxf.124revA",
   "uxf.125revA",
   "uxf.126revA",
   "uxf.322revA",
   "uxf.323revA",
   };
static int sizes[] = {
   1568,
   322,
   326,
   451,
   409,
   428,
   526,
   344,
   345,
   422,
   229,
   397,
   };

fprintf(f, "fPrimer 20 uM 0.2ul\t");
fprintf(f, "rPrimer 50 ng/ul 1ul\t");
fprintf(f, "Template\t");
fprintf(f, "10x Buffer\t");
fprintf(f, "10mM dNTPs\t");
fprintf(f, "H20\t");
fprintf(f, "Ultra PFU\t");
fprintf(f, "Size\n");
for (row=0; row<12; ++row)
    {
    fprintf(f, "%s\t", fPrimers[row]);
    fprintf(f, "%s\t", rPrimers[row]);
    fprintf(f, "0.8 10ng/ul\t");
    fprintf(f, "4\t");
    fprintf(f, "0.8\t");
    fprintf(f, "26\t");
    fprintf(f, "0.4\t");
    fprintf(f, "%d\n", sizes[row]);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
labSheet(argv[1]);
return 0;
}
