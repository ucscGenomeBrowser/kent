/* validateFiles - validate format of different track input files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: validateFiles.c,v 1.5 2009/03/13 16:39:21 mikep Exp $";
static char *version = "$Revision: 1.5 $";

#define MAX_ERRORS 10
int maxErrors;
boolean zeroSizeOk;
boolean printOkLines;
boolean printFailLines;
struct hash *chrHash = NULL;
char dnaChars[256];
char qualChars[256];
char seqName[256];
char digits[256];
char alpha[256];
char csSeqName[256];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "validateFiles - validate format of different track input files\n"
  "usage:\n"
  "   validateFiles -type=FILE_TYPE file1 [file2 [...]]\n"
  "options:\n"
  "   -type=(fastq|csfasta|tagAlign|pairedTagAlign)\n"
  "                                csfasta = Colorspace fasta (SOLiD platform)\n"
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

void initArrays()
// Set up array of chars
// dnaChars:  or DNA chars include colorspace 0-3 as valid dna sequences for SOLiD data
// qualChars: fastq quality scores as ascii [!-~] (ord(!)=33, ord(~)=126)
// seqName:   fastq sequence name chars [A-Za-z0-9_.:/-]
{
int i;
for (i=0 ; i < 256 ; ++i)
    dnaChars[i] = qualChars[i] = seqName[i] = csSeqName[i] = digits[i] = alpha[i] = 0;
dnaChars['a'] = dnaChars['c'] = dnaChars['g'] = dnaChars['t'] = dnaChars['n'] = 1;
dnaChars['A'] = dnaChars['C'] = dnaChars['G'] = dnaChars['T'] = dnaChars['N'] = 1;
dnaChars['0'] = dnaChars['1'] = dnaChars['2'] = dnaChars['3'] = 1;
for (i= (int)'A' ; i <= (int)'Z' ; ++i)
    seqName[i] = seqName[i+(int)('a'-'A')] = alpha[i] = alpha[i+(int)('a'-'A')] = 1;
for (i= (int)'0' ; i <= (int)'9' ; ++i)
    seqName[i] = digits[i] = csSeqName[i] = 1;
seqName['_'] = seqName['.'] = seqName[':'] = seqName['/'] = seqName['-'] = 1;
csSeqName[','] = csSeqName['.'] = csSeqName['-'] = csSeqName['#'] = 1;
for (i= (int)'!' ; i <= (int)'~' ; ++i)
    qualChars[i] = 1;
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

boolean checkSeqName(char *file, int line, char *s, char firstChar, char *name)
// Return TRUE if string has non-zero length and contains only seqName[] chars 
// Othewise print warning that seqName is empty and return FALSE
{
int i;
if (s[0] == 0)
    {
    warn("Error [file=%s, line=%d]: %s empty [%s]", file, line, name, s);
    return FALSE;
    }
else if (s[0] != firstChar)
    {
    warn("Error [file=%s, line=%d]: %s first char invalid (got '%c', wanted '%c') [%s]", 
	file, line, name, s[0], firstChar, s);
    return FALSE;
    }
for ( i = 1; s[i] ; ++i)
    {
    if (!seqName[(int)s[i]])
	{
	warn("Error [file=%s, line=%d]: invalid %s chars in [%s]", file, line, name, s);
	return FALSE;
	}
    }
return TRUE;
}

char *getDigits(char *s)
// Consume 1 or more digits from s, return pointer to next non-digit
// Return NULL if no digits consumed
{
char *s0 = s;
while (digits[(int) *s])
    ++s;
if (s > s0)
    return s;
else
    return NULL;
}

boolean checkTrailingCsSeqName(char *s)
// Return true if all chars in s (if any) are csSeqName chars 
// Return false otherwise
{
while (csSeqName[(int) *s])
    ++s;
if (*s == 0)
    return TRUE;
else
    return FALSE;
}

//     >461_19_209_F3
//     T022213002230311203200200322000
//     >920_22_656_F3,1.-152654094.1.35.35.0###,19.43558664.1.35.35.0###
//     T01301010111200210102321210100112312

boolean checkCsSeqName(char *file, int line, char *s)
// Return TRUE if string has non-zero length, matches CS name pattern contains only csSeqName[] chars 
// Othewise print warning that seqName is empty and return FALSE
{
char *s0;
if (s[0] == 0)
    {
    warn("Error [file=%s, line=%d]: sequence name empty [%s]", file, line, s);
    return FALSE;
    }
else if (s[0] != '>')
    {
    warn("Error [file=%s, line=%d]: sequence name first char invalid (got '%c', wanted '>') [%s]", 
	file, line, s[0], s);
    return FALSE;
    }
if ( (s0 = getDigits(s+1)) 
      && (*(s0++) == '_') 
      && (s0 = getDigits(s0)) && (*(s0++) == '_') 
      && (s0 = getDigits(s0)) && (*(s0++) == '_') 
      && alpha[(int) *(s0++)] && digits[(int) *(s0++)] 
      && checkTrailingCsSeqName(s0) )
    {
    verbose(2,"[%s %3d] OK [%s] file(%s) line=%d\n", __func__, __LINE__, s, file, line);
    return TRUE;
    }
else
    {
    warn("Error [file=%s, line=%d]: invalid sequence name [%s]", file, line, s);
    return FALSE;
    }
}

boolean checkQual(char *file, int line, char *s)
// Return TRUE if string has non-zero length and contains only qualChars[] chars 
// Othewise print warning that quality is empty and return FALSE
{
int i;
for ( i = 0; s[i] ; ++i)
    {
    if (!qualChars[(int)s[i]])
	{
	warn("Error [file=%s, line=%d]: invalid quality chars in [%s]", file, line, s);
	return FALSE;
	}
    }
if (i == 0)
    {
    warn("Error [file=%s, line=%d]: quality empty [%s]", file, line, s);
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

int validateTagOrPairedTagAlign(struct lineFile *lf, char *file, boolean paired)
{
char *row;
char buf[1024];
char *words[9];
int line = 0;
int errs = 0;
unsigned chromSize;
int size;
verbose(2,"[%s %3d] paired=%d file(%s)\n", __func__, __LINE__, paired, file);
while (lineFileNext(lf, &row, &size))
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
return errs;
}

// tagAlign
// chr1     6082    6117    TCTACTGGCTCTGTGTGTACCAGTCTGTCACTGAG     1000    -
// chr1     7334    7369    AGCCAGGGGGTGACGTTGTTAGATTAGATTTCTTA     1000    +

int validateTagAlign(struct lineFile *lf, char *file)
{
return validateTagOrPairedTagAlign(lf, file, FALSE);
}

// pairedTagAlign
// chr10    96316360        96310862        9       1000    +       TCTCACCCGATAACGACCCCCTCCC       TGATCCTTGACTCACTTGCTAATTT
// chr8    126727657       126721865       10      1000    +       AATTCTTCACCTCTCCTGTTCAAAG       TGTGTGAGATCCAAGAATCCTCTCT

int validatePairedTagAlign(struct lineFile *lf, char *file)
{
return validateTagOrPairedTagAlign(lf, file, TRUE);
}

// fastq:
// @NINA_1_FC30G3VAAXX:5:1:110:908
// ATCGTCAGGTGGGATAATCCTTACCTTTTCCTCCTC
// +NINA_1_FC30G3VAAXX:5:1:110:908
// aa`]`a`XQ^VQQ^`aaaaaaa^[[ZG[aXUX[[[X

int validateFastq(struct lineFile *lf, char *file)
{
char *seqName, *seq, *qName, *qual;
int line = 0;
int errs = 0;
boolean startOfFile = TRUE;
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while ( lineFileNext(lf, &seqName, NULL))
    {
    ++line;
    if (startOfFile)
	{
	if (*seqName == '#')
	    continue;
	else
	    startOfFile = FALSE;
	}
    if (checkSeqName(file, line, seqName, '@', "sequence name")
	&& (lineFileNext(lf, &seq, NULL))
	&& checkSeq(file, ++line, seq, seq, "sequence")
	&& (lineFileNext(lf, &qName, NULL))
	&& checkSeqName(file, ++line, qName, '+', "quality name")
	&& (lineFileNext(lf, &qual, NULL))
	&& checkQual(file, ++line, qual) )
	{
	if (printOkLines)
	    printf("%s\n%s\n%s\n%s\n", seqName, seq, qName, qual);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n%s\n%s\n%s\n", seqName, seq, qName, qual);
	if (++errs >= maxErrors)
	    errAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}

// CS Fasta:
// >461_19_209_F3
// T022213002230311203200200322000
// >920_22_656_F3,1.-152654094.1.35.35.0###,19.43558664.1.35.35.0###
// T01301010111200210102321210100112312

int validateCsfasta(struct lineFile *lf, char *file)
{
char *seqName = NULL;
char *seq = NULL; 
int line = 0;
int errs = 0;
boolean startOfFile = TRUE;
verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, file);
while (lineFileNext(lf, &seqName, NULL))
    {
    ++line;
    if (startOfFile)
	{
	if (*seqName == '#')
	    continue;
	else
	    startOfFile = FALSE;
	}
    if (checkCsSeqName(file, line, seqName)
	&& (lineFileNext(lf, &seq, NULL))
	&& checkSeq(file, ++line, seq, seq, "sequence") )
	{
	if (printOkLines)
	    printf("%s\n%s\n", seqName, seq);
	}
    else
	{
	if (printFailLines)
	    printf("%s\n%s\n", seqName, seq);
	if (++errs >= maxErrors)
	    errAbort("Aborting .. found %d errors\n", errs);
	}
    }
return errs;
}

void validateFiles(int (*validate)(struct lineFile *lf, char *file), int numFiles, char *files[])
/* validateFile - validate format of different track input files. */
{
int i;
int errs = 0;
verbose(2,"[%s %3d] numFiles=%d \n", __func__, __LINE__, numFiles);
for (i = 0; i < numFiles ; ++i)
    {
    struct lineFile *lf = lineFileOpen(files[i], TRUE);
    errs += validate(lf, files[i]);
    lineFileClose(&lf);
    }
verbose(2,"[%s %3d] done loop\n", __func__, __LINE__);
if (errs > 0) 
    errAbort("Aborting ... found %d errors in total\n", errs);
verbose(2,"[%s %3d] done\n", __func__, __LINE__);
}

int testFunc(char *f)
{
char *row;
int size;
struct lineFile *lf = lineFileOpen(f, TRUE);
while (lineFileNext(lf, &row, &size))
    printf("size=%d [%s]\n", size, row);
printf("done.\n");
return 0;
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
initArrays();
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
hashAdd(funcs, "fastq", &validateFastq);
hashAdd(funcs, "csfasta", &validateCsfasta);
//hashAdd(funcs, "test", &testFunc);
if (!(func = hashFindVal(funcs, type)))
    errAbort("Cannot validate %s type files\n", type);
validateFiles(func, argc, argv);
return 0;
}
