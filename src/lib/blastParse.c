/* blastParse - read in blast output into C data structure. */

#include "common.h"
#include "dystring.h"
#include "linefile.h"
#include "blastParse.h"

struct blastFile *blastFileReadAll(char *fileName)
/* Read all blast alignment in file. */
{
struct blastFile *bf;
struct blastQuery *bq;

bf = blastFileOpenVerify(fileName);
while ((bq = blastFileNextQuery(bf)) != NULL)
    {
    slAddHead(&bf->queries, bq);
    }
slReverse(&bf->queries);
lineFileClose(&bf->lf);
return bf;
}

static void bfError(struct blastFile *bf, char *message)
/* Print blast file error message. */
{
errAbort("%s\nLine %d of %s", message, 
	bf->lf->lineIx, bf->fileName);
}

static void bfSyntax(struct blastFile *bf)
/* General error message. */
{
bfError(bf, "Can't cope with BLAST output syntax");
}

static void bfNeedNextLine(struct blastFile *bf, char **retLine, int *retLineSize)
/* Fetch next line of input or die trying. */
{
struct lineFile *lf = bf->lf;
if (!lineFileNext(lf, retLine, retLineSize))
    errAbort("Unexpected end of file in %s\n", bf->fileName);
}

static void bfBadHeader(struct blastFile *bf)
/* Report bad header. */
{
bfError(bf, "Bad first line\nShould be something like:\n"
            "BLASTN 2.0.11 [Jan-20-2000]");
}

struct blastFile *blastFileOpenVerify(char *fileName)
/* Open file, read and verify header. */
{
struct blastFile *bf;
char *line;
int lineSize;
char *words[16];
int wordCount;
struct lineFile *lf;

AllocVar(bf);
bf->lf = lf = lineFileOpen(fileName, TRUE);
bf->fileName = cloneString(fileName);

/* Parse first line - something like: */
bfNeedNextLine(bf, &line, &lineSize);
wordCount = chopLine(line, words);
if (wordCount < 3)
    bfBadHeader(bf);
bf->program = cloneString(words[0]);
bf->version = cloneString(words[1]);
bf->buildDate = cloneString(words[2]);
if (!wildMatch("*BLAST*", bf->program))
    bfBadHeader(bf);
if (!isdigit(bf->version[0]))
    bfBadHeader(bf);
if (bf->buildDate[0] != '[')
    bfBadHeader(bf);
return bf;
}

void decomma(char *s)
/* Remove commas from string. */
{
char *d = s;
char c;

for (;;)
    {
    c = *s++;
    if (c != ',')
	*d++ = c;
    if (c == 0)
	break;
    }
}

struct blastQuery *blastFileNextQuery(struct blastFile *bf)
/* Read all alignments associated with next query.  Return NULL at EOF. */
{
struct lineFile *lf = bf->lf;
char *line;
int lineSize;
char *words[16];
int wordCount;
struct blastQuery *bq;
char *s, *e;
struct blastGappedAli *bga;

for (;;)
    {
    if (!lineFileNext(lf, &line, &lineSize))
	return NULL;
    if (startsWith("Query=", line))
	break;
    }

/* Process something like:
 *    Query= MM39H11    00630     
 */
wordCount = chopLine(line, words);
if (wordCount < 2)
    bfError(bf, "No sequence name in query line");
AllocVar(bq);
bq->query = cloneString(words[1]);

/* Process something like:
 *   (45,693 letters)
 */
for (;;)
    {
    bfNeedNextLine(bf, &line, &lineSize);
    s = skipLeadingSpaces(line);
    if (s[0] == '(')
	break;
    }
if (!isdigit(s[1]))
    {
    bfError(bf, "expecting something like:\n"
                "   (45,693 letters)");
    }
s += 1;
if ((e = strchr(s, ' ')) == NULL)
    {
    bfError(bf, "expecting something like:\n"
                "   (45,693 letters)");
    }
*e = 0;
decomma(s);
bq->queryBaseCount = atoi(s);
uglyf("Query %s has %d bases\n", bq->query, bq->queryBaseCount);


/* Seek until get something like:
 * Database: celegans98
 * and process it. */
for (;;)
    {
    bfNeedNextLine(bf, &line, &lineSize);
    if (startsWith("Database:", line))
	break;
    }
wordCount = chopLine(line, words);
if (wordCount < 2)
    bfError(bf, "Expecting database name");
bq->database = cloneString(words[1]);

/* Process something like:
 *        977 sequences; 95,550,797 total letters
 */
bfNeedNextLine(bf, &line, &lineSize);
wordCount = chopLine(line, words);
if (wordCount < 3 || !isdigit(words[0][0]) || !isdigit(words[2][0]))
    bfError(bf, "Expecting database info");
decomma(words[0]);
decomma(words[2]);
bq->dbSeqCount = atoi(words[0]);
bq->dbBaseCount = atoi(words[2]);
    

/* Seek to beginning of first gapped alignment. */
for (;;)
    {
    bfNeedNextLine(bf, &line, &lineSize);
    if (line[0] == '>')
	{
	lineFileReuse(lf);
	break;
	}
    }

/* Read in gapped alignments. */
while ((bga = blastFileNextGapped(bf, bq)) != NULL)
    {
    slAddHead(&bq->gapped, bga);
    }
slReverse(&bq->gapped);
return bq;
}

