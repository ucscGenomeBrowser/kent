/* ncbiTabToPsl - Convert NCBI tab-file alignments to PSL format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errabort.h"
#include "psl.h"
#include "sqlNum.h"
#include "dnautil.h"
#include "obscure.h"
#include "alignSeqSizes.h"
#include "ncbiTabAlign.h"

static char const rcsid[] = "$Id: ncbiTabToPsl.c,v 1.1 2004/01/17 09:37:17 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
    "ncbiTabToPsl - Convert NCBI tab-file pair alignments to PSL format\n"
    "\n"
    "usage:\n"
    "   ncbiTabToPsl [options] in out.psl\n"
    "\n"
    "Options:\n"
    "   -qSizesFile=szfile - read query sizes from tab file of <seqName> <size>\n"
    "   -qSizes='id1 size1 ..' - set query sizes, value is white-space separated pairs of sequence and size\n"
    "   -tSizesFile=szfile - read target sizes from tab file of <seqName> <size>\n"
    "   -tSizes='id1 size1 ..' - set target sizes, value is white-space separated pairs of sequence and size\n"
    "\n"
    "alignment format is:\n"
    "        1  contig accession.version\n"
    "        2  arbitrary alignment idn"
    "        3  EXON = aligned region, GAP = region missing in mRNA\n"
    "        4  start on contig (numbering begins with 1)\n"
    "        5  stop on contig\n"
    "        6  mRNA accession\n"
    "        7  unused\n"
    "        8  start on mRNA\n"
    "        9  stop on mRNA\n"
    "        10 percent identity\n"
    "        11 score (from the alignment algorithm, not a BLAST score)\n"
  );
}


/* command line */
static struct optionSpec optionSpec[] = {
    {"qSizesFile", OPTION_STRING},
    {"qSizes", OPTION_STRING},
    {"tSizesFile", OPTION_STRING},
    {"tSizes", OPTION_STRING},
    {NULL, 0}
};


struct parser
/* data used during parsing */
{
    struct lineFile* lf;            /* alignment file */
    struct alignSeqSizes* ass;      /* sequence sizes */
    struct psl* psl;                /* PSL being constructed */
    struct ncbiTabAlign *recCache;  /* record catch */
    unsigned pslMaxBlks;            /* memory allocated for PSL blocks */
    unsigned curAlnId;              /* current alignment id */
};

void parseErr(struct parser *pr, char* format, ...)
/* prototype to get gnu attribute check */
#if defined(__GNUC__) && defined(JK_WARN)
__attribute__((format(printf, 2, 3)))
#endif
;

void parseErr(struct parser *pr, char* format, ...)
/* abort on parse error */
{
va_list args;
va_start(args, format);
fprintf(stderr, "parse error: %s:%d: ",
        pr->lf->fileName, pr->lf->lineIx);
vaErrAbort(format, args);
va_end(args);

}

struct ncbiTabAlign *loadNextRec(struct parser *pr)
/* read the next record, if one is cached, return it instead. NULL on EOF */
{
struct ncbiTabAlign *rec = NULL;
if (pr->recCache != NULL)
    {
    rec = pr->recCache;
    pr->recCache = NULL;
    }
else
    {
    char *row[NCBITABALIGN_NUM_COLS];
    if (lineFileChopNextTab(pr->lf, row, ArraySize(row)))
        rec = ncbiTabAlignLoad(row);
    }
return rec;
}

void growPslBlocks(struct parser *pr, unsigned newBlks)
/* grow memory allocated to PSL to be able to hold newBLks */
{
int oldSize = pr->pslMaxBlks * sizeof(unsigned);
int newSize = newBlks * sizeof(unsigned);
pr->psl->blockSizes = needMoreMem(pr->psl->blockSizes, oldSize, newSize);
pr->psl->qStarts = needMoreMem(pr->psl->qStarts, oldSize, newSize);
pr->psl->tStarts = needMoreMem(pr->psl->tStarts, oldSize, newSize);
pr->pslMaxBlks = newBlks;
}

boolean nextAlign(struct parser *pr)
/* start a new alignment, allocating psl, etc. */
{
struct ncbiTabAlign *rec = loadNextRec(pr);
if (rec == NULL)
    return FALSE;

AllocVar(pr->psl);
growPslBlocks(pr, 8);
pr->psl->qName = cloneString(rec->query);
pr->psl->qSize = alignSeqSizesMustGetQuery(pr->ass, rec->query);
pr->psl->tName = cloneString(rec->target);
pr->psl->tSize = alignSeqSizesMustGetTarget(pr->ass, rec->target);

/* strand is negative if addressed are reversed */
pr->psl->strand[0] = (rec->qStart > rec->qStop) ? '-' : '+';
pr->psl->strand[1] = (rec->tStart > rec->tStop) ? '-' : '+';

pr->curAlnId = rec->alnId;

/* save record for block parsing */
pr->recCache = rec;
return TRUE;
}

