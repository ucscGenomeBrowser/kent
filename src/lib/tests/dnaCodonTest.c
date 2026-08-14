/* dnaCodonTest - exercise alternative genetic codes in dnautil. */
#include "common.h"
#include "dnautil.h"

static char *testDna = "atgtgataagggcaataa";
/* codons:              atg tga taa ggg caa taa
 * Standard (1):        M   *   *   G   Q   *
 * Vert. Mito (2):      M   W   *   G   Q   *
 * Ciliate (6):         M   *   Q   G   Q   Q   (taa/tag -> Gln)  */

static void translateWith(int id)
/* Print full translation (stops as '*') of testDna under genetic code id. */
{
struct geneticCode *code = geneticCodeForId(id);
int i, n = strlen(testDna);
printf("code %2d %-26s ", id, code->name);
for (i=0; i+3 <= n; i += 3)
    {
    AA aa = lookupCodonInCode(code, testDna+i);
    putchar(aa == 0 ? '*' : aa);
    }
putchar('\n');
}

int main(int argc, char *argv[])
{
/* Same codon, different codes. */
translateWith(1);
translateWith(2);
translateWith(6);

/* tga is stop in the standard code, tryptophan (not stop) in vert. mito. */
printf("tga stop standard=%d mito=%d\n",
    isStopCodonInCode(geneticCodeForId(1), "tga"),
    isStopCodonInCode(geneticCodeForId(2), "tga"));

/* Lookup by id and by name. */
printf("id 11 name=%s\n", geneticCodeForId(11)->name);
printf("name Standard id=%d\n", geneticCodeForName("standard")->id);
printf("id 999 isNull=%d name Bogus isNull=%d\n",
    geneticCodeForId(999) == NULL, geneticCodeForName("Bogus") == NULL);

/* Default code: standard, then switched, then back. */
char buf[64];
dnaTranslateSome(testDna, buf, sizeof(buf));
printf("default(standard) translateSome=%s\n", buf);
setDefaultGeneticCode(2);
dnaTranslateSome(testDna, buf, sizeof(buf));
printf("default(mito)     translateSome=%s\n", buf);
setDefaultGeneticCode(1);
printf("lookupCodon(tga) after reset=%d\n", lookupCodon("tga"));

return 0;
}
