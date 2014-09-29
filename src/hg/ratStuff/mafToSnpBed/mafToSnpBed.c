/* mafToSnpBed - finds SNPs in MAF and builds a bed with their functional consequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genePred.h"
#include "maf.h"
#include "basicBed.h"
#include "soTerm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToSnpBed - finds SNPs in MAF and builds a bed with their functional consequence\n"
  "usage:\n"
  "   mafToSnpBed database input.maf input.gp output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static boolean inExon(struct genePred *gp, int base)
{
int ii;  
for (ii = 0;  ii < gp->exonCount;  ii++)
    if ((base < gp->exonEnds[ii]) && (base >= gp->exonStarts[ii]))
	return TRUE;
return FALSE;
}

static void parseOnePair(struct mafComp *mcMaster, struct mafComp *mcOther, struct genePred *gp,  FILE *f)
{
char *mptr = mcMaster->text;
char *optr = mcOther->text;
int start = mcMaster->start;
boolean inUnaligned = FALSE;
int unAlignedStart, unAlignedEnd;
int so;
char *species = cloneString(mcOther->src);
char *ptr = strchr(species, '.');
if (ptr != NULL)
    *ptr = 0;

for(; *mptr; )
    {
    char masterCh = *mptr++;
    char otherCh = *optr++;

    if ((masterCh == '-') || (masterCh == '.'))
	continue;

    if ((otherCh == '-') || (otherCh == '.'))
	{
	if (inUnaligned)
	    {
	    unAlignedEnd++;
	    }
	else
	    {
	    unAlignedEnd = unAlignedStart = start;
	    inUnaligned = TRUE;
	    }
	start++;
	continue;
	}

    if (inUnaligned)
	{
	fprintf(f,"%s %d %d %s %d\n", mcMaster->src, unAlignedStart, unAlignedEnd+1, species, 0);
	inUnaligned = FALSE;
	}

    so = 1234;
    if (masterCh != otherCh)
	{
	if ((start < gp->txEnd) && (start >= gp->txStart))
	    {
	    if (inExon(gp, start))
		{
		if (start >= gp->cdsStart)
		    {
		    if (start < gp->cdsEnd)
		       so = coding_sequence_variant;
		    else
			so = _3_prime_UTR_variant;
		    }
		else 
		    so = _5_prime_UTR_variant;
		}
	    else
		so = intron_variant;
	    }
	else
	    so = intergenic_variant;

	fprintf(f,"%s %d %d %s %d\n", mcMaster->src, start, start+1, species, so);   
	}

    start++;
    }

if (inUnaligned)
    fprintf(f,"%s %d %d %s %d\n", mcMaster->src, unAlignedStart, unAlignedEnd+1, species, 0);
}

static void parseOneAli(struct mafAli *aliList, struct genePred *gp,  FILE *f)
{
struct mafComp *mcMaster = aliList->components;
struct mafComp *mcOther = mcMaster->next;

for(; mcOther; mcOther = mcOther->next)
    {
    parseOnePair(mcMaster, mcOther, gp, f);
    }
}

static void parseOneGp(char *database, struct mafAli *aliList, struct genePred *gp, FILE *f)
{
struct mafAli *ali = aliList;

for(; ali; ali = ali->next)
    parseOneAli(ali, gp, f);
}

void mafToSnpBed(char *database, char *mafIn, char *gpIn, char *bedOut)
/* mafToSnpBed - finds SNPs in MAF and builds a bed with their functional consequence. */
{
struct mafFile *mafFile = mafReadAll(mafIn);
struct genePred *genePred = genePredLoadAll(gpIn);
FILE *f = mustOpen(bedOut, "w");

struct genePred *gp, *next;
for(gp = genePred; gp; gp = next)
    {
    next = gp->next;
    gp->next = NULL;
    parseOneGp(database, mafFile->alignments, gp, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
mafToSnpBed(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
