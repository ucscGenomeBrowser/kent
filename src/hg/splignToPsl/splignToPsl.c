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
#include "splignAlign.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
    "splignToPsl - Convert splign tab-file pair alignments to PSL format\n"
    "\n"
    "usage:\n"
    "   splignToPsl [options] in out.psl\n"
    "\n"
    "Options:\n"
    "   -qSizesFile=szfile - read query sizes from tab file of <seqName> <size>\n"
    "   -qSizes='id1 size1 ..' - set query sizes, value is white-space separated pairs of sequence and size\n"
    "   -tSizesFile=szfile - read target sizes from tab file of <seqName> <size>\n"
    "   -tSizes='id1 size1 ..' - set target sizes, value is white-space separated pairs of sequence and size\n"
    "\n"
    "alignment format is:\n"
    "        1  query name\n"
    "        2  target name\n"
    "        3  ids\n"
    "        4  aligned block overall size. Including indel and misMatch\n"
    "        5  query start\n"
    "        6  query end\n"
    "        7  target start\n"
    "        8  target end\n"
    "        9  block type. exon, gap \n"
    "        10 alignment anontation. M-match, R-misMatch, I-insert, D-delete\n"
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
    struct splignAlign *recCache;  /* record catch */
    unsigned pslMaxBlks;            /* memory allocated for PSL blocks */
    char* curQName;              /* current alignment id */
    int curQEnd;            /* query end of the current alignment */
};

void parseErr(struct parser *pr, char* format, ...)
/* prototype to get gnu attribute check */
#if defined(__GNUC__)
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

