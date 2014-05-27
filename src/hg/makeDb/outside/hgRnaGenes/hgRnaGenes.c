/* hgRnaGenes - Turn RNA genes from GFF into database format (BED variant). */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "bed.h"
#include "rnaGene.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgRnaGenes - Turn RNA genes from GFF into database format (BED variant)\n"
  "usage:\n"
  "   hgRnaGenes database file.gff\n");
}

static void readQuotedString(struct lineFile *lf, char *in, char *out, char **retNext)
/* Parse quoted string and abort on error. */
{
if (!parseQuotedString(in, out, retNext))
    errAbort("Line %d of %s\n", lf->lineIx, lf->fileName);
}

static void syntaxError(struct lineFile *lf)
/* Complain about syntax error in GFF file. */
{
errAbort("Bad line %d of %s:\n%s",
    lf->lineIx, lf->fileName, lf->buf + lf->lineStart);
}


void parseEnd(struct lineFile *lf, char *s, 
	char **retRnaName, UBYTE *retIsPsuedo)
/* Parse last field in a GFF file. */
{
char *type, *val;
bool gotGene = FALSE;

*retIsPsuedo = FALSE;
for (;;)
   {
   if ((type = nextWord(&s)) == NULL)
       break;
   s = skipLeadingSpaces(s);
   if (s == NULL || s[0] == 0)
       val = NULL;
   else if (s[0] == '"' || s[0] == '\'')
       {
       val = s;
       readQuotedString(lf, s, val, &s);
       }
   else
       val = nextWord(&s);
   s = skipLeadingSpaces(s);
   if (s != NULL && s[0] == ';' && s[0] != 0)
      ++s;
   if (sameString("gene", type))
       {
       if (val == NULL)
           errAbort("Gene with no name value line %d of %s", lf->lineIx, lf->fileName);
       *retRnaName = cloneString(val);
       gotGene = TRUE;
       }
    else if (sameString("pseudo", type))
       {
       *retIsPsuedo = TRUE;
       }
    }
if (!gotGene)
    errAbort("No gene name line %d of %s", lf->lineIx, lf->fileName);
}


struct rnaGene *rnaGffLoad(char *gffFile)
/* hgRnaGenes - Turn RNA genes from GFF into database format (BED variant). */
{
/* Open file and do basic allocations. */
struct lineFile *lf = lineFileOpen(gffFile, TRUE);
char *line;
int lineSize;
struct rnaGene *rnaList = NULL, *rna;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] != '#')
	{
	int i;
	char *words[9];
	for (i=0; i<8; ++i)
	    {
	    if ((words[i] = nextWord(&line)) == NULL)
		syntaxError(lf);
	    }
	AllocVar(rna);
	rna->chrom = cloneString(words[0]);
	rna->chromStart = atoi(words[3])-1;
	rna->chromEnd = atoi(words[4]);
	rna->fullScore = atof(words[5]);
	rna->score = round(1000.0*rna->fullScore/170);
	if (rna->score > 1000) rna->score = 1000;
	rna->strand[0] = words[6][0];
	rna->type = cloneString(words[2]);
	rna->source = cloneString(words[1]);
	parseEnd(lf, line, &rna->name, &rna->isPsuedo);
	slAddHead(&rnaList, rna);
	}
    }
lineFileClose(&lf);
slSort(&rnaList, bedCmp);
return rnaList;
}

void hgRnaGenes(char *database, char *gffFile)
/* hgRnaGenes - Turn RNA genes from GFF into database format (BED variant). */
{
struct rnaGene *rnaList = NULL, *rna;
struct sqlConnection *conn = NULL;
char *tempName = "rnaGenes.tab";
FILE *f = NULL;
char query[256];
char *table = "rnaGene";

/* Load GFF into memory sorted by chromosome & start pos. */
printf("Loading %s\n", gffFile);
rnaList = rnaGffLoad(gffFile);
printf("Got %d RNA genes\n", slCount(rnaList));

/* Write it as tab-delimited file. */
printf("Writing tab-delimited file %s\n", tempName);
f = mustOpen(tempName, "w");
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    rnaGeneTabOut(rna, f);
    }
carefulClose(&f);

/* Import into database. */
printf("Importing into database %s\n", database);
conn = sqlConnect(database);
if (!sqlTableExists(conn, table))
    errAbort("You need to create the table first (with %s.sql)", table);
sqlSafef(query, sizeof query, "delete from %s", table);
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "load data local infile '%s' into table %s", tempName, table);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgRnaGenes(argv[1], argv[2]);
return 0;
}
