/* labSheet - Make a tab-separated file for turning into a lab spreadsheet.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: labSheet.c,v 1.2 2004/09/24 15:23:47 kent Exp $";

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
int row, col;
FILE *f = mustOpen(outFile, "w");
static char *templates[] = {
   "uc462",
   "uc465",
   "uc467",
   "uc468+9",
   "uc326+7",
   "uc275",
   "uc270+1",
   "uc404",
   "uc189",
   };
static char *fPrimers[] = {
   "B04-uc462r",
   "B06-uc465r",
   "B07-uc467r",
   "B08-uc468+9r",
   "B12-uc326+7r",
   "D02-uc275r",
   "D04-uc270+1r",
   "D07-uc404r",
   "D08-uc189r",
   };
static int sizes[] = {
    897,
    609,
    1190,
    963,
    1199,
    525,
    825,
    736,
    680,
   };

fprintf(f, "Sample\t");
fprintf(f, "Template\t");
fprintf(f, "Size\t");
fprintf(f, "Colony H20\t");
fprintf(f, "fPrimer 20 uM 0.2ul\t");
fprintf(f, "rPrimer 50 ng/ul 1ul\t");
fprintf(f, "10x Buffer\t");
fprintf(f, "10mM dNTPs\t");
fprintf(f, "H20\t");
fprintf(f, "Ultra PFU\n");
for (col=0; col < 9; ++col)
    {
    for (row = 'A'; row <= 'H'; ++row)
        {
	fprintf(f, "%c%d\t", row, col+1);
	fprintf(f, "%s\t", templates[col]);
	fprintf(f, "%d\t", sizes[col]);
	fprintf(f, "5\t");
	fprintf(f, "%s\t", fPrimers[col]);
	fprintf(f, "M13 rev\t");
	fprintf(f, "2\t");
	fprintf(f, "0.4\t");
	fprintf(f, "11.2\t");
	fprintf(f, "0.2\n");
	}
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
