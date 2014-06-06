/* hapmapPhaseIIISummary -- read in .bed files for subtracks of
 * hapmapSnps (rel27: phaseII+III merged data, also hapmapAlleles{Chimp,Macaque)
 * and write out .bed for hapmapPhaseIIISummary, which hgTracks uses for
 * filtering items in all of those subtracks. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* This is inflexible about filenames (very rel27-specific).  If we
 * ever need flexibility, add args and options... */

/* Some properties of rel27 genotype data (hapmapSnps*):
 * - observed does not come from dbSNP.  It is assayed allele1/allele2.
 *   That means that as far as the genotype files are concerned, there 
 *   are no complex SNPs -- all are bi-allelic.
 * - hapmapSnps strand is always +.
 * - In filtering, there is no need for "none" as minor allele -- we use 
 *   the monomorphicInPop flags instead. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "linefile.h"
#include "hapmapSnps.h"
#include "hapmapAllelesOrtho.h"
#include "hapmapPhaseIIISummary.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"hapmapPhaseIIISummary - Make hapmapPhaseIIISummary.bed from hapmap*.bed.\n"
"usage:\n"
"   hapmapPhaseIIISummary .\n"
/* could add options to provide support for different dbSNP versions.
"options:\n"
"   -xxx=XXX\n"
*/
"\n"
". is a placeholder arg to suppress this message.  The current directory\n"
"must have table-loading .bed files for SNPS from the 11 HapMap phaseIII\n"
"populations, as well as orthologous alleles for chimp and macaque:\n"
"    hapmapSnps{ASW,CEU,CHB,CHG,GIH,JPT,LWK,MEX,MKK,TSI,YRI}.bed\n"
"    hapmapAlleles{Chimp,Macaque}.bed\n"
"The data from those files is distilled into a form useful for filtering\n"
"based on aggregate characteristics in hgTracks, and dumped out to this\n"
"output file:\n"
"    hapmapPhaseIIISummary.bed\n"
"\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct summary
/* Accumulator for info from each subtrack of HapMap SNPs */
{
    struct summary *next;
    struct hapmapPhaseIIISummary *finalSum;
    char popMajorAlleles[11];
    char popMinorAlleles[11];
};

float minorAlFreq(struct hapmapSnps *hs)
/* Compute the minor allele frequency of hs. */
{
float mAF = ((2.0*hs->homoCount1 + hs->heteroCount) /
	     (2.0*(hs->homoCount1 + hs->homoCount2 + hs->heteroCount)));
if (mAF > 0.5)
    mAF = (1.0 - mAF);
return mAF;
}

#define majorAllele(hs) ((hs->homoCount1 < hs->homoCount2) ? hs->allele2[0] : hs->allele1[0])
#define minorAllele(hs) ((hs->homoCount1 < hs->homoCount2) ? hs->allele1[0] : hs->allele2[0])
#define heterozygosity(hs) ((int)(1000.0 * hs->heteroCount / \
                                  (hs->homoCount1 + hs->homoCount2 + hs->heteroCount) + 0.5))

struct summary *summaryNew(struct hapmapSnps *hs, int popIndex)
/* Return a summary to which hs's info has been added. */
{
struct summary *sum;
AllocVar(sum);
char *popCode = hapmapPhaseIIIPops[popIndex];
char majorAl = majorAllele(hs);
char minorAl = minorAllele(hs);
AllocVar(sum->finalSum);
struct hapmapPhaseIIISummary *fs = sum->finalSum;
fs->chrom = cloneString(hs->chrom);
fs->chromStart = hs->chromStart;
fs->chromEnd = hs->chromEnd;
fs->name = cloneString(hs->name);
fs->score = heterozygosity(hs);
fs->observed = cloneString(hs->observed);;
fs->overallMajorAllele = majorAl;
fs->overallMinorAllele = minorAl;
fs->popCount = 1;
if (sameString(popCode, "CEU") || sameString(popCode, "CHB") || sameString(popCode, "JPT") ||
    sameString(popCode, "YRI"))
    fs->phaseIIPopCount = 1;
fs->foundInPop[popIndex] = TRUE;
if (hs->heteroCount == 0 && (hs->homoCount1 == 0 || hs->homoCount2 == 0))
    fs->monomorphicInPop[popIndex] = TRUE;
fs->orthoCount = HAP_ORTHO_COUNT;
fs->orthoAlleles = cloneString("NN");
fs->orthoQuals = needMem(2 * sizeof(unsigned short));
fs->minFreq = fs->maxFreq = minorAlFreq(hs);

sum->popMajorAlleles[popIndex] = majorAl;
sum->popMinorAlleles[popIndex] = minorAl;
return sum;
}

