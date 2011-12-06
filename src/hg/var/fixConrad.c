/* fixConrad - Assign unique identifer to Conrad input. */

#include "common.h"

#include "chromInfo.h"
#include "hdb.h"
#include "linefile.h"
#include "jksql.h"


int countByChrom[25];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixConrad - Assign unique identifier to Conrad input\n"
  "usage:\n"
  "  fixConrad file\n");
}


int getChromIndex(char *chromString)
/* there is probably a library function that does this... */
{
    if (sameString(chromString, "X")) return 23;
    if (sameString(chromString, "Y")) return 24;
    if (sameString(chromString, "M")) return 25;
    return atoi(chromString);
}

void fixConrad(char *filename)
/* fixConrad - read file. */
{
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
int lineSize;
char *row[5];
int wordCount;
int chromInt = 0;
char *chromName;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopByWhite(line, row, ArraySize(row));
    chromName = cloneString(row[0]);

    stripString(row[0], "chr");
    chromInt = getChromIndex(row[0]);
    countByChrom[chromInt] = countByChrom[chromInt] + 1;

    printf("%s.%d\t%s\t+\t%s\t%s\t%s\t%s\t1\t%s,\t%s,\n", 
            chromName, countByChrom[chromInt], chromName, row[1], row[2], row[3], row[4], row[1], row[2]);
    }
}

int main(int argc, char *argv[])
{
int i;

if (argc != 2)
    usage();

for (i = 0; i++; i<25)
    countByChrom[i] = 0;

fixConrad(argv[1]);

return 0;
}
