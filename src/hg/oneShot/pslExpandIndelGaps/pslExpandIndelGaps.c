/* pslExpandIndelGaps - When small gaps in repetitive regions have ambiguous
 * left/rightmost placement, expand them to double-sided gaps over the whole repetitive region. */
#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "variantProjector.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslExpandIndelGaps - When small gaps in repetitive regions have ambiguous\n"
  "left/rightmost placement, expand them to double-sided gaps over the whole\n"
  "repetitive region.\n"
  "usage:\n"
  "   pslExpandIndelGaps db input.psl output.psl\n"
  "Only RefSeq transcripts are supported for qName in input.psl. Optionally,\n"
  "qName may have a colon and extra name after the NM_* identifier, to\n"
  "distinguish between different alignments of the same sequence.\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

//#*** BEGIN copied from vcfToHgvs.c -- if this becomes a real util, then libify this part.
struct dnaSeq *hGetNmAccAndSeq(char *db, char *nmAccIn)
/* Return a cached sequence with ->name set to the versioned NM accession,
 * given a possibly unversioned NM accession, or NULL if not found.
 * db must never change.  Do not modify or free the returned sequence. */
{
static struct hash *hash = NULL;
static char *firstDb = NULL;
static boolean useNcbi = FALSE;
if (hash == NULL)
    {
    hash = hashNew(0);
    firstDb = cloneString(db);
    useNcbi = hDbHasNcbiRefSeq(db);
    }
if (!sameString(db, firstDb))
    errAbort("hGetNmAccAndSeq: only works for one db.  %s was passed in earlier, now %s.",
             firstDb, db);
struct dnaSeq *accSeq = hashFindVal(hash, nmAccIn);
if (accSeq == NULL)
    {
    if (useNcbi)
        {
        // nmAccIn is already versioned.
        accSeq = hDnaSeqGet(db, nmAccIn, "seqNcbiRefSeq", "extNcbiRefSeq");
        }
    else
        {
        accSeq = hGenBankGetMrna(db, nmAccIn, NULL);
        if (accSeq)
            {
            // Query to get versioned nmAcc and replace accSeq->name with it.
            char query[1024];
            struct sqlConnection *conn = hAllocConn(db);
            sqlSafef(query, sizeof(query),
                     "select concat(s.acc, concat('.', s.version)) "
                     "from %s s where s.acc = '%s';", gbSeqTable, nmAccIn);
            char nmAcc[1024];
            if (sqlQuickQuery(conn, query, nmAcc, sizeof(nmAcc)))
                {
                freeMem(accSeq->name);
                accSeq->name = cloneString(nmAcc);
                }
            hFreeConn(&conn);
            }
        }
    if (accSeq == NULL)
        // Store a dnaSeq with NULL name and seq so we don't waste time sql querying this again.
        accSeq = newDnaSeq(NULL, 0, NULL);
    hashAdd(hash, nmAccIn, accSeq);
    }
return (accSeq->name == NULL) ? NULL : accSeq;
}
//#*** END copied from vcfToHgvs.c

void pslExpandIndelGaps(char *db, char *inFile, char *outFile)
/* pslExpandIndelGaps - When small gaps in repetitive regions have ambiguous
 * left/rightmost placement, expand them to double-sided gaps over the whole repetitive region. */
{
struct lineFile *lf = pslFileOpen(inFile);
FILE *outF = mustOpen(outFile, "w");
struct seqWindow *gSeqWin = chromSeqWindowNew(db, NULL, 0, 0);
struct psl *psl;
while ((psl = pslNext(lf)) != NULL)
    {
    // Chop colon-separated name after accession, if present
    char acc[strlen(psl->qName)+1];
    safecpy(acc, sizeof acc, psl->qName);
    char *colon = strchr(acc, ':');
    if (colon)
        *colon = '\0';
    struct dnaSeq *txSeq = hGetNmAccAndSeq(db, acc);
    //#*** Why doesn't hGetNmAccAndSeq do touppers?
    //#*** Maybe it didn't have to for human but does for chicken??
    touppers(txSeq->dna);
    vpExpandIndelGaps(psl, gSeqWin, txSeq);
    pslTabOut(psl, outF);
    }
lineFileClose(&lf);
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
pslExpandIndelGaps(argv[1], argv[2], argv[3]);
return 0;
}
