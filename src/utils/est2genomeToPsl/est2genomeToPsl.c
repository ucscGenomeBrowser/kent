/* est2genomeToPsl - Convert EMBOSS est2genome and WUSTL pairgon alignments to
 * PSL format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "errAbort.h"
#include "psl.h"
#include "sqlNum.h"
#include "fa.h"
#include "verbose.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "est2genomeToPsl - Convert EMBOSS est2genome and WUSTL pairgon alignments to\n"
  "PSL format\n"
  "\n"
  "usage:\n"
  "   est2genomeToPsl [options] alnFile cdnaFa genomeFa pslFile\n"
  "\n"
  "The est2genome -align flag must be specified when creating the\n"
  "alignment.\n"
  "\n"
  "Options:\n"
  "  -cdsOut=cds.file - for paragon alignments, output the predicted CDS in\n"
  "   a format that can be given to mrnaToGene.\n"
  "  -verbose=1 - \n"
  "      >= 3 dumps in-memory alignment structure after parse \n"
  "      >= 4 dumps after expanding abbrev (big!)\n"
  );
}

/* command line */
static struct optionSpec optionSpec[] = {
    {"cdsOut", OPTION_STRING},
    {NULL, 0}
};


struct seqParser
/* containes current record for query or target */
{
    struct dyString* name;   /* name of sequence */
    int start;               /* strand coordinates */
    int end;
    char strand;
    int size;
    struct dyString* alnAbbrv;   /* accumlated alignment before abbreviation
                                 * expansion*/
    struct dyString* aln;   /* alignment after expansion */
};

struct parser
/* data used during parsing */
{
    struct lineFile* lf;  /* alignment file */

    /* current record */
    struct seqParser query;
    struct seqParser target;
    struct dyString* alignAnn;  /* annotation string for alignment */
};

static char* START_PREFIX = "Note Best alignment is between";
static char* END_PREFIX = "Alignment Score:";

void parseErr(struct parser *parser, char* format, ...)
/* prototype to get gnu attribute check */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

void parseErr(struct parser *parser, char* format, ...)
/* abort on parse error */
{
va_list args;
va_start(args, format);
fprintf(stderr, "parse error: %s:%d: ",
        parser->lf->fileName, parser->lf->lineIx);
vaErrAbort(format, args);
va_end(args);

}

void seqParserInit(struct seqParser *seq)
/* initialize seq parse object */
{
ZeroVar(seq);
seq->name = dyStringNew(64);
seq->alnAbbrv = dyStringNew(2048);
seq->aln = dyStringNew(4096);
}

void seqParserClear(struct seqParser *seq)
/* clear seq parse object */
{
dyStringClear(seq->name);
seq->start = 0;
seq->end = 0;
seq->size = 0;
dyStringClear(seq->alnAbbrv);
dyStringClear(seq->aln);
}

struct parser *parserNew(char *alnFile)
/* construct  parser */
{
struct parser *parser;
AllocVar(parser);
seqParserInit(&parser->query);
seqParserInit(&parser->target);
parser->alignAnn = dyStringNew(4096);
parser->lf = lineFileOpen(alnFile, TRUE);
return parser;
}

void parserClear(struct parser *parser)
/* reset state of parser */
{
seqParserClear(&parser->query);
seqParserClear(&parser->target);
dyStringClear(parser->alignAnn);
}

char *getAlignIdent(struct parser *p)
/* get static-buffered string describing the alignment */
{
static char buf[1024];
snprintf(buf, sizeof(buf), "%s %c %d-%d  %s %c %d-%d",
         p->query.name->string,  p->query.strand,  p->query.start,  p->query.end,
         p->target.name->string, p->target.strand, p->target.start, p->target.end);
return buf;
}

void parserDump(struct parser *p, char* msg)
/* print parser contents for debugging */
{
if (msg != NULL)
    fprintf(stderr, "%s\n", msg);
fprintf(stderr, "%s\n", getAlignIdent(p));
fprintf(stderr, "%s\n", p->query.alnAbbrv->string);
fprintf(stderr, "%s\n", p->alignAnn->string);
fprintf(stderr, "%s\n", p->target.alnAbbrv->string);
fprintf(stderr, "%s\n", p->query.aln->string);
fprintf(stderr, "%s\n", p->target.aln->string);
fprintf(stderr, "\n");
fflush(stderr);
}

char *parserNextAlign(struct parser *parser)
/* scan forward for the next alignment */
{
char *line;
while (lineFileNext(parser->lf, &line, NULL))
    {
    if (startsWith(START_PREFIX, line))
        return line;
    }
return NULL;
}

