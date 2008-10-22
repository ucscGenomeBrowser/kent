/* genbank.c - Various functions for dealing with genbank data */
#include "common.h"
#include "hash.h"
#include "psl.h"
#include "linefile.h"
#include "genbank.h"
#include "dystring.h"

static char const rcsid[] = "$Id: genbank.c,v 1.12 2008/10/22 19:23:54 markd Exp $";

static char *JOIN_PREFIX = "join(";
static char *COMPLEMENT_PREFIX = "complement(";

static boolean convertCoord(char *numStr, int *coord)
/* convert an CDS cooordinate, return false if invalid */
{
char *endPtr;
*coord = strtoul(numStr, &endPtr, 10);
return ((*numStr != '\0') && (*endPtr == '\0'));
}

static boolean parseStartCds(char *startBuf, struct genbankCds* cds)
/* parse a starting CDS coordinate */
{
cds->startComplete = (startBuf[0] != '<');
if (!cds->startComplete)
    startBuf++;
if (!convertCoord(startBuf, &cds->start))
    return FALSE;
cds->start--;  /* convert to zero-based */
return TRUE;
}

static boolean parseEndCds(char *endBuf, struct genbankCds* cds)
/* parse an ending CDS coordinate */
{
cds->endComplete = (endBuf[0] != '>');
if (!cds->endComplete)
    endBuf++;
return convertCoord(endBuf, &cds->end);
}

static boolean parseRange(char *cdsBuf, struct genbankCds* cds)
/* parse a cds range in the for 221..617 */
{
char *p1;

/* find .. */
p1 = strchr(cdsBuf, '.'); 
if ((p1 == NULL) || (p1[1] != '.'))
    return FALSE; /* no .. */

*p1 = '\0';
p1 += 2;
if (!parseStartCds(cdsBuf, cds))
    return FALSE;
return parseEndCds(p1, cds);
}

static boolean parseJoin(char *cdsBuf, struct genbankCds* cds)
/* parse a join cds, taking the first and last coordinates in range */
{
int len = strlen(cdsBuf);
char *startPtr, *endPtr, *p;

/* Pull out last number in join */
if (cdsBuf[len-1] != ')')
    return FALSE;  /* no close paren */
cdsBuf[len-1] = '\0';
endPtr = strrchr(cdsBuf, '.');
if (endPtr == NULL)
    return FALSE;  /* no preceeding dot */
endPtr++;

/* Pull out first number */
startPtr = cdsBuf+strlen(JOIN_PREFIX);
p = strchr(startPtr, '.');
if (p == NULL)
    return FALSE; /* no dot after number */
*p = '\0';

if (!parseStartCds(startPtr, cds))
    return FALSE;  /* invalid start */
return parseEndCds(endPtr, cds);
}

static boolean parseComplement(char *cdsBuf, struct genbankCds* cds)
/* parse a complement cds, perhaps recursively parsing a join */
{
int len = strlen(cdsBuf);
char *p1 = cdsBuf+strlen(COMPLEMENT_PREFIX);

if (cdsBuf[len-1] != ')')
    return FALSE;  /* no closing paren */
cdsBuf[len-1] = '\0';

cds->complement = TRUE;
if (startsWith(JOIN_PREFIX, p1))
    return parseJoin(p1, cds);
else
    return parseRange(p1, cds);
}

boolean genbankCdsParse(char *cdsStr, struct genbankCds* cds)
/* Parse a genbank CDS, returning TRUE if it can be successfuly parsed, FALSE
 * if there are problems.  If a join() is specified, the first and last
 * coordinates are used for the CDS.  Incomplete CDS specifications will still
 * return the start or end.  start/end are set to 0 on error. */
{
static struct dyString* cdsBuf = NULL;   /* buffer for CDS strings */
boolean isOk;
if (cdsBuf == NULL)
    cdsBuf = dyStringNew(512);
/* copy so that string can be modified without changing input */
dyStringClear(cdsBuf);
dyStringAppend(cdsBuf, cdsStr);
ZeroVar(cds);

/*  FIXME: complement handling is wrong here, but it should only occur in DNA*/

if (startsWith(JOIN_PREFIX, cdsBuf->string))
    isOk = parseJoin(cdsBuf->string, cds);
else if (startsWith(COMPLEMENT_PREFIX, cdsBuf->string))
    isOk = parseComplement(cdsBuf->string, cds);
else
    isOk = parseRange(cdsBuf->string, cds);

if (!isOk)
    cds->start = cds->end = 0;
return isOk;
}

boolean genbankParseCds(char *cdsStr, unsigned *cdsStart, unsigned *cdsEnd)
/* Compatiblity function, genbankCdsParse is prefered. Parse a genbank CDS,
 * returning TRUE if it can be successfuly parsed, FALSE if there are
 * problems.  If a join() is specified, the first and last coordinates are
 * used for the CDS.  Incomplete CDS specifications will still return the
 * start or end.  cdsStart and cdsEnd are set to -1 on error.
 */
{
struct genbankCds cds;
boolean isOk = genbankCdsParse(cdsStr, &cds);
if (!isOk)
    cds.start = cds.end = -1;
*cdsStart = cds.start;
*cdsEnd = cds.end;
return isOk;
}

static void checkAccLen(char *acc)
/* check the length of an accession string */
{
if (strlen(acc) > GENBANK_ACC_BUFSZ-1)
    errAbort("invalid Genbank/RefSeq accession (too long): \"%s\"", acc);
}

boolean genbankIsRefSeqAcc(char *acc)
/* determine if a accession appears to be from RefSeq */
{
/* NM_012345, NP_012345, NR_012345, etc */
return (strlen(acc) > 4) && (acc[0] == 'N') && (acc[2] == '_');
}

