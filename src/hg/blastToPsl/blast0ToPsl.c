/* blast0ToPsl - convert blast 0 output to tabbed format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "blastTab.h"
#include "psl.h"

int lineCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blast0ToPsl - convert blast 0 output to tabbed format without gaps\n"
  "usage:\n"
  "   blast0ToPsl XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void mypslTabOut(struct blastTab *bt, FILE *out, FILE *scores)
{
static double lastScore = 0.0;
static char *lastName = NULL;
static unsigned qStarts[10000];
static unsigned tStarts[10000];
static unsigned blockSizes[10000];
char *saveString;
char buffer[200];
static struct psl psl;

if ((bt == NULL ) || ((lastName != NULL) && ((lastScore != bt->bitScore ) ||  !sameString(bt->query, lastName)  )))
    {
    pslTabOut(&psl, out);
    fprintf(scores, "%s\t%c\t%d\t%d\t%s\t%d\t%d\t%g\n", psl.tName,
	    psl.strand[1], psl.tStart, psl.tEnd,
	    psl.qName, psl.qStart, psl.qEnd,
	    lastScore);
    lastName = NULL;
    }
if (bt == NULL)
    return;

if (lastName == NULL)
    {
    memset(&psl, 0, sizeof(psl));
    psl.qStarts = qStarts;
    psl.tStarts = tStarts;
    psl.blockSizes = blockSizes;
    psl.tName = bt->target;
    psl.qName = bt->query;
    psl.strand[0] = '+';
    psl.strand[1] = (bt->tEnd > bt->tStart) ? '+' : '-';
    if (psl.strand[1] == '+')
    {
	psl.tStart = bt->tStart;
	psl.tEnd = bt->tEnd;
    }
    else
    {
	psl.tStart = bt->tEnd;
	psl.tEnd = bt->tStart;
    }
    psl.tStart--;
    psl.qStart = bt->qStart;
    psl.qEnd = bt->qEnd;
    }
psl.qStarts[psl.blockCount] = bt->qStart;
psl.tStarts[psl.blockCount] = bt->tStart - 1;
psl.blockSizes[psl.blockCount] = bt->qEnd - bt->qStart + 1;
psl.blockCount++;
lastName = bt->query;
lastScore = bt->bitScore;
}

void
myErr(char *string)
{
    char buffer[1000];
    sprintf(buffer,  "%s at %d",string,lineCount);
    errAbort(buffer);
}

void blast0ToPsl(char *inFile, char *outPsl, char *outScore)
/* blast0ToPsl - convert blast 0 output to tabbed format. */
{
struct lineFile *in = lineFileOpen(inFile, TRUE);
FILE *out = mustOpen(outPsl, "w");
FILE *scores = mustOpen(outScore, "w");
char *words[50];
int wordCount;
char *lastPtr, *ptr;
int qStart, qEnd, qPos;
int qGaps;
int tIncr;
int tStart, tEnd, tPos;
struct blastTab bt;
char *qString,*qPtr, *tPtr;
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

lineCount = 0;
while(wordCount = lineFileChopNext(in, words, 50))
    {
	lineCount++;
	if (sameString(words[0], "Reference:"))
	{

	    if (pending)
		{
		bt.qEnd = qStart - 1;
		bt.tEnd = tStart;// - tIncr;
		bt.aliLength = bt.qEnd - bt.qStart + 1;
		mypslTabOut(&bt, out, scores);
		}
	    if (state != SAFE_OUT)
		myErr("boop");
	    state = NEED_QUERY;
	}
	else if ((wordCount == 5) && sameString(words[0], "*****"))
	{
	    state = SAFE_OUT;
	}
	else if ((wordCount == 8) && sameString(words[0], "Score"))
	{
	    if (!((state == SAFE_OUT) || (state == GOT_TARGET)))
		myErr("need target or safeout before score");
	    if (pending)
	    {
			bt.qEnd = qStart - 1;
			bt.tEnd = tStart;// - tIncr;
			bt.aliLength = bt.qEnd - bt.qStart + 1;
		mypslTabOut(&bt, out, scores);
	    }
	    pending = TRUE;
	    state = GOT_SCORE;
	    bt.bitScore = atof(words[2]);
	    bt.eValue = atof(words[7]);
	//    printf("bitscore %g eValue %g\n",bt.bitScore, bt.eValue);
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
	    bt.identity = (100.0 * top) / bottom;

	    bt.gapOpen = 0;
	    qGaps = 0;
	    if (wordCount == 12)
		qGaps = atoi(words[10]);
	    bt.mismatch = bottom - top - qGaps;
	 //   printf("identity %g \n",bt.identity);
	}
	else if ((wordCount == 3) && sameString(words[0], "Frame"))
	{
	    if (words[2][0]== '+')
		tIncr = 3;
	    else
		tIncr = -3;
	}
	else if ((wordCount == 2) && sameString(words[0], "Query="))
	{
	    if (state != NEED_QUERY)
		myErr("got Query= and not ready");
	    bt.query = cloneString(words[1]);
	  //  printf("query %s\n",bt.query);
	    state = GOT_QUERY;
	    pending = FALSE;
	}
	else if ((wordCount == 1) && startsWith(">",words[0] ))
	{
	    if (!((state == GOT_QUERY) || (state == SAFE_OUT)))
		myErr("no query at '>'");
	    if ((state == SAFE_OUT) && pending)
		pending=FALSE,mypslTabOut(&bt, out, scores);

	    bt.target = cloneString(&words[0][1]);
	   // printf("target %s\n",bt.target);
	   state = GOT_TARGET;
	}
	else if ((wordCount ==4) && sameString(words[0], "Query:"))
	{

	    if (!((state == GOT_IDENT) || (state == SAFE_OUT)))
	    {printf("state %d\n",state);
		myErr("got second query");}
	    qStart = atoi(words[1]);
	    if (state == SAFE_OUT)
	    {
		//prinf("%d %d\n",qEnd, qStart);
		if (qStart != 1 + qEnd)
		    myErr("query continue");
		state = NEED_TARGET_CONT;
	    }
	    else
		state = NEED_TARGET;

	    qString = cloneString(words[2]);
	    qEnd = atoi(words[3]);
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
		//if (tStart != 1 + tEnd)
		 //   myErr("target continue");
		bt.qEnd = qEnd;
		bt.tEnd = tEnd;
		if (tStart != atoi(words[1]))
		{printf("%d %d\n",tStart,atoi(words[1]));
		    myErr("cont target not right");
		}
		tEnd = atoi(end);
	    }
	    else
	    {
	    tStart = atoi(words[1]);
	    tEnd = atoi(end);
		bt.qStart = qStart;
		bt.qEnd = qEnd;
		bt.tStart = tStart;
		bt.tEnd = tEnd;
	    }
	    qPtr = qString;
	    tPtr = seq;
	    if (strlen(qString) != strlen(seq))
		myErr("q and t string not same length");

	    while(*qPtr)
	    {
		if ((*qPtr == '-') || (*tPtr == '-'))
		{
		    if (pending)
		    {
			bt.qEnd = qStart - 1;
			bt.tEnd = tStart;// - tIncr;
			bt.aliLength = bt.qEnd - bt.qStart+1;
			mypslTabOut(&bt, out, scores);
		    //bt.qStart = qStart;
		    //bt.tStart = tStart;
		    }
		    pending = FALSE;
		}
		else if (!pending && ((*qPtr != '-') && (*tPtr != '-')))
		{
		    pending = TRUE;
		    bt.qStart = qStart;
		    bt.tStart = tStart;
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
if (pending)
		mypslTabOut(&bt, out, scores);
		mypslTabOut(NULL, out, scores);
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