struct splignAlign *loadNextRec(struct parser *pr)
/* read the next record, if one is cached, return it instead. NULL on EOF */
{
struct splignAlign *rec = NULL;
if (pr->recCache != NULL)
    {
    rec = pr->recCache;
    pr->recCache = NULL;
    }
else
    {
    char *row[SPLIGNALIGN_NUM_COLS];
    if (lineFileChopNextTab(pr->lf, row, ArraySize(row)))
        rec = splignAlignLoad(row);
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
struct splignAlign *rec = loadNextRec(pr);
if (rec == NULL)
    return FALSE;

AllocVar(pr->psl);
growPslBlocks(pr, 8);
pr->psl->qName = cloneString(rec->query);
pr->psl->qSize = alignSeqSizesMustGetQuery(pr->ass, rec->query);
pr->psl->tName = cloneString(rec->target);
pr->psl->tSize = alignSeqSizesMustGetTarget(pr->ass, rec->target);

/* strand is negative if addressed are reversed */
pr->psl->strand[0] = (rec->qStart > rec->qEnd) ? '-' : '+';
pr->psl->strand[1] = (rec->tStart > rec->tEnd) ? '-' : '+';

pr->curQName = cloneString(rec->query); //* need end to be saved
pr->curQEnd = 0;
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

void addBlock(struct parser *pr,int  *qStart, int *tStart, int size, int match)
{
int iBlk = pr->psl->blockCount;

if (iBlk >= pr->pslMaxBlks)
    growPslBlocks(pr, 2*pr->pslMaxBlks);

pr->psl->blockSizes[iBlk] = size;
pr->psl->blockCount++;

/* get new coords */
convertCoords(pr, pr->psl->strand[0], *qStart, *qStart + size,
	      pr->psl->qSize, &(pr->psl->qStarts[iBlk]));

if(pr->psl->strand[1] == '-')
    convertCoords(pr, pr->psl->strand[1], *tStart, *tStart - size,
		  pr->psl->tSize, &(pr->psl->tStarts[iBlk]));
else
    convertCoords(pr, pr->psl->strand[1], *tStart, *tStart + size,
                  pr->psl->tSize, &(pr->psl->tStarts[iBlk]));

/* update stats */

pr->psl->match += match;
pr->psl->misMatch += (size - match);
if(pr->psl->strand[1] == '-')
  {
  pr->psl->tStarts[iBlk] = *tStart - size;
  *tStart -= size;
  }
else
  {
    pr->psl->tStarts[iBlk] = *tStart;
    *tStart += size ;
  }
pr->psl->qStarts[iBlk] = *qStart;
*qStart += size ;

}


void addRec(struct parser *pr, struct splignAlign* rec)
/* add a record to the psl. It probably multi blocks for psl */
{
int size=0, match=0, insert=0, delete=0; 
int qStart= rec->qStart -1, 
    tStart=pr->psl->strand[1]== '+'? rec->tStart - 1:rec->tStart;
int anonSize = strlen(rec->anotation);
int i =0;
int anoStage = 0; /*anotation stage, 1-M, 2-R, 3-I, 4-D, 5-already get number for M,R stage */

int shift = 1, shiftBack = -1;

if(pr->psl->strand[1] == '-')
    {/* get shift amount */
    shift = -1;
    shiftBack = 1;
    }


for(i=0; i < anonSize; i++)
    {
    if(rec->anotation[i] == 'M')
	{/*is in a new match blocks */
	
	/* check previous stage and shift start*/
	if(anoStage == 3)
	    {
	    tStart += insert;
	    insert = 0;
	    }
	if(anoStage == 4)
	    {
	    qStart += delete;
	    delete = 0;
	    }
	size++;
	match++;
	anoStage = 1;
	continue;
	}
    else if(rec->anotation[i] == 'R')
	{/*is in a new misMatch Block */
	
	/* check previous stage and shift start*/
	if(anoStage == 3)
	    {
	    tStart += insert;
	    insert = 0;
	    }
	if(anoStage == 4)
	    {
	    qStart += delete;
	    delete = 0;
	    }
	size++;
	anoStage = 2;
	continue;
	}
    else if(rec->anotation[i] == 'I' )
	{/* Is it insert? Add a new block to psl if it is. */
	if (anoStage != 0)
	    addBlock(pr, &qStart, &tStart, size, match);
	anoStage = 3;
	size = 0;
	match = 0;
	insert += shift;
	continue;
	}
    else if( rec->anotation[i] == 'D')
	{/* Is it delete?  Add a new block to psl if it is. */
	if (anoStage != 0)
	    addBlock(pr, &qStart, &tStart, size, match);
	anoStage = 4;
	size = 0;
	match = 0;
	delete += shift;
	continue;
	}
    
    if(insert > 1|| delete > 1|| anoStage == 5)
	/* check whether get fragment size */
	continue;
    else
	{
	int tempSize = atoi(&(rec->anotation[i]));
	switch(anoStage)
	    {
	    case 1:	//match block
		size += (tempSize - 1);
		match += (tempSize - 1);
		anoStage = 5;
		break;
	    case 2: // misMatch block
		size += (tempSize - 1);
		anoStage = 5;
		break;
	    case 3: // insert in target 
		insert = insert + shiftBack + tempSize;
		break;
	    case 4: // delete in target
		delete = delete + shiftBack + tempSize;
		break;
	    case 5:
		continue;
	    }
	}
    }

// add the last block
if(anoStage == 5 || anoStage == 1 || anoStage == 2)
    addBlock(pr, &qStart, &tStart, size, match);
}


void pslRcForSplign(struct psl *psl)
{
int i;

/* swap strand, forcing target to have an explict strand */
psl->strand[0] = (psl->strand[0] != '-') ? '-' : '+';
psl->strand[1] = (psl->strand[1] != '-') ? '-' : '+';

for (i = 0; i < psl->blockCount; i++)
    {
    psl->qStarts[i] = psl->qSize - (psl->qStarts[i] + psl->blockSizes[i]);
    }
reverseUnsigned(psl->qStarts, psl->blockCount);
reverseUnsigned(psl->blockSizes, psl->blockCount);
}




void convertAlign(struct parser *pr, FILE* out)
/* read and comvert an alignment to psl. */
{
struct splignAlign* rec;
int iBlk, i;
boolean hasExon = FALSE;

/* parser rows for this alignment */
while ((rec = loadNextRec(pr)) != NULL)
    {
    if (!sameString(rec->query, pr->curQName) || 
	pr->curQEnd > rec->qStart )
        {/* check whether this is same alignment. Next alignment, save it */
        pr->curQName = rec->query;
	pr->curQEnd = rec->qEnd; 
	pr->recCache = rec;
	break;
        }
    pr->curQEnd = rec->qEnd; /* record current query end for future check*/
    if (stringIn("exon", rec->type)) /* check whether is an exon */ 
	{
        addRec(pr, rec);
	hasExon=TRUE;
	}
    splignAlignFree(&rec);
    }

if(hasExon)
    {/* finish psl */
    iBlk = pr->psl->blockCount-1;
    
    pr->psl->qStart = pr->psl->qStarts[0];
    pr->psl->qEnd = pr->psl->qStarts[iBlk] + pr->psl->blockSizes[iBlk];
    if (pr->psl->strand[0] == '-')
	reverseIntRange(&pr->psl->qStart, &pr->psl->qEnd, pr->psl->qSize);
    
    pr->psl->tStart = pr->psl->tStarts[0];
    pr->psl->tEnd = pr->psl->tStarts[iBlk] + pr->psl->blockSizes[iBlk];
    if (pr->psl->strand[1] == '-')
	{
	pr->psl->tEnd = pr->psl->tStarts[0] + pr->psl->blockSizes[0];
	pr->psl->tStart = pr->psl->tStarts[iBlk];
	reverseInts(pr->psl->tStarts, pr->psl->blockCount);
	}
    
    /* turn in BLAT style untranslated alignment  */
    if (pr->psl->strand[1] == '-')
	{
    if (pr->psl->strand[0] == '-')
        parseErr(pr, "can't handle neg to neg alignments");
    pslRcForSplign(pr->psl);
	}
    
    /* count gap info */
    for(i = 1; i < pr->psl->blockCount; i++)
	{
	if(pr->psl->qStarts[i] > pr->psl->qStarts[i-1] + pr->psl->blockSizes[i-1])
	    {
	    pr->psl->qNumInsert++;
	    pr->psl->qBaseInsert += (pr->psl->qStarts[i] - 
				     pr->psl->qStarts[i-1] - pr->psl->blockSizes[i-1]);
	    }
	if(pr->psl->tStarts[i] > pr->psl->tStarts[i-1] + pr->psl->blockSizes[i-1])
	    {
	    pr->psl->tNumInsert++;
	    pr->psl->tBaseInsert += (pr->psl->tStarts[i] -
				     pr->psl->tStarts[i-1]  - pr->psl->blockSizes[i-1]);
	    }
	}
    
    pr->psl->strand[1] = '\0';
    pslTabOut(pr->psl, out);
    }

pslFree(&pr->psl);
}


void splignToPsl(struct alignSeqSizes *ass, char *inName, char *outName)
/* splignToPsl - Convert NCBI tab splign alignments to psl format. */
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
splignToPsl(ass, argv[1], argv[2]);
alignSeqSizesFree(&ass);
return 0;
}
