/* mrnaToGene - convert PSL alignments of mRNAs to gene annotation.
 */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "genbank.h"
#include "genePred.h"
#include "psl.h"

/* command line options */
int smallInsertSize = 0;
boolean requireUtr = FALSE;
boolean keepInvalid = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mrnaToGene - convert PSL alignments of mRNAs to gene annotations\n"
  "usage:\n"
  "   mrnaToGene [options] database pslTable genePredFile\n"
  "\n"
  "Convert the PSL alignments with CDS annotation from genbank to gene\n"
  "annotation in genePred format.  Accessions without valids CDS are\n"
  "optionally dropped. A best attempt is made to convert incomplete CDS\n"
  "annotations.\n"
  "\n"
  "Options:\n"
  "  -smallInsertSize=n - Merge inserts smaller than this many bases (default 0)\n"
  "  -requireUtr - Drop sequences that don't have both 5' and 3' UTR annotated.\n"
  "  -keepInvalid - Keep sequences with invalid CDS.\n"
  "\n");
}

struct genePred* pslToGenePred(struct psl *psl, char *cdsStr)
/* convert a psl to genePred with specified CDS string; return NULL
 * if should be skipped */
{
unsigned cdsStart, cdsEnd;
boolean stat = genbankParseCds(cdsStr, &cdsStart, &cdsEnd);

if (!stat)
    {
    fprintf(stderr, "Warning: invalid CDS for %s: %s\n", psl->qName, cdsStr);
    if (!keepInvalid)
        return NULL;
    }
else
    {
    if ((cdsEnd-cdsStart) > psl->qSize)
        {
        fprintf(stderr, "Warning: cds for %s (%u..%u) longer than qSize (%u)\n",
                psl->qName, cdsStart, cdsEnd, psl->qSize);
        if (!keepInvalid)
            return NULL;
        cdsStart = -1;
        cdsEnd = -1;
        }
    if (requireUtr && (cdsStart == 0) || (cdsEnd == psl->qSize))
        return NULL;
    }
return genePredFromPsl(psl, cdsStart, cdsEnd, smallInsertSize);
}

void convertPslRow(char **row, FILE *genePredFh)
/* convert a row from the query of cds and psl */
{
char *cdsStr = row[0];
struct psl *psl = pslLoad(row+1);
struct genePred *genePred = pslToGenePred(psl, cdsStr);
if (genePred != NULL)
    {
    genePredTabOut(genePred, genePredFh);
    genePredFree(&genePred);
    }
pslFree(&psl);
}

void convertPslTable(char *database, char *pslTable, FILE *genePredFh)
/* convert mrnas in a psl table to genePred objects */
{
struct sqlConnection *conn = sqlConnect(database);
char query[512], **row;
struct sqlResult *sr;

/* generate join of cds with psls */
safef(query, sizeof(query),
      "SELECT cds.name,matches,misMatches,repMatches,nCount,qNumInsert,qBaseInsert,tNumInsert,tBaseInsert,strand,qName,qSize,qStart,qEnd,tName,tSize,tStart,tEnd,blockCount,blockSizes,qStarts,tStarts "
      "FROM cds,%s,mrna WHERE (%s.qName = mrna.acc) AND (mrna.cds !=0) AND (mrna.cds = cds.id)",
      pslTable, pslTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    convertPslRow(row, genePredFh);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

void mrnaToGene(char *database, char *pslTable, char *genePredFile)
/* convert an PSL mRNA table to a genePred file */
{
FILE* genePredFh = mustOpen(genePredFile, "w");
int i;

convertPslTable(database, pslTable, genePredFh);

if (ferror(genePredFh))
    errAbort("error writing %s", genePredFile);
carefulClose(&genePredFh);

}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean requireUtr;
char *database, *pslTable, *genePredFile;

optionHash(&argc, argv);
if (argc != 4)
    usage();
database = argv[1];
pslTable = argv[2];
genePredFile = argv[3];
requireUtr = optionExists("requireUtr");
smallInsertSize = optionInt("smallInsertSize", 0);
mrnaToGene(database, pslTable, genePredFile);
return 0;
}

