/* genePredToGtf - Convert genePred table or file to gtf.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "genePred.h"

static char const rcsid[] = "$Id: genePredToGtf.c,v 1.2 2004/02/29 00:35:44 daryl Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToGtf - Convert genePred table or file to gtf.\n"
  "usage:\n"
  "   genePredToGtf database genePredTable output.gtf\n"
  "If database is 'file' then track is interpreted as a file\n"
  "rather than a table in database.\n"
  "options:\n"
  "   -utr   Add UTRs to the output\n"
  "   -order Order by chrom, txStart\n"
  );
}

static struct optionSpec options[] = {
   {"utr", OPTION_BOOLEAN},
   {"order", OPTION_BOOLEAN},
};

struct genePred *genePredLoadTable(char *database, char *table)
/* Load all genePreds from table. */
{
struct genePred *gpList = NULL, *gp;
struct sqlConnection *conn = sqlConnect(database);
char query[256];
struct sqlResult *sr;
char **row;

if (optionExists("order"))
    safef(query, sizeof(query), "select * from %s order by chrom, txStart", table);
else
    safef(query, sizeof(query), "select * from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    slAddHead(&gpList, gp);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&gpList);
return gpList;
}

static void writeGtfLine(FILE *f, char *source, char *geneName,
	struct genePred *gp, char *type, 
	int start, int end, int exonIx)
/* Write a single exon to GTF file */
{
fprintf(f, "%s\t", gp->chrom);
fprintf(f, "%s\t", source);
fprintf(f, "%s\t", type);
fprintf(f, "%d\t", start+1);
fprintf(f, "%d\t", end);
fprintf(f, ".\t");	/* Phase, we should fill in when we can. */
fprintf(f, "%s\t", gp->strand);
fprintf(f, ".\t");	/* Score. */
fprintf(f, "gene_id \"%s\"; ", geneName);
fprintf(f, "transcript_id \"%s\"; ", geneName);
fprintf(f, "exon_number \"%d\"; ", exonIx+1);
fprintf(f, "exon_id \"%s.%d\";\n", geneName, exonIx+1);
}

char *findUniqueName(struct hash *dupeHash, char *root)
/* If root name is already in hash, return root_1, root_2
 * or something like that. */
{
struct hashEl *hel;
if ((hel = hashLookup(dupeHash, root)) == NULL)
    {
    hashAddInt(dupeHash, root, 1);
    return root;
    }
else
    {
    static char buf[256];
    int val = ptToInt(hel->val) + 1;
    hel->val = intToPt(val);
    safef(buf, sizeof(buf), "%s_%d", root, val);
    return buf;
    }
}

void genePredWriteToGtf(struct genePred *gp, char *source, 
	struct hash *dupeHash, FILE *f)
/* Write out genePred to GTF file. */
{
int i;
char *geneName = findUniqueName(dupeHash, gp->name);
for (i=0; i<gp->exonCount; ++i)
    {
    int start = gp->exonStarts[i];
    int end = gp->exonEnds[i];
    int exonStart = start;
    int exonEnd = end;

    writeGtfLine(f, source, geneName, gp, "exon", start, end, i);
    if (start < gp->cdsStart) start = gp->cdsStart;
    if (end > gp->cdsEnd) end = gp->cdsEnd;
    if (optionExists("utr"))
	{
	if (start < end)
	    {
	    if (start > exonStart)
		writeGtfLine(f, source, geneName, gp, "UTR", exonStart, start, i);
	    writeGtfLine(f, source, geneName, gp, "CDS", start, end, i);
	    if (end < exonEnd)
		writeGtfLine(f, source, geneName, gp, "UTR", end, exonEnd, i);
	    }
	else 
	    writeGtfLine(f, source, geneName, gp, "UTR", exonStart, exonEnd, i);
	}
    else if (start < end)
	writeGtfLine(f, source, geneName, gp, "CDS", start, end, i);
    }
}

void genePredToGtf(char *database, char *table, char *gtfOut)
/* genePredToGtf - Convert genePred table or file to gtf.. */
{
FILE *f = mustOpen(gtfOut, "w");
struct hash *dupeHash = newHash(16);
struct genePred *gpList = NULL, *gp;
if (sameString(database, "file"))
    gpList = genePredLoadAll(table);
else
    gpList = genePredLoadTable(database, table);
for (gp = gpList; gp != NULL; gp = gp->next)
    genePredWriteToGtf(gp, table, dupeHash, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
genePredToGtf(argv[1], argv[2], argv[3]);
return 0;
}
