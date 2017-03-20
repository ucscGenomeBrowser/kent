/* hgvsTester -- exercise lib/hgHgvs.c */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgHgvs.h"
#include "genbank.h"
#include "knetUdc.h"
#include "linefile.h"
#include "memalloc.h"
#include "options.h"

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* explain usage and exit */
{
errAbort(
"hgvsTester - test program for lib/hgHgvs.c\n\n"
"usage:\n"
"    hgvsTester db inputFile\n"
//"options:\n"
"Each line of inputFile is either a \"#\"-comment line or an HGVS(-like) term.\n"
         );
}

int testHgvsTerms(char *db, char *inputFile)
{
// Each line of input is an HGVS term.  Map it to reference assembly coords and print position.
struct lineFile *lf = lineFileOpen(inputFile, TRUE);
printf("# db: %s\n"
       "# inputFile: %s\n", db, inputFile);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *term = trimSpaces(line);
    if (term[0] == '\0' || term[0] == '#')
        {
        puts(line);
        continue;
        }
    struct hgvsVariant *hgvs = hgvsParseTerm(term);
    if (hgvs == NULL)
        hgvs = hgvsParsePseudoHgvs(db, term);
    if (hgvs == NULL)
        printf("# Failed to parse '%s' as HGVS\n", term);
    else
        {
        struct bed *region = hgvsMapToGenome(db, hgvs, NULL);
        if (region == NULL)
            printf("# Failed to map '%s' to %s assembly\n", term, db);
        else
            printf("%s\t%d\t%d\t%s\t0\t%c\n",
                   region->chrom, region->chromStart, region->chromEnd, term, region->strand[0]);
        }
    }
lineFileClose(&lf);
carefulCheckHeap();
return 0;
}

int main(int argc, char *argv[])
{
// Check args
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
char *db = argv[1];
char *inputFile = argv[2];
// Set up environment
pushCarefulMemHandler(LIMIT_2or6GB);
initGenbankTableNames(db);
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
udcSetDefaultDir("./udcCache");
knetUdcInstall();

return testHgvsTerms(db, inputFile);
}
