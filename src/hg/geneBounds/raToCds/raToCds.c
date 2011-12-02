/* raToCds - Extract CDS positions from ra file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "raToCds - Extract CDS positions from ra file\n"
  "usage:\n"
  "   raToCds in.ra out.cds\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

boolean parseCds(char *gbCds, int start, int end, int *retStart, int *retEnd)
/* Parse genBank style cds string into something we can use... */
{
char *s = gbCds;
boolean gotStart = FALSE, gotEnd = TRUE;

gotStart = isdigit(s[0]);
s = strchr(s, '.');
if (s == NULL || s[1] != '.')
    errAbort("Malformed GenBank cds %s", gbCds);
s[0] = 0;
s += 2;
gotEnd = isdigit(s[0]);
if (gotStart) start = atoi(gbCds) - 1;
if (gotEnd) end = atoi(s);
*retStart = start;
*retEnd = end;
return gotStart && gotEnd;
}


void raToCds(char *inFile, char *outFile)
/* raToCds - Extract CDS positions from ra file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct hash *ra;

fprintf(f, "#name    \tsize\tstart\tend\n");
while ((ra = raNextRecord(lf)) != NULL)
    {
    char *acc = hashFindVal(ra, "acc");
    if (acc != NULL)
        {
	char *cds = hashFindVal(ra, "cds");
	char *siz = hashFindVal(ra, "siz");
	if (siz == NULL)
	    {
	    warn("No size for %s, skipping", acc);
	    continue;
	    }
	if (cds != NULL)
	    {
	    int size = atoi(siz);
	    int cdsStart = 0, cdsEnd = size;
	    parseCds(cds, 0, size, &cdsStart, &cdsEnd);
	    fprintf(f, "%s\t%d\t%d\t%d\n", acc, size, cdsStart, cdsEnd);
	    }
	}
    hashFree(&ra);
    }
lineFileClose(&lf);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
raToCds(argv[1], argv[2]);
return 0;
}
