/* phToPsl - Convert from Pattern Hunter to PSL format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "psl.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "phToPsl - Convert from Pattern Hunter to PSL format\n"
  "usage:\n"
  "   phToPsl in.ph qSizes tSizes out.psl\n"
  "options:\n"
  "   -tName=target  (defaults to 'in')\n"
  );
}

char *tName; /* Name of target side of alignment. */
boolean psl = TRUE;	/* Output PSL format. */

struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
	hashAdd(hash, name, intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

void countInserts(char *s, int size, unsigned *retNumInsert, int *retBaseInsert)
/* Count up number and total size of inserts in s. */
{
char c, lastC = s[0];
int i;
int baseInsert = 0, numInsert = 0;
if (lastC == '-')
    errAbort("%s starts with -", s);
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == '-')
        {
	if (lastC != '-')
	     numInsert += 1;
	baseInsert += 1;
	}
    lastC = c;
    }
*retNumInsert = numInsert;
*retBaseInsert = baseInsert;
}

void aliStringToPsl(struct lineFile *lf, char *qName, char *tName, 
	char *qString, char *tString, 
	int qSize, int tSize, int aliSize, 
	int qStart, int tStart, FILE *f)
/* Output alignment in a pair of strings with insert chars
 * to a psl line in a file. */
{
unsigned match = 0;	/* Number of bases that match */
unsigned misMatch = 0;	/* Number of bases that don't match */
unsigned qNumInsert = 0;	/* Number of inserts in query */
int qBaseInsert = 0;	/* Number of bases inserted in query */
unsigned tNumInsert = 0;	/* Number of inserts in target */
int tBaseInsert = 0;	/* Number of bases inserted in target */
boolean eitherInsert = FALSE;	/* True if either in insert state. */
int blockCount = 1, blockIx=0;
boolean qIsRc = FALSE;
char strand;
int i;
char q,t;
int qs,qe,ts,te;
int *blocks = NULL, *qStarts = NULL, *tStarts = NULL;


/* Deal with minus strand issues. */
if (tStart < 0)
    errAbort("Can't handle minus target strand line %d of %s",
    	lf->lineIx, lf->fileName);
if (qStart < 0)
    {
    qStart = qSize + qStart;
    qIsRc = TRUE;
    }

/* First count up number of blocks and inserts. */
countInserts(qString, aliSize, &qNumInsert, &qBaseInsert);
countInserts(tString, aliSize, &tNumInsert, &tBaseInsert);
blockCount = 1 + qNumInsert + tNumInsert;

/* Count up match/mismatch. */
for (i=0; i<aliSize; ++i)
    {
    q = qString[i];
    t = tString[i];
    if (q == t)
	++match;
    else
	++misMatch;
    }

/* Deal with minus strand. */
qs = qStart;
qe = qs + qBaseInsert + match + misMatch;
strand = '+';
if (qIsRc)
    {
    strand = '-';
    reverseIntRange(&qs, &qe, qSize);
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
fprintf(f, "%c\t", strand);
fprintf(f, "%s\t", qName);
fprintf(f, "%d\t", qSize);
fprintf(f, "%d\t", qs);
fprintf(f, "%d\t", qe);
fprintf(f, "%s\t", tName);
fprintf(f, "%d\t", tSize);
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
qs = qe = qStart;
ts = te = tStart;
for (i=0; i<aliSize; ++i)
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
	else if (i > 0)
	    {
	    /* Handle cases like
	     *     aca---gtagtg
	     *     acacag---gtg
	     */
	    if ((q == '-' && tString[i-1] == '-') || 
	    	(t == '-' && qString[i-1] == '-'))
	        {
		blocks[blockIx] = 0;
		qStarts[blockIx] = qe;
		tStarts[blockIx] = te;
		++blockIx;
		}
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

boolean convertOneAli(struct lineFile *lf, char *qName, char *tName, 
	int qSize, int tSize, FILE *f)
/* Convert one pattern hunter alignment to one psl line. */
{
char c, *line, *parts[16];
int partCount;
int qStart, tStart;
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
    if (c != '-' && !isalpha(c))
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
aliStringToPsl(lf, qName, tName, q->string, t->string, qSize, tSize,
	q->stringSize, qStart, tStart, f);

/* Clean up time. */
freeDyString(&q);
freeDyString(&t);
return TRUE;
}

boolean convertOneQuery(struct lineFile *lf, 
	struct hash *qSizeHash, struct hash *tSizeHash, FILE *f)
/* Convert pattern hunter alignments on one query
 * to psl */
{
char *line, *word;
char *qName;
int qSize, tSize;

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

/* Figure out sizes. */
qSize = findSize(qSizeHash, qName);
tSize = findSize(tSizeHash, tName);

/* Loop through alignments between this query and target. */
while (convertOneAli(lf, qName, tName, qSize, tSize, f))
    ;

freez(&qName);
return TRUE;
}

void phToPsl(char *inName, char *qSizes, char *tSizes, char *outName)
/* phToPsl - Convert from Pattern Hunter to PSL format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char dir[256], root[128], ext[64];
struct hash *qSizeHash = readSizes(qSizes);
struct hash *tSizeHash = readSizes(tSizes);

splitPath(inName, dir, root, ext);
tName = optionVal("tName", root);

pslWriteHead(f);
while (convertOneQuery(lf, qSizeHash, tSizeHash, f))
    ;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
phToPsl(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
