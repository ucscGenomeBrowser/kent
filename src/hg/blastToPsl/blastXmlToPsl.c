/* blastXmlToPsl - convert blast XML output to PSLs. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "ncbiBlast.h"
#include "pslBuild.h"

static char const rcsid[] = "$Id: blastXmlToPsl.c,v 1.2 2010/04/16 17:32:12 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastXmlToPsl - convert blast XML output to PSLs\n"
  "usage:\n"
  "   blastXmlToPsl [options] blastXml psl\n"
  "\n"
  "options:\n"
  "  -scores=file - Write score information to this file.  Format is:\n"
  "       strands qName qStart qEnd tName tStart tEnd bitscore eVal\n"
  "  -verbose=n - n >= 3 prints each line of file after parsing.\n"
  "               n >= 4 dumps the result of each query\n"
  "  -eVal=n n is e-value threshold to filter results. Format can be either\n"
  "          an integer, double or 1e-10. Default is no filter.\n"
  "  -pslx - create PSLX output (includes sequences for blocks)\n"
  "  -convertToNucCoords - convert protein to nucleic alignments to nucleic\n"
  "   to nucleic coordinates\n"
  "\n"
  "Output only results of last round from PSI BLAST\n");
}

static struct optionSpec options[] = {
    {"scores", OPTION_STRING},
    {"eVal", OPTION_DOUBLE},
    {"pslx", OPTION_BOOLEAN},
    {"convertToNucCoords", OPTION_BOOLEAN},
    {NULL, 0},
};

static double eVal = -1; /* default Expect value signifying no filtering */
static boolean pslxFmt = FALSE; /* output in pslx format */
static int errCount = 0; /* count of  PSLs failing checks */
static boolean convertToNucCoords = 1; /* adjust query coordinates */

struct coords
/* structure to return converted coordinates */
{
    int start;
    int end;
    int size;
    char strand;
};

static struct coords blastToUcsc(int blastStart, int blastEnd, int size, int blastFrame)
/* convert coordinates from blast to UCSC convention. */
{
// blastStart >= blastEnd for queries with blastFrame < 0
// blastStart <= blastEnd for hits with blastFrame < 0
struct coords ucsc;
ucsc.start = (blastStart <= blastEnd) ? blastStart-1 : blastEnd-1;
ucsc.end = (blastStart <= blastEnd) ? blastEnd : blastStart;
ucsc.size = size;
ucsc.strand = (blastFrame >= 0) ? '+' : '-';
if (ucsc.strand == '-')
    reverseIntRange(&ucsc.start, &ucsc.end, size);
assert(ucsc.start < ucsc.end);
return ucsc;
}

static unsigned getFlags(struct ncbiBlastBlastOutput *outputRec)
/* determine blast algorithm and other flags */
{
unsigned algo = pslBuildGetBlastAlgo(outputRec->ncbiBlastBlastOutputProgram->text);
if (convertToNucCoords && (algo != tblastn))
    errAbort("-convertToNucCoords only support for TBLASTN");
return algo | (convertToNucCoords ? cnvNucCoords : 0) | (pslxFmt ? bldPslx : 0);
}

static void outputPsl(struct psl *psl, struct ncbiBlastHsp *hspRec, FILE* pslFh, FILE* scoreFh)
/* output a psl and optional score */
{
pslTabOut(psl, pslFh);
if (scoreFh != NULL)
    pslBuildWriteScores(scoreFh, psl, hspRec->ncbiBlastHspBitScore->text, hspRec->ncbiBlastHspEvalue->text);
pslCheck("blastXmlToPsl", stderr, psl);
}

static void processHspRec(struct ncbiBlastIteration *iterRec, struct ncbiBlastHit *hitRec,
                          struct ncbiBlastHsp *hspRec, unsigned flags, FILE *pslFh, FILE *scoreFh)
/* process one HSP record, converting to a PSL */
{
struct coords qUcsc = blastToUcsc(hspRec->ncbiBlastHspQueryFrom->text, hspRec->ncbiBlastHspQueryTo->text, iterRec->ncbiBlastIterationQueryLen->text,
                                  ((hspRec->ncbiBlastHspQueryFrame == NULL) ? 0 : hspRec->ncbiBlastHspQueryFrame->text));
struct coords tUcsc = blastToUcsc(hspRec->ncbiBlastHspHitFrom->text, hspRec->ncbiBlastHspHitTo->text, hitRec->ncbiBlastHitLen->text,
                                  ((hspRec->ncbiBlastHspHitFrame == NULL) ? 0 : hspRec->ncbiBlastHspHitFrame->text));
struct psl *psl = pslBuildFromHsp(firstWordInLine(iterRec->ncbiBlastIterationQueryDef->text), qUcsc.size, qUcsc.start, qUcsc.end, qUcsc.strand, hspRec->ncbiBlastHspQseq->text,
                                  firstWordInLine(hitRec->ncbiBlastHitDef->text), tUcsc.size, tUcsc.start, tUcsc.end, tUcsc.strand, hspRec->ncbiBlastHspHseq->text,
                                  flags);
if  ((psl->blockCount > 0) && ((hspRec->ncbiBlastHspEvalue->text <= eVal) || (eVal == -1)))
    outputPsl(psl, hspRec, pslFh, scoreFh);
pslFree(&psl);
}

