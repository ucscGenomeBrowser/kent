/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.\n"
  "usage:\n"
  "   hgExonerate database table inputFile\n");
}

void hgExonerate(char *database, char *table, char *file)
/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
FILE *f;
char tabFileName[512];
char *row[8];
char *parts[6];
char *subParts[3];
int partCount;
double score;

sprintf(tabFileName, "%s.tab", table);
f = mustOpen(tabFileName, "w");
printf("Converting %s to %s\n", file, tabFileName);
while (lineFileRow(lf, row))
    {
    fprintf(f, "%s\t", row[0]);				/* chromosome. */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 2)-1);       /* chromStart. */
    fprintf(f, "%s\t", row[3]);                         /* chromEnd. */
    partCount = chopString(row[7], ":", parts, ArraySize(parts));
    if (partCount != 4)
       errAbort("Unparsable field 8 line %d of %s", lf->lineIx, lf->fileName);
    fprintf(f, "%s.%s\t", parts[0], parts[1]);          /* name. */
    score = atof(row[4]);                               
    if (score > 1000) score = 1000;
    if (score < 0) score = 0;
    fprintf(f, "%d\t", round(score));			/* milliScore */
    fprintf(f, "%s\t", row[5]);				/* strand */
    partCount = chopByChar(parts[2], '-', subParts, ArraySize(subParts));
    if (partCount != 2 || !isdigit(subParts[0][0]) || !isdigit(subParts[1][0]))
       errAbort("Unparsable number range in field 8 line %d of %s", lf->lineIx, lf->fileName);
    fprintf(f, "%d\t", atoi(subParts[0]-1));		/* otherStart */
    fprintf(f, "%s\t", subParts[1]);			/* otherEnd */
    fprintf(f, "%c\n", (parts[3][0] == '-' ? '-' : '+'));  /* otherStrand */
    }
carefulClose(&f);
lineFileClose(&lf);

uglyAbort("All for now");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgExonerate(argv[1], argv[2], argv[3]);
return 0;
}
