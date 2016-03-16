/* hgTpf - Make TPF table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "tilingPath.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTpf - Make TPF table\n"
  "usage:\n"
  "   hgTpf database tpfDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *create = NOSQLINJ "CREATE TABLE tilingPath (\n"
    "chrom varchar(255) not null,	# Chromosome name: chr1, chr2, etc.\n"
    "accession varchar(255) not null,	# Clone accession or ? or GAP\n"
    "clone varchar(255) not null,	# Clone name in BAC library\n"
    "contig varchar(255) not null,	# Contig (or gap size)\n"
    "chromIx int not null,	# Number of clone in tiling path starting chrom start\n"
              "#Indices\n"
    "INDEX(accession(12))\n"
")";


void loadDatabase(char *database, char *fileName)
/* Load database from tab separated file. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[1024];

sqlRemakeTable(conn, "tilingPath", create);
sqlSafef(query, sizeof query, "load data local infile '%s' into table tilingPath",
    fileName);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}

void addTpfToTabFile(char *chromName, char *tabFile, FILE *f)
/* Add one tpf FILE to tab-separated file */
{
struct lineFile *lf = lineFileOpen(tabFile, TRUE);
char *row[3];
int wordCount;
int ix = 0;
while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    if (wordCount < 3)
        {
	if (wordCount < 2 || !sameWord("GAP", row[0]))
	    lineFileExpectWords(lf, 3, wordCount);
	row[2] = "?";
	}
    fprintf(f, "%s\t", chromName);
    fprintf(f, "%s\t", row[0]);
    fprintf(f, "%s\t", row[1]);
    fprintf(f, "%s\t", row[2]);
    fprintf(f, "%d\n", ix++);
    }
lineFileClose(&lf);
}

void tpfDirToTabFile(char *tpfDir, char *fileName)
/* Read TPF directory and make tab-separated file */
{
FILE *f = mustOpen(fileName, "w");
char tpfFile[512];
char ourChrom[16];
struct fileInfo *chrom, *dir = listDirX(tpfDir, "Chr*", FALSE);

if (dir == NULL)
    errAbort("No Chr files in %s", tpfDir);
for (chrom = dir; chrom != NULL; chrom = chrom->next)
    {
    if (chrom->isDir)
	{
	sprintf(tpfFile, "%s/%s/%s", tpfDir, chrom->name, "tpf.txt");
	sprintf(ourChrom, "chr%s",  chrom->name+3);
	addTpfToTabFile(ourChrom, tpfFile, f);
	}
    }
carefulClose(&f);
}


void hgTpf(char *database, char *tpfDir)
/* hgTpf - Make TPF table. */
{
char *tempFile = "tpf.tab";
tpfDirToTabFile(tpfDir, tempFile);
loadDatabase(database, tempFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
hgTpf(argv[1], argv[2]);
return 0;
}
