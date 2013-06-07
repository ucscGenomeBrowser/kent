/* musAliAt - Produce .fa files where mouse alignments hit on chr22. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "musAliAt - Produce .fa files where mouse alignments hit on chr22\n"
  "usage:\n"
  "   musAliAt database chromosome human.fa mouse.fa\n"
  );
}

void musAliAt(char *database, char *chrom, char *humanFa, char *mouseFa)
/* musAliAt - Produce .fa files where mouse alignments hit on chr22. */
{
char query[256], **row;
struct sqlResult *sr;
struct sqlConnection *conn;
struct dnaSeq *musSeq, *homoSeq;
struct psl *psl;
struct hash *musHash = newHash(10);
FILE *musOut = mustOpen(mouseFa, "w");

hSetDb(database);
conn = hAllocConn();
sqlSafef(query, sizeof query, "select * from blatMouse where tName = '%s'", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row);
    if ((musSeq = hashFindVal(musHash, psl->qName)) == NULL)
        {
	musSeq = hExtSeq(psl->qName);
	hashAdd(musHash, psl->qName, NULL);
	faWriteNext(musOut, musSeq->name, musSeq->dna, musSeq->size);
	freeDnaSeq(&musSeq);
	}
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
musAliAt(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
