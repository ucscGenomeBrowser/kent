/* embossToPsl - Convert axt to psl format. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "psl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: embossToPsl.c,v 1.1 2004/01/04 10:30:10 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "embossToPsl - Convert EMBOSS pair alignments to PSL format\n"
  "usage:\n"
  "   embossToPsl [options] in.pair out.psl\n"
  "\n"
  "where in.pair is an EMBOSS pair format alignment file\n"
  "\n"
  "options:\n"
  "   -qSizes=szfile - read query sizes from tab file of <seqName> <size>\n"
  "   -tSizes=szfile - read target sizes from tab file of <seqName> <size>\n"
  "   -qSize=size - set query size, there can be only one query\n"
  "   -tSize=size - set targetx size, there can be only one target\n"
  );
}

/* command line */
static struct optionSpec optionSpec[] = {
    {"qSizes", OPTION_STRING},
    {"tSizes", OPTION_STRING},
    {"qSize", OPTION_INT},
    {"tSize", OPTION_INT},
    {NULL, 0}
};

struct sizes
{
    struct hash* tbl;    /* table of multiple sequences */    
    char* singleSeq;     /* single seq name, after first access */
    int singleSize;      /* size for single sequence */
};

static struct sizes qSizes = {NULL, NULL, 0};
static struct sizes tSizes = {NULL, NULL, 0};

struct seqParser
/* containes current record for query or target */
{
    struct dyString* name;
    int start;
    int end;
    struct dyString* str;
};

struct parser
/* data used during parsing */
{
    struct lineFile* lf;  /* alignment file */

    /* current record */
    struct seqParser query;
    struct seqParser target;
};

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

void loadSizes(char* fileOpt, char* sizeOpt, struct sizes* sizes)
/* set query or target size info */
{
if (optionExists(fileOpt) && optionExists(sizeOpt))
    errAbort("can't specify both -%s and -%s", fileOpt, sizeOpt);
if (optionExists(fileOpt))
    sizes->tbl = readSizes(optionVal(fileOpt, NULL));
else if (optionExists(sizeOpt))
    sizes->singleSize = optionInt(sizeOpt, 0);
else
    errAbort("must specify one of -%s or -%s", fileOpt, sizeOpt);
}


int getSize(struct sizes* sizes, char *name)
/* Find size of a sequence. */
{
if (sizes->tbl != NULL)
    return ptToInt(hashMustFindVal(sizes->tbl, name));
else 
    {
    if (sizes->singleSeq != NULL)
        {
        if (!sameString(name, sizes->singleSeq))
            errAbort("onle one size specified, however have multipled seqs: %s and %s",
                     sizes->singleSeq, name);
        }
    else
        sizes->singleSeq = cloneString(name);
    return sizes->singleSize;
    }
}

char* skipToPrefix(struct lineFile *inLf, char *prefix)
/* skip to a line starting with prefix, or NULL on EOF */
{
char *line;
while (lineFileNext(inLf, &line, NULL))
    {
    if (startsWith(prefix, line))
        return line;
    }
return NULL;
}

void checkFormat(struct parser *parser)
/* check format of file */
{
char *line = skipToPrefix(parser->lf, "# Align_format:");
if (line == NULL)
    errAbort("Doesn't appear to be an EMBOSS alignment format, can't find Align_format");

if (!sameString(line, "# Align_format: srspair"))
    errAbort("only supports EMBOSS erspair format, found %s", line);
}

boolean findNextAlign(struct parser *parser)
/* find next alignment; could do better format validations */
{
char *line;
if (skipToPrefix(parser->lf, "# Aligned_sequences:") == NULL)
    return FALSE; /* EOF */

if (skipToPrefix(parser->lf, "#=======================================") == NULL)
    errAbort("can't find alignment start: %s line %d", parser->lf->fileName,
             parser->lf->lineIx);
/* skip blank line */
lineFileNeedNext(parser->lf, &line, NULL);
return TRUE;
}

void initSeqParser(struct seqParser *seq)
/* initialize seq parse object */
{
seq->name = dyStringNew(64);
seq->start = 0;
seq->end = 0;
seq->str = dyStringNew(1024);
}

void clearSeqParser(struct seqParser *seq)
/* clear seq parse object */
{
dyStringClear(seq->name);
seq->start = 0;
seq->end = 0;
dyStringClear(seq->str);
}

void parseSeqLine(struct seqParser *seqParser, struct lineFile *inLf, char *line)
/* parse one line of query or target */
{
/* U53442             1 gtgaaattctgctcc-gga-catgtcggg----ccctcgc----------     34 */
char *words[4];
int nWords, start, end;

nWords = chopByWhite(line, words, ArraySize(words));
lineFileExpectWords(inLf, ArraySize(words), nWords);
start = lineFileNeedNum(inLf, words, 1) - 1;
end = lineFileNeedNum(inLf, words, 3);
if (seqParser->start == seqParser->end)
    {
    /* first line */
    dyStringAppend(seqParser->name, words[0]);
    seqParser->start = start;
    }
seqParser->end = end;
dyStringAppend(seqParser->str, words[2]);
}

char *nextNonBlankLine(struct parser *parser)
/* skip to the next non-blank line */
{
char *line;
while (lineFileNext(parser->lf, &line, NULL))
    {
    char *nonBl = skipLeadingSpaces(line);
    if (nonBl[0] != '\0')
        return line;
    }
return NULL;
}

boolean parseAlign(struct parser *parser)
/* parse the next alignment, false if empty */
{
char* line;
clearSeqParser(&parser->query);
clearSeqParser(&parser->target);

while ((line = nextNonBlankLine(parser)) != NULL)
    {
    if (line[0] == '#')
        break;  /* end of alignment */
    /* 3 lines of alignment */
    parseSeqLine(&parser->query, parser->lf, line);
    lineFileNeedNext(parser->lf, &line, NULL);
    lineFileNeedNext(parser->lf, &line, NULL);
    parseSeqLine(&parser->target, parser->lf, line);
    }
return (parser->query.end > parser->query.start);
}

void processAlign(struct parser *parser, FILE *outFh)
/* process the next alignment */
{
if (parseAlign(parser))
    {
    char *qName = parser->query.name->string;
    char *tName = parser->target.name->string;
    struct psl *psl = pslFromAlign(qName, getSize(&qSizes, qName),
                                   parser->query.start, parser->query.end,
                                   parser->query.str->string,
                                   tName, getSize(&tSizes, tName),
                                   parser->target.start, parser->target.end,
                                   parser->target.str->string,
                                   "+", 0);
    if (psl != NULL)
        pslTabOut(psl, outFh);
    pslFree(&psl);
    }
}

void embossToPsl(char *inName, char *outName)
/* embossToPsl - Convert axt to psl format. */
{
struct parser parser;
FILE *outFh;
ZeroVar(&parser);
parser.lf = lineFileOpen(inName, TRUE);
initSeqParser(&parser.query);
initSeqParser(&parser.target);

checkFormat(&parser);
outFh = mustOpen(outName, "w");

while (findNextAlign(&parser))
    processAlign(&parser, outFh);

lineFileClose(&parser.lf);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 3)
    usage();
loadSizes("qSizes", "qSize", &qSizes);
loadSizes("tSizes", "tSize", &tSizes);

embossToPsl(argv[1], argv[2]);
return 0;
}
