/* scaffoldFaToAgp - read a file containing FA scaffold records and generate */
/*                   an AGP file, with gaps introduced between scaffolds */

#include "common.h"
#include "fa.h"
#include "options.h"
#include "../../hg/inc/agpFrag.h"
#include "../../hg/inc/agpGap.h"

static char const rcsid[] = "$Id: scaffoldFaToAgp.c,v 1.1 2003/05/31 04:31:17 kate Exp $";

#define GAP_SIZE 1000
/* TODO: optionize this */

#define GAP_TYPE "scaffold"
#define CHROM_NAME "chrUn"

void usage()
/* Print usage instructions and exit. */
{
errAbort("scaffoldFaToAgp - generate an AGP file from a scaffold FA file.\n"
	 "usage:\n"
	 "    scaffoldFaToAgp source.fa\n"
	 "The resulting file will be source.agp\n"
         "Note: gaps of 1000 bases are inserted between scaffold records\n");
}

void scaffoldFaToAgp(char *scaffoldFile)
/* scaffoldFaToAgp - create AGP file from scaffold FA file */
{
struct dnaSeq scaffold;
struct agpFrag frag, *pFrag = &frag;
struct agpGap gap, *pGap = &gap;
int gapSize = GAP_SIZE;
struct lineFile *lf = lineFileOpen(scaffoldFile, TRUE);
char outDir[256], outFile[128], ext[64], outPath[512];
FILE *f = NULL;

int fileNumber = 1;
int start = 0;
int end;

splitPath(scaffoldFile, outDir, outFile, ext);
sprintf(outPath, "%s%s.agp", outDir, outFile);
f = mustOpen(outPath, "w");
printf("writing %s\n", outPath);

ZeroVar(&scaffold);
ZeroVar(pFrag);
ZeroVar(pGap);

/* Initialize AGP gap that will be used throughout */
pGap->chrom = CHROM_NAME;
pGap->n[0] = 'N';
pGap->size = gapSize;
pGap->type = GAP_TYPE;
pGap->bridge = "no";

/* Initialize AGP fragment */
pFrag->chrom = CHROM_NAME;
pFrag->type[0] = 'D';   /* draft */
pFrag->fragStart = 0;   /* always start at beginning of scaffold */
pFrag->strand[0] = '+';

while (faMixedSpeedReadNext(lf, &scaffold.dna, &scaffold.size, &scaffold.name))
    {
    printf("%s\n", scaffold.name);
    end = start + scaffold.size;

    /* Create AGP fragment for the scaffold */
    pFrag->frag = scaffold.name;
    pFrag->ix = fileNumber++;
    pFrag->chromStart = start;
    pFrag->chromEnd = end;
    pFrag->fragEnd = scaffold.size;
    agpFragOutput(pFrag, f, '\t', '\n');

    /* Create AGP gap to separate scaffolds */
    /* Note: may want to suppress final gap -- not needed as separator */
    start = end + 1;
    end = start + gapSize - 1;

    pGap->ix = fileNumber++;
    pGap->chromStart = start;
    pGap->chromEnd = end;
    agpGapOutput(pGap, f, '\t', '\n');

    start = end;
    }
carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
scaffoldFaToAgp(argv[1]);
return 0;
}