char* alignNextLine(struct parser *parser)
/* Read the next line for an alignment, return NULL on end of alignment,
 * error if EOF. */
{
char *line;
if (!lineFileNext(parser->lf, &line, NULL))
    parseErr(parser, "unexpected EOF before end of alignment");
if (startsWith(START_PREFIX, line))
    parseErr(parser, "unexpected new aligned before end of current one");
if (startsWith(END_PREFIX, line))
    return NULL; /* new alignment */
return line;
}

char* alignNeedNextLine(struct parser *parser)
/* Read the next line for an alignment, abort on EOF or a new alignment. */
{
char *line = alignNextLine(parser);
if (line == NULL)
    parseErr(parser, "unexpect end of alignment");
return line;
}

char *alignSkipToPrefix(struct parser *parser, char* prefix)
/* Skip to a line prefix, return NULL if not found or a new alignment is
 * reached */
{
char *line;

while ((line = alignNextLine(parser)) && !startsWith(prefix, line))
    continue;
return line;
}

char *alignMustSkipToPrefix(struct parser *parser, char* prefix)
/* Skip to a line prefix, aborting if it's not found or a new alignment is
 * reached */
{
char *line = alignSkipToPrefix(parser, prefix);
if (line == NULL)
    parseErr(parser, "unexpect end of alignment looking for \"%s\"", prefix);
return line;
}

char *alignSkipEmptyLines(struct parser *parser)
/* Skip empty lines, return NULL EOF or a new alignment is reached */
{
char *line;

while ((line = alignNextLine(parser)) && (line[0] == '\0'))
    continue;
return line;
}

boolean findAlignHeader(struct parser *parser)
/* locate the next alignment and determine the strand from the 'Note' line */
{
/*
 * Note Best alignment is between forward est and forward genome, and splice sites imply forward gene
 * Note Best alignment is between reversed est and forward genome, but splice sites imply REVERSED GENE
 */
char *line = parserNextAlign(parser);
if (line == NULL)
    return FALSE;
if (stringIn("between forward est", line))
    parser->query.strand = '+';
else if (stringIn("between reversed est", line))
    parser->query.strand = '-';
else
    parseErr(parser, "can't determine EST strand from: %s", line);

if (stringIn("and forward genome", line))
    parser->target.strand = '+';
else if (stringIn("and reversed genome", line))
    parser->target.strand = '-';
else
    parseErr(parser, "can't determine genome strand from %s", line);
return TRUE;
}

void findAligns(struct parser *parser)
/* find the alignment sequences current alignments and set the
 * sequence names */
{
/* chr22-21726147-21808795 vs BC008986:*/
char *line;
while ((line = alignNeedNextLine(parser)) != NULL)
    {
    if (stringIn(" vs ", line) && endsWith(line, ":")
        && (chopString(line, " ", NULL, 0) == 3))
        {
        char *words[3];
        chopString(line, " ", words, 3);
        words[2][strlen(words[2])-1] = '\0';  /* drop training : */
        dyStringAppend(parser->target.name, words[0]);
        dyStringAppend(parser->query.name, words[2]);
        break;  /* found */
        }
    }
}

int findSeqOffLen(struct parser *parser, char *line, int *seqLenRet)
/* find the offset of the sequences in the alignment blocks and length, needed
 * for parsing the annotation string, which has blanks at mismatches */
{
/* chr22-21726147-21808795      1 a-aa */
int off = 0;
char *s = skipLeadingSpaces(line); /* skip to id */
s = skipToSpaces(s);            /* skip id */
s = skipLeadingSpaces(s);      /* skip seq offset */
s = skipToSpaces(s);           /* skip offset */
s = skipLeadingSpaces(s);      /* skip to seq */
off = s - line;
s = skipToSpaces(s);           /* skip to end of sequence */
if (s == NULL)
    parseErr(parser, "can't parse alignment sequence: %s", line);
*seqLenRet = (s - line) - off;
assert(*seqLenRet > 0);
return off;
}

void parseAlignSeq(struct parser *parser, struct seqParser* seqParser, char *line)
/* parse the a sequence line */
{
/* chr22-21726147-21808795      1 a-aagagacttagcacatttattcactcacagaggtgaatgaagggctca     49 */
char *words[6];  /* a few extra to catch problems */
int start, end;
int numWords = chopString(line, " ", words, ArraySize(words));
if (numWords != 4)
    parseErr(parser, "expected 4 words for alignment sequence");
if (!sameString(seqParser->name->string, words[0]))
    parseErr(parser, "expected sequnece id of \"%s\", got \"%s\"",
             seqParser->name->string, words[0]);
start = sqlSigned(words[1])-1; /* to zero-based */
end = sqlSigned(words[3]);
if (seqParser->start == seqParser->end)
    {
    /* first */
    seqParser->start = start;
    seqParser->end = end;
    }
else
    {
    /* rest */
    seqParser->start = min(seqParser->start, start);
    seqParser->end = max(seqParser->end, end);
    }
dyStringAppend(seqParser->alnAbbrv, words[2]);
}

