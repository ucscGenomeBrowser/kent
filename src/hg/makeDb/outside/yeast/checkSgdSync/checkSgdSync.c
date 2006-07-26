/* checkSgdSync - Make sure that genes and sequence are in sync for 
 * SGD yeast database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"

static char const rcsid[] = "$Id: checkSgdSync.c,v 1.1 2006/07/26 04:00:16 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkSgdSync - Make sure that genes and sequence are in sync for\n"
  "SGD yeast database\n"
  "usage:\n"
  "   checkSgdSync sgdDownloadDir\n"
  "options:\n"
  "   -verbose=2 - List status of every CDS\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *loadChroms(char *dir)
/* Load zipped chromosome files into memory. */
{
FILE *f;
struct fileInfo *chromEl, *chromList = listDirX(dir, "*.fsa", TRUE);
struct hash *chromHash = newHash(0);
struct dnaSeq *seq;
char chrom[128];
char *faName;

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
    fclose(f);
    f = NULL;
    }
return chromHash;
}

void checkGff(char *gff, struct hash *chromHash)
/* Check that CDS portions of GFF file have start
 * codons where they are supposed to. */
{
struct lineFile *lf = lineFileOpen(gff, TRUE);
char *row[10];
int start,end;
char *strand;
int cdsCount = 0, goodCount = 0, badCount = 0;

while (lineFileRowTab(lf, row))
    {
    if (startsWith("CDS", row[2]))
        {
	int start = lineFileNeedNum(lf, row, 3) - 1;
	int end = lineFileNeedNum(lf, row, 4);
	int size = end-start;
	char strand = row[6][0];
	boolean good = TRUE;
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
	safef(chrom, sizeof(chrom), "chr%s", row[0]);
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
	    ++badCount;
	else
	    ++goodCount;
	if (verboseLevel()>=2)
	    {
	    char *s = startCodon;
	    printf("%s\t%d\t%c%c%c\t%c\t%s\n", chrom, start, s[0], s[1], s[2], strand, row[9]);
	    }
	if (strand == '-')
	    reverseComplement(startCodon, size);
	++cdsCount;
	}
    }
lineFileClose(&lf);
printf("good %d, bad %d, total %d\n", goodCount, badCount, cdsCount);
}

void checkSgdSync(char *downloadDir)
/* checkSgdSync - Make sure that genes and sequence are in sync for 
 * SGD yeast database. */
{
struct hash *chromHash;
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", downloadDir, "chromosomes");
chromHash = loadChroms(path);
safef(path, sizeof(path), "%s/chromosomal_feature/s_cerevisiae.gff3", 
	downloadDir);
checkGff(path, chromHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
checkSgdSync(argv[1]);
return 0;
}
