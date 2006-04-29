/* gpShuffleData - Shuffle and obscure data, so can experiment with it publically.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"

static char const rcsid[] = "$Id: gpShuffleData.c,v 1.1 2006/04/29 07:26:19 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gpShuffleData - Shuffle and obscure data, so can experiment with it publically.\n"
  "usage:\n"
  "   gpShuffleData in out ID-col\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void randOut(double x, FILE *f, boolean isFloat)
/* Output slightly randomized x. */
{
double factor = 1.0 +  (rand() % 200 - 100)*0.001;
double y = factor*x;
if (isFloat)
    {
    fprintf(f, "%f\t", y);
    }
else
    {
    fprintf(f, "%d\t", (int)(y+0.5));
    }
}

void randomizeNum(char *s, FILE *f)
/* Output slightly randomized x. */
{
if (s[0])
    randOut(atof(s), f, strchr(s, '.') != NULL);
else
    fprintf(f, "\t");
}

void randomizeDiff(char *a, char *b, FILE *f)
/* Output randomized difference */
{
if (a[0] && b[0])
    randOut(atof(a) - atof(b), f, TRUE);
else
    fprintf(f, "\t");
}

void printCel(char *s, FILE *f)
/* Print out a field and a tab. */
{
fprintf(f, "%s\t", s);
}


int seed;

void randomizeId(char *s, FILE *f)
/* Take an ID and turn it into another, random ID. */
{
struct md5_context ctx;
uint8 digest[17];
int i;
md5_starts(&ctx);
md5_update(&ctx, (uint8*)s, strlen(s));
md5_update(&ctx, (uint8*)&seed, sizeof(seed));
md5_finish(&ctx, digest);
digest[16] = 0;
for (i=0; i<16; ++i)
    {
    uint8 c = (digest[i] % (26+26+9));
    if (c < 26)
        c = 'a' + c;
    else if (c < 52)
        c = 'A' + c - 26;
    else 
        c = '1' + c - 52;
    digest[i] = c;
    }
fprintf(f, "%s\t", (char*)digest);
}



void gpShuffleData(char *in, char *out, char *idColumn)
/* gpShuffleData - Shuffle and obscure data, so can experiment with it publically.. */
{
char *row[22];
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
fprintf(f, "#subject");
fprintf(f, "\tethnicity");
fprintf(f, "\tair quality");
fprintf(f, "\turban density");
fprintf(f, "\tdietary fiber");
fprintf(f, "\tvegetables");
fprintf(f, "\tweight");
fprintf(f, "\tBPS");
fprintf(f, "\tAGE");
fprintf(f, "\tPHE1");
fprintf(f, "\tPHE2");
fprintf(f, "\tGeno1");
fprintf(f, "\tGeno2");
fprintf(f, "\tGeno3");
fprintf(f, "\tGeno4");
fprintf(f, "\tGeno5");
fprintf(f, "\tGeno6");
fprintf(f, "\tGeno7");
fprintf(f, "\tGeno8");
fprintf(f, "\n");
while (lineFileRowTab(lf, row))
    {
    randomizeId(row[0], f);	/* ID */
    printCel(row[1], f);	/* Ethnicity */
    randomizeNum(row[17], f);	/* Air Quality */
    randomizeNum(row[16], f);	/* Urban Density */
    randomizeNum(row[14], f);   /* Fiber */
    randomizeNum(row[15], f);   /* Vegetables*/
    randomizeNum(row[13], f);   /* Weight */
    randomizeNum(row[12], f);   /* BPS */
    randomizeNum(row[11], f);   /* Age */
    randomizeDiff(row[18], row[20], f);  /* Phe1 */
    randomizeDiff(row[19], row[21], f);  /* Phe1 */
    printCel(row[4], f);        /* Geno1 */
    printCel(row[6], f);        /* Geno1 */
    printCel(row[10], f);        /* Geno1 */
    printCel(row[7], f);        /* Geno1 */
    printCel(row[5], f);        /* Geno1 */
    printCel(row[3], f);        /* Geno1 */
    printCel(row[9], f);        /* Geno1 */
    fprintf(f, "%s\n", row[8]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
srand(time(NULL));
seed = rand();
if (argc != 4)
    usage();
gpShuffleData(argv[1], argv[2], argv[3]);
return 0;
}
