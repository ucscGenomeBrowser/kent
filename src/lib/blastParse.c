/* blastParse - read in blast output into C data structure. */

#include "common.h"
#include "dystring.h"
#include "linefile.h"
#include "dnautil.h"
#include "sqlNum.h"
#include "blastParse.h"
#include "verbose.h"


#define WARN_LEVEL 1   /* verbose level to enable warnings */
#define TRACE_LEVEL 3  /* verbose level to enable tracing of files */
#define DUMP_LEVEL 4   /* verbose level to enable dumping of parsed */

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
errAbort("%s:%d: %s", bf->fileName, bf->lf->lineIx, message);
}

static void bfWarn(struct blastFile *bf, char *message)
/* Print blast file warning message. */
{
verbose(WARN_LEVEL, "Warning: %s:%d: %s\n", bf->fileName, bf->lf->lineIx, message);
}

static void bfUnexpectedEof(struct blastFile *bf)
/* generate error on unexpected EOF */
{
errAbort("Unexpected end of file in %s\n", bf->fileName);
}

static void bfSyntax(struct blastFile *bf)
/* General error message. */
{
bfError(bf, "Can't cope with BLAST output syntax");
}

static boolean isAllDigits(char *s)
/* test if a string is all digits */
{
for (; *s != '\0'; s++)
    {
    if (!isdigit(*s))
        return FALSE;
    }
return TRUE;
}

static boolean isAllDashes(char *s)
/* test if a string is all dashes */
{
for (; *s != '\0'; s++)
    {
    if (*s != '-')
        return FALSE;
    }
return TRUE;
}

static char *bfNextLine(struct blastFile *bf)
/* Fetch next line of input trying, or NULL if not found */
{
char *line = NULL;
if (lineFileNext(bf->lf, &line, NULL))
    {
    verbose(TRACE_LEVEL, "    => %s\n", line);
    return line;
    }
else
    {
    verbose(TRACE_LEVEL, "    => EOF\n");
    return NULL;
    }
}

static boolean bfSkipBlankLines(struct blastFile *bf)
/* skip blank lines, return FALSE on EOF */
{
char *line = NULL;
while ((line = bfNextLine(bf)) != NULL)
    {
    if (skipLeadingSpaces(line)[0] != '\0')
        {
	lineFileReuse(bf->lf);
        return TRUE;
        }
    }
return FALSE; /* EOF */
}

static char *bfNeedNextLine(struct blastFile *bf)
/* Fetch next line of input or die trying. */
{
char *line = bfNextLine(bf);
if (line == NULL)
    bfUnexpectedEof(bf);
return line;
}