struct blastGappedAli *blastFileNextGapped(struct blastFile *bf, struct blastQuery *bq)
/* Read in next gapped alignment.   Does *not* put it on bf->gapped list. 
 * Return NULL at EOF or end of query. */
{
struct lineFile *lf = bf->lf;
char *line;
int lineSize;
char *words[16];
int wordCount;
struct blastGappedAli *bga;
struct blastBlock *bb;

AllocVar(bga);
bga->query = bq;

/* First line should be query. */
if (!lineFileNext(lf, &line, &lineSize))
    return NULL;
if (startsWith("  Database:", line))
    return NULL;
if (line[0] != '>')
    bfError(bf, "Expecting >target");
bga->targetName = cloneString(line+1); 

/* Process something like:
 *      Length = 100000
 */
bfNeedNextLine(bf, &line, &lineSize);
wordCount = chopLine(line, words);
if (wordCount < 3 || !isdigit(words[2][0]))
    bfError(bf, "Expecting length");
decomma(words[2]);
bga->targetSize = atoi(words[2]);

/* Get all the blocks. */
while ((bb = blastFileNextBlock(bf, bq, bga)) != NULL)
    {
    slAddHead(&bga->blocks, bb);
    }
slReverse(&bga->blocks);
return bga;
}

static int getStrand(struct blastFile *bf, char *strand)
/* Translate "Plus" or "Minus" to +1 or -1. */
{
if (sameWord("Plus", strand))
    return 1;
else if (sameWord("Minus", strand))
    return -1;
else
    {
    bfError(bf, "Expecting Plus or Minus after Strand");
    return 0;
    }
}

static boolean nextBlockLine(struct blastFile *bf, char **retLine)
/* Get next block line.  Return FALSE and reuse line if it's
 * an end of block type line. */
{
struct lineFile *lf = bf->lf;
char *line;
int lineSize;

if (!lineFileNext(lf, retLine, &lineSize))
    return FALSE;
line = *retLine;
if (line[0] == '>' || startsWith("Query=", line) || startsWith("  Database:", line))
    {
    lineFileReuse(lf);
    return FALSE;
    }
return TRUE;
}

double evalToDouble(char *s)
/* Convert string from e-val to floating point rep.
 * e-val is basically ascii floating point, but
 * small ones may be 'e-100' instead of 1.0e-100
 */
{
char buf[64];
if (isdigit(s[0]))
    return atof(s);
else
    {
    sprintf(buf, "1.0%s", s);
    return atof(buf);
    }
}

struct blastBlock *blastFileNextBlock(struct blastFile *bf, 
	struct blastQuery *bq, struct blastGappedAli *bga)
