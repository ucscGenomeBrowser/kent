/* phToPsl - Convert from Pattern Hunter to PSL format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "phToPsl - Convert from Pattern Hunter to PSL format\n"
  "usage:\n"
  "   phToPsl in.ph out.psl\n"
  "options:\n"
  "   -tName=target  (defaults to 'in')\n"
  );
}

char *tName; /* Name of target side of alignment. */
boolean psl = TRUE;	/* Output PSL format. */

void aliStringToPsl(char *qName, char *tName, 
	char *qString, char *tString, int size, 
	int qStart, int tStart, int qIsRc, FILE *f)
/* Output alignment in a pair of strings with insert chars
 * to a psl line in a file. */
{
unsigned match = 0;	/* Number of bases that match */
unsigned misMatch = 0;	/* Number of bases that don't match */
unsigned qNumInsert = 0;	/* Number of inserts in query */
int qBaseInsert = 0;	/* Number of bases inserted in query */
unsigned tNumInsert = 0;	/* Number of inserts in target */
int tBaseInsert = 0;	/* Number of bases inserted in target */
boolean qInInsert = FALSE; /* True if in insert state on query. */
boolean tInInsert = FALSE; /* True if in insert state on target. */
boolean eitherInsert = FALSE;	/* True if either in insert state. */
int blockCount = 1, blockIx=0;
int i;
char q,t;
int qs = qStart,ts = tStart,qe = qStart,te = tStart; /* Start/end of block */
int *blocks = NULL, *qStarts = NULL, *tStarts = NULL;

uglyf("alignStringToPsl\n");
uglyf("%s\n%s\n", qString, tString);
uglyf("size %d, qStart %d, tStart %d, qIsRc %d\n", size, qStart, tStart, qIsRc);

/* First count up number of blocks and inserts. */
for (i=0; i<size; ++i)
    {
    q = qString[i];
    t = tString[i];
    if (q == '-' || t == '-')
        {
	if (!eitherInsert)
	    {
	    ++blockCount;
	    eitherInsert = TRUE;
	    }
	if (t == '-')
	    {
	    if (!qInInsert)
	        {
		++qNumInsert;
		qInInsert = TRUE;
		}
	    ++qBaseInsert;
	    }
	else
	    qInInsert = FALSE;
	if (q == '-')
	    {
	    if (!tInInsert)
	        {
		++tNumInsert;
		tInInsert = TRUE;
		}
	    ++tBaseInsert;
	    }
	else
	    tInInsert = FALSE;

	}
    else
        {
	eitherInsert = FALSE;
	if (q == t)
	    ++match;
	else
	    ++misMatch;
	}
    }

/* Output header */
fprintf(f, "%d\t", match);
fprintf(f, "%d\t", misMatch);
fprintf(f, "0\t");
fprintf(f, "0\t");
fprintf(f, "%d\t", qNumInsert);
fprintf(f, "%d\t", qBaseInsert);
fprintf(f, "%d\t", tNumInsert);
fprintf(f, "%d\t", tBaseInsert);
fprintf(f, "%s\t", (qIsRc ? "-" : "+"));
fprintf(f, "%s\t", qName);
fprintf(f, "~~~\t");
fprintf(f, "%d\t", qStart);
fprintf(f, "%d\t", qStart + qBaseInsert + match + misMatch);
fprintf(f, "%s\t", tName);
fprintf(f, "~~~\t");
fprintf(f, "%d\t", tStart);
fprintf(f, "%d\t", tStart + tBaseInsert + match + misMatch);
fprintf(f, "%d\t", blockCount);
if (ferror(f))
    {
    perror("Error writing psl file\n");
    errAbort("\n");
    }

/* Allocate dynamic memory for block lists. */
AllocArray(blocks, blockCount);
AllocArray(qStarts, blockCount);
AllocArray(tStarts, blockCount);

/* Figure block sizes and starts. */
eitherInsert = FALSE;
for (i=0; i<size; ++i)
    {
    q = qString[i];
    t = tString[i];
    if (q == '-' || t == '-')
        {
	if (!eitherInsert)
	    {
	    blocks[blockIx] = qe - qs;
	    qStarts[blockIx] = qs;
	    tStarts[blockIx] = ts;
	    ++blockIx;
	    eitherInsert = TRUE;
	    }
	if (q != '-')
	   qe += 1;
	if (t != '-')
	   te += 1;
	}
    else
        {
	if (eitherInsert)
	    {
	    qs = qe;
	    ts = te;
	    eitherInsert = FALSE;
	    }
	qe += 1;
	te += 1;
	}
    }
uglyf("blockIx = %d, blockCount = %d\n", blockIx, blockCount);
assert(blockIx == blockCount-1);
blocks[blockIx] = qe - qs;
qStarts[blockIx] = qs;
tStarts[blockIx] = ts;

/* Output block sizes */
for (i=0; i<blockCount; ++i)
    fprintf(f, "%d,", blocks[i]);
fprintf(f, "\t");

/* Output qStarts */
for (i=0; i<blockCount; ++i)
    fprintf(f, "%d,", qStarts[i]);
fprintf(f, "\t");

/* Output tStarts */
for (i=0; i<blockCount; ++i)
    fprintf(f, "%d,", tStarts[i]);
fprintf(f, "\n");

/* Clean Up. */
freez(&blocks);
freez(&qStarts);
freez(&tStarts);
}

