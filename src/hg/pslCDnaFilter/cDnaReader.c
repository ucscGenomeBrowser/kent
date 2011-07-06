/* cDnaReader - read PSLs into cDnaQuery objects */
#include "common.h"
#include "cDnaReader.h"
#include "cDnaAligns.h"
#include "cDnaStats.h"
#include "hapRegions.h"
#include "polyASize.h"
#include "psl.h"
#include "hash.h"
#include "linefile.h"

static struct psl* readNextPsl(struct cDnaReader *reader)
/* Read the next psl. If one is save in alns, return it, otherwise
 * read the next one.  Discard invalid psls and try again */
{
struct psl* psl = NULL;
if (reader->nextCDnaPsl != NULL)
    {
    psl = reader->nextCDnaPsl;
    reader->nextCDnaPsl = NULL;
    }
else
    {
    char *row[PSLX_NUM_COLS];
    int nCols;
    if ((nCols = lineFileChopNextTab(reader->pslLf, row, ArraySize(row))) > 0)
        {
        if (nCols == PSL_NUM_COLS)
            psl = pslLoad(row);
        else if (nCols == PSLX_NUM_COLS)
            psl = pslxLoad(row);
        else
            errAbort("%s:%d: expected %d or %d columns, got %d",
                     reader->pslLf->fileName, reader->pslLf->lineIx,
                     PSL_NUM_COLS, PSLX_NUM_COLS, nCols);
        }
    }
return psl;
}

boolean cDnaReaderNext(struct cDnaReader *reader)
/* load the next set of cDNA alignments, return FALSE if no more */
{
struct psl *psl;
struct cDnaQuery *cdna;
struct polyASize *polyASize = NULL;

if (reader->cdna != NULL)
    {
    cDnaStatsUpdate(&reader->stats);
    cDnaQueryFree(&reader->cdna);
    }

/* first alignment for query */
psl = readNextPsl(reader);
if (psl == NULL)
    return FALSE;
if (reader->polyASizes != NULL)
    polyASize = hashFindVal(reader->polyASizes, psl->qName);
cdna = reader->cdna = cDnaQueryNew(reader->opts, &reader->stats, psl, polyASize);
cDnaAlignNew(cdna, psl);

/* remaining alignments for same sequence */
while (((psl = readNextPsl(reader)) != NULL) && sameString(psl->qName, cdna->id))
    cDnaAlignNew(cdna, psl);

reader->nextCDnaPsl = psl;  /* save for next time (or NULL) */
return TRUE;
}

struct cDnaReader *cDnaReaderNew(char *pslFile, unsigned opts, char *polyASizeFile,
                                 struct hapRegions *hapRegions)
/* construct a new object, opening the psl file */
{
struct cDnaReader *reader;
AllocVar(reader);
reader->opts = opts;
reader->pslLf = pslFileOpen(pslFile);
if (polyASizeFile != NULL)
    reader->polyASizes = polyASizeLoadHash(polyASizeFile);
reader->hapRegions = hapRegions;
return reader;
}

void cDnaReaderFree(struct cDnaReader **readerPtr)
/* free object */
{
struct cDnaReader *reader = *readerPtr;
if (reader != NULL)
    {
    cDnaQueryFree(&reader->cdna);
    lineFileClose(&reader->pslLf);
    hashFree(&reader->polyASizes);
    freeMem(reader);
    *readerPtr = NULL;
    }
}