/* Read in next blast block.  Return NULL at EOF or end of
 * gapped alignment. */
{
struct lineFile *lf = bf->lf;
struct blastBlock *bb;
char *line;
int lineSize;
char *words[16];
int wordCount;
char *parts[3];
int partCount;
static struct dyString *qString = NULL, *tString = NULL;

/* Seek until get something like:
 *   Score = 8770 bits (4424), Expect = 0.0
 * or something that looks like we're done with this gapped
 * alignment. */
for (;;)
    {
    if (!nextBlockLine(bf, &line))
	return NULL;
    if (startsWith(" Score", line))
	break;
    }
AllocVar(bb);
wordCount = chopLine(line, words);
if (wordCount < 8 || !sameWord("Score", words[0]) 
    || !isdigit(words[2][0]) || !(isdigit(words[7][0]) || words[7][0] == 'e')
    || !sameWord("Expect", words[5]))
    {
    bfError(bf, "Expecting something like:\n"
             "Score = 8770 bits (4424), Expect = 0.0");
    }
bb->bitScore = atoi(words[2]);
bb->eVal = evalToDouble(words[7]);

/* Process something like:
 *   Identities = 8320/9618 (86%), Gaps = 3/9618 (0%)
 *             or
 *   Identities = 8320/9618 (86%)
 */
bfNeedNextLine(bf, &line, &lineSize);
wordCount = chopLine(line, words);
if (wordCount < 3 || !sameWord("Identities", words[0]))
    bfError(bf, "Expecting identity count");
partCount = chopByChar(words[2], '/', parts, ArraySize(parts));
if (partCount != 2 || !isdigit(parts[0][0]) || !isdigit(parts[1][0]))
    bfSyntax(bf);
bb->matchCount = atoi(parts[0]);
bb->totalCount = atoi(parts[1]);
if (wordCount >= 7 && sameWord("Gaps", words[4]))
    {
    if (!isdigit(words[6][0]))
	bfSyntax(bf);
    bb->insertCount = atoi(words[6]);
    }

/* Process something like:
 *     Strand = Plus / Plus
 */
bfNeedNextLine(bf, &line, &lineSize);
wordCount = chopLine(line, words);
if (wordCount < 5 || !sameWord("Strand", words[0]))
    bfError(bf, "Expecting Strand info");
bb->qStrand = getStrand(bf, words[2]);
bb->tStrand = getStrand(bf, words[4]);


/* Process alignment lines.  They come in groups of three
 * separated by a blank line - something like:
 * Query: 26429 taccttgacattcctcagtgtgtcatcatcgttctctcctccaaacggcgagagtccgga 26488
 *           |||||| |||||||||| ||| ||||||||||||||||||||||| || || ||||||||
 * Sbjct: 62966 taccttaacattcctcaatgtttcatcatcgttctctcctccaaatggtgaaagtccgga 63025
 */
bb->qStart = bb->tStart = 0x3fffffff;
bb->qEnd = bb->tEnd = -bb->qStart;
if (qString == NULL)
    {
    qString = newDyString(50000);
    tString = newDyString(50000);
    }
else
    {
    dyStringClear(qString);
    dyStringClear(tString);
    }
for (;;)
    {
    int a, b, qs, ts, qe, te;
    /* Fetch next first line. */
    for (;;)
	{
	if (!nextBlockLine(bf, &line))
	    goto DONE;
	if (startsWith(" Score", line))
	    {
	    lineFileReuse(lf);
	    goto DONE;
	    }
	if (startsWith("Query:", line))
	    break;
	}
    wordCount = chopLine(line, words);
    if (wordCount != 4 || !isdigit(words[1][0]) || !isdigit(words[3][0]))
	bfSyntax(bf);
    a = atoi(words[1]);
    b = atoi(words[3]);
    qs = min(a,b);
    qe = max(a,b);
    bb->qStart = min(qs, bb->qStart);
    bb->qEnd = max(qe, bb->qEnd);
    dyStringAppend(qString, words[2]);

    /* Skip next line. */
    bfNeedNextLine(bf, &line, &lineSize);

    /* Fetch target sequence line. */
    bfNeedNextLine(bf, &line, &lineSize);
    wordCount = chopLine(line, words);
    if (wordCount != 4 || !isdigit(words[1][0]) || !isdigit(words[3][0]))
	bfSyntax(bf);
    a = atoi(words[1]);
    b = atoi(words[3]);
    ts = min(a,b);
    te = max(a,b);
    bb->tStart = min(ts, bb->tStart);
    bb->tEnd = max(te, bb->tEnd);
    dyStringAppend(tString, words[2]);
    }
DONE:
bb->tEnd += 1;
bb->qEnd += 1;
bb->qSym = cloneMem(qString->string, qString->stringSize+1);
bb->tSym = cloneMem(tString->string, tString->stringSize+1);
return bb;
}

void blastFileFree(struct blastFile **pBf)
/* Free blast file. */
{
struct blastFile *bf = *pBf;
if (bf != NULL)
    {
    lineFileClose(&bf->lf);
    freeMem(bf->fileName);
    freeMem(bf->program);
    freeMem(bf->version);
    freeMem(bf->buildDate);
    blastQueryFreeList(&bf->queries);
    freez(pBf);
    }
}

void blastFileFreeList(struct blastFile **pList)
/* Free list of blast files. */
{
struct blastFile *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    blastFileFree(&el);
    }
*pList = NULL;
}

void blastQueryFree(struct blastQuery **pBq)
/* Free single blastQuery. */
{
struct blastQuery *bq;
if ((bq = *pBq) != NULL)
    {
    freeMem(bq->query);
    freeMem(bq->database);
    blastGappedAliFreeList(&bq->gapped);
    freez(pBq);
    }
}

void blastQueryFreeList(struct blastQuery **pList)
/* Free list of blastQuery's. */
{
struct blastQuery *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    blastQueryFree(&el);
    }
*pList = NULL;
}


void blastGappedAliFree(struct blastGappedAli **pBga)
/* Free blastGappedAli. */
{
struct blastGappedAli *bga = *pBga;
if (bga != NULL)
    {
    freeMem(bga->targetName);
    blastBlockFreeList(&bga->blocks);
    freez(pBga);
    }
}

void blastGappedAliFreeList(struct blastGappedAli **pList)
/* Free blastGappedAli list. */
{
struct blastGappedAli *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    blastGappedAliFree(&el);
    }
*pList = NULL;
}


void blastBlockFree(struct blastBlock **pBb)
/* Free a single blastBlock. */
{
struct blastBlock *bb = *pBb;
if (bb != NULL)
    {
    freeMem(bb->qSym);
    freeMem(bb->tSym);
    freez(pBb);
    }
}

void blastBlockFreeList(struct blastBlock **pList)
/* Free a list of blastBlocks. */
{
struct blastBlock *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    blastBlockFree(&el);
    }
*pList = NULL;
}

