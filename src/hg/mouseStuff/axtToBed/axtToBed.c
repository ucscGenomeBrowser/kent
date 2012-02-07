/* axtToBed - Convert axt alignments to simple bed format. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static bool extendedOp = FALSE;
static bool bed4 = FALSE;
static bool axtName = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToBed - Convert axt alignments to simple bed format\n"
  "usage:\n"
  "   axtToBed in.axt out.bed\n"
  "options:\n"
  "   -extended     output extended format include other species\n"
  "   -bed4         output bed4 file\n"
  "   -axtName      use axt alignment number as interval name in bed4/extended\n"
  "                 (usually first axt record is numbered 0, next is 1, etc.)\n"
  );
}

void axtToBed(char *inName, char *outName)
/* axtToBed - Convert axt alignments to simple bed format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *row[9];
int s, e;

for (;;)
    {
    if (!lineFileRow(lf, row))
        break;
    s = lineFileNeedNum(lf, row, 2) - 1;
    e = lineFileNeedNum(lf, row, 3);
    if (extendedOp)
        {
        int xs = lineFileNeedNum(lf, row, 5) - 1;
        int xe = lineFileNeedNum(lf, row, 6);
        int score = lineFileNeedNum(lf, row, 8);
        fprintf(f, "%s\t%d\t%d\t%s\t%d\t%s\t%d\t%d\n", row[1], s, e, (axtName? row[0] : row[4]), score, row[7], xs, xe);
        }
    else
	if (bed4)
	    fprintf(f, "%s\t%d\t%d\t%s\n", row[1], s, e, (axtName? row[0] : row[4]));
	else
	    fprintf(f, "%s\t%d\t%d\n", row[1], s, e);
    lineFileSkip(lf, 3);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
extendedOp = optionExists("extended");
bed4 = optionExists("bed4");
axtName = optionExists("axtName");
axtToBed(argv[1],argv[2]);
return 0;
}
