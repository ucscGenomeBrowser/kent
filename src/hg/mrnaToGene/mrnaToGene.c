/* mrnaToGene - convert PSL alignments of mRNAs to gene annotation.
 */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mrnaToGene - convert PSL alignments of mRNAs to gene annotations\n"
  "usage:\n"
  "   mrnaToGene [options] database pslTable genePredFile\n"
  "\n"
  "Convert the PSL alignments with CDS annotation from genbank to gene\n"
  "annotation in genePred format.  Accessions without CDS or with incomplete\n"
  "CDS annotation (e.g. <1..206, or 4821..5028> forms) are dropped.  The\n"
  "CDS annotation is obtained from the mrna and cds tables in the database.\n"
  "\n"
  "Options:\n"
  "  -requireUtr - Drop sequences that don't have both 5' and 3' UTR annotated.\n"
  "\n");
}

/* status using in parsing CDS */
#define CDS_OK             0
#define CDS_INCOMPLETE     1
#define CDS_BAD            2
#define CDS_TRUNCATED_JOIN 3
#define CDS_COMPLEMENT     4  /* not handled */
#define CDS_NO_UTR         5
#define CDS_NUM_STATS      6

/* stats */
static unsigned gPslTotal = 0;  /* total count it table */
static unsigned gPslWithCdsCnt = 0;  /* num returned by select */
static unsigned gCdsStatCnt[CDS_NUM_STATS] = {
    0, 0, 0, 0, 0
};
static char *gCdsStatDesc[CDS_NUM_STATS] = {
    "ok",
    "incomplete",
    "bad",
    "truncated join",
    "complement",
    "no UTR"
};

boolean convertCoord(char *numStr, unsigned* coord)
/* convert an CDS cooordinate, return false if invalid */
{
char *endPtr;
*coord = strtoul(numStr, &endPtr, 10);
return ((*numStr != '\0') && (*endPtr == '\0'));
}

int parseCdsRange(char *cdsStr, unsigned *cdsStart, unsigned* cdsEnd)
/* parse a cds range in the for 221..617 */
{
char *dotPtr;

/* check for incomplete */
if ((strchr(cdsStr, '<') != NULL) || (strchr(cdsStr, '>') != NULL))
    return CDS_INCOMPLETE;
/* find .. */
dotPtr = strchr(cdsStr, '.'); 
if ((dotPtr != NULL) && (*(dotPtr+1) == '.'))
    {
    *dotPtr = '\0';
    if (convertCoord(cdsStr, cdsStart)
        && convertCoord(dotPtr+2, cdsEnd))
        {
        *dotPtr = '.';
        (*cdsStart)--;  /* to zero base */
        if (*cdsStart < *cdsEnd)
            return CDS_OK;  /* looks ok */
        }
    *dotPtr = '.';
    }
return CDS_BAD;
}

int parseJoinCds(char *cdsStr, unsigned *cdsStart, unsigned* cdsEnd)
/* parse a join cds and check if they are contiguous. */
{
int cnt = 0;
int stat = CDS_OK;
unsigned subStart, subEnd;
char *p = cdsStr+5;  /* +"join(" */

if (strchr(cdsStr, ')') == NULL)
    return CDS_TRUNCATED_JOIN;

while ((*p != '\0') && (*p != ')') && (stat == CDS_OK))
    {
    /* extract next subrange and parse */
    int len = strcspn(p, ",)");
    char *next = p+len;
    char hold = *next;
    *next = '\0';
    stat = parseCdsRange(p, &subStart, &subEnd);
    *next = hold;

    /* advance to next, if any */
    p = next;
    if (*p != '\0')
        p++;
    /* start new range or append if contiguous  */
    if (stat == CDS_OK)
        {
        if (cnt == 0)
            *cdsStart = subStart;
        else if (subStart != *cdsEnd)
            stat = CDS_INCOMPLETE;  /* not contiguous */
        *cdsEnd = subEnd;
        }
    cnt++;
    }
if (stat == CDS_OK)
    {
    /* check that we found something or made the end */
    if (cnt == 0)
        stat = CDS_BAD;
    else if (*(p-1) != ')')
        stat = CDS_BAD;
    }
return stat;
}

