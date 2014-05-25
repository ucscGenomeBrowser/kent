/* blastXmlToPsl - convert blast XML output to PSLs. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "ncbiBlast.h"
#include "pslBuild.h"


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
  "       strands qName qStart qEnd tName tStart tEnd bitscore eVal qDef tDef\n"
  "  -verbose=n - n >= 3 prints each line of file after parsing.\n"
  "               n >= 4 dumps the result of each query\n"
  "  -eVal=n n is e-value threshold to filter results. Format can be either\n"
  "          an integer, double or 1e-10. Default is no filter.\n"
  "  -pslx - create PSLX output (includes sequences for blocks)\n"
  "  -convertToNucCoords - convert protein to nucleic alignments to nucleic\n"
  "   to nucleic coordinates\n"
  "  -qName=src - define element used to obtain the qName.  The following\n"
  "   values are support:\n"
  "     o query-ID - use contents of the <Iteration_query-ID> element if it\n"
  "       exists, otherwise use <BlastOutput_query-ID>\n"
  "     o query-def0 - use the first white-space separated word of the\n"
  "       <Iteration_query-def> element if it exists, otherwise the first word\n"
  "       of <BlastOutput_query-def>.\n"
  "   Default is query-def0.\n"
  "  -tName=src - define element used to obtain the tName.  The following\n"
  "   values are support:\n"
  "     o Hit_id - use contents of the <Hit-id> element.\n"
  "     o Hit_def0 - use the first white-space separated word of the\n"
  "       <Hit_def> element.\n"
  "     o Hit_accession - contents of the <Hit_accession> element.\n"
  "   Default is Hit-def0.\n"
  "  -forcePsiBlast - treat as output of PSI-BLAST. blast-2.2.16 and maybe\n"
  "   others indentify psiblast as blastp."
  "\n"
  "Output only results of last round from PSI BLAST\n");
}

static struct optionSpec options[] = {
    {"scores", OPTION_STRING},
    {"eVal", OPTION_DOUBLE},
    {"pslx", OPTION_BOOLEAN},
    {"convertToNucCoords", OPTION_BOOLEAN},
    {"qName", OPTION_STRING},
    {"tName", OPTION_STRING},
    {"forcePsiBlast", OPTION_BOOLEAN},
    {NULL, 0},
};

enum qNameSrc {
    qNameSrcQueryId,
    qNameSrcQueryDef0
};

enum tNameSrc {
    tNameSrcHitId,
    tNameSrcHitDef0,
    tNameSrcHitAccession
};


static double eVal = -1; /* default Expect value signifying no filtering */
static boolean pslxFmt = FALSE; /* output in pslx format */
static int errCount = 0; /* count of  PSLs failing checks */
static boolean convertToNucCoords = FALSE; /* adjust query coordinates */
static boolean forcePsiBlast = FALSE; /* assume PSI-BLAST output  */
static enum qNameSrc qNameSrc = qNameSrcQueryDef0;   /* source of qName */
static enum tNameSrc tNameSrc = tNameSrcHitDef0;   /* source of tName */

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
if (forcePsiBlast)
    algo = psiblast;
if (convertToNucCoords && (algo != tblastn))
    errAbort("-convertToNucCoords only support for TBLASTN");
return algo | (convertToNucCoords ? cnvNucCoords : 0) | (pslxFmt ? bldPslx : 0);
}

static void outputPsl(struct psl *psl, FILE* pslFh)
/* output a psl */
{
pslTabOut(psl, pslFh);
pslCheck("blastXmlToPsl", stderr, psl);
}

static void outputScore(struct psl *psl, struct ncbiBlastBlastOutput *outputRec, struct ncbiBlastIteration *iterRec, struct ncbiBlastHit *hitRec, struct ncbiBlastHsp *hspRec, FILE* scoreFh)
/* output score record */
{
pslBuildScoresWriteWithDefs(scoreFh, psl, hspRec->ncbiBlastHspBitScore->text, hspRec->ncbiBlastHspEvalue->text, 
                            (iterRec->ncbiBlastIterationQueryDef != NULL) ? iterRec->ncbiBlastIterationQueryDef->text : outputRec->ncbiBlastBlastOutputQueryDef->text,
                            hitRec->ncbiBlastHitDef->text);
}

static void appendFirstWord(struct dyString *buf, char *str)
/* append the first white-spaced word from str */
{
char *end = skipToSpaces(str);
if (end == NULL)
    end = str + strlen(str);
dyStringAppendN(buf, str, (end - str));
}

static char *getQName(struct ncbiBlastBlastOutput *outputRec, struct ncbiBlastIteration *iterRec)
/* obtain the qName give the requested source */
{
static struct dyString *buf = NULL;
if (buf == NULL)
    buf = dyStringNew(32);
dyStringClear(buf);
switch (qNameSrc)
    {
    case qNameSrcQueryId:
        dyStringAppend(buf, (iterRec->ncbiBlastIterationQueryID != NULL)
                       ? iterRec->ncbiBlastIterationQueryID->text
                       : outputRec->ncbiBlastBlastOutputQueryID->text);
        break;
    case qNameSrcQueryDef0:
        appendFirstWord(buf, (iterRec->ncbiBlastIterationQueryDef != NULL)
                        ? iterRec->ncbiBlastIterationQueryDef->text
                        : outputRec->ncbiBlastBlastOutputQueryDef->text);
        break;        
    }
return buf->string;
}

