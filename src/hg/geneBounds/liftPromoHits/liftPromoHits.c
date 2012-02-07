/* liftPromoHits - Lift motif hits from promoter to chromosome coordinates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftPromoHits - Lift motif hits from promoter to chromosome coordinates\n"
  "usage:\n"
  "   liftPromoHits promoPos.bed hit.tab hit.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *readPromoPos(char *fileName)
/* Return hash who's value is just the strand. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
struct hash *hash = newHash(18);
int count = 0;
while (lineFileRow(lf, row))
    {
    char *strand;
    if (row[4][0] == '-')
        strand = "-";
    else
        strand = "+";
    hashAdd(hash, row[3], strand);
    ++count;
    }
printf("Read %d elements in %s\n", count, fileName);
lineFileClose(&lf);
return hash;
}

void liftPromoHits(char *inBed, char *inTab, char *outBed)
/* liftPromoHits - Lift motif hits from promoter to chromosome coordinates. */
{
struct hash *hash = readPromoPos(inBed);
struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *f = mustOpen(outBed, "w");
char *row[6], *parts[4];
char *chrom, *strand;
int start, end, size, s, e, orientation;

while (lineFileRow(lf, row))
    {
    strand = hashMustFindVal(hash, row[0]);
    if (chopString(row[0], ":-", parts, 4) != 3)
        errAbort("Expecting chrN:start-end line %d of %s\n", lf->lineIx, lf->fileName);
    chrom = parts[0];
    start = atoi(parts[1])-1;
    end = atoi(parts[2]);
    size = end - start;
    s = atoi(row[1]);
    e = atoi(row[2]);
    orientation = (row[5][0] == '-' ? -1 : 1);
    if (strand[0] == '-')
	{
	reverseIntRange(&s, &e, size);
	orientation = -orientation;
	}
    fprintf(f, "%s\t%d\t%d\t%s\t%s\t%s\n",
         chrom, start + s, start + e, row[3], row[4],
	 (orientation < 0 ? "-" : "+"));
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
liftPromoHits(argv[1], argv[2], argv[3]);
return 0;
}
