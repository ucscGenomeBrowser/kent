/* getRnaPred - Get RNA for gene predictions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "genePred.h"
#include "fa.h"
#include "jksql.h"
#include "hdb.h"
#include "options.h"

static char const rcsid[] = "$Id: getRnaPred.c,v 1.7 2004/01/14 02:55:41 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRnaPred - Get RNA for gene predictions\n"
  "usage:\n"
  "   getRnaPred database table chromosome output.fa\n"
  "table can be a table or a file.  Specify chromosome of 'all' to\n"
  "to process all chromosome\n"
  "\n"
  "options:\n"
  "   -weird Only get ones with weird splice sites\n"
  );
}

static struct optionSpec options[] = {
   {"weird", OPTION_BOOLEAN},
   {NULL, 0},
};


/* parsed from command line */
boolean weird;

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

void processGenePred(struct genePred *gp, struct dyString *dnaBuf, FILE* faFh)
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

    /* Reverse complement if necessary and output. */
    if (gp->strand[0] == '-')
        reverseComplement(dnaBuf->string, dnaBuf->stringSize);
    faWriteNext(faFh, gp->name, dnaBuf->string, dnaBuf->stringSize);
}

void getRnaForTable(char *table, char *chrom, struct dyString *dnaBuf, FILE *faFh)
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
    processGenePred(gp, dnaBuf, faFh);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void getRnaForTables(char *table, char *chrom, struct dyString *dnaBuf, FILE *faFh)
/* get RNA for one for possibly splite genePred table */
{
struct slName* chroms = NULL, *chr;
if (sameString(chrom, "all"))
    chroms = hAllChromNames();
else
    chroms = slNameNew(chrom);
for (chr = chroms; chr != NULL; chr = chr->next)
    getRnaForTable(table, chr->name, dnaBuf, faFh);
}

void getRnaForFile(char *table, char *chrom, struct dyString *dnaBuf, FILE *faFh)
/* get RNA for a genePred file */
{
boolean all = sameString(chrom, "all");
struct lineFile *lf = lineFileOpen(table, TRUE);
char *row[GENEPRED_NUM_COLS];
while (lineFileNextRowTab(lf, row, GENEPRED_NUM_COLS))
    {
    struct genePred *gp = genePredLoad(row);
    if (all || sameString(gp->chrom, chrom))
        processGenePred(gp, dnaBuf, faFh);
    genePredFree(&gp);
    }
lineFileClose(&lf); 
}

void getRnaPred(char *database, char *table, char *chrom, char *faOut)
/* getRna - Get RNA for gene predictions and write to file. */
{
struct dyString *dnaBuf = dyStringNew(16*1024);
FILE *faFh = mustOpen(faOut, "w");
hSetDb(database);

if (fileExists(table))
    getRnaForFile(table, chrom, dnaBuf, faFh);
else
    getRnaForTables(table, chrom, dnaBuf, faFh);

dyStringFree(&dnaBuf);
carefulClose(&faFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
weird = optionExists("weird");
getRnaPred(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
