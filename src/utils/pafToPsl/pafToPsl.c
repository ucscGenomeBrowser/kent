/* pafToPsl - converting paf output from minimap2 into UCSC psl format.
 * This is a conversion on permission from author: Chenxi Zhou from code
 * in the Gene Myers project: fastGA/FASTGA utility PAFtoPSL
 *   https://github.com/thegenemyers/FASTGA
 * to function in the kent C code environment.
 */

#include "common.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "sqlNum.h"
#include "hash.h"
#include "obscure.h"
#include <ctype.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pafToPsl - converting paf output from minimap2 into UCSC psl format\n"
  "usage:\n"
  "   pafToPsl in.paf out.psl\n"
  "options:\n"
  "   -cigarTag=tag  SAM-style tag that carries the alignment CIGAR string\n"
  "                  (default cg:Z:).  The CIGAR is made of M/=/X, I and D\n"
  "                  operators, as minimap2 writes it.\n"
  "   -tSizes=file.chrom.sizes  Tab-separated <name><size> file.  When given,\n"
  "                  each record's target size is taken from here instead of\n"
  "                  the PAF's own tSize column; a target name missing from\n"
  "                  this file causes that record to be skipped with a\n"
  "                  warning, and a size that disagrees with the PAF is\n"
  "                  also warned about (this file's value wins).\n"
  "   -qSizes=file.chrom.sizes  Same as -tSizes, for the query side.\n"
  "out.psl may be \"stdout\" to write to standard output.\n"
  "### Thank you to Chenxi Zhou for permission to translate the original  ###\n"
  "### PAFtoPSL code from the Gene Myers project: FASTGA                  ###\n"
  "###     https://github.com/thegenemyers/FASTGA                         ###\n"
  "### into this kent C code implementation.                              ###"
  );
}

static char *cigarTag = "cg:Z:";
static char *tSizesFile = NULL;
static char *qSizesFile = NULL;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"cigarTag", OPTION_STRING},
   {"tSizes", OPTION_STRING},
   {"qSizes", OPTION_STRING},
   {NULL, 0},
};

static void pushBlock(struct psl *psl, int *blockSpace, int qPos, int tPos, int size)
/* Append one ungapped alignment block to psl, growing the block arrays if
 * needed. */
{
if (psl->blockCount >= *blockSpace)
    pslGrow(psl, blockSpace);
psl->qStarts[psl->blockCount] = qPos;
psl->tStarts[psl->blockCount] = tPos;
psl->blockSizes[psl->blockCount] = size;
psl->blockCount++;
}

static boolean cigarToPsl(struct psl *psl, int *blockSpace, char *cigar,
	char *fileName, int lineIx)