void addSnpToSum(struct summary *sum, struct hapmapSnps *hs, int popIndex)
/* Fold in hs's info into the accumulator sum. */
{
char *popCode = hapmapPhaseIIIPops[popIndex];
char majorAl = majorAllele(hs);
char minorAl = minorAllele(hs);
struct hapmapPhaseIIISummary *fs = sum->finalSum;
if ((!sameString(fs->chrom, hs->chrom) &&
     !sameString(fs->chrom, "chrX") && !sameString(hs->chrom, "chrY")) || // PAR OK
    fs->chromStart != hs->chromStart ||
    fs->chromEnd != hs->chromEnd || !sameString(fs->name, hs->name))
    errAbort("Incompat summary and hs?! %s|%d|%d|%s != %s|%d|%d|%s",
	     fs->chrom, fs->chromStart, fs->chromEnd, fs->name,
	     hs->chrom, hs->chromStart, hs->chromEnd, hs->name);
fs->score += heterozygosity(hs);
if (!sameString(fs->observed, hs->observed))
    errAbort("Diff observed!?  %s != %s's %s", fs->observed, popCode, hs->observed);
if (fs->overallMajorAllele != majorAl)
    fs->isMixed = TRUE;
fs->popCount++;
if (sameString(popCode, "CEU") || sameString(popCode, "CHB") || sameString(popCode, "JPT") ||
    sameString(popCode, "YRI"))
    fs->phaseIIPopCount++;
fs->foundInPop[popIndex] = TRUE;
if (hs->heteroCount == 0 && (hs->homoCount1 == 0 || hs->homoCount2 == 0))
    fs->monomorphicInPop[popIndex] = TRUE;
sum->popMajorAlleles[popIndex] = majorAl;
sum->popMinorAlleles[popIndex] = minorAl;
float mAF = minorAlFreq(hs);
if (mAF < fs->minFreq)
    fs->minFreq = mAF;
if (mAF > fs->maxFreq)
    fs->maxFreq = mAF;
}

void addOrthoToSum(struct summary *sum, struct hapmapAllelesOrtho *ho, int orthoIndex)
/* Fold in orthologous allele & base quality info into the accumulator sum. */
{
struct hapmapPhaseIIISummary *fs = sum->finalSum;
fs->orthoAlleles[orthoIndex] = ho->orthoAllele[0];
fs->orthoQuals[orthoIndex] = ho->score;
}

void dumpHapmapPhaseIIISummary()
/* Read .bed files, accumulate info, and aggregate into hapmapPhaseIIISummary file. */
{
int i;
char inFile[256];
struct lineFile *lf = NULL;
int wordCount;
char *words[13];
char key[128];
struct summary *sum, *sumList = NULL;
struct hash *hash = hashNew(24);

for (i = 0;  i < HAP_PHASEIII_POPCOUNT;  i++)
    {
    struct hapmapSnps hs;
    safef(inFile, sizeof(inFile), "hapmapSnps%s.bed", hapmapPhaseIIIPops[i]);
    lf = lineFileOpen(inFile, TRUE);
    while ((wordCount = lineFileChopTab(lf, words)) > 0)
	{
	lineFileExpectWords(lf, 12, wordCount);
	hapmapSnpsStaticLoad(words, &hs);
	// Key by chrom as well as name because the pseudoautosomal regions (PAR)
	// of chrX and chrY have independent (but identical) SNP items.
	safef(key, sizeof(key), "%s:%s", hs.chrom, hs.name);
	sum = hashFindVal(hash, key);
	if (sum == NULL)
	    {
	    sum = summaryNew(&hs, i);
	    hashAdd(hash, key, sum);
	    slAddHead(&sumList, sum);
	    }
	else
	    addSnpToSum(sum, &hs, i);
	}
    lineFileClose(&lf);
    }
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    struct hapmapAllelesOrtho ho;
    safef(inFile, sizeof(inFile), "hapmapAlleles%s.bed", hapmapOrthoSpecies[i]);
    lf = lineFileOpen(inFile, TRUE);
    while ((wordCount = lineFileChopTab(lf, words)) > 0)
	{
	lineFileExpectWords(lf, 13, wordCount);
	hapmapAllelesOrthoStaticLoad(words, &ho);
	safef(key, sizeof(key), "%s:%s", ho.chrom, ho.name);
	sum = hashFindVal(hash, key);
	if (sum == NULL)
	    errAbort("Ortho SNP '%s' doesn't match any HapMap SNPs!", ho.name);
	addOrthoToSum(sum, &ho, i);
	}
    lineFileClose(&lf);
    }
slReverse(&sumList);
// That leaves it mostly sorted, but not all!  Leave final sorting up to hgLoadBed.
FILE *f = mustOpen("hapmapPhaseIIISummary.bed", "w");
for (sum = sumList;  sum != NULL;  sum = sum->next)
    {
    struct hapmapPhaseIIISummary *fs = sum->finalSum;
    // Convert fs->score (heterozygosity * 1000) from total into average:
    fs->score = (int)((float)fs->score / fs->popCount + 0.5);
    // Determine whether the overall{Major,Minor}Alleles are indeed the same
    // as the first population encountered:
    char firstPopMajorAl = fs->overallMajorAllele;
    char firstPopMinorAl = fs->overallMinorAllele;
    int firstPopYea = 0, firstPopNay = 0;
    for (i = 0;  i < HAP_PHASEIII_POPCOUNT;  i++)
	{
	if (fs->foundInPop[i])
	    {
	    if (sum->popMajorAlleles[i] == firstPopMajorAl)
		firstPopYea++;
	    else
		firstPopNay++;
	    }
	}
    if (firstPopNay > firstPopYea)
	{
	fs->overallMajorAllele = firstPopMinorAl;
	fs->overallMinorAllele = firstPopMajorAl;
	}
    hapmapPhaseIIISummaryTabOut(fs, f);
    }
carefulClose(&f);
// All done -- no need to waste time freeing hash and sumList.
}

int main(int argc, char *argv[])
/* Make hapmapPhaseIIISummary.bed */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
dumpHapmapPhaseIIISummary();
return 0;
}

