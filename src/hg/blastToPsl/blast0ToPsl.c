/* blast0ToPsl - convert blast 0 output to tabbed format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

int lineCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blast0ToPsl - convert blast 0 output to tabbed format without gaps\n"
  "usage:\n"
  "   blast0ToPsl in.blast out.psl out.score\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

#if 0
void mypslTabOut(struct blastTab *bt, FILE *out, FILE *scores, boolean flush)
{
static double lastScore = 0.0;
static char *lastName = NULL;
static unsigned qStarts[10000];
static unsigned tStarts[10000];
static unsigned blockSizes[10000];
char *saveString;
char buffer[200];
static struct psl psl;

printf("fliust %d lastName %x\n",flush, lastName);
if ((flush == TRUE   ) || ((lastName != NULL) && ((lastScore != bt->bitScore ) ||  !sameString(bt->query, lastName)  )))
    {
printf("flushing\n");
    pslTabOut(&psl, out);
    fprintf(scores, "%s\t%c\t%d\t%d\t%s\t%d\t%d\t%g\n", psl.tName,
	    psl.strand[1], psl.tStart, psl.tEnd,
	    psl.qName, psl.qStart, psl.qEnd,
	    lastScore);
    lastName = NULL;
    }

if (lastName == NULL)
    {
    memset(&psl, 0, sizeof(psl));
    psl.qStarts = qStarts;
    psl.tStarts = tStarts;
    psl.blockSizes = blockSizes;
    psl.tSize = tSize;
    psl.tName = bt->target;
    psl.qName = bt->query;
    psl.strand[0] = '+';
    psl.strand[1] = (bt->tEnd > bt->tStart) ? '+' : '-';
    if (psl.strand[1] == '+')
    {
	psl.tStart = bt->tStart - 1;
    }
    else
    {
	psl.tEnd =  (bt->tStart - 1);
    }
    psl.qStart = bt->qStart - 1;
    psl.qEnd = bt->qEnd;
    }
if (psl.strand[1] == '+')
{
	psl.tEnd = bt->tEnd - 1;
    psl.tStarts[psl.blockCount] = bt->tStart - 1;
    }
else
	{
	psl.tStart =  bt->tEnd - 1;
    psl.tStarts[psl.blockCount] = tSize - (bt->tStart - 1);
    }
    //if ( (bt->tEnd > bt->tStart) && (psl.strand[1] == '-'))
    	//abort();
psl.qEnd = bt->qEnd;
psl.qStarts[psl.blockCount] = bt->qStart - 1;
psl.blockSizes[psl.blockCount] = bt->qEnd - bt->qStart + 1;
psl.blockCount++;
lastName = bt->query;
lastScore = bt->bitScore;
}
#endif

void
myErr(char *string)
{
    char buffer[1000];
    sprintf(buffer,  "%s at %d",string,lineCount);
    errAbort(buffer);
}

void outPsl(double bitScore, struct psl *psl, FILE *out, FILE *scores)
{
if (psl->blockSizes[psl->blockCount] != 0)
    {
//Printf("flushing\n");
    psl->blockCount++;
    if (psl->strand[1] == '-')
    {
    	int temp = psl->tEnd;

    	psl->tEnd = psl->tStart;
	psl->tStart = temp;
    }

    psl->tStart--;
    pslTabOut(psl, out);
    fprintf(scores, "%s\t%c\t%d\t%d\t%s\t%d\t%d\t%g\n", psl->tName,
	    psl->strand[1], psl->tStart, psl->tEnd,
	    psl->qName, psl->qStart, psl->qEnd,
	    bitScore);
    psl->blockCount = 0;
    psl->blockSizes[psl->blockCount] = 0;
    }
}


void blast0ToPsl(char *inFile, char *outPslName, char *outScore)
/* blast0ToPsl - convert blast 0 output to tabbed format. */
{
int qIncr;
//int tSize;
struct lineFile *in = lineFileOpen(inFile, TRUE);
static unsigned qStarts[10000];
static unsigned tStarts[10000];
static unsigned blockSizes[10000];
FILE *out = mustOpen(outPslName, "w");
FILE *scores = mustOpen(outScore, "w");
char *words[50];
int wordCount;
char *lastPtr, *ptr;
int qStart, tStart;
//int qStart, qEnd, qPos;
//int qGaps;
int tIncr;
boolean tGap, qGap;
//int tStart, tEnd, tPos;
struct psl psl;
char *qString,*qPtr, *tPtr;
double bitScore;
#define SAFE_OUT	1
#define NEED_QUERY	2
#define GOT_QUERY	3
#define GOT_SCORE	4
#define GOT_TARGET	5
#define GOT_IDENT	6
#define NEED_TARGET	7
#define NEED_TARGET_CONT 8
int state = SAFE_OUT;
boolean pending = FALSE;

qGap = tGap = FALSE;
lineCount = 0;
memset(&psl, 0, sizeof(psl));
psl.qStarts = qStarts;
psl.tStarts = tStarts;
psl.blockSizes = blockSizes;
psl.blockSizes[0] = 0;
psl.strand[0] = '+';
while(wordCount = lineFileChopNext(in, words, 50))
    {
	lineCount++;
	if (sameString(words[0], "Reference:"))
	{
	    outPsl(bitScore, &psl, out, scores);

	    if (state != SAFE_OUT)
		myErr("boop");
	    state = NEED_QUERY;
	}
	else if ((wordCount == 3) && sameString(words[0], "Length"))
	{
	    psl.tSize = atoi(words[2]);
	}
	else if ((wordCount == 5) && sameString(words[0], "*****"))
	{
	    state = SAFE_OUT;
	}
	else if ((wordCount == 8) && sameString(words[0], "Score"))
	{
	    if (!((state == SAFE_OUT) || (state == GOT_TARGET)))
		myErr("need target or safeout before score");
	    outPsl(bitScore, &psl, out, scores);
	    state = GOT_SCORE;
	    bitScore = atof(words[2]);
//	    bt.eValue = atof(words[7]);
	}
	else if (((wordCount == 12) || (wordCount == 8)) && sameString(words[0], "Identities"))
	{
	    int top, bottom;

	    if (state != GOT_SCORE)
		myErr("Ident without score");

	    top = atoi(words[2]);
	    if ((ptr = strchr(words[2], '/')) != NULL)
		bottom = atoi(ptr+1);
	    else 
		myErr("beep");

	    state = GOT_IDENT;
	  //  bt.identity = (100.0 * top) / bottom;

	   // bt.gapOpen = 0;
	    //qGaps = 0;
	  //  if (wordCount == 12)
	//	qGaps = atoi(words[10]);
	 //   bt.mismatch = bottom - top - qGaps;
	 //   printf("identity %g \n",bt.identity);
	}
	else if ((wordCount == 3) && sameString(words[0], "Frame"))
	{
	    if (words[2][0]== '+')
		{
		tIncr = 3;
		qIncr = 1;
		}
	    else
		{
		tIncr = -3;
		qIncr = -1;
		}
	}
	else if ((wordCount == 2) && sameString(words[0], "Query="))
	{
	    if (state != NEED_QUERY)
		myErr("got Query= and not ready");
	    if (psl.qName != NULL)
	    	freeMem(psl.qName);
	    psl.qName = cloneString(words[1]);
	  //  printf("query %s\n",bt.query);
	    state = GOT_QUERY;
	    //pending = FALSE;
	}
	else if ((wordCount == 1) && startsWith(">",words[0] ))
	{
	    if (!((state == GOT_QUERY) || (state == SAFE_OUT)))
		myErr("no query at '>'");
	    outPsl(bitScore, &psl, out, scores);

	    if (psl.tName != NULL)
	    	freeMem(psl.tName);
	    psl.tName = cloneString(&words[0][1]);
	   // printf("target %s\n",bt.target);
	   state = GOT_TARGET;
	}
	else if ((wordCount ==4) && sameString(words[0], "Query:"))
	{

	    //pending = TRUE;
	    if (!((state == GOT_IDENT) || (state == SAFE_OUT)))
	    {printf("state %d\n",state);
		myErr("got second query");}
	    qStart = atoi(words[1]);
	    if (state == SAFE_OUT)
	    {
		//prinf("%d %d\n",qEnd, qStart);
		if (qStart != 1 + psl.qEnd)
		    myErr("query continue");
		state = NEED_TARGET_CONT;
	    }
	    else
	    {
		psl.qStart = qStart - 1;
		psl.blockCount = 0;
		psl.blockSizes[0] = 0;
		psl.qStarts[psl.blockCount] = psl.qStart;
		state = NEED_TARGET;
		}

	    qString = cloneString(words[2]);
	    psl.qEnd = atoi(words[3]);
	}
	else if (((wordCount ==4)|| (wordCount ==3)) && sameString(words[0], "Sbjct:"))
	{
	    char *seq, *end; 
	    end = words[3];
	    seq = words[2];
	    if (wordCount == 3)
	    {
		end = words[2];
		seq = words[1];
		while(isdigit(*seq))
		    seq++;
	    }

	    if (!((state == NEED_TARGET) || (state == NEED_TARGET_CONT)))
		myErr("don't need target");
	    if (state == NEED_TARGET_CONT)
	    {
		tStart = atoi(words[1]);
		//printf("tStart %d pslEnd %d\n",tStart, psl.tEnd);
		if (tStart != qIncr + psl.tEnd)
		    myErr("target continue");
		//bt.qEnd = qEnd;
		//bt.tEnd = tEnd;
		//if (psl.tStart != atoi(words[1]))
		//{printf("%d %d\n",psl.tStart,atoi(words[1]));
		    //myErr("cont target not right");
		//}
		psl.tEnd = atoi(end);
	    }
	    else
	    {
	    tStart = psl.tStart = atoi(words[1]);
	    psl.tEnd = atoi(end);
	    if (tIncr < 0)
		psl.tStarts[psl.blockCount] = psl.tSize - psl.tStart;
	    else
		psl.tStarts[psl.blockCount] =  psl.tStart - 1;
		//bt.qStart = qStart;
		//bt.qEnd = qEnd;
		//bt.tStart = tStart;
		//bt.tEnd = tEnd;
	    }
	    if (psl.tEnd > psl.tStart)
	    {
		psl.strand[1] = '+';
	    }
	    else
	    {
		int temp;

		psl.strand[1] = '-';
		//temp = psl.tEnd;
		//psl.tEnd = psl.tStart;
		//psl.tStart = temp;
	    }
	    qPtr = qString;
	    tPtr = seq;
	    if (strlen(qString) != strlen(seq))
		myErr("q and t string not same length");

	    //qStart = psl.qStart;
	    //tStart = psl.tStart;
	    while(*qPtr)
	    {
		if (*qPtr == '-') 
		{
		    if (!qGap)
			{
			//if (tIncr < 0)
			    //psl.tStarts[psl.blockCount] = psl.tSize - (tStart - 1);
			psl.blockCount++;
			psl.blockSizes[psl.blockCount] = 0;
			}
		    qGap = TRUE;
		}
		else if (*tPtr == '-')
		{
		    if (!tGap)
			{
			//if (tIncr < 0)
			    //psl.tStarts[psl.blockCount] = psl.tSize - (tStart - 1);
			psl.blockCount++;
			psl.blockSizes[psl.blockCount] = 0;
			}
		    tGap = TRUE;
		}
		else // if (((*qPtr != '-') && (*tPtr != '-')))
		{
		    if (qGap || tGap )
		    {
			psl.qStarts[psl.blockCount] = qStart - 1;
			if (tIncr < 0)
			    psl.tStarts[psl.blockCount] = psl.tSize - tStart;
			else
			    psl.tStarts[psl.blockCount] =  tStart - 1;
			}
		    qGap = tGap = FALSE;
		    psl.blockSizes[psl.blockCount]++;
		    //pending = TRUE;
		   // bt.qStart = qStart;
		   // bt.tStart = tStart;
		}

		if (*qPtr != '-')
		    qStart++;
		if (*tPtr != '-')
		    tStart += tIncr;
		qPtr++;
		tPtr++;
	    }
	    state = SAFE_OUT;
//	    printf("query: %d %s\n",qStart, qString);
//	    printf("sbjct: %d %s\n",tStart, tString);
	}
    }

    outPsl(bitScore, &psl, out, scores);

	/*
	    {
	    qPos = queryStart;
	    for (lastPtr=ptr= words[2]; ptr =strchr(ptr, '-');lastPtr = ptr)
	    {
		*ptr++ = 0;
		if ((ptr - lastPtr) > 1)
		{
		    printf("chop %d %s\n",qPos, lastPtr);
		    qPos += ptr - lastPtr - 1;
		}
	    }
	    if(lastPtr != ptr)
		    printf("chop %d %s\n",qPos, lastPtr);
	    }
	    */

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
blast0ToPsl(argv[1], argv[2], argv[3]);
return 0;
}