static char *getTName(struct ncbiBlastHit *hitRec)
/* obtain the tName give the requested source */
{
static struct dyString *buf = NULL;
if (buf == NULL)
    buf = dyStringNew(32);
dyStringClear(buf);
switch (tNameSrc)
    {
    case tNameSrcHitId:
        dyStringAppend(buf, hitRec->ncbiBlastHitId->text);
        break;
    case tNameSrcHitDef0:
        appendFirstWord(buf, hitRec->ncbiBlastHitDef->text);
        break;        
    case tNameSrcHitAccession:
        dyStringAppend(buf, hitRec->ncbiBlastHitAccession->text);
        break;
    }
return buf->string;
}

static void processHspRec(struct ncbiBlastBlastOutput *outputRec, struct ncbiBlastIteration *iterRec, struct ncbiBlastHit *hitRec,
                          struct ncbiBlastHsp *hspRec, unsigned flags, FILE *pslFh, FILE *scoreFh)
/* process one HSP record, converting to a PSL */
{
int queryLen = (iterRec->ncbiBlastIterationQueryLen != NULL) 
    ? iterRec->ncbiBlastIterationQueryLen->text
    : outputRec->ncbiBlastBlastOutputQueryLen->text;
struct coords qUcsc = blastToUcsc(hspRec->ncbiBlastHspQueryFrom->text, hspRec->ncbiBlastHspQueryTo->text, queryLen,
                                  ((hspRec->ncbiBlastHspQueryFrame == NULL) ? 0 : hspRec->ncbiBlastHspQueryFrame->text));
struct coords tUcsc = blastToUcsc(hspRec->ncbiBlastHspHitFrom->text, hspRec->ncbiBlastHspHitTo->text, hitRec->ncbiBlastHitLen->text,
                                  ((hspRec->ncbiBlastHspHitFrame == NULL) ? 0 : hspRec->ncbiBlastHspHitFrame->text));
struct psl *psl = pslBuildFromHsp(getQName(outputRec, iterRec), qUcsc.size, qUcsc.start, qUcsc.end, qUcsc.strand, hspRec->ncbiBlastHspQseq->text,
                                  getTName(hitRec),  tUcsc.size, tUcsc.start, tUcsc.end, tUcsc.strand, hspRec->ncbiBlastHspHseq->text,
                                  flags);
if  ((psl->blockCount > 0) && ((hspRec->ncbiBlastHspEvalue->text <= eVal) || (eVal == -1)))
    {
    outputPsl(psl, pslFh);
    if (scoreFh != NULL)
        outputScore(psl, outputRec, iterRec, hitRec, hspRec, scoreFh);
    }
pslFree(&psl);
}

static void processIterRec(struct ncbiBlastBlastOutput *outputRec, struct ncbiBlastIteration *iterRec, unsigned flags, FILE *pslFh, FILE *scoreFh)
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
                processHspRec(outputRec, iterRec, hitRec, hspRec, flags, pslFh, scoreFh);
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
        processIterRec(outputRec, iterRec, flags, pslFh, scoreFh);
    }
}

static struct ncbiBlastIteration *findLastIterForQuery(struct ncbiBlastIteration *iterRec)
/* find the last iteration record for the query in the specified record */
{
struct ncbiBlastIteration *nextRec;
for (nextRec = iterRec->next; nextRec != NULL ; iterRec = nextRec, nextRec = nextRec->next)
    {
    if ((nextRec->ncbiBlastIterationQueryDef != NULL)
        && !sameString(nextRec->ncbiBlastIterationQueryDef->text, iterRec->ncbiBlastIterationQueryDef->text))
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
    struct ncbiBlastIteration *iterRec = findLastIterForQuery(itersRec->ncbiBlastIteration);
    processIterRec(outputRec, iterRec, flags, pslFh, scoreFh);
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
    scoreFh = pslBuildScoresOpen(scoreFile, TRUE);

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
forcePsiBlast = optionExists("forcePsiBlast");

char *qNameSrcStr = optionVal("qName", "query-def0");
if (sameString(qNameSrcStr, "query-ID"))
    qNameSrc = qNameSrcQueryId;
else if (sameString(qNameSrcStr, "query-def0"))
    qNameSrc = qNameSrcQueryDef0;
else
    errAbort("invalid value for -qName, expect on of: \"query-ID\", or \"query-def0\", got \"%s\"", qNameSrcStr);

char *tNameSrcStr = optionVal("tName", "Hit_def0");
if (sameString(tNameSrcStr, "Hit_id"))
    tNameSrc = tNameSrcHitId;
else if (sameString(tNameSrcStr, "Hit_def0"))
    tNameSrc = tNameSrcHitDef0;
else if (sameString(tNameSrcStr, "Hit_accession"))
    tNameSrc = tNameSrcHitAccession;
else
    errAbort("invalid value for -tName, expect on of: \"Hit_id\",  \"Hit_def0\", or \"Hit_accession\", got \"%s\"", tNameSrcStr);

blastXmlToPsl(argv[1], argv[2], optionVal("scores", NULL));
if (errCount > 0)
    errAbort("%d invalid PSLs created", errCount);
return 0;
}