static void processIterRec(struct ncbiBlastIteration *iterRec, unsigned flags, FILE *pslFh, FILE *scoreFh)
/* process one iteration record, converting all HSPs to PSLs */
{
struct ncbiBlastIterationHits *hitsRec;
for (hitsRec = iterRec->ncbiBlastIterationHits; hitsRec != NULL; hitsRec = hitsRec->next)
    {
    struct ncbiBlastHit *hitRec;
    for (hitRec = hitsRec->ncbiBlastHit; hitRec != NULL; hitRec = hitRec->next)
        {
        struct ncbiBlastHitHsps *hspsRec;
        for (hspsRec = hitRec->ncbiBlastHitHsps; hspsRec != NULL; hspsRec = hspsRec->next)
            {
            struct ncbiBlastHsp *hspRec;
            for (hspRec = hspsRec->ncbiBlastHsp; hspRec != NULL; hspRec = hspRec->next)
                {
                processHspRec(iterRec, hitRec, hspRec, flags, pslFh, scoreFh);
                }
            }
        }
    }
}

static void convertOnePassBlast(struct ncbiBlastBlastOutput *outputRec, unsigned flags, FILE *pslFh, FILE *scoreFh)
/* convert standard single-pass blast */
{
struct ncbiBlastBlastOutputIterations *itersRec;
for (itersRec = outputRec->ncbiBlastBlastOutputIterations; itersRec != NULL; itersRec = itersRec->next)
    {
    struct ncbiBlastIteration *iterRec;
    for (iterRec = itersRec->ncbiBlastIteration; iterRec != NULL; iterRec = iterRec->next)
        processIterRec(iterRec, flags, pslFh, scoreFh);
    }
}

static struct ncbiBlastIteration *findLastIterForQuery(struct ncbiBlastIteration *iterRec)
/* find the last iteration record for the query in the specified record */
{
struct ncbiBlastIteration *nextRec;
for (nextRec = iterRec->next; nextRec != NULL ; iterRec = nextRec, nextRec = nextRec->next)
    {
    if (!sameString(nextRec->ncbiBlastIterationQueryDef->text, iterRec->ncbiBlastIterationQueryDef->text))
        break;
    }
return iterRec;
}

static void convertPsiBlast(struct ncbiBlastBlastOutput *outputRec, unsigned flags, FILE *pslFh, FILE *scoreFh)
/* convert psi-blast */
{
struct ncbiBlastBlastOutputIterations *itersRec;
for (itersRec = outputRec->ncbiBlastBlastOutputIterations; itersRec != NULL; itersRec = itersRec->next)
    {
    struct ncbiBlastIteration *iterRec;
    for (iterRec = itersRec->ncbiBlastIteration; iterRec != NULL; iterRec = iterRec->next)
        {
        iterRec = findLastIterForQuery(iterRec);
        processIterRec(iterRec, flags, pslFh, scoreFh);
        }
    }
}

static void blastXmlToPsl(char *blastXmlFile, char *pslFile, char *scoreFile)
/* blastXmlToPsl - convert blast XML output to PSLs. */
{
struct xap *xap = xapNew(ncbiBlastStartHandler, ncbiBlastEndHandler, blastXmlFile);
xapParseFile(xap, blastXmlFile);
FILE *pslFh = mustOpen(pslFile, "w");
FILE *scoreFh = NULL;
if (scoreFile != NULL)
    {
    scoreFh = mustOpen(scoreFile, "w");
    fputs(pslBuildScoreHdr, scoreFh);
    }

if (xap->topObject == NULL)
    errAbort("empty BLAST XML file: %s", blastXmlFile);
char *expectType = "BlastOutput";
if (!sameString(xap->topType, expectType))
    errAbort("expected top XML element of type \"%s\", got \"%s\"", expectType, xap->topType);
struct ncbiBlastBlastOutput *outputRec = xap->topObject;
unsigned flags = getFlags(outputRec);

if (flags & psiblast)
    convertPsiBlast(outputRec, flags, pslFh, scoreFh);
else
    convertOnePassBlast(outputRec, flags, pslFh, scoreFh);

carefulClose(&scoreFh);
carefulClose(&pslFh);
ncbiBlastBlastOutputFree(&outputRec);
xapFree(&xap);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
eVal = optionDouble("eVal", eVal);
pslxFmt = optionExists("pslx");
convertToNucCoords = optionExists("convertToNucCoords");
blastXmlToPsl(argv[1], argv[2], optionVal("scores", NULL));
if (errCount > 0)
    errAbort("%d invalid PSLs created", errCount);
return 0;
}