int parseCds(char *acc, char *cdsStr, unsigned *cdsStart, unsigned* cdsEnd)
/* parse a cds, returning one of the status.  Handles joins if ranges are
 * contiuous: join(221..257,258..320,321..431) */
{
int stat;
if (startsWith("join(", cdsStr))
    stat = parseJoinCds(cdsStr, cdsStart, cdsEnd);
else if (startsWith("complement(", cdsStr))
    {
    fprintf(stderr, "Warning: %s: complement not current handled: \"%s\"\n",
            acc, cdsStr);
    return CDS_COMPLEMENT;
    }
else
    stat = parseCdsRange(cdsStr, cdsStart, cdsEnd);
if (stat == CDS_BAD)
    fprintf(stderr, "Warning: %s: invalid CDS annotation: \"%s\"\n",
            acc, cdsStr);
else if (strchr(cdsStr, '(') != NULL)
    {
    fprintf(stderr, "%s: %s => ", acc, cdsStr);
    if (stat == CDS_OK)
        fprintf(stderr, "%u-%u\n", *cdsStart, *cdsEnd);
    else
        fprintf(stderr, "%s\n", gCdsStatDesc[stat]);
    }
return stat;
}

struct genePred* pslToGenePred(struct psl *psl, char *cdsStr,
                               boolean requireUtr)
/* convert a psl to genePred with specified CDS string; return NULL
 * if should be skipped */
{
unsigned cdsStart, cdsEnd;
int stat = parseCds(psl->qName, cdsStr, &cdsStart, &cdsEnd);
if (stat == CDS_OK)
    {
    if ((cdsEnd-cdsStart) > psl->qSize)
        {
        fprintf(stderr, "Warning: cds for %s (%u..%u) longer than qSize (%u)\n",
                psl->qName, cdsStart, cdsEnd, psl->qSize);
        stat = CDS_BAD;
        }
    if (requireUtr && (cdsStart == 0) || (cdsEnd == psl->qSize))
        stat = CDS_NO_UTR;
    }
gPslWithCdsCnt++;
gCdsStatCnt[stat]++;
if (stat != CDS_OK)
    return NULL;
return genePredFromPsl(psl, cdsStart, cdsEnd, 5);
}

void convertPslRow(char **row, boolean requireUtr, FILE *genePredFh)
/* convert a row from the query of cds and psl */
{
char *cdsStr = row[0];
struct psl *psl = pslLoad(row+1);
struct genePred *genePred = pslToGenePred(psl, cdsStr, requireUtr);
if (genePred != NULL)
    {
    genePredTabOut(genePred, genePredFh);
    genePredFree(&genePred);
    }
pslFree(&psl);
}

void convertPslTable(char *database, char *pslTable, boolean requireUtr,
                     FILE *genePredFh)
/* convert mrnas in a psl table to genePred objects */
{
struct sqlConnection *conn = sqlConnect(database);
char query[512], **row;
struct sqlResult *sr;

safef(query, sizeof(query), "SELECT count(*) FROM %s", pslTable);
gPslTotal = sqlQuickNum(conn, query);

/* generate join of cds with psls */
safef(query, sizeof(query),
      "SELECT cds.name,matches,misMatches,repMatches,nCount,qNumInsert,qBaseInsert,tNumInsert,tBaseInsert,strand,qName,qSize,qStart,qEnd,tName,tSize,tStart,tEnd,blockCount,blockSizes,qStarts,tStarts "
      "FROM cds,%s,mrna WHERE (%s.qName = mrna.acc) AND (mrna.cds !=0) AND (mrna.cds = cds.id)",
      pslTable, pslTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    convertPslRow(row, requireUtr, genePredFh);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

void mrnaToGene(char *database, char *pslTable, boolean requireUtr,
                char *genePredFile)
/* convert an PSL mRNA table to a genePred file */
{
FILE* genePredFh = mustOpen(genePredFile, "w");
int i;

convertPslTable(database, pslTable, requireUtr, genePredFh);

if (ferror(genePredFh))
    errAbort("error writing %s", genePredFile);
carefulClose(&genePredFh);

fprintf(stderr, "PSL records: %u\n", gPslTotal);
fprintf(stderr, "CDS annotated: %u\n", gPslWithCdsCnt);
for (i = 0; i < CDS_NUM_STATS; i++)
    if ((i == CDS_OK) || (gCdsStatCnt[i] > 0))
        fprintf(stderr, "     %s: %u\n", gCdsStatDesc[i], gCdsStatCnt[i]);
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
mrnaToGene(database, pslTable, requireUtr, genePredFile);
return 0;
}

