/* txGeneFromBed - Convert from bed to knownGenes format table (genePred + uniProt ID). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "cdsPick.h"

static char const rcsid[] = "$Id: txGeneFromBed.c,v 1.2 2007/03/17 23:10:59 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneFromBed - Convert from bed to knownGenes format table (genePred + uniProt ID)\n"
  "usage:\n"
  "   txGeneFromBed in.bed in.picks out.kg\n"
  "where:\n"
  "   in.bed is input file in bed 12 format\n"
  "   in.picks is one of the outputs of txCdsPick\n"
  "   out.kg is known genes table in tab-separated format\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outputKg(struct bed *bed, char *uniProtAcc, FILE *f)
/* Convert bed to genePred with a few extended fields and write. */
{
fprintf(f, "%s\t", bed->name);
fprintf(f, "%s\t", bed->chrom);
fprintf(f, "%s\t", bed->strand);
fprintf(f, "%d\t", bed->chromStart);
fprintf(f, "%d\t", bed->chromEnd);
fprintf(f, "%d\t", bed->thickStart);
fprintf(f, "%d\t", bed->thickEnd);
fprintf(f, "%d\t", bed->blockCount);
int i;
for (i=0; i<bed->blockCount; ++i)
    fprintf(f, "%d,", bed->chromStart + bed->chromStarts[i]);
fprintf(f, "\t");
for (i=0; i<bed->blockCount; ++i)
    fprintf(f, "%d,", bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i]);
fprintf(f, "\t");
fprintf(f, "%s\t", uniProtAcc);
fprintf(f, "%s\n", bed->name);
}

void txGeneFromBed(char *inBed, char *inPicks, char *outKg)
/* txGeneFromBed - Convert from bed to knownGenes format table (genePred + uniProt ID). */
{
/* Load picks into hash.  We don't use cdsPicksLoadAll because empty fields
 * cause that autoSql-generated routine problems. */
struct hash *pickHash = newHash(18);
struct cdsPick *pick;
struct lineFile *lf = lineFileOpen(inPicks, TRUE);
char *row[CDSPICK_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    pick = cdsPickLoad(row);
    hashAdd(pickHash, pick->name, pick);
    }

/* Load in bed */
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);

/* Do reformatting and write output. */
FILE *f = mustOpen(outKg, "w");
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    char *protAcc = "";
    if (bed->thickStart < bed->thickEnd)
	{
        pick = hashMustFindVal(pickHash, bed->name);
	if (pick->swissProt[0])
	    protAcc = pick->swissProt;
	else if (pick->refProt[0])
	    protAcc = pick->refProt;
	else if (pick->uniProt[0])
	    protAcc = pick->uniProt;
	}
    outputKg(bed, protAcc, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txGeneFromBed(argv[1], argv[2], argv[3]);
return 0;
}