void parseAlignAnn(struct parser *parser, char *line, int seqOff, int seqLen)
/* parse the annotation line */
{
int cnt;
dyStringAppendN(parser->alignAnn, line+seqOff, seqLen);
for (cnt = strlen(line)-seqOff; cnt < seqLen; cnt++)
    dyStringAppendC(parser->alignAnn, ' ');
}

boolean parseAlignRow(struct parser *parser)
/* parse the next 3-line alignment row */
{
int seqOff, seqLen;
char *line = alignSkipEmptyLines(parser);
if (line == NULL)
    return FALSE;  /* end of alignment */

seqOff = findSeqOffLen(parser, line, &seqLen);

parseAlignSeq(parser, &parser->target, line);
line = alignNeedNextLine(parser);
parseAlignAnn(parser, line, seqOff, seqLen);
line = alignNeedNextLine(parser);
parseAlignSeq(parser, &parser->query, line);
return TRUE;
}

void parseAlignRows(struct parser *parser)
/* parse the alignment sequence rows */
{
while (parseAlignRow(parser))
    continue;
if ((parser->query.alnAbbrv->stringSize != parser->alignAnn->stringSize)
    || (parser->target.alnAbbrv->stringSize != parser->alignAnn->stringSize))
    parseErr(parser, "abbreviated alignments are not the same length");
}

void abbrvParseError(struct parser *parser, int start)
/* print an error about not being able to parse an abbrv */
{
parseErr(parser, "can't parse abbrv: %15.15s", parser->alignAnn->string+start);
}

int findNextAbbrv(struct parser *parser, int next, int *abvStartRet, int *abvEndRet)
/* find the next intron abbreviation, return the count, along with the start
 * and end in the alignment strings.  Return -1 if no more. */
{
char *ann = parser->alignAnn->string;
char *end;
int remainLen = parser->alignAnn->stringSize-next;
int len, cnt;

/* find start of next abbrv */
len = strcspn(ann+next, "<>?");
if (len == remainLen)
    return -1;  /* no more */
next += len;
remainLen -= len;
*abvStartRet = next;

/* find count */
len = strspn(ann+next, "<>?");
if ((len == remainLen) || (*(ann+next+len) != ' '))
    abbrvParseError(parser, *abvStartRet);
next += len+1;
remainLen -= len+1;

/* convert count */
cnt = strtol(ann+next, &end, 10);
if ((end == ann+next) || (*end != ' '))
    abbrvParseError(parser, *abvStartRet);
len = end-(ann+next);
next += len+1;
remainLen -= len+1;

/* skip trailing intron abbrv */
len = strspn(ann+next, "<>?");
if (len == remainLen)
    abbrvParseError(parser, *abvStartRet);
next += len;
*abvEndRet = next;
return cnt;
}

void appendExon(struct parser *parser, int off, int len)
/* append the next exons */
{
dyStringAppendN(parser->query.aln, parser->query.alnAbbrv->string+off, len);
dyStringAppendN(parser->target.aln, parser->target.alnAbbrv->string+off, len);
}

void fillIntron(struct parser *parser, int cnt)
/* fill in intron part of alignment.  target gets Ns, since sequence doesn't
 * matter. */
{
int i;
for (i = 0; i < cnt; i++)
    {
    dyStringAppendC(parser->query.aln, '-');
    dyStringAppendC(parser->target.aln, 'N');
    }
}

void expandAbbrv(struct parser *parser)
/* expand the abbreviated sequences */
{
/*
 * expand abbrvs like this:
 *    ggcccctaca.......cctacctctaggcacc
 *    |||||<<<<< 69901 <<<<<|||||||||||
 *    ggccc.................ctctaggcacc
 * abbrvs can be '<', '>', or '?'.
 */
int cnt, next = 0, abvStart = 0, abvEnd = 0;

while ((cnt = findNextAbbrv(parser, next, &abvStart, &abvEnd)) >= 0)
    {
    appendExon(parser, next, abvStart-next);
    fillIntron(parser, cnt);
    next = abvEnd;
    }
appendExon(parser, next, parser->alignAnn->stringSize-next);
}


