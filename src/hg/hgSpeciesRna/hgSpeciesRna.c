/* hgSpeciesRna - Create fasta file with RNA from one species. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "jksql.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSpeciesRna - Create fasta file with RNA from one species\n"
  "usage:\n"
  "   hgSpeciesRna database Genus species output.fa\n"
  "options:\n"
  "   -est         - If set will get ESTs rather than mRNAs\n"
  "   -filter=file - only read accessions listed in file\n"
  );
}

static struct optionSpec options[] = {
   {"est", OPTION_BOOLEAN},
   {"filter", OPTION_STRING},
   {NULL, 0},
};

void hgSpeciesRna(char *database, char *genus, char *species, char *outFile)
/* hgSpeciesRna - Create fasta file with RNA from one species. */
{
char query[256];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
int taxon;
char *type = "mRNA";
char *filter = NULL;
FILE *f = mustOpen(outFile, "w");
char *line;
struct lineFile *lf = NULL;
struct hash *filterHash = newHash(0);

filter = optionVal("filter", NULL);
if (filter != NULL)
    {
    lf = lineFileOpen(filter, TRUE);
    while (lineFileNext(lf, &line, NULL))
        {
        char *name = nextWord(&line);
        if (name == NULL)
           errAbort("bad line %d of %s", lf->lineIx, lf->fileName);
        hashStoreName(filterHash, name);
        }
    lineFileClose(&lf);
    }
if (optionExists("est"))
    type = "EST";
conn = hAllocConn(database);
safef(query, sizeof(query),
	"select id from organism where name = '%s %s'", genus, species);
taxon = sqlQuickNum(conn, query);
if (taxon <= 0)
    errAbort("Can't find taxon for %s %s", genus, species);
safef(query, sizeof(query), 
    "select acc from gbCdnaInfo where organism=%d and type='%s'",
    taxon, type);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct dnaSeq *seq = NULL;
    if ( filter == NULL || hashLookup(filterHash, row[0]) != NULL)
        {
        seq = hGenBankGetMrna(database, row[0], NULL);
        faWriteNext(f, seq->name, seq->dna, seq->size);
        dnaSeqFree(&seq);
        }
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hgSpeciesRna(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
