/* nibFrag - Extract part of a nib file as .fa. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "options.h"
#include "fa.h"


static struct optionSpec optionSpecs[] = {
    {"masked", OPTION_BOOLEAN},
    {"hardMasked", OPTION_BOOLEAN},
    {"upper", OPTION_BOOLEAN},
    {"name", OPTION_STRING},
    {"dbHeader", OPTION_STRING},
    {"tbaHeader", OPTION_STRING},
    {NULL, 0}
};
void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibFrag - Extract part of a nib file as .fa (all bases/gaps lower case by default)\n"
  "usage:\n"
  "   nibFrag [options] file.nib start end strand out.fa\n"
  "where strand is + (plus) or m (minus)\n"
  "options:\n"
  "   -masked       Use lower-case characters for bases meant to be masked out.\n"
  "   -hardMasked   Use upper-case for not masked-out, and 'N' characters for masked-out bases.\n"
  "   -upper        Use upper-case characters for all bases.\n"
  "   -name=name    Use given name after '>' in output sequence.\n"
  "   -dbHeader=db  Add full database info to the header, with or without -name option.\n"
  "   -tbaHeader=db Format header for compatibility with tba, takes database name as argument.\n"
  );
}

void nibFrag(int options, char *nibFile, int start, int end, char strand, 
	     char *faFile, int optUpper, boolean hardMask)
/* nibFrag - Extract part of a nib file as .fa. */
{
struct dnaSeq *seq;
char header[255];
char name[128];
int chromLength;

if (strand != '+' && strand != '-' && strand != 'm')
   usage();
if (strand == 'm')
    strand = '-';
if (start >= end)
   usage();
seq = nibLoadPartMasked(options, nibFile, start, end-start);
if (strand == '-')
    reverseComplement(seq->dna, seq->size);
if (optUpper == 1)
    touppers(seq->dna);
if (hardMask)
    lowerToN(seq->dna, seq->size);
if (optionExists("dbHeader"))
    {
    chromLength = nibGetSize(nibFile);
    splitPath(nibFile, NULL, name, NULL);
    safef(header,sizeof(header),"%s:%s.%s:%d-%d:%c:%d", 
	  optionVal("name", seq->name), optionVal("dbHeader", "db"),
	  name, start, end, strand, chromLength);
    }
else if (optionExists("tbaHeader"))
    {
    chromLength = nibGetSize(nibFile);
    splitPath(nibFile, NULL, name, NULL);
    safef(header,sizeof(header),"%s.%s:%d-%d of %d", 
	  optionVal("tbaHeader", "tba"),
	  name, start, end, chromLength);
    }
else
    safef(header,sizeof(header),"%s",optionVal("name", seq->name));
faWrite(faFile, header, seq->dna, seq->size);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int options = 0;
int optUpper = 0;
boolean hardMask = FALSE;

optionInit(&argc, argv, optionSpecs);
if(optionExists("masked") && optionExists("hardMasked"))
    {
    warn("\nError: Must choose 'masked' or 'hardMasked' but not both.\n");
    usage();
    }
if (optionExists("masked"))
    options = NIB_MASK_MIXED;
if(optionExists("hardMasked"))
    {
    /* setup to read out of nib in mixed case and then
       convert all lower case to 'N' */
    options = NIB_MASK_MIXED;
    hardMask = TRUE;
    }
if (optionExists("upper"))
    optUpper = 1;
if (argc != 6 || !isdigit(argv[2][0]) || !isdigit(argv[3][0]))
    usage();
nibFrag(options, argv[1], atoi(argv[2]), atoi(argv[3]), argv[4][0], argv[5], optUpper, hardMask);
return 0;
}
