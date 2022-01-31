/* hgvsTester -- exercise lib/hgHgvs.c */

/* Copyright (C) 2016 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgHgvs.h"
#include "genbank.h"
#include "knetUdc.h"
#include "linefile.h"
#include "memalloc.h"
#include "options.h"
#include "chromAlias.h"

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
struct dyString *dy = dyStringNew(0);
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
    struct hgvsVariant *hgvsList = hgvsParseTerm(term);
    if (hgvsList == NULL)
        hgvsList = hgvsParsePseudoHgvs(db, term);
    if (hgvsList == NULL)
        printf("# Failed to parse '%s' as HGVS\n", term);
    else
        {
        struct hgvsVariant *hgvs = NULL;
        struct hash *bedHash = hashNew(0);
        int okCount = 0;
        for (hgvs = hgvsList; hgvs != NULL; hgvs = hgvs->next)
            {
            struct bed *region = hgvsMapToGenome(db, hgvs, NULL);
            if (region != NULL)
                {
                okCount++;
                // Now that hgvsParsePsuedoHgvs returns a list of valid positions
                // we only want to print the unique ones in the list
                dyStringClear(dy);;
                dyStringPrintf(dy, "%s\t%d\t%d\t%s\t0\t%c\n",
                    region->chrom, region->chromStart, region->chromEnd, term, region->strand[0]);
                if (hashFindVal(bedHash, dy->string) == NULL)
                    hashAdd(bedHash, cloneString(dy->string), region);
                }
            }
        // Complain only if none of the HGVS terms in the list could be mapped
        if (okCount == 0)
            printf("# Failed to map '%s' to %s assembly\n", term, db);
        struct hashEl *hel, *helList = hashElListHash(bedHash);
        for (hel = helList; hel != NULL; hel = hel->next)
            {
            struct bed *region = (struct bed *)hel->val;
            printf("%s\t%d\t%d\t%s\t0\t%c\n",
                   region->chrom, region->chromStart, region->chromEnd, term, region->strand[0]);
            }
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
chromAliasSetup(db);
initGenbankTableNames(db);
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
udcSetDefaultDir("./udcCache");
knetUdcInstall();

return testHgvsTerms(db, inputFile);
}
