/* spideyToPsl - Convert NCBI spidey alignments to PSL format. */

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
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "spideyToPsl - Convert NCBI spidey pair alignments to PSL format\n"
  "usage:\n"
  "   spideyToPsl [options] in out.psl\n"
  "\n"
  "where`in' is the alignment output of spidey (with summaries). \n"
  "Certain problem cases, such as overlaping exons, result in a\n"
  "warning and skipping the alignment.\n"
  "\n"
  "Options:\n"
  "  -untranslated - format PSLs as untranslated alignments.\n"
  );
}


/* command line */
static struct optionSpec optionSpec[] = {
    {"untranslated", OPTION_BOOLEAN},
    {NULL, 0}
};


boolean gUntranslated;  /* create PSLs in untranslated format */

struct seqParser
/* containes current record for query or target */
{
    struct dyString* name;   /* name of sequence */
    int start;               /* strand coordinates */
    int end;
    int size;
    char strand;
    struct dyString* aln;   /* accumlated alignment */
};

struct parser
/* data used during parsing */
{
    struct lineFile* lf;  /* alignment file */

    /* current record */
    struct seqParser query;
    struct seqParser target;
    boolean skip;  /* skip this alignment */
};

static char* START_PREFIX = "--SPIDEY";  /* first line */

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

void initSeqParser(struct seqParser *seq)
/* initialize seq parse object */
{
ZeroVar(seq);
seq->name = dyStringNew(64);
seq->aln = dyStringNew(4096);
}