boolean parseAlign(struct parser *parser, struct hash *cdnaSizeTbl, 
                   struct hash *genomeSizeTbl)
/* parse the next alignment, false if EOF */
{
parserClear(parser);

if (!findAlignHeader(parser))
    return FALSE;
findAligns(parser);
parser->query.size = hashIntVal(cdnaSizeTbl, parser->query.name->string);
parser->target.size = hashIntVal(genomeSizeTbl, parser->target.name->string);
parseAlignRows(parser);
if (verboseLevel() >= 3)
    parserDump(parser, "after parse");
expandAbbrv(parser);
if (verboseLevel() >= 4)
    parserDump(parser, "after abbrev expand");
return TRUE;
}

void convertToPsl(struct parser *parser, FILE *pslFh)
/* convert parsed alignment to a psl */
{
int qStart = parser->query.start;
int qEnd = parser->query.end;
int tStart = parser->target.start;
int tEnd = parser->target.end;
char strand[3];
struct psl *psl;

/* convert overall range to positive coordinates */
if (parser->query.strand == '-') 
    reverseIntRange(&qStart, &qEnd, parser->query.size);
if (parser->target.strand == '-') 
    reverseIntRange(&tStart, &tEnd, parser->target.size);

/* build PSL with both strands specified */
strand[0] = parser->query.strand;
strand[1] = parser->target.strand;
strand[2] = '\0';
psl = pslFromAlign(parser->query.name->string, parser->query.size,
                   qStart, qEnd, parser->query.aln->string,
                   parser->target.name->string, parser->target.size,
                   tStart, tEnd, parser->target.aln->string,
                   strand, 0);
if (psl != NULL)
    {
    /* fix up psl to be an untranslated alignment */
    if (parser->target.strand == '-')
        pslRc(psl);
    psl->strand[1] = '\0';
    pslTabOut(psl, pslFh);
    pslFree(&psl);
    }
}

void outputCds(struct parser *parser, FILE *cdsFh)
/* locate the predicted CDS and output */
{
char *aln = parser->query.aln->string;
int cdsStart = 0, i;

/* find cdsStart */
for (i = 0; (aln[i] != '\0') && !isupper(aln[i]) ; i++)
    if (aln[i] != '-')
        cdsStart++;
if (aln[i] != '\0')
    {
    int cdsEnd = cdsStart;
    /* find cdsEnd */
    for (; (aln[i] != '\0') && !islower(aln[i]) ; i++)
        if (aln[i] != '-')
            cdsEnd++;
    cdsStart += parser->query.start;
    cdsEnd += parser->query.start;
    if (parser->query.strand == '-')
        reverseIntRange(&cdsStart, &cdsEnd, parser->query.size);
    fprintf(cdsFh, "%s\t%d..%d\n", parser->query.name->string, cdsStart+1, cdsEnd);
    }
}

struct hash *seqSizeLoad(char *faFile)
/* load sequence sizes from a fasta file */
{
struct lineFile *lf = lineFileOpen(faFile, TRUE);
struct hash *sizeTbl = hashNew(22);
char *seqName;
DNA *seq;
int seqSize;

while (faSomeSpeedReadNext(lf, &seq, &seqSize, &seqName, TRUE))
    hashAddInt(sizeTbl, seqName, seqSize);

lineFileClose(&lf);
return sizeTbl;
}

void est2genomeToPsl(char *alnFile, char *cdnaFaFile, char *genomeFaFile,
                     char *pslFile, char *cdsFile)
/* Convert axt to psl format. */
{
struct parser* parser = parserNew(alnFile);
struct hash *cdnaSizeTbl = seqSizeLoad(cdnaFaFile);
struct hash *genomeSizeTbl = seqSizeLoad(genomeFaFile);
FILE *pslFh = mustOpen(pslFile, "w");
FILE *cdsFh = (cdsFile != NULL) ? mustOpen(cdsFile, "w") : NULL;

while (parseAlign(parser, cdnaSizeTbl, genomeSizeTbl))
    {
    convertToPsl(parser, pslFh);
    if (cdsFh != NULL)
        outputCds(parser, cdsFh);
    }

lineFileClose(&parser->lf);
hashFree(&cdnaSizeTbl);
hashFree(&genomeSizeTbl);
carefulClose(&pslFh);
carefulClose(&cdsFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 5)
    usage();
est2genomeToPsl(argv[1], argv[2], argv[3], argv[4],
                optionVal("cdsOut", NULL));
return 0;
}
