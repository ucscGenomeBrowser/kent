/* table of psl alignments, grouped by query */
#include "common.h"
#include "pslTbl.h"
#include "psl.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"

static struct pslQuery *pslQueryGet(struct pslTbl *pslTbl,
                                    char *qName)
/* get pslQuery object for qName, creating if it doesn't exist. */
{
struct hashEl *qHel = hashStore(pslTbl->queryHash, qName);
if (qHel->val == NULL)
    {
    struct pslQuery *pslQuery;
    lmAllocVar(pslTbl->queryHash->lm, pslQuery);
    pslQuery->qName = qHel->name;
    qHel->val = pslQuery;
    }
return qHel->val;
}

static void loadPsl(struct pslTbl *pslTbl, char **row)
/* load a psl record into the table */
{
struct psl *psl = pslLoadLm(row, pslTbl->queryHash->lm);
struct pslQuery *pslQuery = pslQueryGet(pslTbl, psl->qName);
slAddHead(&pslQuery->psls, psl);
}

static void loadPsls(struct pslTbl *pslTbl, char *pslFile)
/* load a psl records into the table */
{
struct lineFile *lf = lineFileOpen(pslFile, TRUE);
char *row[PSL_NUM_COLS];
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    loadPsl(pslTbl, row);
lineFileClose(&lf);
}


struct pslTbl *pslTblNew(char *pslFile)
/* construct a new object, opening the psl file */
{
struct pslTbl *pslTbl;
AllocVar(pslTbl);
pslTbl->queryHash = hashNew(22);
loadPsls(pslTbl, pslFile);
return pslTbl;
}

void pslTblFree(struct pslTbl **pslTblPtr)
/* free object */
{
struct pslTbl *pslTbl = *pslTblPtr;
if (pslTbl != NULL)
    {
    hashFree(&pslTbl->queryHash);
    freeMem(pslTbl);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

