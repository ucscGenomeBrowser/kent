/* validateFiles - validate format of different track input files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: validateFiles.c,v 1.2 2009/03/12 22:41:01 mikep Exp $";
static char *version = "$Revision: 1.2 $";

#define MAX_ERRORS 10
int maxErrors;
boolean zeroSizeOk;
boolean printOkLines;
boolean printFailLines;
struct hash *chrHash = NULL;
char dnaChars[256];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "validateFiles - validate format of different track input files\n"
  "usage:\n"
  "   validateFiles -type=FILE_TYPE file1 [file2 [...]]\n"
  "options:\n"
  "   -type=(tagAlign|pairedTagAlign)\n"
  "   -chromInfo=file.txt          Specify chromInfo file to validate chrom names and sizes\n"
  "   -maxErrors=N                 Maximum lines with errors to report in one file before \n"
  "                                  stopping (default %d)\n"
  "   -zeroSizeOk                  For BED-type positional data, allow rows with start==end\n"
  "                                  otherwise require start < end\n"
  "   -printOkLines                Print lines which pass validation to stdout\n"
  "   -printFailLines              Print lines which fail validation to stdout\n"
  "   -version                     Print version\n"
  , MAX_ERRORS);
}

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"chromInfo", OPTION_STRING},
   {"maxErrors", OPTION_INT},
   {"zeroSizeOk", OPTION_BOOLEAN},
   {"printOkLines", OPTION_BOOLEAN},
   {"printFailLines", OPTION_BOOLEAN},
   {"version", OPTION_BOOLEAN},
   {NULL, 0},
};

void initDnaChars()
// Set up array of dna chars
// Include colorspace 0-3 as valid dna sequences for SOLiD data
{
int i;
for (i=0 ; i < 256 ; ++i)
    dnaChars[i] = 0;
dnaChars['a'] = dnaChars['c'] = dnaChars['g'] = dnaChars['t'] = dnaChars['n'] = 1;
dnaChars['A'] = dnaChars['C'] = dnaChars['G'] = dnaChars['T'] = dnaChars['N'] = 1;
dnaChars['0'] = dnaChars['1'] = dnaChars['2'] = dnaChars['3'] = 1;
}

struct hash *chromHash(struct chromInfo *ci)
// Return a hash table of chrom name to chrom size
{
unsigned *size;
struct hash *h = newHash(0);
for ( ; ci ; ci = ci->next )
    {
    AllocVar(size);
    *size = ci->size;
    verbose(3,"[%s %3d] hashAdd(%s -> %p = %u)\n", __func__, __LINE__, ci->chrom, size, *size);
    hashAdd(h, ci->chrom, size);
    }
return h;
}

boolean checkString(char *file, int line, char *row, char *s, char *name)
// Return TRUE if string has non-zero length
// Othewise print warning that name column is empty and return FALSE
{
if (strlen(s) > 0)
    {
    verbose(2,"[%s %3d] %s(%s)\n", __func__, __LINE__, name, s);
    return TRUE;
    }
warn("Error [file=%s, line=%d]: %s column empty [%s]", file, line, name, row);
return FALSE;
}

boolean checkChrom(char *file, int line, char *row, char *s, unsigned *chromSize)
// Return TRUE if string has non-zero length
// Othewise print warning that name column is empty and return FALSE
{
unsigned *size;
*chromSize = 0;
if (strlen(s) > 0)
    {
    if (chrHash)
	{
	if ( (size = hashFindVal(chrHash, s)) != NULL)
	    {
	    *chromSize = *size;
	    verbose(2,"[%s %3d] hashFindVal(%s -> %p = %u)\n", __func__, __LINE__, s, size, *size);
	    return TRUE; // found chrom
	    }
	else
	    {
	    warn("Error [file=%s, line=%d]: chrom %s not found [%s]", file, line, s, row);
	    return FALSE; // chrom not found
	    }
	}
    else
	{
	verbose(2,"[%s %3d] chrom(%s) \n", __func__, __LINE__, s);
	return TRUE; // chrom name not blank, and not validating against chromInfo
	}
    }
warn("Error [file=%s, line=%d]: chrom column empty [%s]", file, line, row);
return FALSE;
}

boolean checkSeq(char *file, int line, char *row, char *s, char *name)
// Return TRUE if string has non-zero length and contains only chars [ACGTNacgtn0-3]
// Othewise print warning that name column is empty and return FALSE
{
int i;
for ( i = 0; s[i] ; ++i)
    {
    if (!dnaChars[(int)s[i]])
	{
	warn("Error [file=%s, line=%d]: invalid DNA chars in %s(%s) [%s]", file, line, name, s, row);
	return FALSE;
	}
    }
if (i == 0)
    {
    warn("Error [file=%s, line=%d]: %s column empty [%s]", file, line, name, row);
    return FALSE;
    }
return TRUE;
}

boolean checkStartEnd(char *file, int line, char *row, char *start, char *end, char *chrom, unsigned chromSize)
// Return TRUE if start and end are both >= 0,
// and if zeroSizeOk then start <= end 
//        otherwise  then start < end
// Othewise print warning and return FALSE
{
unsigned s = sqlUnsigned(start);
unsigned e = sqlUnsigned(end);
verbose(2,"[%s %3d] inputLine=%d [%s..%s] -> [%u..%u] (chrom=%s,size=%u) [%s]\n", __func__, __LINE__, line, start, end, s, e, chrom, chromSize, row);
if (chromSize > 0)
    {
    if (e > chromSize)
	{
	warn("Error [file=%s, line=%d]: end(%u) > chromSize(%s=%u) [%s]", file, line, e, chrom, chromSize, row);
	return FALSE;
	}
    else
	verbose(2,"[%s %3d] end <= chromSize (%u <= %u)\n", __func__, __LINE__, e, chromSize);
    }
if (zeroSizeOk)
    {
    if (s <= e)
	{
	verbose(2,"[%s %3d] start <= end (%u <= %u)\n", __func__, __LINE__, s, e);
	return TRUE;
	}
    else
	warn("Error [file=%s, line=%d]: start(%u) > end(%u) [%s]", file, line, s, e, row);
    }
else
    {
    if (s < e)
	{
	verbose(2,"[%s %3d] start < end (%u < %u)\n", __func__, __LINE__, s, e);
	return TRUE;
	}
    else
	warn("Error [file=%s, line=%d]: start(%u) >= end(%u) [%s]", file, line, s, e, row);
    }
return FALSE;
}

boolean checkIntBetween(char *file, int line, char *row, char *val, char *name, int min, int max)
// Return TRUE if val is integer between min and max
// Othewise print warning and return FALSE
{
int i = sqlSigned(val);
verbose(2,"[%s %3d] inputLine=%d [%s] -> [%d] [%s,%d..%d]\n", __func__, __LINE__, line, val, i, name, min, max);
if (i >= min && i <= max)
    {
    verbose(2,"[%s %3d] min <= value <= max (%d <= %d <= %d)\n", __func__, __LINE__, min, i, max);
    return TRUE;
    }
warn("Error [file=%s, line=%d]: %s %d outside bounds (%d, %d) [%s]", file, line, name, i, min, max, row);
return FALSE;
}

boolean checkStrand(char *file, int line, char *row, char *strand)
// Return TRUE if strand == '+' or '-',
// Othewise print warning and return FALSE
{
if (strlen(strand) == 1 && (*strand == '+' || *strand == '-' || *strand == '.'))
    {
    verbose(2,"[%s %3d] strand(%s)\n", __func__, __LINE__, strand);
    return TRUE;
    }
warn("Error [file=%s, line=%d]: invalid strand '%s' (want '+','-','.') [%s]", file, line, strand, row);
return FALSE;
}

int validateTagOrPairedTagAlign(char *file, boolean paired)
{
char *row;
char buf[1024];
char *words[9];
struct lineFile *lf = lineFileOpen(file, TRUE);
int line = 0;
int errs = 0;
unsigned chromSize;
verbose(2,"[%s %3d] paired=%d file(%s)\n", __func__, __LINE__, paired, file);
while (lineFileNextReal(lf, &row))
    {
    ++line;
    safecpy(buf, sizeof(buf), row);
    int n = chopByWhite(buf, words, 9);
    if ( n != (paired ? 8 : 6))
	errAbort("Error: found %d columns, expected %d [%s]\n", n, (paired ? 8 : 6), row);
    if (checkChrom(file, line, row, words[0], &chromSize)
	&& checkStartEnd(file, line, row, words[1], words[2], words[0], chromSize)
	&& checkIntBetween(file, line, row, words[4], "score", 0, 1000)
	&& checkStrand(file, line, row, words[5])
	&& (paired ? 
		(checkString(file, line, row, words[3], "name")
		&& checkSeq(file, line, row, words[6], "seq1") 
		&& checkSeq(file, line, row, words[7], "seq2"))
	    :
		checkSeq(file, line, row, words[3], "sequence")
	    ) )
	{
	if (printOkLines)
	    printf("%s\n", row);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n", row);
	if (++errs >= maxErrors)
	    errAbort("Aborting .. found %d errors\n", errs);
	}
    }
lineFileClose(&lf);
return errs;
}

// tagAlign
// chr1     6082    6117    TCTACTGGCTCTGTGTGTACCAGTCTGTCACTGAG     1000    -
// chr1     7334    7369    AGCCAGGGGGTGACGTTGTTAGATTAGATTTCTTA     1000    +

int validateTagAlign(char *file)
{
return validateTagOrPairedTagAlign(file, FALSE);
}

// pairedTagAlign
// chr10    96316360        96310862        9       1000    +       TCTCACCCGATAACGACCCCCTCCC       TGATCCTTGACTCACTTGCTAATTT
// chr8    126727657       126721865       10      1000    +       AATTCTTCACCTCTCCTGTTCAAAG       TGTGTGAGATCCAAGAATCCTCTCT

int validatePairedTagAlign(char *file)
{
return validateTagOrPairedTagAlign(file, TRUE);
}

void validateFiles(int (*validate)(char *file), int numFiles, char *files[])
/* validateFile - validate format of different track input files. */
{
int i;
int errs = 0;
verbose(2,"[%s %3d] numFiles=%d \n", __func__, __LINE__, numFiles);
for (i = 0; i < numFiles ; ++i)
    {
    errs += validate(files[i]);
    }
verbose(2,"[%s %3d] done loop\n", __func__, __LINE__);
if (errs > 0) 
    errAbort("Aborting ... found %d errors in total\n", errs);
verbose(2,"[%s %3d] done\n", __func__, __LINE__);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *type;
void *func;
struct chromInfo *ci = NULL;
struct hash *funcs = newHash(0);
optionInit(&argc, argv, options);
++argv; 
--argc;
initDnaChars();
if (optionExists("version"))
    errAbort(version);
if (argc==0)
    usage();
type = optionVal("type", "");
if (strlen(type) == 0)
    errAbort("please specify type");
maxErrors      = optionInt("maxErrors", MAX_ERRORS);
zeroSizeOk     = optionExists("zeroSizeOk");
printOkLines   = optionExists("printOkLines");
printFailLines = optionExists("printFailLines");
if (strlen(optionVal("chromInfo", "")) > 0)
    {
    if (!(ci = chromInfoLoadAll(optionVal("chromInfo", ""))))
	errAbort("could not load chromInfo file %s\n", optionVal("chromInfo", ""));
    chrHash = chromHash(ci);
    chromInfoFree(&ci);
    }
verbose(2,"[%s %3d] type=%s\n", __func__, __LINE__, type);
// Setup the function hash keyed by type
hashAdd(funcs, "tagAlign", &validateTagAlign);
hashAdd(funcs, "pairedTagAlign", &validatePairedTagAlign);
if (!(func = hashFindVal(funcs, type)))
    errAbort("Cannot validate %s type files\n", type);
validateFiles(func, argc, argv);
return 0;
}
