/* getRnaPred - Get RNA for gene predictions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "genePred.h"
#include "fa.h"
#include "jksql.h"
#include "hdb.h"
#include "options.h"

static char const rcsid[] = "$Id: getRnaPred.c,v 1.8 2004/01/19 23:53:56 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRnaPred - Get RNA for gene predictions\n"
  "usage:\n"
  "   getRnaPred [options] database table chromosome output.fa\n"
  "table can be a table or a file.  Specify chromosome of 'all' to\n"
  "to process all chromosome\n"
  "\n"
  "options:\n"
  "   -weird - only get ones with weird splice sites\n"
  "   -cdsUpper - output CDS in update case\n"
  "   -cdsOut=file - write CDS to this tab-separated file, in the form\n"
  "      acc  start..end\n"
  "    where start..end are genbank style, one-based coordinates\n"
  );
}

static struct optionSpec options[] = {
   {"weird", OPTION_BOOLEAN},
   {"cdsOut", OPTION_STRING},
   {NULL, 0},
};


/* parsed from command line */
boolean weird, cdsUpper;
char *cdsOut = NULL;

boolean hasWeirdSplice(struct genePred *gp)
/* see if a gene has weird splice sites */
{
int i;
boolean gotOdd = FALSE;
for (i=1; (i<gp->exonCount) && !gotOdd; ++i)
    {
    int start = gp->exonEnds[i-1];
    int end = gp->exonStarts[i];
    int size = end - start;
    if (size > 0)
        {
        struct dnaSeq *seq = hDnaFromSeq(gp->chrom, start, end, dnaLower);
        DNA *s = seq->dna;
        DNA *e = seq->dna + seq->size;
        uglyf("%s %c%c/%c%c\n", gp->name, s[0], s[1], e[-2], e[-1]);
        if (gp->strand[0] == '-')
            reverseComplement(seq->dna, seq->size);
        if (s[0] != 'g' || (s[1] != 't' && s[1] != 'c') || e[-2] != 'a' || e[-1] != 'g')
            gotOdd = TRUE;
        freeDnaSeq(&seq);
        }
    }
return gotOdd;
}

int findGeneOff(struct genePred *gp, int chrPos)
/* find the mrna offset containing the specified position.*/
{
int iRna = 0, iExon;
int prevEnd = gp->exonStarts[0];

for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    if (chrPos < gp->exonStarts[iExon])
        {
        /* intron before this exon */
        return iRna;
        }
    if (chrPos < gp->exonEnds[iExon])
        {
        return iRna + (chrPos - gp->exonStarts[iExon]);
        }
    iRna += (gp->exonEnds[iExon] - gp->exonStarts[iExon]);
    prevEnd = gp->exonEnds[iExon];
    }
return iRna-1;
}

void processCds(struct genePred *gp, struct dyString *dnaBuf, FILE* cdsFh)
/* find the CDS bounds in the mRNA defined by the annotation.
 * output and/or update as requested */
{
int cdsStart = findGeneOff(gp, gp->cdsStart);
int cdsEnd = findGeneOff(gp, gp->cdsEnd-1)+1;
int rnaSize = genePredBases(gp);
if (gp->strand[0] == '-')
    reverseIntRange(&cdsStart, &cdsEnd, rnaSize);
assert(cdsStart <= cdsEnd);
assert(cdsEnd <= rnaSize);

if (cdsUpper)
    toUpperN(dnaBuf->string+cdsStart, (cdsEnd-cdsStart));

if (cdsFh != NULL)
    fprintf(cdsFh,"%s\t%d..%d\n", gp->name, cdsStart+1, cdsEnd);
}

void processGenePred(struct genePred *gp, struct dyString *dnaBuf, FILE* faFh, FILE* cdsFh)
/* output genePred DNA, check for weird splice sites if requested */
{
int i;
if (weird && !hasWeirdSplice(gp))
    return;

/* Load exons one by one into dna string. */
dyStringClear(dnaBuf);
for (i=0; i<gp->exonCount; ++i)
    {
    int start = gp->exonStarts[i];
    int end = gp->exonEnds[i];
    int size = end - start;
    if (size < 0)
        warn("%d sized exon in %s\n", size, gp->name);
    else
        {
        struct dnaSeq *seq = hDnaFromSeq(gp->chrom, start, end, dnaLower);
        dyStringAppendN(dnaBuf, seq->dna, size);
        freeDnaSeq(&seq);
        }
    }

/* Reverse complement if necessary */
if (gp->strand[0] == '-')
    reverseComplement(dnaBuf->string, dnaBuf->stringSize);

if (gp->cdsStart < gp->cdsEnd)
    processCds(gp, dnaBuf, cdsFh);
faWriteNext(faFh, gp->name, dnaBuf->string, dnaBuf->stringSize);

}

void getRnaForTable(char *table, char *chrom, struct dyString *dnaBuf, FILE *faFh, FILE *cdsFh)
/* get RNA for a genePred table */
{
int rowOffset;
char **row;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hChromQuery(conn, table, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Load gene prediction from database. */
    struct genePred *gp = genePredLoad(row+rowOffset);
    processGenePred(gp, dnaBuf, faFh, cdsFh);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void getRnaForTables(char *table, char *chrom, struct dyString *dnaBuf, FILE *faFh, FILE *cdsFh)
/* get RNA for one for possibly splite genePred table */
{
struct slName* chroms = NULL, *chr;
if (sameString(chrom, "all"))
    chroms = hAllChromNames();
else
    chroms = slNameNew(chrom);
for (chr = chroms; chr != NULL; chr = chr->next)
    getRnaForTable(table, chr->name, dnaBuf, faFh, cdsFh);
}

void getRnaForFile(char *table, char *chrom, struct dyString *dnaBuf, FILE *faFh, FILE* cdsFh)
/* get RNA for a genePred file */
{
boolean all = sameString(chrom, "all");
struct lineFile *lf = lineFileOpen(table, TRUE);
char *row[GENEPRED_NUM_COLS];
while (lineFileNextRowTab(lf, row, GENEPRED_NUM_COLS))
    {
    struct genePred *gp = genePredLoad(row);
    if (all || sameString(gp->chrom, chrom))
        processGenePred(gp, dnaBuf, faFh, cdsFh);
    genePredFree(&gp);
    }
lineFileClose(&lf); 
}

void getRnaPred(char *database, char *table, char *chrom, char *faOut)
/* getRna - Get RNA for gene predictions and write to file. */
{
struct dyString *dnaBuf = dyStringNew(16*1024);
FILE *faFh = mustOpen(faOut, "w");
FILE *cdsFh = NULL;
if (cdsOut != NULL)
    cdsFh = mustOpen(cdsOut, "w");
hSetDb(database);

if (fileExists(table))
    getRnaForFile(table, chrom, dnaBuf, faFh, cdsFh);
else
    getRnaForTables(table, chrom, dnaBuf, faFh, cdsFh);

dyStringFree(&dnaBuf);
carefulClose(&cdsFh);
carefulClose(&faFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
weird = optionExists("weird");
cdsUpper = optionExists("cdsUpper");
cdsOut = optionVal("cdsOut", NULL); 
getRnaPred(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