boolean convertOneAli(struct lineFile *lf, char *qName, char *tName, FILE *f)
/* Convert one pattern hunter alignment to one psl line. */
{
char c, *line, *parts[16];
int partCount;
int qStart, tStart;
boolean isRc = FALSE;
struct dyString *q = newDyString(4096);
struct dyString *t = newDyString(4096);

/* Grab first line and extract starting position from it. */
if (!lineFileNext(lf, &line, NULL))
    return FALSE;
if (!isdigit(line[0]))
    {
    lineFileReuse(lf);
    return FALSE;
    }
partCount = chopString(line, ":(,)<>", parts, ArraySize(parts));
if (partCount < 6)
    errAbort("Can't deal with format of Pattern Hunter file %s line %d",
    	lf->fileName, lf->lineIx);
qStart = lineFileNeedNum(lf, parts, 1);
if (qStart < 0)
    {
    isRc = TRUE;
    qStart = -qStart;
    }
tStart = lineFileNeedNum(lf, parts, 2);

/* Loop through rest of file 4 lines at a time, appending
 * sequence to q and t strings. */
for (;;)
    {
    /* Get next line and check to see if we are at end of
     * data we want to parse. Append it to q. */
    if (!lineFileNext(lf, &line, NULL))
        break;
    c = line[0];
    if (!isalpha(c))
        {
	lineFileReuse(lf);
	break;
	}
    dyStringAppend(q, line);

    /* Skip line with match/mismatch */
    lineFileNeedNext(lf, &line, NULL);

    /* Append next line to t. */
    lineFileNeedNext(lf, &line, NULL);
    dyStringAppend(t, line);

    /* Skip blank line. */
    lineFileNext(lf, &line, NULL);
    }

/* Do some sanity checks and then output. */
if (q->stringSize != t->stringSize)
    {
    errAbort("Alignment strings not same size line %d of %s",
    	lf->lineIx, lf->fileName);
    }
if (q->stringSize <= 0)
    errAbort("Empty alignment string line %d of %s", lf->lineIx, lf->fileName);
aliStringToPsl(qName, tName, q->string, t->string, 
	q->stringSize, qStart-1, tStart-1, isRc, f);

/* Clean up time. */
freeDyString(&q);
freeDyString(&t);
return TRUE;
}

boolean convertOneQuery(struct lineFile *lf, FILE *f)
/* Convert pattern hunter alignments on one query
 * to psl */
{
char *line, *word;
char *qName;
struct block *blockList = NULL;

/* Grab first line and extract query name from it. */
if (!lineFileNext(lf, &line, NULL))
    return FALSE;
if (line[0] != '>')
    errAbort("Expecting line starting with '>' line %d of %s", 
    	lf->lineIx, lf->fileName);
line += 1;
word = nextWord(&line);
if (word == NULL)
    errAbort("Expecting sequence name after '>' line %d of %s",
        lf->lineIx, lf->fileName);
qName = cloneString(word);

/* Loop through alignments between this query and target. */
while (convertOneAli(lf, qName, tName, f))
    ;

freez(&qName);
return TRUE;
}

void phToPsl(char *inName, char *outName)
/* phToPsl - Convert from Pattern Hunter to PSL format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char dir[256], root[128], ext[64];

splitPath(inName, dir, root, ext);
tName = optionVal("tName", root);
uglyf("tName = %s\n", tName);

pslWriteHead(f);
while (convertOneQuery(lf, f))
    ;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
phToPsl(argv[1], argv[2]);
return 0;
}