boolean genbankIsRefSeqCodingMRnaAcc(char *acc)
/* determine if a accession appears to be a protein-coding RefSeq
 * accession. */
{
/* NM_012345 */
return (strlen(acc) > 4) && (acc[0] == 'N') && (acc[0] == 'M') && (acc[2] == '_');
}

boolean genbankIsRefSeqNonCodingMRnaAcc(char *acc)
/* determine if a accession appears to be a non-protein-coding RefSeq
 * accession. */
{
return genbankIsRefSeqAcc(acc) && ! genbankIsRefSeqCodingMRnaAcc(acc);
}

char* genbankDropVer(char *outAcc, char *inAcc)
/* strip the version from a genbank id.  Input and output
 * strings maybe the same. Length is checked against
 * GENBANK_ACC_BUFSZ. */
{
checkAccLen(inAcc);
if (outAcc != inAcc)
    strcpy(outAcc, inAcc);
chopPrefix(outAcc);
return outAcc;
}

void genbankExceptionsHash(char *fileName, 
	struct hash **retSelenocysteineHash, struct hash **retAltStartHash)
/* Will read a genbank exceptions file, and return two hashes parsed out of
 * it filled with the accessions having the two exceptions we can handle, 
 * selenocysteines, and alternative start codons. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *scHash = *retSelenocysteineHash  = hashNew(0);
struct hash *altStartHash = *retAltStartHash = hashNew(0);
char *row[3];
while (lineFileRowTab(lf, row))
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *row[3];
    while (lineFileRow(lf, row))
        {
	if (sameString(row[1], "selenocysteine") && sameString(row[2], "yes"))
	    hashAdd(scHash, row[0], NULL);
	if (sameString(row[1], "exception") 
		&& sameString(row[2], "alternative_start_codon"))
	    hashAdd(altStartHash, row[0], NULL);
	}
    }
lineFileClose(&lf);
}

struct genbankCds genbankCdsToGenome(struct genbankCds* cds, struct psl *psl)
/* Convert set cdsStart/end from mrna to genomic coordinates using an
 * alignment.  Returns a genbankCds object with genomic (positive strand)
 * coordinates */
{
// FIXME: this is used only by genePred code, but since  frame was added,
// genome mapping  of CDS is computed both here and genePred.getFrame().
// this should really be unified.
int nblks = psl->blockCount;
int rnaCdsStart = cds->start,  rnaCdsEnd = cds->end;
int iBlk;
struct genbankCds genomeCds;
ZeroVar(&genomeCds);
genomeCds.start = genomeCds.end = -1;
char geneStrand = (psl->strand[1] == '\0') ? psl->strand[0]
    : ((psl->strand[0] != psl->strand[1]) ? '-' : '+');

/* use start/end completeness from CDS specification, and adjust if unaligned */
boolean startComplete = cds->startComplete, endComplete = cds->endComplete;

if (psl->strand[0] == '-')
    {
    // swap CDS
    reverseIntRange(&rnaCdsStart, &rnaCdsEnd, psl->qSize);
    startComplete = cds->endComplete;
    endComplete = cds->startComplete;
    }

/* find query block or gap containing start and map to target */
for (iBlk = 0; (iBlk < nblks) && (genomeCds.start < 0); iBlk++)
    {
    if (rnaCdsStart < psl->qStarts[iBlk])
        {
        /* in gap before block, set to start of block */
        genomeCds.start = psl->tStarts[iBlk];
        startComplete = FALSE;
        }
    else if (rnaCdsStart < (psl->qStarts[iBlk] + psl->blockSizes[iBlk]))
        {
        /* in this block, map to target */
        genomeCds.start = psl->tStarts[iBlk] + (rnaCdsStart - psl->qStarts[iBlk]);
        }
    }
if (genomeCds.start < 0)
    {
    /* after last block, set after end of that block */
    genomeCds.start = psl->tStarts[iBlk-1] + psl->blockSizes[iBlk-1];
    startComplete = FALSE;
    }

/* find query block or gap containing end and map to target */
for (iBlk = 0; (iBlk < nblks) && (genomeCds.end < 0); iBlk++)
    {
    if (rnaCdsEnd <= psl->qStarts[iBlk])
        {
        /* in gap before block, set to start of gap */
        if (iBlk == 0)
            genomeCds.end = psl->tStarts[0]; /* before first block */
        else
            genomeCds.end = psl->tStarts[iBlk-1] + psl->blockSizes[iBlk-1];
        endComplete = FALSE;
        }
    else if (rnaCdsEnd <= (psl->qStarts[iBlk] + psl->blockSizes[iBlk]))
        {
        /* in this block, map to target */
        genomeCds.end = psl->tStarts[iBlk] + (rnaCdsEnd - psl->qStarts[iBlk]);
        }
    }
if (genomeCds.end < 0)
    {
    /* after last block, set to end of that block */
    genomeCds.end = psl->tStarts[nblks-1] + psl->blockSizes[nblks-1];
    endComplete = FALSE;
    }
if (genomeCds.start >= genomeCds.end)
    {
    // CDS not aligned
    genomeCds.start = genomeCds.end = psl->tStarts[nblks-1] + psl->blockSizes[nblks-1];
    startComplete = endComplete = FALSE;
    }

if (geneStrand == '+')
    {
    genomeCds.startComplete = startComplete;
    genomeCds.endComplete = endComplete;
    }
else
    {
    genomeCds.startComplete = endComplete;
    genomeCds.endComplete = startComplete;
    }
if (psl->strand[1] == '-')
    reverseIntRange(&genomeCds.start, &genomeCds.end, psl->tSize);
return genomeCds;
}

