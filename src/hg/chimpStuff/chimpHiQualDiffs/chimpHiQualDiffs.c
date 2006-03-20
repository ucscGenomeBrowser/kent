/* chimpHiQualDiffs - Create list of chimp high quality differences. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "qaSeq.h"
#include "axt.h"
#include "portable.h"
#include "rle.h"

static char const rcsid[] = "$Id: chimpHiQualDiffs.c,v 1.2 2006/03/20 08:35:13 daryl Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chimpHiQualDiffs - Create list of chimp high quality differences\n"
  "usage:\n"
  "   chimpHiQualDiffs axtDir chimp.qac out.bed\n"
  "options:\n"
  "   -winSize      Window Size (11)\n"
  "   -diffQualMin  Min quality score for difference (30)\n"
  "   -winQualMin   Min quality score for window (25)\n"
  "   -winMaxDiff   Maximum number of differences in window (0)\n"
  "   -indelOk      Allow indels in window (currently not implemented)\n"
  );
}

static struct optionSpec options[] = {
    {"winSize", OPTION_INT},
    {"diffQualMin", OPTION_INT},
    {"winQualMin", OPTION_INT},
    {"winMaxDiff", OPTION_INT},
    {"indelOk", OPTION_BOOLEAN},
    {NULL, 0}
};

struct qac
/* A run-length-compressed set of quality scores. */
    {
    int uncSize;		/* Uncompressed size. */
    int compSize;		/* Compressed size. */
    signed char data[1];	/* Compressed data. */
    };

struct hash *qacReadToHash(char *fileName)
/* Read in a qac file into a hash of qacs keyed by name. */
{
boolean isSwapped;
FILE *f = qacOpenVerify(fileName, &isSwapped);
bits32 compSize, uncSize;
struct qac *qac;
char *name;
struct hash *hash = newHash(18);
int count = 0;

for (;;)
    {
    name = readString(f);
    if (name == NULL)
       break;
    mustReadOne(f, uncSize);
    if (isSwapped)
	uncSize = byteSwap32(uncSize);
    mustReadOne(f, compSize);
    if (isSwapped)
	compSize = byteSwap32(compSize);
    qac = needHugeMem(sizeof(*qac) + compSize - 1);
    qac->uncSize = uncSize;
    qac->compSize = compSize;
    mustRead(f, qac->data, compSize);
    hashAdd(hash, name, qac);
    ++count;
    }
carefulClose(&f);
printf("Read %d qacs from %s\n", count, fileName);
return hash;
}

void axtHiQualDiffs(char *axtFile, struct hash *qacHash, FILE *f)
/* Write out high quality diffs in axtFile to f. */
{
char *qName = cloneString("");
UBYTE *qQuals = NULL;
UBYTE *quals = NULL;
struct qac *qac = NULL;
struct axt *axt = NULL;
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
int qStart, qDir, qPos, qWinStart, qWinEnd, tPos;
int qWinSize     = optionInt("winSize",     11);
int qQualMin     = optionInt("diffQualMin", 30);
int qWinQualMin  = optionInt("winQualMin",  25);
int qWinMaxDiff  = optionInt("winMaxDiff",  2);
boolean qIndelOk = optionExists("indelOk");
int qHalfWinSize = qWinSize/2;

while ((axt = axtRead(lf)) != NULL)
    {
    char *qSym = axt->qSym, *tSym = axt->tSym;
    int symIx, symCount = axt->symCount;
    char qc,tc;
    toUpperN(qSym, symCount);
    toUpperN(tSym, symCount);
    if (!sameString(axt->qName, qName))
        {
	freez(&qName);
	qName = cloneString(axt->qName);
	qac = hashMustFindVal(qacHash, qName);
	freez(&qQuals);
	qQuals = needHugeMem(qac->uncSize);
	rleUncompress(qac->data, qac->compSize, qQuals, qac->uncSize);
	}
    if (axt->qStrand == '+')
        {
	qStart = axt->qStart;
	qDir = 1;
	}
    else
        {
	qStart = qac->uncSize - axt->qStart - 1;
	qDir = -1;
	}
    qPos = qStart;
    tPos = axt->tStart;
    for (symIx = 0; symIx < symCount; ++symIx)
        {
	qc = qSym[symIx];
	tc = tSym[symIx];
	if (qc == '-')
	    tPos += 1;
	else if (tc == '-')
	    qPos += qDir;
	else 
	    {
	    if (qc != tc)
		{
		qWinStart = qPos - qHalfWinSize;
		qWinEnd = qWinStart + qWinSize;
		if (qWinStart >= 0 && qWinEnd < qac->uncSize)
		    {
		    if (qQuals[qPos] >= qQualMin)
		        {
			int i;
			boolean ok = TRUE;
			for (i = qWinStart; i<qWinEnd; ++i)
			    if (qQuals[i] < qWinQualMin)
			        {
				ok = FALSE;
				break;
				}
			if (ok)
			    {
			    int diffCount = 0;
			    int symWinStart = symIx - qHalfWinSize;
			    int symWinEnd = symWinStart + qWinSize;
			    for (i=symWinStart; i < symWinEnd; ++i)
			        {
				qc = qSym[i];
				tc = tSym[i];
				if (qc == '-' || tc == '-')
				    {
				    ok = FALSE;
				    break;
				    }
				if (qc != tc)
				    ++diffCount;
				}
			    if (ok && diffCount <= qWinMaxDiff)
				fprintf(f, "%s\t%d\t%d\t%c\t%c\n",
					axt->tName, tPos, tPos+1, tSym[symIx], qSym[symIx]);
			    }
			}
		    }
		}
	    qPos += qDir;
	    tPos += 1;
	    }
	}
    axtFree(&axt);
    }
lineFileClose(&lf);
}

void chimpHiQualDiffs(char *axtDir, char *qacName, char *bedName)
/* chimpHiQualDiffs - Create list of chimp high quality differences. */
{
struct hash *qacHash = qacReadToHash(qacName);
struct fileInfo *axtEl, *axtList = listDirX(axtDir, "*.axt", TRUE);
FILE *f = mustOpen(bedName, "w");

if (axtList==NULL)
    printf("No axt files were found in the '%s' directory.\n",axtDir);
for (axtEl = axtList; axtEl != NULL; axtEl = axtEl->next)
    axtHiQualDiffs(axtEl->name, qacHash, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
chimpHiQualDiffs(argv[1], argv[2], argv[3]);
return 0;
}