/* Parse a CIGAR string (as found in a PAF cg:Z: tag) into psl's alignment
 * blocks, and fill in the fields that are derived from it: the trimmed
 * qStart/qEnd/tStart/tEnd, the insert counts, misMatch and nCount.
 * psl->match must already be set from the PAF record's own match count.
 * Returns FALSE, after printing a warning, if the CIGAR is inconsistent
 * with psl's alignment range. */
{
int qNumInsert = 0, qBaseInsert = 0, tNumInsert = 0, tBaseInsert = 0;
int qPos = 0, tPos = 0, lens = 0, insl = 0, insr = 0, opLen = 0;
char *c = cigar;
char op, prevOp = '\0';
int i;

psl->blockCount = 0;
while (*c != '\0')
    {
    opLen = 0;
    while (isdigit((unsigned char)*c))
        opLen = 10*opLen + (*c++ - '0');
    if (opLen == 0)
	{
	warn("%s:%d: CIGAR operator length is zero", fileName, lineIx);
	return FALSE;
	}
    op = *c++;
    switch (op)
        {
	case 'M':
	case 'X':
	case '=':
	    qPos += opLen;
	    tPos += opLen;
	    lens += opLen;
	    break;
	case 'I':
	    if (prevOp == '\0')	// leading insertion
		insl = opLen;
	    else
		{
		pushBlock(psl, blockSpace, qPos-lens, tPos-lens, lens);
		lens = 0;
		}
	    qNumInsert += 1;
	    qBaseInsert += opLen;
	    qPos += opLen;
	    break;
	case 'D':
	    if (prevOp == '\0')	// leading deletion
		insl = -opLen;
	    else
		{
		pushBlock(psl, blockSpace, qPos-lens, tPos-lens, lens);
		lens = 0;
		}
	    tNumInsert += 1;
	    tBaseInsert += opLen;
	    tPos += opLen;
	    break;
	default:
	    warn("%s:%d: invalid CIGAR operator '%c'", fileName, lineIx, op);
	    return FALSE;
	}
    prevOp = op;
    }

// the last CIGAR operator
if (prevOp == 'I')		// trailing insertion
    insr = opLen;
else if (prevOp == 'D')	// trailing deletion
    insr = -opLen;
else
    pushBlock(psl, blockSpace, qPos-lens, tPos-lens, lens);

if (qPos != psl->qEnd - psl->qStart)
    {
    warn("%s:%d: CIGAR length does not match alignment length (query): %d != %d",
	fileName, lineIx, psl->qEnd - psl->qStart, qPos);
    return FALSE;
    }
if (tPos != psl->tEnd - psl->tStart)
    {
    warn("%s:%d: CIGAR length does not match alignment length (target): %d != %d",
	fileName, lineIx, psl->tEnd - psl->tStart, tPos);
    return FALSE;
    }

// block offsets computed above are measured from position 0 of the CIGAR,
// which is the *untrimmed* qStart/tStart the PAF reported -- save those
// before trimming for leading/trailing indels below, so the shift to
// absolute coordinates doesn't double-count the trim.
int origQStart = psl->qStart;
int origQEnd = psl->qEnd;
int origTStart = psl->tStart;

// handle leading and trailing indels: PSL blocks must start and end on a match
if (insl > 0)
    { qNumInsert -= 1; qBaseInsert -= insl; psl->qStart += insl; }
else if (insl < 0)
    { tNumInsert -= 1; tBaseInsert += insl; psl->tStart -= insl; }

if (insr > 0)
    { qNumInsert -= 1; qBaseInsert -= insr; psl->qEnd -= insr; }
else if (insr < 0)
    { tNumInsert -= 1; tBaseInsert += insr; psl->tEnd += insr; }

// shift target block positions to absolute target coordinates
for (i = 0; i < psl->blockCount; i++)
    psl->tStarts[i] += origTStart;

// shift query block positions to absolute coordinates, taking the query
// strand into account
if (psl->strand[0] == '-')
    for (i = 0; i < psl->blockCount; i++)
	psl->qStarts[i] = psl->qSize - origQEnd + psl->qStarts[i];
else
    for (i = 0; i < psl->blockCount; i++)
	psl->qStarts[i] += origQStart;

psl->qNumInsert = qNumInsert;
psl->qBaseInsert = qBaseInsert;
psl->tNumInsert = tNumInsert;
psl->tBaseInsert = tBaseInsert;

int misMatch = psl->qEnd - psl->qStart - qBaseInsert - (int)psl->match;
if (misMatch < 0)
    {
    warn("%s:%d: negative misMatch value: %d", fileName, lineIx, misMatch);
    return FALSE;
    }
psl->misMatch = misMatch;

lens = 0;
for (i = 0; i < psl->blockCount; i++)
    lens += psl->blockSizes[i];
int nCount = lens - (int)psl->match - (int)psl->misMatch - (int)psl->repMatch;
if (nCount < 0)
    {
    warn("%s:%d: negative nCount value: %d", fileName, lineIx, nCount);
    return FALSE;
    }
psl->nCount = nCount;

return TRUE;
}

