/* scaffoldFaToAgp - read a file containing FA scaffold records and generate */
/*                   an AGP file, with gaps introduced between scaffolds */

#include "common.h"
#include "fa.h"
#include "options.h"
#include "../../hg/inc/agpFrag.h"
#include "../../hg/inc/agpGap.h"

static char const rcsid[] = "$Id: scaffoldFaToAgp.c,v 1.2 2003/05/31 05:06:04 kate Exp $";

#define GAP_SIZE 1000
/* TODO: optionize this */

#define GAP_TYPE "scaffold"
#define CHROM_NAME "chrUn"

void usage()
/* Print usage instructions and exit. */
{
errAbort("scaffoldFaToAgp - generate an AGP file and lift file from a scaffold FA file.\n"
	 "usage:\n"
	 "    scaffoldFaToAgp source.fa\n"
	 "The resulting file will be source.agp\n"
         "Note: gaps of 1000 bases are inserted between scaffold records\n");
}

void scaffoldFaToAgp(char *scaffoldFile)
/* scaffoldFaToAgp - create AGP file and lift file from scaffold FA file */
{
struct dnaSeq scaffold, *scaffoldList, *pScaffold;
DNA *dna;
char *name;
int size;
struct agpFrag frag, *pFrag = &frag;
struct agpGap gap, *pGap = &gap;
int gapSize = GAP_SIZE;
struct lineFile *lf = lineFileOpen(scaffoldFile, TRUE);
char outDir[256], outFile[128], ext[64], outPath[512];
FILE *agpFile = NULL;
FILE *liftFile = NULL;

int fileNumber = 1;
int start = 0;
int end = 0;
int chromSize = 0;
int scaffoldCount = 0;

/* Read in scaffold info */
while (faMixedSpeedReadNext(lf, &dna, &size, &name))
    {
    AllocVar(pScaffold);
    pScaffold->name = cloneString(name);
    pScaffold->size = size;
    printf("%s size=%d\n", pScaffold->name, pScaffold->size);
    slAddTail(&scaffoldList, pScaffold);
    chromSize += pScaffold->size;
    chromSize += GAP_SIZE;
    scaffoldCount++;
    }
printf("gap size is %d, total gaps: %d\n", GAP_SIZE, scaffoldCount);
printf("chrom size is %d\n", chromSize);

/* Munge file paths */
splitPath(scaffoldFile, outDir, outFile, ext);

sprintf(outPath, "%s%s.agp", outDir, outFile);
agpFile = mustOpen(outPath, "w");
printf("writing %s\n", outPath);

sprintf(outPath, "%s%s.lft", outDir, outFile);
liftFile = mustOpen(outPath, "w");
printf("writing %s\n", outPath);

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

/* Generate AGP and lift files */
for (pScaffold = scaffoldList; 
                pScaffold != NULL; pScaffold = pScaffold->next) 
    {
    end = start + pScaffold->size;

    /* Create AGP fragment for the scaffold */
    pFrag->frag = pScaffold->name;
    pFrag->ix = fileNumber++;
    pFrag->chromStart = start;
    pFrag->chromEnd = end;
    pFrag->fragEnd = pScaffold->size;
    agpFragOutput(pFrag, agpFile, '\t', '\n');

    /* Write lift file for this fragment */
    fprintf(liftFile, "%d\t%s\t%d\t%s\t%d\n",
            start, pScaffold->name, pScaffold->size, CHROM_NAME, chromSize);

    /* Create AGP gap to separate scaffolds */
    /* Note: may want to suppress final gap -- not needed as separator */
    start = end + 1;
    end = start + gapSize - 1;

    pGap->ix = fileNumber++;
    pGap->chromStart = start;
    pGap->chromEnd = end;
    agpGapOutput(pGap, agpFile, '\t', '\n');

    /* Write lift file for this gap */
    fprintf(liftFile, "%d\t%s\t%d\t%s\t%d\n",
            start-1, "gap", GAP_SIZE, CHROM_NAME, chromSize);

    start = end;
    }
carefulClose(&agpFile);
carefulClose(&liftFile);
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
