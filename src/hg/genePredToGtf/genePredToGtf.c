/* genePredToGtf - Convert genePred table or file to gtf.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredName.h"

static char const rcsid[] = "$Id: genePredToGtf.c,v 1.4 2004/03/09 01:42:41 daryl Exp $";

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
  "Note: use refFlat table to include geneName\n"
  );
}

static struct optionSpec options[] = {
   {"utr", OPTION_BOOLEAN},
   {"order", OPTION_BOOLEAN},
};

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

struct genePredName *genePredNameLoadTable(char *database, char *table)
/* Load all genePredNames from table. */
{
struct genePredName *gpnList = NULL;
struct sqlConnection *conn = sqlConnect(database);
char query[256];
char fields[]= "'' as geneName, ";
char order[]= "order by chrom, txStart";

if (!optionExists("order"))
    ZeroVar(order);
if (sameString(table,"refFlat"))
    ZeroVar(fields);
safef(query, sizeof(query), "select %s%s.* from %s %s", fields, table, table, order);
gpnList = genePredNameLoadByQuery(conn, query);
sqlDisconnect(&conn);
return gpnList;
}

static void writeGtfLine(FILE *f, char *source, char *name, char *geneName,
	char *chrom, char *strand, char *type, 
	int start, int end, int exonIx)
/* Write a single exon to GTF file */
{
fprintf(f, "%s\t", chrom);
fprintf(f, "%s\t", source);
fprintf(f, "%s\t", type);
fprintf(f, "%d\t", start+1);
fprintf(f, "%d\t", end);
fprintf(f, ".\t");	/* Phase, we should fill in when we can. */
fprintf(f, "%s\t", strand);
fprintf(f, ".\t");	/* Score. */
fprintf(f, "gene_id \"%s\"; ", name);
fprintf(f, "transcript_id \"%s\"; ", name);
fprintf(f, "exon_number \"%d\"; ", exonIx+1);
fprintf(f, "exon_id \"%s.%d\";", name, exonIx+1);
if (strlen(geneName)>0)
    fprintf(f, " gene_name \"%s\";", geneName);
fprintf(f, "\n");
}

void genePredWriteToGtf(struct genePredName *gpn, char *source, 
	struct hash *dupeHash, FILE *f)
/* Write out genePredName to GTF file. */
{
int i;
char *name = findUniqueName(dupeHash, gpn->name);
char *geneName = gpn->geneName;
char *chrom = gpn->chrom;
char *strand = gpn->strand;

for (i=0; i<gpn->exonCount; ++i)
    {
    int start = gpn->exonStarts[i];
    int end = gpn->exonEnds[i];
    int exonStart = start;
    int exonEnd = end;

    writeGtfLine(f, source, name, geneName, chrom, strand, "exon", start, end, i);
    if (start < gpn->cdsStart) start = gpn->cdsStart;
    if (end > gpn->cdsEnd) end = gpn->cdsEnd;
    if (optionExists("utr"))
	{
	if (start < end)
	    {
	    if (start > exonStart)
		writeGtfLine(f, source, name, geneName, chrom, strand, "UTR", exonStart, start, i);
	    writeGtfLine(f, source, name, geneName, chrom, strand, "CDS", start, end, i);
	    if (end < exonEnd)
		writeGtfLine(f, source, name, geneName, chrom, strand, "UTR", end, exonEnd, i);
	    }
	else 
	    writeGtfLine(f, source, name, geneName, chrom, strand, "UTR", exonStart, exonEnd, i);
	}
    else if (start < end)
	writeGtfLine(f, source, name, geneName, chrom, strand, "CDS", start, end, i);
    }
}

char *sqlUnsignedDynamicArraySpoof(int *ui, int length)
{
char *tmp=NULL;
char *ret=NULL;
int i;

for (i=0; i<length; i++)
    {
    sprintf(tmp, ",%d", ui[i]);
    strcat(ret, tmp);
    }
return ret+1;
}

struct genePredName *genePredNameSpoof(struct genePred *gp)
/* Convert a genePred struct to a genePredName struct by adding a NULL geneName *
 * Dispose of this with genePredNameFree(). */
{
struct genePredName *ret;
int sizeOne,i;
char *s, *tmp;

if (gp==NULL) 
    return NULL;

AllocVar(ret);
ret->exonCount = gp->exonCount;
ret->geneName = NULL;
ret->name = cloneString(gp->name);
ret->chrom = cloneString(gp->chrom);
strcpy(ret->strand, gp->strand);
ret->txStart = gp->txStart;
ret->txEnd = gp->txEnd;
ret->cdsStart = gp->cdsStart;
ret->cdsEnd = gp->cdsEnd;
tmp=sqlUnsignedDynamicArraySpoof(ret->exonStarts, gp->exonCount);
sqlUnsignedDynamicArray(tmp, &ret->exonStarts, &sizeOne);
assert(sizeOne == ret->exonCount);
tmp=sqlUnsignedDynamicArraySpoof(ret->exonEnds, gp->exonCount);
sqlUnsignedDynamicArray(tmp, &ret->exonEnds, &sizeOne);
assert(sizeOne == ret->exonCount);
return ret;
}

void genePredToGtf(char *database, char *table, char *gtfOut)
/* genePredToGtf - Convert genePredName table or file to gtf.. */
{
FILE *f = mustOpen(gtfOut, "w");
struct hash *dupeHash = newHash(16);
struct genePred *gpList = NULL, *gp = NULL;
struct genePredName *gpnList = NULL, *gpn;

if (sameString(database, "file"))
    {
    printf("FILE\n");
    gpList = genePredLoadAll(table);
    gpnList = genePredNameSpoof(gp);
    }
else
    gpnList = genePredNameLoadTable(database, table);
for (gpn = gpnList; gpn != NULL; gpn = gpn->next)
    genePredWriteToGtf(gpn, table, dupeHash, f);
carefulClose(&f);
return;
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