int convertCoords(struct parser *pr, char strand, int alnStart, int alnStop,
                  int seqSize, int *startRet)
/* convert a start/end pair to start and return block size */
{
if (((alnStart > alnStop) ? '-' : '+') != strand)
    parseErr(pr, "strand implied by addresses doesn't match expected: %d-%d vs %c",
             alnStart, alnStop, strand);
if (strand == '-')
    {
    int hold = alnStart;
    alnStart = alnStop;
    alnStop = hold;
    }
alnStart--;
if (strand == '-')
    reverseIntRange(&alnStart, &alnStop, seqSize);

*startRet = alnStart;
return (alnStop - alnStart);

}

void addRec(struct parser *pr, struct ncbiTabAlign* rec)
/* add a record to the psl */
{
int iBlk = pr->psl->blockCount;
int size, match, prevEnd;
if (iBlk >= pr->pslMaxBlks)
    growPslBlocks(pr, 2*pr->pslMaxBlks);

size = convertCoords(pr, pr->psl->strand[0], rec->qStart, rec->qStop,
                     pr->psl->qSize, &(pr->psl->qStarts[iBlk]));
convertCoords(pr, pr->psl->strand[1], rec->tStart, rec->tStop,
              pr->psl->tSize, &(pr->psl->tStarts[iBlk]));
pr->psl->blockSizes[iBlk] = size;
pr->psl->blockCount++;

/* update stats */
match = size * (rec->perId/100.0);
pr->psl->match += match;
pr->psl->misMatch += (size - match);

if (iBlk > 0)
    {
    /* count gap info */
    prevEnd = pr->psl->qStarts[iBlk-1] + pr->psl->blockSizes[iBlk-1];
    if (prevEnd < pr->psl->qStarts[iBlk])
        {
        pr->psl->tNumInsert++;
        pr->psl->tBaseInsert += (pr->psl->qStarts[iBlk] - prevEnd);
        }
    prevEnd = pr->psl->tStarts[iBlk-1] + pr->psl->blockSizes[iBlk-1];
    if (prevEnd < pr->psl->tStarts[iBlk])
        {
        pr->psl->qNumInsert++;
        pr->psl->qBaseInsert += (pr->psl->tStarts[iBlk] - prevEnd);
        }
    }
}

void convertAlign(struct parser *pr, FILE* out)
/* read and comvert an alignment to psl. */
{
struct ncbiTabAlign* rec;
int iBlk;

/* parser rows for this alignment */
while ((rec = loadNextRec(pr)) != NULL)
    {
    if (rec->alnId != pr->curAlnId)
        {
        /* next alignment, save it */
        pr->recCache = rec;
        break;
        }
    if (sameString(rec->type, "EXON"))
        addRec(pr, rec);
    ncbiTabAlignFree(&rec);
    }

/* finish psl */
iBlk = pr->psl->blockCount-1;

pr->psl->qStart = pr->psl->qStarts[0];
pr->psl->qEnd = pr->psl->qStarts[iBlk] + pr->psl->blockSizes[iBlk];
if (pr->psl->strand[0] == '-')
    reverseIntRange(&pr->psl->qStart, &pr->psl->qEnd, pr->psl->qSize);

pr->psl->tStart = pr->psl->tStarts[0];
pr->psl->tEnd = pr->psl->tStarts[iBlk] + pr->psl->blockSizes[iBlk];
if (pr->psl->strand[1] == '-')
    reverseIntRange(&pr->psl->tStart, &pr->psl->tEnd, pr->psl->tSize);

/* turn in BLAT style untranslated alignment  */
if (pr->psl->strand[1] == '-')
    {
    if (pr->psl->strand[0] == '-')
        parseErr(pr, "can't handle neg to neg alignments");
    pslRc(pr->psl);
    }
 pr->psl->strand[1] = '\0';
pslTabOut(pr->psl, out);
pslFree(&pr->psl);
}

void ncbiTabToPsl(struct alignSeqSizes *ass, char *inName, char *outName)
/* ncbiTabToPsl - Convert NCBI tab alignments to psl format. */
{
struct parser parser;
FILE *outFh;
ZeroVar(&parser);
parser.lf = lineFileOpen(inName, TRUE);
parser.ass = ass;
outFh = mustOpen(outName, "w");

while (nextAlign(&parser))
    convertAlign(&parser, outFh);


lineFileClose(&parser.lf);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct alignSeqSizes* ass;
optionInit(&argc, argv, optionSpec);
if (argc != 3)
    usage();
ass = alignSeqSizesNew(optionVal("qSizesFile", NULL),
                       optionVal("qSizes", NULL),
                       optionVal("tSizesFile", NULL),
                       optionVal("tSizes", NULL));
ncbiTabToPsl(ass, argv[1], argv[2]);
alignSeqSizesFree(&ass);
return 0;
}
