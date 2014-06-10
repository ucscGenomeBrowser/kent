/* axtToMaf - Convert from XMFA to MAF format. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"
#include "hdb.h"
#include "fa.h"


/* counts the number of non-gap bases in a seq */
int ungappedSize(struct dnaSeq * seq) {
    int size = 0;
    char *c = seq->dna;
    while(*c != '\0') {
        if(*c != '-')
            size++;
        ++c;
    }

    return size;
}

/* faReadMixedNext  */

boolean myFaReadMixedNext(FILE *f, boolean preserveCase, char *defaultName, 
    boolean mustStartWithComment, char **retCommentLine, struct dnaSeq **retSeq)
/* Read next sequence from .fa file. Return sequence in retSeq.  
 * If retCommentLine is non-null return the '>' line in retCommentLine.
 * The whole thing returns FALSE at end of file. 
 * Contains parameter to preserve mixed case. */
{
char lineBuf[1024];
int lineSize;
char *words[1];
int c;
off_t offset = ftello(f);
size_t dnaSize = 0;
DNA *dna, *sequence, b;
int bogusChars = 0;
char *name = defaultName;

if (name == NULL)
    name = "";
dnaUtilOpen();
if (retCommentLine != NULL)
    *retCommentLine = NULL;
*retSeq = NULL;

/* Skip first lines until it starts with '>' */
for (;;)
    {
    if(fgets(lineBuf, sizeof(lineBuf), f) == NULL)
        {
        if (ferror(f))
            errnoAbort("read of fasta file failed");
        return FALSE;
        }
    lineSize = strlen(lineBuf);
    if (lineBuf[0] == '>')
        {
	if (retCommentLine != NULL)
            *retCommentLine = cloneString(lineBuf);
        offset = ftello(f);
        chopByWhite(lineBuf, words, ArraySize(words));
        name = words[0]+1;
        break;
        }
    else if (!mustStartWithComment)
        {
        if (fseeko(f, offset, SEEK_SET) < 0)
            errnoAbort("fseek on fasta file failed");
        break;
        }
    else
        offset += lineSize;
    }
/* Count up DNA. */
for (;;)
    {
    c = fgetc(f);
    if (c == EOF || c == '>' || c == '=')
        break;
    if (!isspace(c) && !isdigit(c))
        {
        ++dnaSize;
        }
    }

/* Allocate DNA and fill it up from file. */
dna = sequence = needHugeMem(dnaSize+1);
if (fseeko(f, offset, SEEK_SET) < 0)
    errnoAbort("fseek on fasta file failed");
for (;;)
    {
    c = fgetc(f);
    if (c == EOF || c == '>' || c == '=')
        break;
    if (!isspace(c) && !isdigit(c))
        {
        // check for non-DNA char
        if (ntChars[c] == 0)
            {
            *dna++ = preserveCase ? 'N' : 'n';
            }
        else
            {
            *dna++ = preserveCase ? c : ntChars[c];
            }
        }
    }
if (c == '>' || c == '=')
    ungetc(c, f);
*dna = 0;

*retSeq = newDnaSeq(sequence, dnaSize, name);
if (ferror(f))
    errnoAbort("read of fasta file failed");
return TRUE;
}


void usage()
/* Explain usage and exit. */
{
errAbort(
  "xmfaToMaf - Convert from xmfa to maf format\n"
  "usage:\n"
  "   xmfaToMaf in.xmfa out.maf org1=db1 org2=db2 ... orgN=dbN\n"
  );
}

void xmfaToMaf(char *in, char *out)
/* xmfaToMaf - Convert from xmfa to maf format. */
{
int c;
FILE *input  = mustOpen(in,  "r");
FILE *output = mustOpen(out, "w");

char* commentLine;
struct dnaSeq* sequence;

struct mafAli *ali;

struct sqlConnection* conn = hAllocConn();

mafWriteStart(output, "mlagan");

AllocVar(ali);
while(myFaReadMixedNext(input, TRUE, "default name", TRUE, &commentLine, &sequence)) {
    char srcName[128];
    
    c = fgetc(input);
    if(c == '=' || c == '>') { /* add the current sequence and process the block if we've see an '='*/
        char org[32];
        char chrom[32];
        int start;
        int stop;
        char strand;
        struct mafComp *comp;
        double score;

        char buffer[1024];

        ungetc(c, input);
        
        AllocVar(comp);
        /* parse the comment line */
        sscanf(commentLine, ">%s %[^:]:%d-%d %c", org, chrom, &start, &stop, &strand);
        /* build the name */
        safef(srcName, sizeof(srcName), "%s.%s", optionVal(org, org), chrom);
        comp->src = cloneString(srcName);

        sqlSafef(buffer, 1024, "SELECT size FROM %s.chromInfo WHERE chrom = \"%s\"", optionVal(org, org), chrom);
        assert(sqlQuickQuery(conn, buffer, buffer, 1024) != 0);
        comp->srcSize = atoi(buffer);

        comp->strand = strand;

        start = start - 1;

        comp->start = start;
        comp->size = ungappedSize(sequence);

        if(strand == '-')
            comp->start = comp->srcSize - (comp->start + comp->size);
        
        comp->text = sequence->dna;
        sequence->dna = 0;
        slAddHead(&ali->components, comp);
        freeDnaSeq(&sequence);

        if(c == '=') {
            fscanf(input, "= score=%lf\n", &score);

            ali->score = score;

            slReverse(&ali->components);
            mafWrite(output, ali);
            mafAliFree(&ali);

            AllocVar(ali);
        }
    }
}

mafWriteEnd(output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 3)
    usage();
xmfaToMaf(argv[1],argv[2]);
return 0;
}
