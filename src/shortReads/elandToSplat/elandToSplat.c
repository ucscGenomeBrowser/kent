/* elandToSplat - Convert eland output to splat output.. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: elandToSplat.c,v 1.5 2008/11/06 07:03:00 kent Exp $";

boolean multi = FALSE;
char *dna = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elandToSplat - Convert eland output to splat output.\n"
  "usage:\n"
  "   elandToSplat in.vmf out.splat\n"
  "options:\n"
  "   -multi Use eland multiple-mapping format rather than default.\n"
  );
}

static struct optionSpec options[] = {
   {"multi", OPTION_BOOLEAN},
   {NULL, 0},
};

void parseOutMulti(char *readName, char *seq, char *mappings, struct lineFile *lf, FILE *f)
/* Parse out mappings, and output a splat line for each one. */
/* Mappings look like: 
   18_21_22.fa/chr18:17714750R2,18_21_22.fa/chr21:10039036F2,17066364R1,39312480R2,18_21_22.fa/chr22:20454541R1
   or just
   chr22.fa:10039036F2,17066364R1 */
{
if (readName[0] == '>')
    readName += 1;
char *chrom = NULL;
char *s, *e;
int seqLen = strlen(seq);
int commaCount = countChars(mappings, ',');
for (s = mappings; s != NULL && s[0] != 0; s = e)
    {
    e = strchr(s, ',');
    if (e != NULL)
      *e++ = 0;
    char *colon = strchr(s, ':');
    if (colon != NULL)
       {
       chrom = s;
       char *slash = strchr(s, '/');
       *colon = 0;
       if (slash != NULL)
	   chrom = slash+1;
       else
	   chopSuffix(chrom);
       s = colon + 1;
       }
    char strand = '+';
    char *endNum = NULL;
    if ((endNum = strchr(s, 'F')) != NULL)
       *endNum = 0;
    else if ((endNum = strchr(s, 'R')) != NULL)
       {
       *endNum = 0;
       strand = '-';
       }
    else
       errAbort("Expecting a 'R' or 'F' after number line %d of %s\n", lf->lineIx, lf->fileName);
    if (chrom == NULL)
       errAbort("Expecting chromosome line %d of %s\n", lf->lineIx, lf->fileName);
    int start = sqlUnsigned(s);
    fprintf(f, "%s\t%d\t%d\t%s\t%d\t%c\t%s\n", 
    	chrom, start, start+seqLen, seq, 1000/(commaCount+1), strand, readName);
    }
}

void multiElandToSplat(char *vmfIn, char *splatOut)
/* multiElandToSplat - Convert eland multiple mapping output to splat output.. */
{
struct lineFile *lf = lineFileOpen(vmfIn, TRUE);
FILE *f = mustOpen(splatOut, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *words[5];
    int wordCount = chopLine(line, words);
    if (wordCount != 3 && wordCount != 4)
       errAbort("Expecting 3 or 4 columns line %d of %s, got %d", lf->lineIx, lf->fileName, wordCount);
    if (wordCount == 4)
	parseOutMulti(words[0], words[1], words[3], lf, f);
    }
}

