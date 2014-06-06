/* checkSgdSync - Make sure that genes and sequence are in sync for 
 * SGD yeast database. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"


static char *gffFile = "chromosomal_feature/s_cerevisiae.gff3";
static char *faExtn = "fsa";
static int gffRowCount = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkSgdSync - Make sure that genes and sequence are in sync for\n"
  "SGD yeast database\n"
  "usage:\n"
  "   checkSgdSync <sgdDownloadDir>\n"
  "options:\n"
  "   -gffRowCount=N - expected field count in gff file, default: %d\n"
  "   -faExtn=fastaExtension for sequence files in\n"
  "          <sgdDownloadDir>/chromosomes - default:'%s'\n"
  "   -gffFile=pathToGff - default:\n\t<sgdDownloadDir>/%s\n"
  "   -verbose=2 - show file arguments\n"
  "   -verbose=3 - show fasta files scanned\n"
  "   -verbose=4 - List status of every CDS\n"
  "example: checkSgdSync download -faExtn=fa\\\n\t-gffFile=chromosomal_feature/saccharomyces_cerevisiae.gff", gffRowCount, faExtn, gffFile
  );
}


static struct optionSpec options[] = {
    {"gffRowCount", OPTION_INT},
    {"faExtn", OPTION_STRING},
    {"gffFile", OPTION_STRING},
    {NULL, 0},
};

struct hash *loadChroms(char *dir)
/* Load zipped chromosome files into memory. */
{
FILE *f;
char fastaScan[16];
safef(fastaScan, sizeof(fastaScan), "*.%s", faExtn);
struct fileInfo *chromEl, *chromList = listDirX(dir, fastaScan, TRUE);
struct hash *chromHash = newHash(0);
struct dnaSeq *seq;
char chrom[128];
char *faName;
int count = 0;

verbose(2, "#    scanning '%s/%s'\n", dir, fastaScan);
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    char *fileName = chromEl->name;
    splitPath(fileName, NULL, chrom, NULL);
    chopSuffix(chrom);
    if (startsWith("chr0", chrom)) /* Convert chr01 to chr1, etc. */
	stripChar(chrom, '0');
    if (sameString(chrom, "chrmt"))
        strcpy(chrom, "chr17");
    f = fopen(fileName, "r");
    AllocVar(seq);
    seq->name = cloneString(chrom);
    if (!faFastReadNext(f, &seq->dna, &seq->size, &faName))
        errAbort("Couldn't load sequence from %s", fileName);
    seq->dna = cloneMem(seq->dna, seq->size+1);
    toUpperN(seq->dna, seq->size);
    hashAdd(chromHash, chrom, seq);
    verbose(3, "#    loadChrom %s '%s'\n", fileName, chrom);
    fclose(f);
    f = NULL;
    count++;
    }
if (0 == count)
    errAbort("not fasta files found in '%s/%s'\n", dir, fastaScan);
return chromHash;
}

void checkGff(char *gff, struct hash *chromHash)
/* Check that CDS portions of GFF file have start
 * codons where they are supposed to. */
{
struct lineFile *lf = lineFileOpen(gff, TRUE);
char *row[10];
int cdsCount = 0, goodCount = 0, badCount = 0;

verbose(2,"#    scanning %d fields of gff file:\n#\t'%s'\n", gffRowCount, gff);
while (lineFileNextRowTab(lf, row, gffRowCount))
    {
    if (startsWith("CDS", row[2]))
        {
	int start = lineFileNeedNum(lf, row, 3) - 1;
	int end = lineFileNeedNum(lf, row, 4);
	int size = end-start;
	char strand = row[6][0];
	char chrom[64];
	struct dnaSeq *seq;
	char *startCodon;
	
	if (size < 1)
	    {
	    errAbort("start not before end line %d of %s",
	    	lf->lineIx, lf->fileName);
	    }
	if (strand != '+' && strand != '-')
	    {
	    errAbort("Expecting strand got %s line %d of %s",
	    	row[6], lf->lineIx, lf->fileName);
	    }
	if (startsWith("2-micron", row[0]))  // need to stop processing here
	    break;
	if (!startsWith("chr", row[0]))
	    continue;
	if (startsWith("chrMito", row[0]))	// change name to UCSC chrM
	    safef(chrom, sizeof(chrom), "%s", "chrM");
	else
	    safef(chrom, sizeof(chrom), "%s", row[0]);
	if ((seq = hashFindVal(chromHash, chrom)) == NULL)
	    errAbort("Unknown chromosome %s line %d of %s",
	    	row[0], lf->lineIx, lf->fileName);
	if (end > seq->size)
	    {
	    printf("end (%d) greater than %s size (%d) line %d of %s",
	        end, chrom, seq->size, lf->lineIx, lf->fileName);
	    ++badCount;
	    continue;
	    }
	startCodon = seq->dna + start;
	if (strand == '-')
	    reverseComplement(startCodon, size);
	if (!startsWith("ATG", startCodon))
	    {
	    char *s = startCodon;
	    verbose(3,"# not ATG: %s:%d-%d\t%c%c%c\t%c\n",
		chrom, start, end, s[0], s[1], s[2], strand);
	    ++badCount;
	    }
	else
	    ++goodCount;
	if (verboseLevel()>=4)
	    {
	    char *s = startCodon;
	    if (gffRowCount > 9)
		printf("%s\t%d\t%c%c%c\t%c\t%s\n", chrom, start, s[0], s[1], s[2], strand, row[9]);
	    else
		printf("%s\t%d\t%c%c%c\t%c\t%s\n", chrom, start, s[0], s[1], s[2], strand, row[8]);
	    }
	if (strand == '-')
	    reverseComplement(startCodon, size);
	++cdsCount;
	}
    }
lineFileClose(&lf);
printf("#    good %d, bad %d, total %d\n", goodCount, badCount, cdsCount);
}

void checkSgdSync(char *downloadDir)
/* checkSgdSync - Make sure that genes and sequence are in sync for 
 * SGD yeast database. */
{
struct hash *chromHash;
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", downloadDir, "chromosomes");
chromHash = loadChroms(path);
safef(path, sizeof(path), "%s/%s", 
	downloadDir, gffFile);
checkGff(path, chromHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
gffRowCount = optionInt("gffRowCount", gffRowCount);
faExtn = optionVal("faExtn", faExtn);
gffFile = optionVal("gffFile", gffFile);
checkSgdSync(argv[1]);
return 0;
}
