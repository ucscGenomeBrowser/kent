/* hgvsToVcf - Convert HGVS terms to VCF tab-separated output. */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "genbank.h"
#include "hdb.h"
#include "hgHgvs.h"
#include "vcf.h"
#include "linefile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgvsToVcf - Convert HGVS terms to VCF tab-separated output\n"
  "usage:\n"
  "   hgvsToVcf db input.hgvs output.vcf\n"
  "options:\n"
  "   -noLeftShift        Don't do the VCF-conventional left shifting of ambiguous placements\n"
  "db is a UCSC database such as hg19, hg38 etc.\n"
  "Only nucleotide HGVS terms (g., c., n., m.) are supported, not protein (p.).\n"
  );
}

/* Command line option global vars */
static boolean noLeftShift = TRUE;

/* Command line validation table. */
static struct optionSpec options[] = {
    {"noLeftShift", OPTION_BOOLEAN},
    {NULL, 0},
};

static void writeVcfHeader(FILE *f, char *db, char *inFile)
/* Write a simple VCF header to f. */
{
fprintf(f,
        "##fileformat=VCFv4.2\n"
        "##reference=%s\n"
        "##source=hgvsToVcf %s %s\n"
        HGVS_VCF_HEADER_DEFS
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n", db, db, inFile);
}

void hgvsToVcf(char *db, char *inFile, char *outFile, boolean doLeftShift)
/* hgvsToVcf - Convert HGVS terms to VCF tab-separated output. */
{
initGenbankTableNames(db);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
writeVcfHeader(f, db, inFile);
struct vcfRow *rowList = NULL;
struct dyString *dyError = dyStringNew(0);
char *term = NULL;
while (lineFileNextReal(lf, &term))
    {
    // In order to output position-sorted VCF, we need to accumulate VCF rows and sort before
    // printing.  In the meantime, print out a comment line for each HGVS term that we are
    // unable to parse/map/etc.
    dyStringClear(dyError);
    struct vcfRow *row = hgvsToVcfRow(db, term, doLeftShift, dyError);
    if (row)
        slAddHead(&rowList, row);
    else
        fprintf(f, "# %s\n", dyStringContents(dyError));
    }
// Reverse so that when multiple terms map to a position, they are more or less in the
// same order that we saw in the input.
slReverse(&rowList);
slSort(&rowList, vcfRowCmp);
vcfRowTabOutList(f, rowList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
noLeftShift = optionExists("noLeftShift");
if (argc != 4)
    usage();
hgvsToVcf(argv[1], argv[2], argv[3], !noLeftShift);
return 0;
}