void singleElandToSplat(char *vmfIn, char *splatOut)
/* Convert single mapping (default) eland output to splat output. 
 * The file format of single-mapping eland is tab-delimited, but with
 * a variable number of fields.  See eland docs for details.  Here's an
 * example:
 *  11      >chr22_20M_752479_752636_a/1    TGGGGGAAGCCAGTTGCCATTTGTG       U1      0       1       0       21_22.fa/chr22  20752479        F       ..      6

 * The fields are:
 *    0 - read index?
 *    1 - read name
 *    2 - read sequence
 *    3 - match code
 *    4 - number of perfect matches
 *    5 - number of matches with one substitution
 *    6 - number of matches with two substitutions
 *    7 - target (chromosome) sequence
 *    8 - position in target sequence - starting counting with 1.  Disregards strand
 *    9 - strand - either F or R
 *   10 - correction code for N's
 *   11 - position of first substitution in genome forward strand coordinates relative to field 8
 *   12 - position of second substitution
 * Note the last two fields are only present when the best match has one or two mismatches.
 * Fields 7-12 are missing when there is no mapping.  There may in the case of low quality
 * reads be lines with only the first 4 fields, but I haven't seen that. */
{
struct lineFile *lf = lineFileOpen(vmfIn, TRUE);
FILE *f = mustOpen(splatOut, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    /* Parse into space-delimited words and make sure that the number of words
     * is within the relatively wide range that we accept. */
    char *words[16];
    int wordCount = chopLine(line, words);
    if (wordCount < 4)
       errAbort("Expecting at least 4 columns line %d of %s, got %d", 
       	lf->lineIx, lf->fileName, wordCount);
    if (wordCount > 13)
       errAbort("Expecting no more than 13 columns line %d of %s, got %d",
       	lf->lineIx, lf->fileName, wordCount);

    /* Go to next line if we don't have enough fields to convey a match. */
    if (wordCount < 11)
        continue;	/* No match of some sort. */

    /* Get read name, and skip over the fasta > char if present. */
    char *readName = words[1];
    if (readName[0] == '>')
        ++readName;

    /* Look at the columns that say the number of matches at various substitution
     * levels and figure out number of substitutions for best hit, and number of
     * these best hits. */
    int sub0 = lineFileNeedNum(lf, words, 4);
    int sub1 = lineFileNeedNum(lf, words, 5);
    int sub2 = lineFileNeedNum(lf, words, 6);
    int matchCount = -1;
    int subCount = -1;
    if (sub0 > 0)
	{
        subCount = 0;
	matchCount = sub0;
	}
    else if (sub1 > 0)
	{
        subCount = 1;
	matchCount = sub1;
	}
    else if (sub2 > 0)
	{
        subCount = 2;
	matchCount = sub2;
	}
    else
        errAbort("Expecting non-zero in columns 5, 6, or 7 line %d of %s\n", 
		lf->lineIx, lf->fileName);

    /* Convert strand from F/R to +/- */
    char *strandChar = words[9];
    char strand = '.';
    if (strandChar[0] == 'F')
        strand = '+';
    else if (strandChar[0] == 'R')
        strand = '-';
    else
        errAbort("Expecting F or R got %s column 10, line %d of %s\n", words[9], lf->lineIx, lf->fileName);
  

    /* Make sure we have enough columns to deal with all the substitutions. */
    if (wordCount != 11 + subCount)
        errAbort("With %d substitutions expecting %d columns, got %d line %d of %s\n",
		subCount, 11+subCount, wordCount, lf->lineIx, lf->fileName);

    /* Turn sequence into all upper case except for the mismatches. */
    char *readSeq = words[2];
    int seqLen = strlen(readSeq);
    toUpperN(readSeq, seqLen);
    int i;
    for (i=0; i<subCount; ++i)
        {
	int subIx = atoi(words[11+i]);
	if (subIx > seqLen || subIx <= 0)
	    errAbort("Expecting 1 to %d (readLength) in substitution field, got %d (%s), line %d of %s",
	    	seqLen, subIx, (words[11+i]), lf->lineIx, lf->fileName);
	subIx -= 1;	/* Get out of one based coordinates. */
	if (strand == '-')
	    subIx = seqLen - subIx - 1; 
	readSeq[subIx] = tolower(readSeq[subIx]);
	}

    /* Parse out chromosome name, which may be in file/record or just file format. 
     * That is   21_22.fa/chr22  or chr22.fa */
    char chromName[256];
    splitPath(words[7], NULL, chromName, NULL);
    int chromStart = lineFileNeedNum(lf, words, 8) - 1;

    /* Write out splat record. */
    fprintf(f, "%s\t%d\t%d\t", chromName,  chromStart, chromStart + seqLen);
    fprintf(f, "%s\t%d\t%c\t%s\n", readSeq, 1000/matchCount, strand, readName);
    }
}

void elandToSplat(char *vmfIn, char *splatOut)
/* elandToSplat - Convert eland output to splat output.. */
{
if (multi)
    multiElandToSplat(vmfIn, splatOut);
else
    singleElandToSplat(vmfIn, splatOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
multi = optionExists("multi");
elandToSplat(argv[1], argv[2]);
return 0;
}