static char *bfSearchForLine(struct blastFile *bf, char *start)
/* scan for a line starting with the specified string */
{
for (;;)
    {
    char *line = bfNextLine(bf);
    if (line == NULL)
	return NULL;
    if (startsWith(start, line))
        return line;
    }
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
char *words[16];
int wordCount;
struct lineFile *lf;

AllocVar(bf);
bf->lf = lf = lineFileOpen(fileName, TRUE);
bf->fileName = cloneString(fileName);

/* Parse first line - something like: */
line = bfNeedNextLine(bf);
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

static void parseQueryLines(struct blastFile *bf, char *line, struct blastQuery *bq)
/* Parse the Query= lines */
{
char *s, *e;
char *words[16];
int wordCount;
if (bq->query != NULL)
    bfError(bf, "already parse Query=");

/* Process something like:
 *    Query= MM39H11    00630     
 */
wordCount = chopLine(line, words);
if (wordCount < 2)
    bfError(bf, "No sequence name in query line");
bq->query = cloneString(words[1]);

for (;;)
    {
    line = bfNeedNextLine(bf);
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
}

static void parseDatabaseLines(struct blastFile *bf, char *line, struct blastQuery *bq)
/* Process something like:
 * Database: chr22.fa 
 *        977 sequences; 95,550,797 total letters
 */
{
static struct dyString *tmpBuf = NULL;
char *words[16];
int wordCount;
if (bq->database != NULL)
    bfError(bf, "already parse Database:");

if (tmpBuf == NULL)
    tmpBuf = dyStringNew(512);

/* parse something like
 * Database: celegans98
 * some versions of blastp include the absolute path, but
 * then split it across lines.
 */
wordCount = chopLine(line, words);
if (wordCount < 2)
    bfError(bf, "Expecting database name");
dyStringClear(tmpBuf);
dyStringAppend(tmpBuf, words[1]);
while (line = bfNeedNextLine(bf), !isspace(line[0]))
    {
    dyStringAppend(tmpBuf, line);
    }
bq->database = cloneString(tmpBuf->string);

/* Process something like:
 *        977 sequences; 95,550,797 total letters
 */
wordCount = chopLine(line, words);
if (wordCount < 3 || !isdigit(words[0][0]) || !isdigit(words[2][0]))
    bfError(bf, "Expecting database info");
decomma(words[0]);
decomma(words[2]);
bq->dbSeqCount = atoi(words[0]);
bq->dbBaseCount = atoi(words[2]);
}

static char *roundLinePrefix = "Results from round "; // start of a round line

static boolean isRoundLine(char *line)
/* check if a line is a PSI round number line */
{
return startsWith(roundLinePrefix, line);
}

static void parseRoundLine(char *line, struct blastQuery *bq)
/* round line and save current round in query
 *   Results from round 1
 */
{
char *p = skipLeadingSpaces(line + strlen(roundLinePrefix));
bq->psiRounds = atoi(p);
}

struct blastQuery *blastFileNextQuery(struct blastFile *bf)
/* Read all alignments associated with next query.  Return NULL at EOF. */
{
char *line;
struct blastQuery *bq;
struct blastGappedAli *bga;
AllocVar(bq);

verbose(TRACE_LEVEL, "blastFileNextQuery\n");

/* find and parse Query= */
line = bfSearchForLine(bf, "Query=");
if (line == NULL)
    return NULL;
parseQueryLines(bf, line, bq);

/* find and parse Database: */
line = bfSearchForLine(bf, "Database:");
if (line == NULL)
    bfUnexpectedEof(bf);
parseDatabaseLines(bf, line, bq);

/* Seek to beginning of first gapped alignment. */
for (;;)
    {
    line = bfNeedNextLine(bf);
    if (line[0] == '>')
	{
	lineFileReuse(bf->lf);
	break;
	}
    else if (isRoundLine(line))
        parseRoundLine(line, bq);
    else if (stringIn("No hits found", line) != NULL)
        break;
    }

/* Read in gapped alignments. */
while ((bga = blastFileNextGapped(bf, bq)) != NULL)
    {
    slAddHead(&bq->gapped, bga);
    }
slReverse(&bq->gapped);
if (verboseLevel() >= DUMP_LEVEL)
    {
    verbose(DUMP_LEVEL, "blastFileNextQuery result:\n");
    blastQueryPrint(bq, stderr);
    }
return bq;
}

static char *findNextGapped(struct blastFile *bf, struct blastQuery *bq)
/* scan for next gapped alignment, return line or NULL if not hit */
{
while (TRUE)
    {
    if (!bfSkipBlankLines(bf))
        return NULL;
    char *line = bfNextLine(bf);
    /*
     * the last condition was added to deal with the new blast output format and is meant to find lines such as this one:
     * TBLASTN 2.2.15 [Oct-15-2006]
     * I am hoping that by looking for only "BLAST" this will work with things like blastp, blastn, psi-blast, etc
     */
    if (startsWith("  Database:", line) || (stringIn("BLAST", line) != NULL))
        {
	lineFileReuse(bf->lf);
        return NULL;
        }
    if (line[0] == '>')
        return line;
    if (isRoundLine(line))
        parseRoundLine(line, bq);
    }
}

struct blastGappedAli *blastFileNextGapped(struct blastFile *bf, struct blastQuery *bq)
/* Read in next gapped alignment.   Does *not* put it on bf->gapped list. 
 * Return NULL at EOF or end of query. */
{
char *words[16];
int wordCount;
struct blastGappedAli *bga;
struct blastBlock *bb;
int lenSearch;

verbose(TRACE_LEVEL, "blastFileNextGapped\n");

char *line = findNextGapped(bf, bq);
if (line == NULL)
    return NULL;

AllocVar(bga);
bga->query = bq;
bga->targetName = cloneString(line+1); 
bga->psiRound = bq->psiRounds;

/* Process something like:
 *      Length = 100000
 * however this follows a possible multi-line description, so be specified
 * and limit how far we can scan
 */
for (lenSearch=0; lenSearch<25; lenSearch++)
	{
	line = bfNeedNextLine(bf);
        if (isRoundLine(line))
            parseRoundLine(line, bq);
	wordCount = chopLine(line, words);
	if (wordCount == 3 && sameString(words[0], "Length") &&  sameString(words[1], "=")
            && isdigit(words[2][0]))
		break;
	}
if (lenSearch>=25)
    bfError(bf, "Expecting Length =");
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

static boolean nextBlockLine(struct blastFile *bf, struct blastQuery *bq, char **retLine)
/* Get next block line.  Return FALSE and reuse line if it's
 * an end of block type line. */
{
struct lineFile *lf = bf->lf;
char *line;

*retLine = line = bfNextLine(bf);
if (line == NULL)
    return FALSE;
if (isRoundLine(line))
    parseRoundLine(line, bq);

/*
the last condition was added to deal with the new blast output format and is meant to find lines such as this one:
TBLASTN 2.2.15 [Oct-15-2006]
I am hoping that by looking for only "BLAST" this will work with things like blastp, blastn, psi-blast, etc
*/
if (line[0] == '>' || startsWith("Query=", line) || startsWith("  Database:", line) || (stringIn("BLAST", line) != NULL))
    {
    lineFileReuse(lf);
    return FALSE;
    }
return TRUE;
}

static double evalToDouble(char *s)
/* Convert string from e-val to floating point rep.
 * e-val is basically ascii floating point, but
 * small ones may be 'e-100' instead of 1.0e-100
 */
{
if (isdigit(s[0]))
    return atof(s);
else
    {
    char buf[64];
    safef(buf, sizeof(buf), "1.0%s", s);
    return atof(buf);
    }
}

static boolean parseBlockLine(struct blastFile *bf, int *startRet, int *endRet,
                           struct dyString *seq)
/* read and parse the next target or query line, like:
 *   Query: 26429 taccttgacattcctcagtgtgtcatcatcgttctctcctccaaacggcgagagtccgga 26488
 *
 * also handle broken NCBI tblastn output like:
 *   Sbjct: 1181YYGEQRSTNGQTIQLKTQVFRRFPDDDDESEDHDDPDNAHESPEQEGAEGHFDLHYYENQ 1360
 *
 * Ignores and returns FALSE on bogus records generated by PSI BLAST, such as
 *   Query: 0   --------------------------                                  
 *   Sbjct: 38  PPGPPGVAGGNQTTVVVIYGPPGPPG                                   63
 *   Query: 0                                                               
 *   Sbjct: 63                                                               63
 * If FALSE is returned, the output parameters will be unchanged.
 */
{
char* line = bfNeedNextLine(bf);
int a, b, s, e;
char *words[16];
int wordCount = chopLine(line, words);
if ((wordCount < 2) || (wordCount > 4) || !(sameString("Query:", words[0]) || sameString("Sbjct:", words[0])))
    bfSyntax(bf);

/* look for one of the bad formats to ignore, as described above */
if (((wordCount == 2) && isAllDigits(words[1]))
    || ((wordCount == 3) && isAllDigits(words[1]) && isAllDigits(words[2]))
    || ((wordCount == 3) && isAllDigits(words[1]) && isAllDashes(words[2])))
    {
    bfWarn(bf, "Ignored invalid alignment format for aligned sequence pair");
    return FALSE;
    }

/* special handling for broken output with no space between start and
 * sequence */
if (wordCount == 3)
    {
    char *p;
    if (!isdigit(words[1][0]) || !isdigit(words[2][0]))
        bfSyntax(bf);
    a = atoi(words[1]);
    b = atoi(words[2]);
    p = words[1];
    while ((*p != '\0') && (isdigit(*p)))
        p++;
    dyStringAppend(seq, p);
    }
else
    {
    if (!isdigit(words[1][0]) || !isdigit(words[3][0]))
        bfSyntax(bf);
    a = atoi(words[1]);
    b = atoi(words[3]);
    dyStringAppend(seq, words[2]);
    }
s = min(a,b);
e = max(a,b);
*startRet = min(s, *startRet);
*endRet = max(e, *endRet);
return TRUE;
}

static boolean findBlockSeqPair(struct blastFile *bf, struct blastQuery *bq)
/* scan forward for the next pair of Query:/Sbjct: sequences */
{
char *line;
for (;;)
    {
    if (!nextBlockLine(bf, bq, &line))
        return FALSE;
    if (startsWith(" Score", line))
        {
        lineFileReuse(bf->lf);
        return FALSE;
        }
    if (startsWith("Query:", line))
        {
        lineFileReuse(bf->lf);
        return TRUE;
        }
    }
}

static void clearBlastBlock(struct blastBlock *bb, struct dyString *qString, struct dyString *tString)
/* reset the contents of a blast block being accummulated */
{
bb->qStart = bb->tStart = 0x3fffffff;
bb->qEnd = bb->tEnd = -bb->qStart;
dyStringClear(qString);
dyStringClear(tString);
}

static void parseBlockSeqPair(struct blastFile *bf, struct blastBlock *bb,
                              struct dyString *qString, struct dyString *tString)
/* parse the current pair Query:/Sbjct: sequences */
{
boolean isOk = TRUE;

/* Query line */
if (!parseBlockLine(bf, &bb->qStart, &bb->qEnd, qString))
    isOk = FALSE;

/* Skip next line. */
bfNeedNextLine(bf);

/* Fetch target sequence line. */
if (!parseBlockLine(bf, &bb->tStart, &bb->tEnd, tString))
    isOk = FALSE;
if (!isOk)
    {
    // reset accumulated data so we discard everything before bogus pair
    clearBlastBlock(bb, qString, tString);
    }
}


static struct blastBlock *nextBlock(struct blastFile *bf, struct blastQuery *bq,
                                    struct blastGappedAli *bga, boolean *skipRet)
/* Read in next blast block.  Return NULL at EOF or end of gapped
 * alignment. If an unparsable block is found, set skipRet to TRUE and return
 * NULL. */
{
struct blastBlock *bb;
char *line;
char *words[16];
int wordCount;
char *parts[3];
int partCount;
static struct dyString *qString = NULL, *tString = NULL;

verbose(TRACE_LEVEL,  "blastFileNextBlock\n");
*skipRet = FALSE;

/* Seek until get something like:
 *   Score = 8770 bits (4424), Expect = 0.0
 * or something that looks like we're done with this gapped
 * alignment. */
for (;;)
    {
    if (!nextBlockLine(bf, bq, &line))
	return NULL;
    if (startsWith(" Score", line))
	break;
    }
AllocVar(bb);
bb->gappedAli = bga;
wordCount = chopLine(line, words);
if (wordCount < 8 || !sameWord("Score", words[0]) 
    || !isdigit(words[2][0]) || !(isdigit(words[7][0]) || words[7][0] == 'e')
    || !startsWith("Expect", words[5]))
    {
    bfError(bf, "Expecting something like:\n"
             "Score = 8770 bits (4424), Expect = 0.0");
    }
bb->bitScore = atof(words[2]);
bb->eVal = evalToDouble(words[7]);

/* Process something like:
 *   Identities = 8320/9618 (86%), Gaps = 3/9618 (0%)
 *             or
 *   Identities = 8320/9618 (86%)
 *             or
 *   Identities = 10/19 (52%), Positives = 15/19 (78%), Frame = +2
 *     (wu-tblastn)
 *             or
 *   Identities = 256/400 (64%), Positives = 306/400 (76%)
 *   Frame = +1 / -2
 *     (tblastn)
 *
 *   Identities = 1317/10108 (13%), Positives = 2779/10108 (27%), Gaps = 1040/10108
 *   (10%)
 *      - wrap on long lines
 *
 * Handle weird cases where the is only a `Score' line, with no `Identities'
 * lines by skipping the alignment; they seem line small, junky alignments.
 */
line = bfNeedNextLine(bf);
wordCount = chopLine(line, words);
if (wordCount < 3 || !sameWord("Identities", words[0]))
    {
    if (wordCount > 1 || sameWord("Score", words[0]))
        {
        /* ugly hack to skip block with no identities */
        *skipRet = TRUE;
        blastBlockFree(&bb);
        return NULL;
        }
    bfError(bf, "Expecting identity count");
    }
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
if ((wordCount >= 11) && sameWord("Frame", words[8]))
    {
    bb->qStrand = '+';
    bb->tStrand = words[10][0];
    bb->tFrame = atoi(words[10]);
    }

line = bfNeedNextLine(bf);
boolean wrapped = (startsWith("(", line));

/* Process something like:
 *     Strand = Plus / Plus (blastn)
 *     Frame = +1           (tblastn)
 *     Frame = +1 / -2      (tblastx)
 *     <blank line>         (blastp)
 * note that wu-tblastn puts frame on Identities line
 */
if (wrapped)
    line = bfNeedNextLine(bf);
wordCount = chopLine(line, words);
if ((wordCount >= 5) && sameWord("Strand", words[0]))
    {
    bb->qStrand = getStrand(bf, words[2]);
    bb->tStrand = getStrand(bf, words[4]);
    }
else if ((wordCount >= 5) && sameWord("Frame", words[0]) && (words[3][0] == '/'))
    {
    // Frame = +1 / -2      (tblastx)
    bb->qStrand = (words[2][0] == '-') ? -1 : 1;
    bb->tStrand = (words[4][0] == '-') ? -1 : 1;
    bb->qFrame = atoi(words[2]);
    bb->tFrame = atoi(words[4]);
    }
else if ((wordCount >= 3) && sameWord("Frame", words[0]))
    {
    // Frame = +1           (tblastn)
    bb->qStrand = 1;
    bb->tStrand = (words[2][0] == '-') ? -1 : 1;
    bb->qFrame = atoi(words[2]);
    bb->tFrame = 1;
    }
else if (wordCount == 0)
    {
    /* if we didn't parse frame, default it */
    if (bb->qStrand == 0)
        {
        bb->qStrand = '+';
        bb->tStrand = '+';
        }
    }
else
    bfError(bf, "Expecting Strand, Frame or blank line");


/* Process alignment lines.  They come in groups of three
 * separated by a blank line - something like:
 * Query: 26429 taccttgacattcctcagtgtgtcatcatcgttctctcctccaaacggcgagagtccgga 26488
 *              |||||| |||||||||| ||| ||||||||||||||||||||||| || || ||||||||
 * Sbjct: 62966 taccttaacattcctcaatgtttcatcatcgttctctcctccaaatggtgaaagtccgga 63025
 */
if (qString == NULL)
    {
    qString = newDyString(50000);
    tString = newDyString(50000);
    }
clearBlastBlock(bb, qString, tString);
for (;;)
    {
    if (!findBlockSeqPair(bf, bq))
        break;
    parseBlockSeqPair(bf, bb, qString, tString);
    }

/* convert to [0..n) and move to strand coords if necessary */
bb->qStart--;
if (bb->qStrand < 0)
    reverseIntRange(&bb->qStart, &bb->qEnd, bq->queryBaseCount);
bb->tStart--;
if (bb->tStrand < 0)
    reverseIntRange(&bb->tStart, &bb->tEnd, bga->targetSize);
bb->qSym = cloneMem(qString->string, qString->stringSize+1);
bb->tSym = cloneMem(tString->string, tString->stringSize+1);
return bb;
}

struct blastBlock *blastFileNextBlock(struct blastFile *bf, 
	struct blastQuery *bq, struct blastGappedAli *bga)
/* Read in next blast block.  Return NULL at EOF or end of
 * gapped alignment. */
{
struct blastBlock *bb = NULL;
boolean skip = FALSE;

while (((bb = nextBlock(bf, bq, bga, &skip)) == NULL) && skip)
    continue; /* skip to next one */

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

void blastBlockPrint(struct blastBlock* bb, FILE* out)
/* print a BLAST block for debugging purposes  */
{
fprintf(out, "    blk: %d-%d <=> %d-%d tcnt=%d mcnt=%d icnt=%d\n",
        bb->qStart, bb->qEnd,  bb->tStart, bb->tEnd,
        bb->totalCount, bb->matchCount, bb->insertCount);
fprintf(out, "        Q: %s\n", bb->qSym);
fprintf(out, "        T: %s\n", bb->tSym);
}

void blastGappedAliPrint(struct blastGappedAli* ba, FILE* out)
/* print a BLAST gapped alignment for debugging purposes  */
{
struct blastBlock *bb;
fprintf(out, "%s <=> %s", ba->query->query, ba->targetName);
if (ba->psiRound > 0)
    fprintf(out, " round: %d", ba->psiRound);
fputc('\n', out);
for (bb = ba->blocks; bb != NULL; bb = bb->next)
    {
    blastBlockPrint(bb, out);
    }
}

void blastQueryPrint(struct blastQuery *bq, FILE* out)
/* print a BLAST query for debugging purposes  */
{
struct blastGappedAli *ba;
for (ba = bq->gapped; ba != NULL; ba = ba->next)
    blastGappedAliPrint(ba, out);
}