void clearSeqParser(struct seqParser *seq)
/* clear seq parse object */
{
dyStringClear(seq->name);
seq->start = 0;
seq->end = 0;
seq->size = 0;
dyStringClear(seq->aln);
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

void dumpParser(struct parser *p, char* msg)
/* print parser contents for debuggins */
{
if (msg != NULL)
    fprintf(stderr, "%s\n", msg);
fprintf(stderr, "%s\n", getAlignIdent(p));
fprintf(stderr, "%s\n", p->query.aln->string);
fprintf(stderr, "%s\n", p->target.aln->string);
fprintf(stderr, "\n");
fflush(stderr);
}

char* alignNextLine(struct parser *parser, char* prefix)
/* Read the next line for an alignment, return NULL on EOF or a new
 * alignment. */
{
char *line;
if (!lineFileNext(parser->lf, &line, NULL))
    return NULL; /*EOF*/
if (startsWith(START_PREFIX, line))
    {
    lineFileReuse(parser->lf);
    return NULL; /* new alignment */
    }
return line;
}

char* alignNeedNextLine(struct parser *parser, char* prefix)
/* Read the next line for an alignment, abort on EOF or a new alignment.  If
 * prefix is not null, verify that the line starts with it. */
{
char *line = alignNextLine(parser, prefix);
if (line == NULL)
    parseErr(parser, "unexpect end of alignment");
if ((prefix != NULL) && !startsWith(prefix, line))
    parseErr(parser, "line does not start with \"%s\": %s", prefix, line);
return line;
}

char *alignSkipToPrefix(struct parser *parser, char* prefix)
/* Skip to a line prefix, return NULL if not found or a new alignment is
 * reached */
{
char *line;

while ((line = alignNextLine(parser, prefix)) && !startsWith(prefix, line))
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

while ((line = alignNextLine(parser, NULL)) && (line[0] == '\0'))
    continue;
return line;
}

void parseSeqHeader(struct parser *parser, struct seqParser* seqParser,
                    char* prefix)
/* parse one of the sequence lines, saving the name and length in seqParser */
{
char *line = alignNeedNextLine(parser, prefix);
char *row[32], *cptr;

/* Genomic: lcl|set2.dna No definition line found, 11443 bp */
int nCols = chopByWhite(line, row, ArraySize(row));
if (nCols < 4)
    parseErr(parser, "not enough columns in %s line", prefix);

/* parse id; no idea what the lcl means */
cptr = strchr(row[1], '|');
if ((cptr == NULL) || (cptr[1] == '\0'))
    parseErr(parser, "don't know how to parse seq id: %s", row[1]);
dyStringAppend(seqParser->name, cptr+1);

/* get the size */
if (!sameString(row[nCols-1], "bp"))
    parseErr(parser, "expected \"bp\" at end of line, got \"%s\"", row[nCols-1]);
seqParser->size = sqlUnsigned(row[nCols-2]);
}

void parseStrand(struct parser *parser)
/* parse the strand line */
{
char *line = alignNeedNextLine(parser, "Strand:");
char *row[4];
int nCols = chopByWhite(line, row, ArraySize(row));
if (nCols < 2)
    parseErr(parser, "can't parse Strand: line");


/* DNA */
if (sameString(row[1], "plus"))
    parser->target.strand = '+';
else if (sameString(row[1], "minus"))
    parser->target.strand = '-';
else
    parseErr(parser, "unknown DNA strand value: \"%s\"", row[1]);

/* mRNA */
if (nCols == 2)
    parser->query.strand = '+';
else if (sameString(row[2], "Reverse"))
    parser->query.strand = '-';
else
    parseErr(parser, "unknown mRNA direction value: \"%s\"", row[2]);
}

boolean parseAlignHeader(struct parser *parser)
/* parse the header part of the next alignment, false if no more */
{
/* search for start of alignment; this is required if the previous alignment
 * was skipped due to an error case */
char *line;
do 
    {
    if (!lineFileNext(parser->lf, &line, NULL))
        return FALSE; /* EOF */
    }
while (!startsWith(START_PREFIX, line));

parseSeqHeader(parser, &parser->target, "Genomic:");
parseSeqHeader(parser, &parser->query, "mRNA:");
parseStrand(parser);
return TRUE;
}

void checkAlignSeq(struct parser *parser, struct seqParser* seqParser)
/* see if an alignment line contains only the expected characters */
{
char *line = seqParser->aln->string;
size_t validLen = strspn(line, "ATGCNatgcn-");
if (validLen < strlen(line))
    parseErr(parser, "invalid character \"%c\" in alignment line: %s",
             line[validLen], line);
}

void findAlignLineMinMax(char* line, int* startIdxPtr, int* endIdxPtr)
/* update alignment start/end indexes based on start/end of non-blank
   alignment line chars  */
{
int startIdx, endIdx;

for (startIdx = 0; (line[startIdx] != '\0') && (line[startIdx] == ' ');
     startIdx++)
    continue;
for (endIdx = strlen(line)-1; (endIdx >= 0) && (line[endIdx] == ' ');
     endIdx--)
    continue;
if (startIdx > *startIdxPtr)
    *startIdxPtr = startIdx;
if (endIdx < *endIdxPtr)
    *endIdxPtr = endIdx;
}

boolean parseAlignRec(struct parser *parser)
/* pares one alignment record */
{
static struct dyString* dnaBuf = NULL;
int startIdx, endIdx;
char *line = alignSkipEmptyLines(parser);
if (line == NULL)
    return FALSE;  /* end of alignment */
if (startsWith("Exon", line))
    {
    lineFileReuse(parser->lf);
    return FALSE;  /* end of exon */
    }
assert(parser->query.aln->stringSize == parser->target.aln->stringSize);

/* need a tmp buffer for parsing, it's static to this function */
if (dnaBuf == NULL)
     dnaBuf = dyStringNew(1024);
dyStringClear(dnaBuf);


/* Format is a pain.  The leading and trainling unaligned sequence is not
 * actually included in the exon bounds, so it must be skipped.  This means
 * that both sequences must be read before we can process either.  If there is
 * a protein line, it appears that there are two blanks lines after it.  If no
 * protein line, just one blank line.  Sometimes there is a stray single line
 * with a few bases at the end of the exon, not sure what this is.
 */

/* DNA, buffer for later processing */
dyStringAppend(dnaBuf, line);

/* match indicator line */
line = alignNeedNextLine(parser, NULL);
if (line[0] == '\0')
    {
    /* first line was weird stray line at end of exon */
    return FALSE;
    }

/* mRNA */
line = alignNeedNextLine(parser, NULL);

/* now find beginning and end of actual aligned region, skipping leading
 * and trailing spaces, which are not part of the exon coordinates */
startIdx = 0;
endIdx = BIGNUM;
findAlignLineMinMax(dnaBuf->string, &startIdx, &endIdx);
findAlignLineMinMax(line, &startIdx, &endIdx);
assert(endIdx >= 0);

dyStringAppendN(parser->target.aln, dnaBuf->string+startIdx, (endIdx-startIdx)+1);
dyStringAppendN(parser->query.aln, line+startIdx, (endIdx-startIdx)+1);

/* next line is either blank or protein, just skip it */
alignNextLine(parser, NULL);

assert(parser->query.aln->stringSize == parser->target.aln->stringSize);

return TRUE;
}

boolean addExonToSeq(struct parser *parser, struct seqParser* seqParser,
                     int start, int end, struct seqParser* otherParser)
/* convert spidey coords to [0..n), add or extend seq coordinates given an
 * exon; return false if an error is detected */
{
if (seqParser->strand == '-')
    {
    int tmp = start;
    assert(end <= start);  /* reversed */
    start = end;  /* coords are reversed in alignment */
    end = tmp;
    start--;
    reverseIntRange(&start, &end, seqParser->size);
    }
else
    {
    assert(end >= start);
    start--;
    }

/* check for overlap */
if (start < seqParser->end)
    {
    fprintf(stderr, "Warning: %s has overlapping exons at %s %d\n",
            getAlignIdent(parser), seqParser->name->string, start);
    parser->skip = TRUE;
    return FALSE;
    }

if (seqParser->start == seqParser->end)
    {
    /* first exon */
    seqParser->start = start;
    seqParser->end = end;
    }
else
    {
    /* other exons, since spidey doesn't output sequence on long insert, add
     * Ns to current seq, `-' to other */
    int gapSize = start-seqParser->end;
    if (gapSize > 0)
        {
        dyStringAppendMultiC(seqParser->aln, 'N', gapSize);
        dyStringAppendMultiC(otherParser->aln, '-', gapSize);
        }
    seqParser->end = end;
    }
return TRUE;
}

boolean parseExon(struct parser *parser)
/* parse an exon entry from the alignment, return FALSE if end of alignment */
{
int tStart, tEnd, qStart, qEnd;
char *line = alignSkipToPrefix(parser, "Exon");
if (line == NULL)
    return FALSE;
/* Exon 11: 9933-10031 (gen)  2351-2449 (mRNA) */
if (sscanf(line, "Exon %*d: %d-%d (gen)  %d-%d (mRNA)", &tStart, &tEnd,
           &qStart, &qEnd) != 4)
    parseErr(parser, "can't parse exon line");

/* adjusrt and extend coordinates and alignments */
if (!addExonToSeq(parser, &parser->query, qStart, qEnd, &parser->target))
    return FALSE;  /* error, give up on this alignment */
if (!addExonToSeq(parser, &parser->target, tStart, tEnd, &parser->query))
    return FALSE;  /* error, give up on this alignment */

/* parse sequences */
while (parseAlignRec(parser))
    continue;

return TRUE;
}

boolean parseAlign(struct parser *parser)
/* parse the next alignment, false if EOF */
{
clearSeqParser(&parser->query);
clearSeqParser(&parser->target);
parser->skip = FALSE;

if (!parseAlignHeader(parser))
    return FALSE;

/* verify some other expected line before alignment */
alignMustSkipToPrefix(parser, "Missing mRNA ends:");
alignMustSkipToPrefix(parser, "Genomic:");
alignMustSkipToPrefix(parser, "mRNA:");

/* parse exons in alignment */
while (parseExon(parser))
    continue;

if (!parser->skip)
    {
    checkAlignSeq(parser, &parser->query);
    checkAlignSeq(parser, &parser->target);
    }
return TRUE;
}

void convertAlign(struct parser *parser, FILE *outFh)
/* convert a parsed alignment to a psl */
{
int qStart = parser->query.start;
int qEnd = parser->query.end;
int tStart = parser->target.start;
int tEnd = parser->target.end;
char strand[3];
struct psl *psl;

#if 0
dumpParser(parser, "convertAlign");
#endif

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
    if (gUntranslated)
        {
        /* fix up psl to be an untranslated alignment */
        if (parser->target.strand == '-')
            pslRc(psl);
        psl->strand[1] = '\0';  /* single strand */
        }

    if (psl != NULL)
        pslTabOut(psl, outFh);
    pslFree(&psl);
    }
}

void spideyToPsl(char *inName, char *outName)
/* spideyToPsl - Convert axt to psl format. */
{
struct parser parser;
FILE *outFh;
ZeroVar(&parser);
parser.lf = lineFileOpen(inName, TRUE);
initSeqParser(&parser.query);
initSeqParser(&parser.target);

outFh = mustOpen(outName, "w");

while (parseAlign(&parser))
    {
    if (!parser.skip)
        convertAlign(&parser, outFh);
    }

lineFileClose(&parser.lf);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 3)
    usage();
gUntranslated = optionExists("untranslated");
spideyToPsl(argv[1], argv[2]);
return 0;
}