static void pafToPsl(char *inFile, char *outFile)
/* pafToPsl - converting paf output from minimap2 into UCSC psl format. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
int tagLen = strlen(cigarTag);
struct hash *tSizeHash = tSizesFile ? hashNameIntFile(tSizesFile) : NULL;
struct hash *qSizeHash = qSizesFile ? hashNameIntFile(qSizesFile) : NULL;
char *line;
int lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    char *s = line;
    char *fields[11];
    int i;

    for (i = 0; i < ArraySize(fields); i++)
	{
	fields[i] = nextWord(&s);
	if (fields[i] == NULL)
	    break;
	}
    if (i < ArraySize(fields))
	{
	warn("%s:%d: PAF line has fewer than %d fields, skipping",
	    lf->fileName, lf->lineIx, (int)ArraySize(fields));
	continue;
	}

    char *cigar = NULL;
    char *tag;
    while ((tag = nextWord(&s)) != NULL)
	{
	if ((int)strlen(tag) > tagLen && startsWith(cigarTag, tag))
	    {
	    cigar = tag + tagLen;
	    break;
	    }
	}
    if (cigar == NULL)
	{
	warn("%s:%d: PAF line is missing a %s CIGAR tag, skipping",
	    lf->fileName, lf->lineIx, cigarTag);
	continue;
	}

    char strand[2];
    strand[0] = fields[4][0];
    strand[1] = '\0';
    if (strand[0] != '+' && strand[0] != '-')
	{
	warn("%s:%d: invalid PAF strand '%c', skipping",
	    lf->fileName, lf->lineIx, strand[0]);
	continue;
	}

    unsigned qSize = sqlUnsigned(fields[1]);
    if (qSizeHash != NULL)
	{
	int sizesVal = hashIntValDefault(qSizeHash, fields[0], -1);
	if (sizesVal < 0)
	    {
	    warn("%s:%d: query '%s' not found in %s, skipping",
		lf->fileName, lf->lineIx, fields[0], qSizesFile);
	    continue;
	    }
	if ((unsigned)sizesVal != qSize)
	    warn("%s:%d: query '%s' size %u in PAF does not match %d in %s, using %s",
		lf->fileName, lf->lineIx, fields[0], qSize, sizesVal, qSizesFile, qSizesFile);
	qSize = (unsigned)sizesVal;
	}

    unsigned tSize = sqlUnsigned(fields[6]);
    if (tSizeHash != NULL)
	{
	int sizesVal = hashIntValDefault(tSizeHash, fields[5], -1);
	if (sizesVal < 0)
	    {
	    warn("%s:%d: target '%s' not found in %s, skipping",
		lf->fileName, lf->lineIx, fields[5], tSizesFile);
	    continue;
	    }
	if ((unsigned)sizesVal != tSize)
	    warn("%s:%d: target '%s' size %u in PAF does not match %d in %s, using %s",
		lf->fileName, lf->lineIx, fields[5], tSize, sizesVal, tSizesFile, tSizesFile);
	tSize = (unsigned)sizesVal;
	}

    int blockSpace = 16;
    struct psl *psl = pslNew(fields[0], qSize,
	sqlSigned(fields[2]), sqlSigned(fields[3]),
	fields[5], tSize,
	sqlSigned(fields[7]), sqlSigned(fields[8]),
	strand, blockSpace, 0);
    psl->match = sqlUnsigned(fields[9]);
    // fields[10], the PAF "number of minimizers"/alignment block length
    // field, is not used -- as in the original converter.

    if (cigarToPsl(psl, &blockSpace, cigar, lf->fileName, lf->lineIx))
	pslTabOut(psl, f);
    pslFree(&psl);
    }

carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cigarTag = optionVal("cigarTag", cigarTag);
if (strlen(cigarTag) != 5 || cigarTag[2] != ':' || cigarTag[4] != ':')
    errAbort("-cigarTag must look like xx:Z: (got '%s')", cigarTag);
tSizesFile = optionVal("tSizes", NULL);
qSizesFile = optionVal("qSizes", NULL);
pafToPsl(argv[1], argv[2]);
return 0;
}
